/*
 * msl_file_io.c — MSL's fclose, fflush, fopen, freopen, __get_file_modes (mweppc src/msl/file_io.c).
 *
 * Byte-identical between mweppc's GameCube 2.0 compiler and ours; written from our code with the
 * original's bit masks. FILE layout used here:
 *   +0x00 handle               +0x04 mode word: open_mode bits 0-1, io_mode 2-4, file_kind 7-9,
 *   +0x08 state byte: io_state bits 0-2, free_buffer bit 3            orientation 10-11, binary 12
 *   +0x0c eof   +0x0d error    +0x1c position   +0x20 buffer   +0x2c buffer_len
 *   +0x48 close_proc (cdecl, takes the handle)
 */
#include "orig.h"

typedef UInt8 FILE_;
#define F_handle(f) (*(UInt32 *)((f) + 0x00))
#define F_mode(f) (*(UInt16 *)((f) + 0x04))
#define F_modeb0(f) (*(UInt8 *)((f) + 0x04))
#define F_state(f) (*(UInt8 *)((f) + 0x08))
#define F_error(f) (*(UInt8 *)((f) + 0x0d))
#define F_position(f) (*(UInt32 *)((f) + 0x1c))
#define F_buffer(f) (*(void **)((f) + 0x20))
#define F_buflen(f) (*(UInt32 *)((f) + 0x2c))
#define F_close(f) (*(int (**)(UInt32))((f) + 0x48))
#define FILE_KIND(f) ((F_mode(f) >> 7) & 7)
#define IO_MODE(f) ((F_modeb0(f) >> 2) & 7)
#define IO_STATE(f) (F_state(f) & 7)
#define FREE_BUFFER(f) ((F_state(f) >> 3) & 1)

ORIG_FN(int, __flush_all, 0x405640, (void));
ORIG_FN(UInt32, orig_ftell, 0x4105f0, (FILE_ * f));
ORIG_FN(int, __flush_buffer, 0x40c730, (FILE_ * f, UInt32 *length));
ORIG_FN(void, __begin_critical_region, 0x403e70, (int region));
ORIG_FN(void, __end_critical_region, 0x403e90, (int region));
ORIG_FN(FILE_ *, __find_unopened_file, 0x405490, (void));
ORIG_FN(void, __init_file, 0x4054f0, (FILE_ * f, UInt32 modes, void *buffer, UInt32 size));
ORIG_FN(int, __open_file, 0x40c490, (const char *name, UInt32 modes, FILE_ *f));
ORIG_FN(int, orig_fseek, 0x4106f0, (FILE_ * f, SInt32 offset, int whence));
ORIG_FN(void, orig_free, 0x404730, (void *ptr));
ORIG_FN(void, orig_stdio_atexit, 0x410540, (void));
ORIG_FN(void, orig_clearerr, 0x410530, (FILE_ * f));

int fflush_(FILE_ *f);

int fclose_(FILE_ *f) {
    int flush_res, close_res;
    if (!f)
        return -1;
    if (!FILE_KIND(f))
        return 0;
    flush_res = fflush_(f);
    close_res = F_close(f)(F_handle(f));
    F_mode(f) &= 0xfc7f; /* file_kind = 0 */
    F_handle(f) = 0;
    if (FREE_BUFFER(f))
        orig_free(F_buffer(f));
    return (flush_res || close_res) ? -1 : 0;
}
HOOK(0x40c8f0, fclose_);

int fflush_(FILE_ *f) {
    UInt32 pos;
    if (!f)
        return __flush_all();
    if (F_error(f) || !FILE_KIND(f))
        return -1;
    if (IO_MODE(f) == 1) /* read only */
        return 0;
    if (IO_STATE(f) >= 3)
        F_state(f) = (F_state(f) & 0xf8) | 2;
    if (IO_STATE(f) == 2)
        F_buflen(f) = 0;
    if (IO_STATE(f) != 1) {
        F_state(f) &= 0xf8;
        return 0;
    }
    pos = FILE_KIND(f) == 1 ? orig_ftell(f) : 0;
#ifdef RECOMP_NEGATIVE_CONTROL_MSLFILEIO /* --cflags=-DRECOMP_NEGATIVE_CONTROL_MSLFILEIO */
    F_state(f) &= 0xf8;
    F_buflen(f) = 0;
    return 0; /* drops the unwritten buffer */
#endif
    if (__flush_buffer(f, 0)) {
        F_error(f) = 1;
        F_buflen(f) = 0;
        return -1;
    }
    F_state(f) &= 0xf8;
    F_position(f) = pos;
    F_buflen(f) = 0;
    return 0;
}
HOOK(0x40c980, fflush_);

int __get_file_modes_(const char *mode, UInt8 *modes) {
    const char *p = mode + 1;
    int combo = *mode;
    UInt8 open_mode, io_mode = 0;

    *(UInt16 *)modes = (UInt16)((*(UInt16 *)modes & 0xfc7f) | 0x80); /* file_kind = 1 */
    modes[1] &= 0xf3;                                                /* orientation = 0 */
    modes[1] &= 0xef;                                                /* binary_io = 0 */
    switch (*mode) {
    case 'r': open_mode = 0; break;
    case 'w': open_mode = 2; break;
    case 'a': open_mode = 1; break;
    default: return 0;
    }
    modes[0] = (UInt8)((modes[0] & 0xfc) | (open_mode & 3));
    switch (*p++) {
    case 'b':
        modes[1] = (UInt8)((modes[1] & 0xef) | 0x10);
        if (*p == '+')
            combo = (combo << 8) | '+';
        break;
    case '+':
        combo = (combo << 8) | '+';
        if (*p == 'b')
            modes[1] = (UInt8)((modes[1] & 0xef) | 0x10);
        break;
    }
    switch (combo) {
    case 'r': io_mode = 1; break;
    case 'w': io_mode = 2; break;
    case 'a': io_mode = 6; break;
    case ('r' << 8) | '+': io_mode = 3; break;
    case ('w' << 8) | '+': io_mode = 3; break;
    case ('a' << 8) | '+': io_mode = 7; break;
    }
    modes[0] = (UInt8)((modes[0] & 0xe3) | ((io_mode & 7) << 2));
    return 1;
}
HOOK(0x40cb60, __get_file_modes_);

FILE_ *freopen_(const char *name, const char *mode, FILE_ *f) {
    /* the original leaves this uninitialized: __get_file_modes sets only some bits, so the rest
       (buffer_mode bits 5-6, bits 13-31) are its stack leftovers; here they start at 0 */
    UInt32 modes = 0;
    orig_stdio_atexit();
    if (!f)
        return 0;
    fclose_(f);
    orig_clearerr(f);
    if (!__get_file_modes_(mode, (UInt8 *)&modes))
        return 0;
    __init_file(f, modes, 0, 0x1000);
    if (__open_file(name, modes, f)) {
        F_mode(f) &= 0xfc7f;
        if (FREE_BUFFER(f))
            orig_free(F_buffer(f));
        return 0;
    }
    if ((((UInt8)modes >> 2) & 7) & 4) /* append */
        orig_fseek(f, 0, 2);
    return f;
}
HOOK(0x40caa0, freopen_);

FILE_ *fopen_(const char *name, const char *mode) {
    FILE_ *r;
    __begin_critical_region(2);
    r = freopen_(name, mode, __find_unopened_file());
    __end_critical_region(2);
    return r;
}
HOOK(0x40ca70, fopen_);
