#ifndef TIME_INFO_HPP
#define TIME_INFO_HPP

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

std::string toISO8601(int year, int dayOfYear, int hour, int minute, int second);
int get_leap_seconds(uint32_t tai_local, uint32_t* leap_sec_local);
int tai_to_ydhms(uint32_t tai_in, uint16_t* year, uint16_t* doy, uint32_t* sod, uint16_t* hh, uint16_t* mm, uint16_t* ss, std::string& iso8601);
std::string tai_to_iso8601(uint32_t tai);
std::string tai_to_iso8601sss(const std::string& isoTimestamp, uint32_t subseconds);

constexpr double TAI_LEAP_SECONDS = 37.0; //update as needed

// static offset is Jan 1 1958 to Jan 1 1970
constexpr double TAI_EPOCH_OFFSET_TO_UNIX = 378691200.0; // no leap seconds
// leap seconds are non-integers before 1972
// this epoch and leap seconds are added to unix time to create TAI
/// from unix time

// GPS has an epoch of Jan 6, 1980 at 0 UTC
// it is just an offset to TAI
constexpr double GPS_LEAP_SECONDS = TAI_LEAP_SECONDS - 18.0;
constexpr double GPS_EPOCH_OFFSET_TO_TAI = 694224000.0; // no leap seconds
constexpr double GPS_EPOCH_OFFSET_TO_UNIX = GPS_EPOCH_OFFSET_TO_TAI - TAI_EPOCH_OFFSET_TO_UNIX;

constexpr double MICROSECONDS_PER_SECOND = 1000000.0;

// define the TimeInfo class
class TimeInfo {
public:
    TimeInfo();
    void updateNow();

    //void updateTimeComponents();

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

    long calculateTimeDifferenceInMilliseconds(const TimeInfo& other) const;

private:
    std::chrono::system_clock::time_point now;
    int year, dayOfYear, month, dayOfMonth, hour, minute, second;
    double microsecondsSinceEpoch, utcSubseconds;

    static const int64_t taiEpochOffset;

    void updateTimeComponents();
    //static int64_t calculateTAIOffset();
};


#endif //TIME_INFO_HPP