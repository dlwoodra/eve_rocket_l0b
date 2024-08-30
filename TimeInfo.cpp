#include "TimeInfo.hpp"

// implement the TimeInfo class
TimeInfo::TimeInfo() {
    updateNow();
}

void TimeInfo::updateNow() {
    now = std::chrono::system_clock::now();
    updateTimeComponents();
}

void TimeInfo::updateTimeComponents() {
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);

    std::tm now_tm;
    gmtime_r(&now_c, &now_tm);

    auto durationSinceEpoch = now.time_since_epoch();
    microsecondsSinceEpoch = std::chrono::duration_cast<std::chrono::microseconds>(durationSinceEpoch).count();

    auto secondsSinceEpoch = std::chrono::duration_cast<std::chrono::seconds>(durationSinceEpoch).count();
    utcSubseconds = (microsecondsSinceEpoch - (secondsSinceEpoch * MICROSECONDS_PER_SECOND)) / MICROSECONDS_PER_SECOND;

    year = now_tm.tm_year + 1900;
    dayOfYear = now_tm.tm_yday + 1;
    month = now_tm.tm_mon + 1;
    dayOfMonth = now_tm.tm_mday;
    hour = now_tm.tm_hour;
    minute = now_tm.tm_min;
    second = now_tm.tm_sec;
}

int TimeInfo::getYear() { return year; }
int TimeInfo::getDayOfYear() { return dayOfYear; }
int TimeInfo::getMonth() { return month; }
int TimeInfo::getDayOfMonth() { return dayOfMonth; }
int TimeInfo::getHour() { return hour; }
int TimeInfo::getMinute() { return minute; }
int TimeInfo::getSecond() { return second; }

double TimeInfo::getMicrosecondsSinceEpoch() { return microsecondsSinceEpoch; }
double TimeInfo::getUTCSubseconds() { return utcSubseconds; }

double TimeInfo::getTAISeconds() {
    double tai = (microsecondsSinceEpoch / MICROSECONDS_PER_SECOND) + TAI_LEAP_SECONDS + TAI_EPOCH_OFFSET_TO_UNIX;
    return tai;
}

double TimeInfo::getTAISubseconds() const {
    return utcSubseconds;
}

long TimeInfo::calculateTimeDifferenceInMilliseconds(const TimeInfo& other) const {
    auto duration = now - other.now;
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    return static_cast<int>(milliseconds);
}

// int64_t TimeInfo::calculateTAIOffset() {
//     std::tm taiEpoch = {};
//     taiEpoch.tm_year = 58 - 1900;
//     taiEpoch.tm_mon = 0;
//     taiEpoch.tm_mday = 1;
//     taiEpoch.tm_hour = 0;
//     taiEpoch.tm_min = 0;
//     taiEpoch.tm_sec = 0;
//     taiEpoch.tm_isdst = 0;

//     std::time_t taiTime = std::mktime(&taiEpoch);

//     std::tm unixEpoch = {};
//     unixEpoch.tm_year = 70 - 1900;
//     unixEpoch.tm_mon = 0;
//     unixEpoch.tm_mday = 1;
//     unixEpoch.tm_hour = 0;
//     unixEpoch.tm_min = 0;
//     unixEpoch.tm_sec = 0;
//     unixEpoch.tm_isdst = 0;

//     std::time_t unixTime = std::mktime(&unixEpoch);

//     return std::difftime(unixTime, taiTime);
// }
