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

    // Get the current time point
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

    // Extract time since epoch
    std::chrono::system_clock::duration durationSinceEpoch = now.time_since_epoch();

    // Calculate seconds and microseconds since epoch
    std::chrono::seconds secondsSinceEpoch = std::chrono::duration_cast<std::chrono::seconds>(durationSinceEpoch);
    microsecondsSinceEpoch = std::chrono::duration_cast<std::chrono::microseconds>(durationSinceEpoch).count();
    utcSubseconds = (microsecondsSinceEpoch % 1'000'000) / 1'000'000.0;

    // Convert seconds to std::tm
    std::time_t now_c = secondsSinceEpoch.count();
    std::tm now_tm = *std::gmtime(&now_c);

    // Extract components
    year = now_tm.tm_year + 1900;
    dayOfYear = now_tm.tm_yday + 1;
    month = now_tm.tm_mon + 1;
    dayOfMonth = now_tm.tm_mday;
    hour = now_tm.tm_hour;
    minute = now_tm.tm_min;
    second = now_tm.tm_sec;

    // std::time_t now_c = std::chrono::system_clock::to_time_t(now);

    // std::tm now_tm;
    // gmtime_r(&now_c, &now_tm);

    // auto durationSinceEpoch = now.time_since_epoch();
    // microsecondsSinceEpoch = std::chrono::duration_cast<std::chrono::microseconds>(durationSinceEpoch).count();

    // auto secondsSinceEpoch = std::chrono::duration_cast<std::chrono::seconds>(durationSinceEpoch).count();
    // utcSubseconds = (microsecondsSinceEpoch - (secondsSinceEpoch * MICROSECONDS_PER_SECOND)) / MICROSECONDS_PER_SECOND;

    // year = now_tm.tm_year + 1900;
    // dayOfYear = now_tm.tm_yday + 1;
    // month = now_tm.tm_mon + 1;
    // dayOfMonth = now_tm.tm_mday;
    // hour = now_tm.tm_hour;
    // minute = now_tm.tm_min;
    // second = now_tm.tm_sec;
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

// between 0 and 2^32 - 1
uint32_t TimeInfo::getSubSecondTicks() { return (static_cast<uint32_t>(std::round(utcSubseconds * CLOCK_TICKS_PER_SECOND))); }

// between 0.0 and 1.0
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

uint32_t TimeInfo::ydsod_to_tai( uint16_t year, uint16_t doy, uint32_t sod,
		  uint32_t *tai_in, uint8_t return_with_leap_sec  )
{

  //int32_t get_leap_seconds(uint32_t tai_local, uint32_t *leap_sec_local);

  //
  //  10) Declare local variables
  //
  //constexpr uint32_t j2000_tai=1325376000; /* tai @jan 01, 2000 0:00:00UT no leapsec*/
  uint32_t leap_sec;
  uint32_t tai=0;
  constexpr uint32_t seconds_in_year = 365*86400;
  constexpr uint32_t extra_seconds_in_leap_year = 86400;
  constexpr uint32_t tai_epoch_year = 1958;

  //
  //  20) Check input variable ranges
  //
  if (year < tai_epoch_year)
    {
      printf("YDSOD_TO_TAI: ERROR - invalid year input %i\n",year);
      return 1;
    }
  if (doy < 1 || doy > 366)
    {
      printf("YDSOD_TO_TAI: ERROR - invalid doy input %i\n",doy);
      return 1;
    }
  if (sod > 86399)
    {
      printf("YDSOD_TO_TAI: ERROR - invalid sod input %i\n",sod);
      return 1;
    }

  
  //
  //  30) Accumulate seconds for all years since 1958 to year-1
  //
  if ( year > tai_epoch_year )
  {
    for (uint32_t i=tai_epoch_year; i<=(year - (uint32_t) 1); i++)
    {
	  tai+=seconds_in_year;
	  if (!(i & 3)) // faster than % 4
	  { 
	    tai+=extra_seconds_in_leap_year;
	  } 
	}
  }

  //
  //  40) Add seconds in the days
  //
  tai+=((doy-1)*86400);

  //
  //  50) Add seconds in sod
  //
  tai+=sod;

  //
  //  60) Assign result to output
  //
  *tai_in = tai;

  //
  //  70) Return if no leap_second is needed
  //
  if (return_with_leap_sec == (uint8_t) 0) return 0;

  //
  //  80) Add leap seconds
  //
  get_leap_seconds(tai, &leap_sec);
  *tai_in+=(leap_sec);

  //
  //  90) Return
  //
  return 0;
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
