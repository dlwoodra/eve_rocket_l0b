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

int32_t assemble_image( uint8_t * vcdu, MEGS_IMAGE_REC * ptr, uint16_t sourceSequenceCounter, bool testPattern, int8_t *status)
{
  int parityerrors = 0;

  uint16_t expectedparity=0;
  uint16_t pixelparity=0;
  uint32_t src_seq = sourceSequenceCounter;
  constexpr uint32_t MEGS_IMAGE_WIDTH_SHIFT=11; // number of bits to shift
  constexpr uint32_t MEGS_IMAGE_HEIGHT_LESS1 = MEGS_IMAGE_HEIGHT - 1;
  constexpr uint32_t MEGS_IMAGE_WIDTH_LESS1 = MEGS_IMAGE_WIDTH - 1;
  uint16_t pix_val14;
  const uint32_t src_seq_times_pixels_per_half_vcdu = sourceSequenceCounter * PIXELS_PER_HALF_VCDU;

  int32_t xpos, ypos, kk;
  uint32_t not_testpattern;
  uint32_t not_tp2043;

  not_testpattern = static_cast<uint32_t>(!testPattern);
  not_tp2043 = not_testpattern * 2044; /* 2047 - 4 compensates for virtual column insertion */

  // print the byte in the vcdu
  if (sourceSequenceCounter == 0) {
    std::cout << "assemble_image: first vcdu in image to assemble " << std::endl;
    printBytesToStdOut(vcdu, 30, 60);
  }

  // pixel data begins 30 bytes from the start of the VCDU

  // Loop over 875 pixel pairs in packet
  for (int32_t j = 30; j <= 1780; j += 2) 
  {

    // The valid range of byte offsets into the VCDU
    // This check is for the last partial packet (#2394) which has 16 bytes.
    if ((src_seq == 2394) && (j >= 46)) // 30+16
    {
      break;
    }

    // 16-bits allocated to each pixel, top msb is parity, second is framestart, lsb 14 bits are twos comp encoded data
    uint16_t pixval16 = (uint16_t (vcdu[j] << 8) & 0xFF00) | (uint16_t (vcdu[j+1]));

    // rocket fpga messes up the first 2 pixels, then its OK
    // ff ff aa aa 00 02 00 01 00 04 00 02 00 06 00 03 for MEGS-A testpatterns
    // ff ff aa aa 8f fc 87 fe 8f fa 87 fd 0f f8 07 fc for MEGS-B testpatterns
    pix_val14 = pixval16 & 0x3FFF; // 14 bits of data

    // Only do twos comp for ccd data (not test patterns)
    if( not_testpattern )
    {
      // for David's new rocketFPGA, do both parity and 2s complement
      // Decode (2's complement) the 14 bit (with sign) pixel value
      uint16_t tcval = uint16_t (twoscomp_table[pix_val14]);
      pix_val14 = tcval; 
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
      
    uint32_t jrel = ((j - 30)>>1); // the pixel number offset in the vcdu
    // when combined with the src_seq number, a pixel location can be determined
    // jrel is the pixel index 0,1,2... where evens are top and odds are bottom
    // jrel spans from 0 to 875

    // assign pix_val14 to proper x,y or xpos,ypos location in image
    // find whether the jrel pixel is from the top (even) or bottom (odd) half of the CCD
    if ((jrel & 0x1) == 0)
    {
      /* jrel is even, so pixel is in top half */
              
      kk = int32_t (src_seq_times_pixels_per_half_vcdu) + int32_t ((jrel) >> 1);
      ypos = int32_t (kk >> MEGS_IMAGE_WIDTH_SHIFT);
      xpos = (int32_t (kk + not_tp2043) & MEGS_IMAGE_WIDTH_LESS1); 
      // for the mode is fixed topmode=0 always
      // if ( topmode == 1)
      // {
      //   xpos = MEGS_IMAGE_WIDTH_LESS1 - xpos; /* the old simple way */
      // }
    }
      else 
    {
      /* jrel is odd, so pixel is in bottom half */

      kk = int32_t (src_seq_times_pixels_per_half_vcdu) + (int32_t (jrel-1) >> 1);
      ypos = int32_t (MEGS_IMAGE_HEIGHT_LESS1) - (int32_t) (kk >> MEGS_IMAGE_WIDTH_SHIFT);
      xpos = (int32_t (kk + not_tp2043) & MEGS_IMAGE_WIDTH_LESS1); //assumes left
      //if ( bottommode == 1) // always true
      //{
        xpos = MEGS_IMAGE_WIDTH_LESS1 - xpos; 
      //}
    }
          
    // Insert the pixel value into the image in memory            
    ptr->image[xpos][ypos] = (uint16_t) pix_val14;
    // the less complicated way is to have 1024x2048, so here we do all the math using 2048x1024 then switch x and y
    //ptr->image[ypos][xpos] = (uint16_t) pix_val14;

  } //end of j for loop
         
  // increment vcdu counter for assembling the image
  ptr->vcdu_count++;
  *status = NOERROR;
  return parityerrors;
}
