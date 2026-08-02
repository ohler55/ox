/* time_conv.h
 * Copyright (c) 2011, Peter Ohler
 * All rights reserved.
 */

#ifndef OX_TIME_CONV_H
#define OX_TIME_CONV_H

#include <stdint.h>
#include <time.h>

// Howard Hinnant's civil_from_days and days_from_civil, which are inverses.
// Neither consults a timezone database, and that is the point: mktime() reads
// the wall clock as local time no matter what offset the document carries, and
// the Microsoft CRT returns -1 for anything before the epoch.

// Splits seconds since the epoch into a UTC date and wall clock.
inline static void
ox_civil_from_epoch(int64_t sec, long long *yearp, int *monp, int *dayp, int *hourp, int *minp, int *secp) {
    long long days = (long long)sec / 86400;
    long long rem  = (long long)sec % 86400;
    long long era, doe, yoe, doy, mp;

    if (0 > rem) {
        rem += 86400;
        days--;
    }
    *hourp = (int)(rem / 3600);
    *minp  = (int)(rem % 3600 / 60);
    *secp  = (int)(rem % 60);

    // The era starts in March so the leap day is last in the year and needs no
    // special case.
    days += 719468;
    era    = (0 <= days ? days : days - 146096) / 146097;
    doe    = days - era * 146097;
    yoe    = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    doy    = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp     = (5 * doy + 2) / 153;
    *dayp  = (int)(doy - (153 * mp + 2) / 5 + 1);
    *monp  = (int)(10 > mp ? mp + 3 : mp - 9);
    *yearp = yoe + era * 400 + (2 >= *monp);
}

// Joins a UTC date and wall clock back into seconds since the epoch.
inline static int64_t ox_epoch_from_civil(long year, long mon, long day, long hour, long min, long sec) {
    int64_t y = (int64_t)year;
    int64_t era, yoe, doy, doe, days;

    if (2 >= mon) {
        y--;
    }
    era  = (0 <= y ? y : y - 399) / 400;
    yoe  = y - era * 400;
    doy  = (153 * (mon + (2 < mon ? -3 : 9)) + 2) / 5 + day - 1;
    doe  = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    days = era * 146097 + doe - 719468;

    return days * 86400 + (int64_t)hour * 3600 + (int64_t)min * 60 + (int64_t)sec;
}

#endif /* OX_TIME_CONV_H */
