// Clocks, time zones, and conversion between SYSTEMTIME and FILETIME.
#include "win32.h"

#include <time.h>
#ifdef _WIN32
#include <windows.h>
#define tzset _tzset
#endif

static constexpr u32 sMonthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static bool Leap_Year(u32 year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static u32 Month_Days(u32 year, u32 month) {
    return sMonthDays[month - 1] + (month == 2 && Leap_Year(year));
}

u32 KERNEL32_GetTickCount() {
#ifdef _WIN32
    return GetTickCount();
#else
    timespec now = {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<u32>(static_cast<u64>(now.tv_sec) * 1000 + now.tv_nsec / 1000000);
#endif
}

// SYSTEMTIME fields: year, month, weekday, day, hour, minute, second, millisecond.
static void Write_Time(u32 address, const tm* time) {
    if (address && time) {
        const u16 fields[] = {static_cast<u16>(time->tm_year + 1900), static_cast<u16>(time->tm_mon + 1),
            static_cast<u16>(time->tm_wday), static_cast<u16>(time->tm_mday), static_cast<u16>(time->tm_hour),
            static_cast<u16>(time->tm_min), static_cast<u16>(time->tm_sec), 0};
        memcpy(gMemory + address, fields, sizeof(fields));
    }
}

// These return nothing on Windows, so eax stays as the guest had it.
u32 KERNEL32_GetSystemTime(u32 address) {
    time_t now = time(nullptr);
    Write_Time(address, gmtime(&now));
    return cpu.eax;
}

u32 KERNEL32_GetLocalTime(u32 address) {
    time_t now = time(nullptr);
    Write_Time(address, localtime(&now));
    return cpu.eax;
}

u32 KERNEL32_SystemTimeToFileTime(u32 source, u32 destination) {
    if (!source || !destination) {
        return Fail(87);
    }
    u16 fields[8];
    memcpy(fields, gMemory + source, sizeof(fields));
    u32 year = fields[0];
    u32 month = fields[1];
    u32 day = fields[3];
    if (year < 1601 || month < 1 || month > 12 || !day || day > Month_Days(year, month) || fields[4] > 23 ||
        fields[5] > 59 || fields[6] > 59 || fields[7] > 999) {
        return Fail(87);
    }
    u64 days = u64{year - 1601} * 365 + (year - 1) / 4 - (year - 1) / 100 + (year - 1) / 400 - 388 + day - 1;
    for (u32 index = 1; index < month; index++) {
        days += Month_Days(year, index);
    }
    u64 milliseconds = ((days * 24 + fields[4]) * 60 + fields[5]) * 60000 + fields[6] * 1000 + fields[7];
    if (milliseconds > 0x7fffffffffffffffull / 10000) {
        return Fail(87);
    }
    Write<u64>(destination, milliseconds * 10000);
    return 1;
}

u32 KERNEL32_FileTimeToSystemTime(u32 source, u32 destination) {
    if (!source || !destination) {
        return Fail(87);
    }
    u64 ticks = Read<u64>(source);
    if (ticks >> 63) {
        return Fail(87);
    }
    u32 days = static_cast<u32>(ticks / 864000000000ull);
    u32 weekday = (days + 1) % 7;
    u32 year = 1601 + days / 146097 * 400;
    days %= 146097;
    while (days >= 365u + Leap_Year(year)) {
        days -= 365 + Leap_Year(year);
        year++;
    }
    u32 month = 1;
    while (month < 12 && days >= Month_Days(year, month)) {
        days -= Month_Days(year, month++);
    }
    u32 milliseconds = static_cast<u32>(ticks % 864000000000ull / 10000);
    const u16 fields[] = {static_cast<u16>(year), static_cast<u16>(month), static_cast<u16>(weekday),
        static_cast<u16>(days + 1), static_cast<u16>(milliseconds / 3600000),
            static_cast<u16>(milliseconds / 60000 % 60),
        static_cast<u16>(milliseconds / 1000 % 60), static_cast<u16>(milliseconds % 1000)};
    memcpy(gMemory + destination, fields, sizeof(fields));
    return 1;
}

// Fills a TIME_ZONE_INFORMATION from the host zone and returns its daylight state.
u32 KERNEL32_GetTimeZoneInformation(u32 destination) {
    if (!destination) {
        return Fail(87, 0xffffffff);
    }
    tzset();
    time_t now = time(nullptr);
    const tm* local = localtime(&now);
    bool summer = local && local->tm_isdst > 0;
#ifdef _WIN32
    s64 offset = _timezone - (summer && _daylight ? 3600 : 0);
    bool has_daylight = _daylight != 0;
    const char* names[] = {_tzname[0], _tzname[1]};
#else
    s64 offset = local ? -local->tm_gmtoff : 0;
    bool has_daylight = daylight != 0;
    const char* names[] = {tzname[0], tzname[1]};
#endif
    memset(gMemory + destination, 0, 172);
    Write(destination, static_cast<u32>(offset / 60));
    if (!has_daylight || !names[1]) {
        names[1] = names[0];
    }
    for (u32 index = 0; index < 2; index++) {
        for (u32 character = 0; names[index] && character < 31 && names[index][character]; character++) {
            Write<u16>(destination + (index ? 88 : 4) + character * 2, static_cast<u8>(names[index][character]));
        }
    }
    if (has_daylight && summer) {
        Write(destination + 168, 0xffffffc4);
        return 2;
    }
    return 1;
}
