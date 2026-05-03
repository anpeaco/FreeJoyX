/* Strong-symbol overrides for newlib's stub system calls.
 *
 * newlib-nano (libc_nano.a) ships weak default implementations of
 * _close / _lseek / _read / _write that emit a link-time warning of the
 * form "<syscall> is not implemented and will always fail" whenever they
 * are referenced -- even when the linker's --gc-sections later prunes
 * them. The warnings are cosmetic but noisy.
 *
 * Providing strong definitions here lets the linker resolve the
 * references against this translation unit instead of the libc_nano
 * weak stubs, which silences the warnings without changing observed
 * behaviour: every stub still fails the same way, just without the
 * vendor banner.
 *
 * Linked into both the bootloader and the application. No state, no
 * dependencies, never called by FreeJoy code itself; only present to
 * answer newlib's internal references (e.g. printf -> _write_r ->
 * _write) that survive until link time. */

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

int _close(int fd)
{
    (void)fd;
    errno = ENOSYS;
    return -1;
}

int _lseek(int fd, int offset, int whence)
{
    (void)fd; (void)offset; (void)whence;
    errno = ENOSYS;
    return -1;
}

int _read(int fd, char *buf, int count)
{
    (void)fd; (void)buf; (void)count;
    errno = ENOSYS;
    return -1;
}

int _write(int fd, const char *buf, int count)
{
    (void)fd; (void)buf; (void)count;
    errno = ENOSYS;
    return -1;
}

int _fstat(int fd, struct stat *st)
{
    (void)fd; (void)st;
    errno = ENOSYS;
    return -1;
}

int _isatty(int fd)
{
    (void)fd;
    errno = ENOSYS;
    return 0;
}
