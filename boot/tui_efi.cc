#include "tui.h"
#include "bootefi.h"
#include "ctors.h"
#include "tui_scancode.h"
#include "likely.h"
#include "debug.h"

static EFI_GUID efi_simple_text_input_protocol_guid = {
    0x387477c1,0x69c7,0x11d2, {
        0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b
    }
};

static EFI_SIMPLE_TEXT_INPUT_PROTOCOL *efi_simple_text_input;

static EFI_EVENT efi_timer_event;

static int efi_scancode_lookup[] = {
    0,          // EFI_SCAN_NULL       0x0000
    key_up,     // EFI_SCAN_UP         0x0001
    key_down,   // EFI_SCAN_DOWN       0x0002
    key_right,  // EFI_SCAN_RIGHT      0x0003
    key_left,   // EFI_SCAN_LEFT       0x0004
    key_home,   // EFI_SCAN_HOME       0x0005
    key_end,    // EFI_SCAN_END        0x0006
    key_ins,    // EFI_SCAN_INSERT     0x0007
    key_del,    // EFI_SCAN_DELETE     0x0008
    key_pgup,   // EFI_SCAN_PAGE_UP    0x0009
    key_pgdn,   // EFI_SCAN_PAGE_DOWN  0x000A
    key_F1,     // EFI_SCAN_F1         0x000B
    key_F1+1,   // EFI_SCAN_F2         0x000C
    key_F1+2,   // EFI_SCAN_F3         0x000D
    key_F1+3,   // EFI_SCAN_F4         0x000E
    key_F1+4,   // EFI_SCAN_F5         0x000F
    key_F1+5,   // EFI_SCAN_F6         0x0010
    key_F1+6,   // EFI_SCAN_F7         0x0011
    key_F1+7,   // EFI_SCAN_F8         0x0012
    key_F1+8,   // EFI_SCAN_F9         0x0013
    key_F1+9,   // EFI_SCAN_F10        0x0014
    -1,         // 15
    -1,         // 16
    27          // EFI_SCAN_ESC        0x0017
};

template<typename T>
class simple_atomic {
public:
    using value_type = T;

    simple_atomic() noexcept = default;
    simple_atomic(T const& n) noexcept
        : value(n)
    {
    }

    T get() const noexcept
    {
        return __atomic_load_n(&value, __ATOMIC_ACQUIRE);
    }

    void set(T n) noexcept
    {
        __atomic_store_n(&value, n, __ATOMIC_RELEASE);
    }

    T pre_inc() noexcept
    {
        return __atomic_add_fetch(&value, 1, __ATOMIC_RELEASE);
    }

    T post_inc() noexcept
    {
        return __atomic_fetch_add(&value, 1, __ATOMIC_RELEASE);
    }

    simple_atomic &operator=(T n)
    {
        value = n;
        return *this;
    }

private:
    T value;
};

static simple_atomic<int> systime_counter;

static constexpr int keybuf_mask = 15;
static int keybuf_queue[keybuf_mask + 1];
static int keybuf_head, keybuf_tail;

static int service_keyboard()
{
    EFI_STATUS status;

    EFI_INPUT_KEY key{};

    status = efi_simple_text_input->ReadKeyStroke(efi_simple_text_input, &key);

    // If nothing pressed, return 0
    if (status == EFI_NOT_READY)
        return 0;

    debug_out(TSTR "Got keypress\n", -1);

    int translated = -1;

    // If a direct translation exists, look it up
    if (key.ScanCode && key.ScanCode < countof(efi_scancode_lookup)) {
        translated = efi_scancode_lookup[key.ScanCode];

        if (translated > 0) {
            return translated;
        }
    }

    // If it is an ASCII character, return that
    if (key.UnicodeChar < 0x100)
        return key.UnicodeChar;

    // Did not find a way to translate, ignore the keypress
    return 0;
}

static EFIAPI VOID efi_timer_callback(EFI_EVENT Event, VOID *Context)
{
    systime_counter.pre_inc();
}

_constructor(ctor_console) void conin_init()
{
    EFI_STATUS status;

    EFI_HANDLE *efi_text_input_handles;
    UINTN efi_num_text_input_handles;

    status = efi_systab->BootServices->LocateHandleBuffer(
                ByProtocol,
                &efi_simple_text_input_protocol_guid,
                nullptr,
                &efi_num_text_input_handles,
                &efi_text_input_handles);

    if (EFI_ERROR(status))
        halt(TSTR "Unable to query text input handle");

    status = efi_systab->BootServices->HandleProtocol(
                efi_text_input_handles[0],
            &efi_simple_text_input_protocol_guid,
            (VOID**)&efi_simple_text_input);

    if (EFI_ERROR(status))
        halt(TSTR "Unable to query text output interface");

    status = efi_systab->BootServices->CreateEvent(
                EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK,
                efi_timer_callback, nullptr, &efi_timer_event);

    if (EFI_ERROR(status))
        halt(TSTR "Could not create timer event");

    // Set timer tick period to 59.4ms
    status = efi_systab->BootServices->SetTimer(
                efi_timer_event,
                TimerPeriodic, 549000);

    if (EFI_ERROR(status))
        halt(TSTR "Could not set timer");
}

int readkey()
{
    int result = 0;

    if (pollkey())
        result = keybuf_queue[keybuf_tail++ & keybuf_mask];

    return result;
}

// Get ticks since midnight (54.9ms units)
int systime()
{
    return systime_counter.get();
}

void idle()
{
    efi_systab->BootServices->Stall(16000);
}

// Returns true if a key is availble
bool pollkey()
{
    if (((keybuf_head + 1) & keybuf_mask) != (keybuf_tail & keybuf_mask)) {
        // Not full

        int scan = service_keyboard();

        if (scan != 0)
            keybuf_queue[keybuf_head++ & keybuf_mask] = scan;
    } else {
        debug_out(TSTR "Keyboard buffer full!\n", -1);
    }

    if (((keybuf_head & keybuf_mask) == (keybuf_tail & keybuf_mask))) {
        // Empty
        return false;
    }

    return true;
}
