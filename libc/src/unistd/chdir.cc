#include <unistd.h>
#include <sys/syscall.h>
#include <sys/syscall_num.h>
#include <sys/types.h>
#include <errno.h>

int chdir(char const *path)
{
    long status = syscall1(long(path), SYS_chdir);

    if (status >= 0)
        return status;

    errno = -status;

    return -1;
}
