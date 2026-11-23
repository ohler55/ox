/* time_conv.h
 * Copyright (c) 2011, Peter Ohler
 * All rights reserved.
 */

#ifndef OX_TIME_CONV_H
#define OX_TIME_CONV_H

#include <stdint.h>
#include <time.h>

#include "ruby.h"

// ox_time_t is the second count Ox converts Time objects to and from.
//
// Where Ruby's time_t is 64 bits it is time_t, and the conversions go straight
// through rb_time_nano_new() and rb_time_timespec().
//
// Where it is narrower -- 32-bit glibc without _TIME_BITS=64 -- it is int64_t,
// and the conversions go through Time.at, Time#to_i and Time#nsec instead, so
// no time_t crosses into Ruby. That matters twice over. A 32-bit time_t only
// reaches 1901 to 2038, and Ruby's Time does not have that limit. And a
// precompiled binary can meet a Ruby with a different time_t: 32-bit ARM
// distros that moved to a 64-bit time_t (Debian 13, Ubuntu 24.04) build Ruby
// with one, while cross compilers for that target still default to 32 bits. A
// time_t passed between the two is read at the wrong width, and on 32-bit ARM
// that garbles the nanoseconds as well as the seconds.
#if SIZEOF_TIME_T >= 8

typedef time_t ox_time_t;

// The largest time_t, which C has no name for. Built from two halves so that no
// step overflows on its way to the sign bit.
#define OX_TIME_MAX ((time_t)((((time_t)1 << (sizeof(time_t) * 8 - 2)) - 1) + ((time_t)1 << (sizeof(time_t) * 8 - 2))))

// A local Time for sec seconds and nsec (0 to 999,999,999) nanoseconds.
inline static VALUE ox_time_nano_new(ox_time_t sec, long nsec) {
    return rb_time_nano_new(sec, nsec);
}

// Splits a Time into whole seconds, rounded down, and the nanoseconds past
// them, which are never negative.
inline static void ox_time_split(VALUE time, ox_time_t *secp, long *nsecp) {
    struct timespec ts = rb_time_timespec(time);

    *secp  = ts.tv_sec;
    *nsecp = ts.tv_nsec;
}

#else

typedef int64_t ox_time_t;

#define OX_TIME_MAX INT64_MAX

inline static VALUE ox_time_nano_new(ox_time_t sec, long nsec) {
    return rb_funcall(rb_cTime, rb_intern("at"), 3, LL2NUM(sec), LONG2NUM(nsec), ID2SYM(rb_intern("nanosecond")));
}

// Time#to_i rounds down and Time#nsec is never negative, the same split
// rb_time_timespec() makes. A Time past the int64_t range raises RangeError.
inline static void ox_time_split(VALUE time, ox_time_t *secp, long *nsecp) {
    *secp  = (ox_time_t)NUM2LL(rb_funcall(time, rb_intern("to_i"), 0));
    *nsecp = NUM2LONG(rb_funcall(time, rb_intern("nsec"), 0));
}

#endif

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
