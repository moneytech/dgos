#include "iso9660.h"
#include "malloc.h"
#include "diskio.h"
#include "paging.h"
#include "string.h"
#include "cpu.h"
#include "fs.h"
#include "elf64.h"

struct uint8_both_t {
    uint8_t le;
    uint8_t be;
};

struct uint16_both_t {
    uint16_t le;
    uint16_t be;
};

struct uint32_both_t {
    uint32_t le;
    uint32_t be;
};

struct uint64_both_t {
    uint64_t le;
    uint64_t be;
};

struct int8_both_t {
    int8_t le;
    int8_t be;
};

struct int16_both_t {
    int16_t le;
    int16_t be;
};

struct int32_both_t {
    int32_t le;
    int32_t be;
};

struct int64_both_t {
    int64_t le;
    int64_t be;
};

struct iso9660_pvd_datetime_t {
    char year[4];
    char month[2];
    char day[2];
    char hour[2];
    char min[2];
    char sec[2];
    char hundreths[2];
    int8_t tzofs;
};

struct iso9660_datetime_t {
    // Since 1900
    uint8_t year;

    // 1 to 12
    uint8_t month;

    // 1 to 31
    uint8_t day;

    // 0 to 23
    uint8_t hour;

    // 0 to 59
    uint8_t min;

    // 0 to 59
    uint8_t sec;

    // -48 to +52 (15 minute intervals)
    int8_t tz_ofs;
};

// 34 bytes
struct iso9660_dir_ent_t {
    // directory entries cannot span a sector boundary
    // treat 0 len as 1 byte ignored entry
    uint8_t len;
    uint8_t ea_len;

    //
    // ISO, come on, morons. Misaligned fields.

    // Miasligned LBA field
    uint16_t lba_lo_le;
    uint16_t lba_hi_le;
    uint16_t lba_hi_be;
    uint16_t lba_lo_be;

    // Misaligned size field
    uint16_t size_lo_le;
    uint16_t size_hi_le;
    uint16_t size_hi_be;
    uint16_t size_lo_be;

    // 7 bytes
    iso9660_datetime_t date;

    uint8_t flags;

    uint8_t interleave_unit;

    uint8_t interleve_gap;

    int16_both_t disc_number;

    uint8_t filename_len;

    char name[1];

    // ...followed by filename
    // use len field to find next dir entry
};

// Primary Volume Descriptor
struct iso9660_pvd_t {
    uint8_t type_code;

    // Always "CD001"
    char id[5];

    // Always 1
    uint8_t ver;

    char unused[1];

    // The name of the system that can act
    // upon sectors 0x00-0x0F for the volume.
    char system_id[32];

    char volume_id[32];

    char unused2[8];

    int32_both_t block_count;

    char unused3[32];

    uint16_both_t disc_count;

    uint16_both_t disc_number;

    // Probably 2KB
    uint16_both_t block_size;

    uint32_both_t path_table_bytes;

    uint32_t path_table_le_lba;

    uint32_t opt_path_table_le_lba;

    uint32_t path_table_be_lba;

    uint32_t opt_path_table_be_lba;

    // 34 bytes! watch out!
    iso9660_dir_ent_t root_dirent;

    char volume_set_id[128];

    char publisher_id[128];

    char preparer_id[128];

    char app_id[128];

    char copyright_file[38];

    char abstract_file[36];

    char bibliography_id[37];

    iso9660_pvd_datetime_t created_datetime;
    iso9660_pvd_datetime_t modified_datetime;
    iso9660_pvd_datetime_t expiry_datetime;
    iso9660_pvd_datetime_t effective_datetime;

    // Always 1
    uint8_t structure_version;

    char unused4[1];

    char application_specific[512];

    char reserved[653];
};

struct iso9660_sector_iterator_t {
    uint64_t lba;
    uint32_t size;
    uint16_t err;
};

#define MAX_HANDLES 5
iso9660_sector_iterator_t *file_handles;

static char *iso9660_sector_buffer;

typedef int (*iso9660_name_cmp_t)(
        void const *candidate, void const *goal, size_t len);

typedef void *(*iso9660_name_search_t)(
        void const *candidate, int c, size_t len);

static int iso9660_lvl2_cmp(void const *candidate,
                              void const *goal,
                              size_t len);

static iso9660_name_search_t iso9660_name_search = memchr;
static iso9660_name_cmp_t iso9660_name_comparer = iso9660_lvl2_cmp;

static uint32_t iso9660_root_dir_lba;
static uint32_t iso9660_root_dir_size;
static uint64_t iso9660_serial;

static uint8_t iso9660_char_shift;

static uint64_t iso9660_boot_serial()
{
    return iso9660_serial;
}

static int iso9660_find_available_file_handle()
{
    for (size_t i = 0; i < MAX_HANDLES; ++i) {
        if (file_handles[i].lba == 0)
            return i;
    }
    return -1;
}

static uint16_t bswap_16(uint16_t n)
{
    return (n >> 8) | uint16_t(n << 8);
}

static int iso9660_lvl2_cmp(void const *candidate,
                              void const *goal,
                              size_t len)
{
    uint8_t const *g = (uint8_t const *)goal;
    uint8_t const *c = (uint8_t const *)candidate;
    if (c[len-1] == '.')
        --len;
    for (size_t i = 0; i < len; ++i) {
        int8_t expect = *g;
        if (expect >= 'a' && expect <= 'z')
            expect += 'A' - 'a';
       int diff = g[i] - expect;
        if (diff)
            return diff;
    }
    return 0;
}

static int iso9660_joliet_compare(void const *candidate,
                           void const *goal,
                           size_t len)
{
    char const *g = (char const *)goal;
    uint16_t const *c = (uint16_t const *)candidate;
    for (size_t i = 0; i < len; ++i) {
        int diff = g[i] - bswap_16(c[i]);
        if (diff)
            return diff;
    }
    return 0;
}

static void *iso9660_joliet_search(void const *candidate,
                            int c,
                            size_t len)
{
    uint16_t const *can = (uint16_t const *)candidate;
    for (size_t i = 0; i < len; ++i)
        if (bswap_16(can[i]) == c)
            return (void*)can;
    return nullptr;
}

static uint32_t find_file_by_name(char const *filename,
                                  uint64_t dir_lba,
                                  uint32_t dir_size,
                                  uint32_t *file_size)
{
    size_t filename_len = strlen(filename);

    for (uint32_t ofs = 0; ofs < (dir_size >> 11); ++ofs) {
        if (!disk_read_lba(uint64_t(iso9660_sector_buffer),
                              dir_lba + ofs, 11, 1))
            return 0;

        iso9660_dir_ent_t *de = (iso9660_dir_ent_t*)iso9660_sector_buffer;
        iso9660_dir_ent_t *de_end = (iso9660_dir_ent_t*)((char*)de + 2048);

        do {
            if (de->len >= sizeof(*de)) {
                char *name = de->name;
                char *name_end = (char*)iso9660_name_search(
                            name, ';', de->filename_len);
                size_t name_len = name_end - name;
                if (name_len > de->filename_len)
                    name_len = de->filename_len;
                name_len >>= iso9660_char_shift;
                if (filename_len == name_len &&
                        !iso9660_name_comparer(name, filename, name_len)) {
                    *file_size = de->size_lo_le |
                            (de->size_hi_le << 16);
                    return de->lba_lo_le | (de->lba_hi_le << 16);
                }

                de = (iso9660_dir_ent_t*)((char*)de + ((de->len + 1) & -2));
            } else {
                // Give up, rest of space is zeros
                // Directory entries never cross sector boundaries
                break;
            }
        } while (de < de_end);
    }

    return 0;
}

// 0 on success, otherwise BIOS error code
static int8_t iso9660_sector_iterator_begin(
        iso9660_sector_iterator_t *iter,
        char *sector,
        uint64_t cluster,
        uint32_t size)
{
    iter->lba = cluster;
    iter->size = size;

    return disk_read_lba(uint64_t(sector), cluster, 11, 1);
}

static int iso9660_boot_open(char const *filename)
{
    uint32_t cluster;
    uint32_t file_size;

    // Find the start of the file
    cluster = find_file_by_name(
                filename, iso9660_root_dir_lba,
                iso9660_root_dir_size, &file_size);
    if (cluster == 0)
        return -1;

    // Allocate a file handle
    int file = iso9660_find_available_file_handle();
    if (file < 0)
        return -1;

    // Get ready to read the file
    int16_t status = iso9660_sector_iterator_begin(
                file_handles + file, iso9660_sector_buffer,
                cluster, file_size);
    if (status < 0)
        return -1;

    // Return file handle
    return file;
}

static int iso9660_boot_close(int file)
{
    if (file < 0 || file >= MAX_HANDLES)
        return -1;

    int result = 0;

    // Fail the close if the file is in error state
    if (file_handles[file].err)
        result = -1;

    // Mark as available
    memset(file_handles + file, 0, sizeof(*file_handles));

    return result;
}

static ssize_t iso9660_boot_pread(int file, void *buf, size_t bytes, off_t ofs)
{
    if (file < 0 || file >= MAX_HANDLES)
        return -1;

    uint32_t sector_offset = ofs >> 11;
    size_t byte_offset = ofs & ((1 << 11)-1);

    char *output = (char*)buf;

    uint8_t ok;

    ok = disk_read_lba(uint64_t(iso9660_sector_buffer),
                          file_handles[file].lba + sector_offset, 11, 1);

    int total = 0;
    for (;;) {
        // Error?
        if (!ok)
            return -1;

        // EOF?
        if (ofs > file_handles[file].size)
            return 0;

        size_t limit = 2048 - byte_offset;
        if (limit > bytes)
            limit = bytes;

        if (limit == 0)
            break;

        // Copy data from sector buffer
        memcpy(output, iso9660_sector_buffer + byte_offset, limit);

        bytes -= limit;
        output += limit;
        total += limit;

        byte_offset = 0;

        if (bytes > 0) {
            ok = disk_read_lba(
                        uint64_t(iso9660_sector_buffer),
                        file_handles[file].lba + (++sector_offset), 11, 1);
        }
    }

    return total;
}

void iso9660_boot_partition(uint32_t pvd_lba)
{
    file_handles = (iso9660_sector_iterator_t *)
            calloc(MAX_HANDLES, sizeof(*file_handles));

    iso9660_sector_buffer = (char *)malloc(2048);

    iso9660_pvd_t *pvd = (iso9660_pvd_t*)iso9660_sector_buffer;
    uint32_t best_ofs = 0;

    for (uint32_t ofs = 0; ofs < 4; ++ofs) {
        disk_read_lba(uint64_t(iso9660_sector_buffer), pvd_lba + ofs, 11, 1);

        if (pvd->type_code == 2) {
            best_ofs = ofs;
            iso9660_name_search = iso9660_joliet_search;
            iso9660_name_comparer = iso9660_joliet_compare;
            iso9660_char_shift = 1;
            break;
        }
    }

    if (best_ofs == 0)
        disk_read_lba(uint64_t(iso9660_sector_buffer),
                         pvd_lba + best_ofs, 11, 1);

    iso9660_root_dir_lba = pvd->root_dirent.lba_lo_le |
            (pvd->root_dirent.lba_hi_le << 16);

    iso9660_root_dir_size = pvd->root_dirent.size_lo_le |
            (pvd->root_dirent.size_hi_le << 16);

    iso9660_serial = 0;
    for (size_t i = 0, c; i < sizeof(pvd->app_id) &&
         (c = pvd->app_id[i]); ++i) {
        if (c >= '0' && c <= '9')
            c -= '0';
        else if (c >= 'a' && c <= 'z')
            c-= 'a';
        else if (c >= 'A' && c <= 'Z')
            c -= 'A';
        else
            continue;

        iso9660_serial = ((iso9660_serial << 8) |
                          (iso9660_serial >> 56)) ^ (uint8_t)c;
    }

    fs_api.boot_open = iso9660_boot_open;
    fs_api.boot_close = iso9660_boot_close;
    fs_api.boot_pread = iso9660_boot_pread;
    fs_api.boot_drv_serial = iso9660_boot_serial;

    elf64_run(cpu_choose_kernel());
}
