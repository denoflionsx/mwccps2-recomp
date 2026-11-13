/*
 * msl_time.c — MSL's Win32 time helpers (mweppc src/msl/time_win32.c).
 *
 * All 4 functions are byte-identical between mweppc's GameCube 2.0 compiler and ours; written from
 * our code. __get_time/__get_clock turn GetLocalTime into seconds (milliseconds) since 1970 with the
 * original's own leap-year count (signed division by 4, +1 after February) and month table; each has
 * its own copy of the table. __to_gm_time / subtract_time_zone_bias return 0 when
 * GetTimeZoneInformation says TIME_ZONE_ID_UNKNOWN (0), as the original does. 32-bit arithmetic
 * wraps (-fwrapv).
 */
#include "orig.h"

typedef void(__stdcall *GetLocalTime_t)(UInt16 *st);
typedef UInt32(__stdcall *GetTimeZoneInformation_t)(void *tzi);
#define imp_GetLocalTime ORIG_VAR(GetLocalTime_t, 0x547248)
#define imp_GetTimeZoneInformation ORIG_VAR(GetTimeZoneInformation_t, 0x54724c)
#define MonthDays_time ((const SInt32 *)0x52de98)  /* MonthDays$2 */
#define MonthDays_clock ((const SInt32 *)0x52dec8) /* MonthDays$28 */

/* days since 1970-01-01 for a SYSTEMTIME (wYear, wMonth, wDayOfWeek, wDay, ...) */
static SInt32 days_since_1970(const UInt16 *st, const SInt32 *monthDays) {
    SInt32 year = (SInt32)st[0] - 1970, leap = year, days;
    int m;
    if (st[1] > 2)
        leap++;
    days = year * 365 + (leap + 1) / 4; /* C division truncates, like the cdq/sar sequence */
    for (m = 1; m < st[1]; m++)
        days += monthDays[m - 1];
    return days + (SInt32)st[3] - 1;
}

SInt32 __get_time_(void) {
    UInt16 st[8];
    imp_GetLocalTime(st);
    return ((days_since_1970(st, MonthDays_time) * 24 + st[4 + 0]) * 60 + st[5]) * 60 + st[6];
}
HOOK(0x421ec0, __get_time_);

SInt32 __get_clock_(void) {
    UInt16 st[8];
    imp_GetLocalTime(st);
    return (((days_since_1970(st, MonthDays_clock) * 24 + st[4]) * 60 + st[5]) * 60 + st[6]) * 1000 + st[7];
}
HOOK(0x421fa0, __get_clock_);

int __to_gm_time_(SInt32 *t) {
    SInt32 tzi[0xac / 4]; /* TIME_ZONE_INFORMATION; Bias first */
    if (!imp_GetTimeZoneInformation(tzi))
        return 0;
    *t += tzi[0] * 60;
    return 1;
}
HOOK(0x422090, __to_gm_time_);

int subtract_time_zone_bias_(SInt32 *t) {
    SInt32 tzi[0xac / 4];
    if (!imp_GetTimeZoneInformation(tzi))
        return 0;
#ifdef RECOMP_NEGATIVE_CONTROL_MSLTIME /* --cflags=-DRECOMP_NEGATIVE_CONTROL_MSLTIME */
    *t += tzi[0] * 60;
#else
    *t -= tzi[0] * 60;
#endif
    return 1;
}
HOOK(0x4220d0, subtract_time_zone_bias_);
