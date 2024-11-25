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
    microSeconds = microsecondsSinceEpoch % 1'000'000;
    utcSubseconds = microSeconds / 1'000'000.0;

    // Convert seconds to std::tm
    std::time_t now_c = secondsSinceEpoch.count();
    std::tm now_tm = *std::gmtime(&now_c);

    // Extract components
    minute = now_tm.tm_min;
}

int TimeInfo::getMinute() { return minute; } // used in LogFileWriter and RecordFileWriter

double TimeInfo::getMicrosecondsSinceEpoch() { return microsecondsSinceEpoch; } // only now used in test_main.cpp

double TimeInfo::getUTCSubseconds() { return utcSubseconds; } // only now used in test_main.cpp

// between 0 and 2^32 - 1 ; used in commonFunctions.cpp
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

  //Note that TAI at j2000 is 1325376000 + Leap Sec -
  // The TAI count @ Jan 01, 2000 0:00:00UT is shown without leapsec
  uint32_t leap_sec;
  uint32_t tai=0;
  constexpr uint32_t seconds_in_year = 365*86400;
  constexpr uint32_t extra_seconds_in_leap_year = 86400;
  constexpr uint32_t tai_epoch_year = 1958;

  //
  //  Check input variable ranges
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
  //  Accumulate seconds for all years since 1958 to year-1
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
  //  Add seconds in the days since Jan 1
  //
  tai+=((doy-1)*86400);

  //
  //  Add seconds from current day
  //
  tai+=sod;

  //
  //  Assign result to output
  //
  *tai_in = tai;

  //
  //  Return if no leap_second is needed
  //
  if (return_with_leap_sec == (uint8_t) 0) return 0;

  //
  //  Add leap seconds
  //
  get_leap_seconds(tai, &leap_sec);
  *tai_in+=(leap_sec);

  return 0;
}
