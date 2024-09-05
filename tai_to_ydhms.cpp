#include <cstdint>
#include "TimeInfo.hpp"

constexpr uint32_t startdoy = 1;
constexpr uint32_t j2000_tai = 1325376000;
constexpr uint32_t relative_tai = j2000_tai + (86400 * (365 * 5 + 2));

inline int get_leap_seconds(uint32_t tai_local, uint32_t* leap_sec_local) {
    // Function to get leap seconds
    // should read tai-utc.dat, refer to         
    *leap_sec_local = TAI_LEAP_SECONDS;
    return 0;
}

int tai_to_ydhms(uint32_t tai_in, uint16_t* year, uint16_t* doy, uint32_t* sod, uint16_t* hh, uint16_t* mm, uint16_t* ss) {
    uint32_t jepoch_tai = j2000_tai;
    uint16_t npyear = 2000;
    uint32_t leap_sec = 32; //for year 2000
    uint32_t tai = tai_in;

    get_leap_seconds(tai, &leap_sec);
    tai -= leap_sec;

    if (tai >= relative_tai) {
        jepoch_tai = relative_tai;
        npyear = 2005;
    } else if (tai >= j2000_tai) {
        jepoch_tai = j2000_tai;
        npyear = 2000;
    } else {
        jepoch_tai = 0;
        npyear = 1958;
    }

    uint32_t days = (tai - jepoch_tai) / 86400;
    tai -= days * 86400;
    uint16_t dayofyear = startdoy + days;

    while (dayofyear > 365) {
        bool isLeapYear = (npyear % 4 == 0) && ((npyear % 100 != 0) || (npyear % 400 == 0));
        if (isLeapYear && dayofyear > 366) {
            dayofyear -= 366;
            npyear++;
        } else if (!isLeapYear && dayofyear > 365) {
            dayofyear -= 365;
            npyear++;
        } else {
            break;
        }
    }

    *year = npyear;
    *doy = dayofyear;
    *sod = ((tai-jepoch_tai) % 86400);
    *ss = (uint16_t) ((*sod) % 60);
    *mm = (uint16_t) (((*sod)/60) % 60);
    *hh = (uint16_t) ((*sod)/3600);

    return 0;
}


// /*************************************************************
// * FILENAME: tai_to_ydhms.c
// *
// * PURPOSE:
// *  Convert TAI seconds into UTC
// *
// * CATEGORY: 
// *  LIB 
// *
// * CALLING SEQUENCE:
// * tai_to_ydhms(uint32_t tai_in, uint16_t &year, uint16_t &doy, 
// *		 uint32_t &sod,
// *		 uint16_t &hh, uint16_t &mm, uint16_t &ss)
// *
// * INPUTS:
// *  tai : uint32_t - the seconds elapsed since Jan 1, 1958 0 UT
// *
// * OUTPUTS:
// *  year : uint16_t - 4-digit UTC year from 1958 up
// *  doy  : uint16_t - 3-digit UTC day of year from 1-366 inclusive
// *  sod  : uint32_t - UTC seconds of day from 0-86399 inclusive
// *  hh   : uint16_t - UTC seconds of day from 0-86399 inclusive
// *  mm   : uint16_t - UTC seconds of day from 0-86399 inclusive
// *  ss   : uint16_t - UTC seconds of day from 0-86399 inclusive
// *
// * ABNORMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES:
// *  0: OK
// *  1: invalid input - value out of range
// *
// * EXTERNAL VARIABLES:
// * Name    Type    I/O             Description
// * ----------------------------------------------------------
// *  none
// *
// * FILES NEEDED:
// * 
// * Name                 Description
// * ----------------------------------------------------------
// * get_leap_seconds.c   used to get leap seconds
// *
// *
// * PROCEDURE: 
// *  10) Declare local variables
// *  20) Subtract off leap seconds before any conversion
// *  30) Determine optimal epoch to begin search
// *  40) Loop over days since epoch to determine year and day of year
// *  50) Assign results to return variables year and doy
// *  60) Return
// *
// * NOTES:
// *  TAI time DOES NOT include leap seconds. All EVE science telemetry
// *  has TAI time without leap seconds. For UTC time, the leap
// *  seconds must be included and are inserted during convertion.
// *
// * REQUIREMENTS/REFERENCES:
// * 
// * MODIFICATION HISTORY:
// * 
// * Date     Author Change Id Release  Description
// * ------------------------------------------------------------ 
// * ??/??/04 DLW    1         Initial file creation
// * 01/28/05 DLW    2         Included leap second offset
// *
// * $Id: tai_to_ydhms.c,v 1.1 2007/12/11 17:42:06 evesdp Exp $
// *
// * $Log: tai_to_ydhms.c,v $
// * Revision 1.1  2007/12/11 17:42:06  evesdp
// * First revision
// *
// * Revision 1.1  2006/12/08 20:56:58  evesdp
// * Initial install
// *
// * Revision 1.1  2005/12/13 21:30:29  evesdp
// * Initial Save
// *
// * Revision 1.3  2005/01/31 23:06:29  evesdp
// * *** empty log message ***
// *
// * Revision 1.1.1.1  2005/01/28 20:15:55  evesdp
// * Imported Sources
// *
// *
// *************************************************************/

// #include <stdio.h>
// #include <stdint.h>

// #define startdoy 1

// /* j2000_tai was calculated from jan 1, 2000 at 0 UT */
// #define j2000_tai 1325376000

// /* relative_tai is calculated from jan 1 2005 at 0 UT */
// /* this is done only for speed, so the first year of harware is used 2005 */
// /* after launch, we should change it to 2008 instead of 2005 */
// #define relative_tai j2000_tai + (86400*(365*5 + 2)) /*jan 1,05 (2 leap yrs)*/

// int tai_to_ydhms(uint32_t tai_in, uint16_t *year, uint16_t *doy, 
// 		 uint32_t *sod,
// 		 uint16_t *hh, uint16_t *mm, uint16_t *ss)
// {
//   //int32_t get_leap_seconds(uint32_t tai_local, uint32_t *leap_sec_local);

//   //
//   // 10) Declare local variables
//   //
//   uint16_t dayofyear = startdoy, npyear = 2000;
//   uint32_t i;
//   uint32_t jepoch_tai = j2000_tai, tai = tai_in;
//   uint32_t leap_sec = 32; /* value from Jan 1 1999-at least Jun 2005 */

//   //
//   // 20) Subtract off leap seconds before any conversion
//   //
//   //get_leap_seconds((uint32_t) tai,  &leap_sec);
//   leap_sec = 37; // use get_leap_seconds eventually
//   tai -= (leap_sec);

//   //
//   // 30) Determine optimal epoch to begin search
//   //
//   /* note that jepoch must be <= tai or negative numbers cause problems */
//   if (tai >= relative_tai )
//     {
//       jepoch_tai = relative_tai;
//       npyear = 2005;
//     }
//   else
//     {
//       if( tai > j2000_tai )
// 	{
// 	  jepoch_tai = j2000_tai; /* jan 01, 2000 0:00:00UT */
// 	  npyear = 2000;
// 	}
//       else
// 	{
// 	  jepoch_tai = 0; /* tai @jan 01, 1958 0:00:00UT */
// 	  npyear = 1958;
// 	}
//     }
	
//   /* printf("tai passed in %i\n", tai);  */

//   /* days=(tai-jepoch_tai)/86400.e0; */
//   /* printf("days = %f\n",(double) (tai-j2000_tai)/86400.0); */

//   //
//   // 40) Loop over days since epoch to determine year and day of year
//   //
//   for (i=0;i<(uint32_t) ((tai-jepoch_tai)/86400.);i++) 
//     {
//       dayofyear ++; /* increment doy */
//       /* determine if new day is valid (<366 is fine) */
//       if ( dayofyear > 365 ) 
// 	{
// 	  if ( !(npyear & 3) ) 
// 	    {
// 	      /* leap year case */
// 	      /* & 3 is the same as mod 4 */
// 	      /* good enough for determining leap year (fails on year 2100) */
// 	      if ( dayofyear > 366 ) {
// 		(npyear)++;
// 		dayofyear = (uint16_t) 1;
// 	      }
// 	    } 
// 	  else 
// 	    {
// 	      /* not a leap year */
// 	      npyear ++;
// 	      dayofyear = (uint16_t) 1;
// 	    }
// 	}
//     }

//   //
//   // 50) Assign results to return variables
//   //
//   *year = npyear;
//   *doy = dayofyear;
  
//   *sod=(uint32_t) ((tai-jepoch_tai) % 86400);
//   /* printf("sod = %i\n",sod); */

//   /* ss */
//   *ss=(uint16_t) ((*sod) % 60);
//   /* mm */
//   *mm=(uint16_t) (((*sod)/60) % 60);
//   /* hh */
//   *hh=(uint16_t) ((*sod)/3600);
//   /* printf("yeardoy = %4.4i%3.3i - %2.2i:%2.2i:%2.2i\n",*year,*doy,*hh,*mm,*ss)
// ; */

//   //
//   // 60) Return
//   //
//   return 0;
// }


