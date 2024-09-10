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

Date        Author      Change Id       Release      Description of Change
-------------------------------------------------------------------------------
03/2004         Don Woodraska           Original     ---        
09/2004         Brian Templeman         1            Optimization/Byte swap*
07/2006         Brian Templeman         1            Skip parity if SKIPPARITY set
09/13/06        Don Woodraska           1            Compensating for dark pixel INSERTION
************************************************************/
#include "eve_l0b.h"
#include "eve_megs_pixel_parity.h"
#include "eve_megs_twoscomp.h"

int assemble_image( uint8_t * vcdu, struct MEGS_IMAGE_REC * ptr, int8_t *status)
{
  int   parityerrors = 0;

  static uint8_t topmode, bottommode;

  uint16_t parity=0, src_seq;
  uint16_t * u16p = (uint16_t *) vcdu;  // 16 bit pointer to data
  uint16_t MEGS_IMAGE_WIDTH_SHIFT=11;
  uint16_t MEGS_IMAGE_HEIGHT_LESS1=MEGS_IMAGE_HEIGHT-1, MEGS_IMAGE_WIDTH_LESS1=MEGS_IMAGE_WIDTH-1;
  unsigned short pix_val;
  uint32_t src_seq_times_pixels_per_half_vcdu;  

  register uint32_t j, jrel, ki, kj, kk;
  uint32_t halfj;
  static uint32_t istestpattern, not_istestpattern, not_tp2043;

  src_seq    = u16p[8] & 0x3fff;                        // Sequence number
  src_seq_times_pixels_per_half_vcdu = src_seq*PIXELS_PER_HALF_VCDU;

  // if megs_image_rec has vcdu_count=0, then assign all header info
  if (ptr->vcdu_count == 0)
    {

      topmode    = getbit8( vcdu[28], 4 );      // topmode
      bottommode = getbit8( vcdu[28], 5 );      // bottom mode

      /* time hi */
      ptr->tai_time_seconds = ps_headers.tai_time_seconds;
          
      // time lo 
      ptr->tai_time_subseconds = ps_headers.tai_time_subseconds;

      // int_time (DN units, so seconds=DN*10)
      ptr->integration_time = vcdu[29];

      // Validity bit
      ptr->valid = getbit8( vcdu[28], 0 );

      // Ram bank
      ptr->ram_bank = getbit8( vcdu[28], 1 );

      // Integration warning
      ptr->int_time_warn = getbit8( vcdu[28], 3 );

      // Reverse clock
      ptr->reverse_clock = getbit8( vcdu[28], 6 );

      // hw and sw Test bits
      ptr->hw_test = getbit8( vcdu[28], 2 );
      ptr->sw_test = getbit8( vcdu[28], 7 );

      // readout_mode
      ptr->readout_mode = ( topmode << 1 ) + bottommode;
// ********* Modify to only check parity for hardware test patterns *************
      istestpattern = (ptr->hw_test) + (ptr->sw_test);
      not_istestpattern = (istestpattern + 1) & 0x1;
      not_tp2043 = not_istestpattern * 2044; /* 2047 - 4 compensates for dark pixel insertion */
// ********* Modify to only check parity for hardware test patterns *************
    }
    else
    {
    	if( (ptr->hw_test != getbit8( vcdu[28], 2 )) || (ptr->hw_test != getbit8( vcdu[28], 2)) )
    	{
   			printf("Mode changed to/from test pattern\n" );
    		if(ptr->valid == getbit8( vcdu[28], 0))
    		{
    			printf("Mode changed to/from test pattern and valid bit is stil set\n" );
    		}
    	}
    }

/*    
  if(!ptr->valid)
    {
      ptr->vcdu_count = 0;
      return 0;
    }
*/

  // Loop over 875 pixel pairs in packet
  for (j = 30; j <= 1780; j += 2)
    {
      jrel = (int) ((j - 30)>>1); // the pixel number offset in the vcdu
      // when combined with the src_seq number, a pixel can be determined

      halfj = j>>1;
      // The valid range of byte offsets into the VCDU, DO NOT EXCEED
      // This check is for the last partial packet (#2394) which has 16 bytes.
      if ((src_seq == 2394) && (j >= 46)) 
        {
          break;
        }

      uint16_t pixval16 = u16p[halfj];
      pix_val = pixval16 & 0x3FFF;

      // Only check parity if NOT a test pattern
      //  and image is marked valid
      if( (istestpattern == 0) && ((ptr->valid) == 1) )
      {
          /* now perform parity check for each pixel */
          /* if a pixel passes parity check, copy the vcdu pixel data to the image */
          /*  ODD-PARITY ON 14 LSBs */

// #ifndef SKIPPARITY
          parity = odd_parity_15bit_table[pixval16 & 0x7FFF];
// #endif
          // Decode (2's complement) the 14 bit (with sign) pixel value
          pix_val = twoscomp_table[pixval16 & 0x3FFF];
      }
      else
      {
    	  parity = ( u16p[halfj] >> 15 ) & 0x01;
      }

      // parity is parity bit + start bit OR if a test pattern
#ifndef SKIPPARITY
      if( parity ==  (( u16p[halfj] >> 15 ) & 0x01) )
#else
      if( TRUE )
#endif
      {
          /* parity check passed, assign pix_val to proper location in image */
          /*  is the pixel from the top (even) or bottom half of the CCD? */
          if ((jrel & 0x1) == 0)
            {
              /* j is even, so pixel is in top half */
              
              kk = (src_seq_times_pixels_per_half_vcdu) + ((jrel) >> 1);
              kj = (int) (kk >> MEGS_IMAGE_WIDTH_SHIFT);
              /* ki = (int) (kk & MEGS_IMAGE_WIDTH_LESS1);  the old simple way */
              ki = (int) ((kk + not_tp2043) & MEGS_IMAGE_WIDTH_LESS1); 
              if ( topmode == 1)
                {
                  ki = (int) MEGS_IMAGE_WIDTH_LESS1 - ki; /* the old simple way */
                }
            }
          else 
            {
              /* j is odd, so pixel is in bottom half */
              kk = (int) src_seq_times_pixels_per_half_vcdu + ((jrel-1) >> 1);
              kj = (int) MEGS_IMAGE_HEIGHT_LESS1 - (int) (kk >> MEGS_IMAGE_WIDTH_SHIFT);
              /* ki = (int) (kk & MEGS_IMAGE_WIDTH_LESS1); //assumes left */
              ki = (int) ((kk + not_tp2043) & MEGS_IMAGE_WIDTH_LESS1); //assumes left
              if ( bottommode == 1)
                {
                  ki = (int) MEGS_IMAGE_WIDTH_LESS1 - ki;
                }
            }
          
          // can use next line to flip the image vertically, NOT NEEDED
          // kj = MEGS_IMAGE_HEIGHT - 1 - kj;

          // Insert the pixel value into the image in memory            
          ptr->image[ki][kj] = (unsigned short) pix_val;
        }
      else
        {
#ifndef SKIPPARITY
          parityerrors ++;
          *status = W_INVALID_PARITY;
#endif
        }
    }
  // increment vcdu counter for assembling the image
  ptr->vcdu_count++;
  *status = NOERROR;
  return parityerrors;
}
