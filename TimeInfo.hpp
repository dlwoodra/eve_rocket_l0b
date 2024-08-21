#ifndef TIME_INFO_HPP
#define TIME_INFO_HPP

#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>

const double TAI_LEAP_SECONDS = 37.0; //update as needed

// static offset is Jan 1 1958 to Jan 1 1970
const double TAI_EPOCH_OFFSET_TO_UNIX = 378691200.0; // no leap seconds
// leap seconds are non-integers before 1972
// this epoch and leap seconds are added to unix time to create TAI
/// from unix time

// GPS has an epoch of Jan 6, 1980 at 0 UTC
// it is just an offset to TAI
const double GPS_LEAP_SECONDS = TAI_LEAP_SECONDS - 18.0;
const double GPS_EPOCH_OFFSET_TO_TAI = 694224000.0; // no leap seconds
const double GPS_EPOCH_OFFSET_TO_UNIX = GPS_EPOCH_OFFSET_TO_TAI - TAI_EPOCH_OFFSET_TO_UNIX;

const double MICROSECONDS_PER_SECOND = 1000000.0;

// define the TimeInfo class
class TimeInfo {
public:
    TimeInfo();
    void updateNow();
    int getYear();
    int getDayOfYear();
    int getMonth();
    int getDayOfMonth();
    int getHour();
    int getMinute();
    int getSecond();
    double getMicrosecondsSinceEpoch();
    double getUTCSubseconds();
    double getTAISeconds();
    double getTAISubseconds() const;

private:
    std::chrono::system_clock::time_point now;
    int year, dayOfYear, month, dayOfMonth, hour, minute, second;
    double microsecondsSinceEpoch, utcSubseconds;

    //static constexpr double TAI_OFFSET = 37.0;
    static const int64_t taiEpochOffset;

    void updateTimeComponents();
    static int64_t calculateTAIOffset();
};

#endif