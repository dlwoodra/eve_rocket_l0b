/*************************************************************
$Id: assemble_image.c,v 1.2 2008/01/22 23:56:59 evesdp Exp $

FILENAME: assemble_image.c

PURPOSE:  Decomposes a tlm data packet and fills a buffer with re-organized
        image data.  Used with Megs A and Megs B images

FILE REFERENCES:

Name            I/O             Description
----------------------------------------------------------
eve_l0b.h   Input               Main header
eve_meg_pixel_parity.h          Parity lookup table
eve_megs_twos_comp.h            2s complement + shift lookup table

EXTERNAL VARIABLES:
Name    Type    I/O             Description
----------------------------------------------------------

ABNORMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES:

NOTES:
Dark pixel insertion causes the spectral lines to be split by 8 pixels across the 
top/botom boundary (in TL/BR mode)

REQUIREMENTS/REFERENCES:

DEVELOPMENT HISTORY:

Date     Author          Change Id Description of Change
------------------------------------------------------------------------------
03/2004  Don Woodraska   Original  ---        
09/2004  Brian Templeman 1         Optimization/Byte swap*
07/2006  Brian Templeman 1         Skip parity if SKIPPARITY set
09/13/06 Don Woodraska   1         Compensating for dark pixel INSERTION
08/17/24 Don Woodraska   2         Modified for David's rocket FPGA to support SURF
************************************************************/

#include "byteswap.hpp"
#include "commonFunctions.hpp"
#include "eve_l0b.hpp"
#include "eve_megs_pixel_parity.h"
#include "eve_megs_twoscomp.h"
#include <iostream>
#include <iomanip>

int32_t assemble_image( uint8_t * vcdu, MEGS_IMAGE_REC * ptr, uint16_t sourceSequenceCounter, 
  bool testPattern, int32_t& xpos, int32_t& ypos, int8_t *status)
{
  int parityerrors = 0;

  uint16_t expectedparity=0;
  uint16_t pixelparity=0;
  uint32_t src_seq = sourceSequenceCounter;
  constexpr int32_t MEGS_IMAGE_WIDTH_SHIFT=11; // number of bits to shift
  constexpr int32_t MEGS_IMAGE_HEIGHT_LESS1 = MEGS_IMAGE_HEIGHT - 1;
  constexpr int32_t MEGS_IMAGE_WIDTH_LESS1 = MEGS_IMAGE_WIDTH - 1;
  constexpr int32_t PIXELS_PER_HALF_VCDU = 438;
  uint16_t pix_val14;
  const int32_t src_seq_times_pixels_per_half_vcdu = sourceSequenceCounter * PIXELS_PER_HALF_VCDU;

  int32_t kk, jrel;
  uint32_t not_tp2043;

  not_tp2043 = (!testPattern) * 2044; /* 2047 - 4 compensates for virtual column insertion */
  *status = NOERROR; // default to no error

  // pixel data begins 30 bytes from the start of the VCDU

  // Loop over 875 pixel pairs in packet
  for (int32_t j = 30; j <= 1780; j += 2) 
  {
    jrel = (j-30)>>1;
    // jrel is the pixel number offset in the vcdu (0-874)

    // The valid range of byte offsets into the VCDU
    // This check is for the last partial packet (#2394) which has 16 bytes.
    if ((src_seq == 2394) && (j >= 46)) // 30+16
    {
      //continue;
      break;
    }

    // 16-bits allocated to each pixel, top msb is parity, second is framestart, lsb 14 bits are twos comp encoded data
    //uint16_t pixval16 = (uint16_t (vcdu[j] << 8) & 0xFF00) | (uint16_t (vcdu[j+1]));
    uint16_t pixval16 = (static_cast<uint16_t>(vcdu[j] << 8) & 0xFF00) | vcdu[j + 1];

  // fix the first 2 pixels
  if ( sourceSequenceCounter == 0 && ((j == 30) || (j == 32)) ) {
    //pixval16 = (uint16_t (vcdu[j+4] << 8) & 0xFF00) | (uint16_t (vcdu[j+5]));
    pixval16 = (static_cast<uint16_t>(vcdu[j + 4] << 8) & 0xFF00) | vcdu[j + 5];
  }

    // rocket fpga messes up the first 2 pixels, then its OK
    // ff ff aa aa 00 02 00 01 00 04 00 02 00 06 00 03 for MEGS-A testpatterns
    // ff ff aa aa 8f fc 87 fe 8f fa 87 fd 0f f8 07 fc for MEGS-B testpatterns
    pix_val14 = pixval16 & 0x3FFF; // 14 bits of data

    // Only do twos comp for ccd data (not test patterns)
    if( !testPattern )
    {
      // for David's new rocketFPGA, do both parity and 2s complement
      // Decode (2's complement) the 14 bit (with sign) pixel value
      //uint16_t tcval = uint16_t (twoscomp_table[pix_val14]);
      //pix_val14 = tcval; 
      pix_val14 = static_cast<uint16_t>(twoscomp_table[pix_val14]);
    }

#ifndef SKIPPARITY
    // Always check parity for the rocket
    // parity is in msb
    pixelparity = ( pixval16 >> 15 ) & 0x01;
    expectedparity = odd_parity_15bit_table[pixval16 & 0x7FFF];

    if( expectedparity != pixelparity ) 
    {
      parityerrors++;
      *status = W_INVALID_PARITY;

    }
#endif

    // assemble regardless of whether the parity check fails
      
    //jrel = ((j - 30)>>1); // the pixel number offset in the vcdu
    // jrel is the pixel number in the VCDU, 0 to 875 with top/bottom interleaved
    // when combined with the src_seq number, a pixel location can be determined
    // jrel is the pixel index 0,1,2... where evens are top and odds are bottom

    // assign pix_val14 to proper x,y or xpos,ypos location in image
    // find whether the jrel pixel is from the top (even) or bottom (odd) half of the CCD

    kk = src_seq_times_pixels_per_half_vcdu + (jrel>>1);
    ypos = (jrel & 0x1) == 0 ? (kk >> MEGS_IMAGE_WIDTH_SHIFT) : (MEGS_IMAGE_HEIGHT_LESS1) - (kk >> MEGS_IMAGE_WIDTH_SHIFT);
    xpos = ((kk + not_tp2043) & MEGS_IMAGE_WIDTH_LESS1);
    if ((jrel & 0x1) == 1) {
      xpos = MEGS_IMAGE_WIDTH_LESS1 - xpos;
    }

    // Insert the pixel value into the image in memory            
    ptr->image[ypos][xpos] = pix_val14;

  } //end of j for loop
         
  // increment vcdu counter for assembling the image
  ptr->vcdu_count++;

  return parityerrors;
}
