#include "dev_storage.h"

struct dev_fs_t final : public fs_base_t {
    FS_BASE_RW_IMPL

    struct file_handle_t : public fs_file_info_t {
        enum type_t {
            NODE,
            DIR
        };

        file_handle_t(type_t type)
            : type(type)
        {
        }

        type_t type;
    };

    struct node_handle_t : public file_handle_t {
        node_handle_t()
            : file_handle_t(NODE)
        {
        }
    };

    struct dir_handle_t : public file_handle_t {
        dir_handle_t()
            : file_handle_t(DIR)
        {
        }
    };
};

static dev_fs_t dev_fs;

void dev_fs_t::unmount()
{

}

bool dev_fs_t::is_boot() const
{
    return false;
}

int dev_fs_t::opendir(fs_file_info_t **fi, fs_cpath_t path)
{
    return -int(errno_t::ENOENT);
}

ssize_t dev_fs_t::readdir(fs_file_info_t *fi, dirent_t* buf, off_t offset)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::releasedir(fs_file_info_t *fi)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::getattr(fs_cpath_t path, fs_stat_t* stbuf)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::access(fs_cpath_t path, int mask)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::readlink(fs_cpath_t path, char* buf, size_t size)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::mknod(fs_cpath_t path, fs_mode_t mode, fs_dev_t rdev)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::mkdir(fs_cpath_t path, fs_mode_t mode)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::rmdir(fs_cpath_t path)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::symlink(fs_cpath_t to, fs_cpath_t from)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::rename(fs_cpath_t from, fs_cpath_t to)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::link(fs_cpath_t from, fs_cpath_t to)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::unlink(fs_cpath_t path)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::chmod(fs_cpath_t path,
     fs_mode_t mode)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::chown(fs_cpath_t path, fs_uid_t uid, fs_gid_t gid)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::truncate(fs_cpath_t path, off_t size)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::utimens(fs_cpath_t path, fs_timespec_t const *ts)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::open(fs_file_info_t **fi, fs_cpath_t path,
                   int flags, mode_t mode)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::release(fs_file_info_t *fi)
{
    return -int(errno_t::ENOSYS);
}

ssize_t dev_fs_t::read(fs_file_info_t *fi,
        char *buf,
        size_t size,
        off_t offset)
{
    return -int(errno_t::ENOSYS);
}

ssize_t dev_fs_t::write(fs_file_info_t *fi, char const *buf,
                        size_t size, off_t offset)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::ftruncate(fs_file_info_t *fi, off_t offset)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::fstat(fs_file_info_t *fi, fs_stat_t *st)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::fsync(fs_file_info_t *fi, int isdatasync)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::fsyncdir(fs_file_info_t *fi, int isdatasync)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::flush(fs_file_info_t *fi)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::lock(fs_file_info_t *fi, int cmd, fs_flock_t* locks)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::bmap(fs_cpath_t path, size_t blocksize, uint64_t* blockno)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::statfs(fs_statvfs_t* stbuf)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::setxattr(fs_cpath_t path, char const* name, char const* value,
                       size_t size, int flags)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::getxattr(fs_cpath_t path, char const* name,
                       char* value, size_t size)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::listxattr(fs_cpath_t path, char const* list, size_t size)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::ioctl(fs_file_info_t *fi, int cmd, void* arg,
                    unsigned int flags, void* data)
{
    return -int(errno_t::ENOSYS);
}

int dev_fs_t::poll(fs_file_info_t *fi, fs_pollhandle_t* ph, unsigned* reventsp)
{
    return -int(errno_t::ENOSYS);
}
