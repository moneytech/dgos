#include <fcntl.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <errno.h>

int open(char const *path, int flags, ...)
{
    va_list ap;
    va_start(ap, flags);
    mode_t mode = va_arg(ap, mode_t);
    va_end(ap);

    long status = syscall3(long(path), long(flags), long(mode), SYS_open);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
