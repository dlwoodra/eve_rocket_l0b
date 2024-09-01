/* EVE_l0b.h */
#ifndef EVE_l0b_defined
#define EVE_l0b_defined

#define DEBUG

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <byteswap.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/file.h>
#include <wchar.h>

#define BAD_PIXEL 16384 			// 2^14	- Image fill value

#define n_bytes_per_syncmarker 4		// The sync marker is 4 bytes
#define n_bytes_per_vcdu 1784			// Includes CRC 16bit word
#define n_bytes_per_vcdu_header 28		// The vcdu header is 28 bytes
#define n_bytes_per_vcdu_data 1756		// Each vcdu is 1756 bytes

#define megsa1boundary 556  // Y		The pixel value (row) defining the row between Slit 1 and Slit 2
#define megsa2boundary 750  // X		The pixel value (column) defining the row between Slit 2 and SAM

#define PIXELS_PER_HALF_VCDU 438	 	// 1752 bytes or 876 total pixels/vcdu
#define FONTWIDTH 32				// Used in quicklook images
#define FONTHEIGHT 32				// Used in quicklook images
#define skipval 4				// Scale quicklook images by this factor

//#define SYNC_MARKER 0x1ACFFC1D			// The SDO defined sync marker
#define BYTE_SWAPPED_SYNC_MARKER 0xFC1D1ACF	// The sync marker byte swapped

#define MAX_STRING_LENGTH		512		// Maximum string length value
#define N_PKT_PER_IMAGE 		2395	// There are 2395 packets in each image
#define MAX_MEGS_A_IMAGES_PER_FILE 	7		//	No more than 7 images per minute possible
#define MAX_MEGS_B_IMAGES_PER_FILE 	7		//	No more than 7 images per minute possible
#define MEGS_IMAGE_WIDTH 		2048	// The CCD images are 2048 x 1024 pixels
#define MEGS_IMAGE_HEIGHT 		1024
#define PHOTO_SAMPLES_PER_10SEC 	40		// ESP is sampled at 4 Hz
#define Y_TOP 						0		// Image orientation
#define Y_MIDDLE 					511		//  The middle row
#define Y_BOTTOM					1023 
#define X_DARK_COLUMNS 				4		// The number of dark columns
#define X_DARK_START_LEFT 			0		// The starting column for dark pixels
#define X_DARK_START_RIGHT MEGS_IMAGE_WIDTH - X_DARK_COLUMNS

// RateMeterPixel Conversion
// Valid range of X is 0 to 2047
// Valid range of Y is 0 to 511
#define RateMeterPixel_X 1023
#define RateMeterPixel_Y 255
// Convert to L0B image X, Y
#define RM_Top_X RateMeterPixel_X
#define RM_Top_Y 1023 - RateMeterPixel_Y
#define RM_Bottom_X 2047 - RateMeterPixel_X
#define RM_Bottom_Y RateMeterPixel_Y

// The APID's for EVE
//#define MEGS_A_APID 	601
//#define MEGS_B_APID 	602
//#define PHOTOMETER_APID 603
//#define SHK_APID    	604
//#define SHK_ECHO_APID   605
//#define MA1_APID	900
//#define MA2_APID	901
//#define SAM_APID	902

// DEFINE ERROR CODES SPECIFIC TO L0B
#define ERROR 			-1		// USED as default value per previous versions
#define NOERROR			0

// General Errors and Warnings
#define E_INVALID_SYNC		101			// Invalid Sync Marker
#define E_INVALID_CRC 		102			// Invalid CRC
#define E_MEMORY_ALLOC		103			// Invalid Memory Allocation Error
#define W_MISSING_PACKITS 	104			// Not all packets received
#define EOF_REACHED			105			// Reached EOF before processing all packets
#define WRONG_APID			106			// Procedure received wrong APID
#define NEXT_IMAGE 			107			// The packet received starts a new image - NOT AN ERROR
#define COMMAND_LINE_ERROR	150			// Error in command line 
#define FILE_NOT_VALID		151			// The file entered on the command is not a valid TLM file
#define BAD_ENVIRONMENT		152			//  The environment can not be read or bad env variable
#define QUICKLOOK_ERROR		153			//  Generic quicklook error
#define OPEN_FILE_ERROR		154			//  Error opening file

// MEGS ERORS
#define W_INVALID_PARITY 	201			// MEGS image - Invalid Parity

// FITS Errors
#define E_FITS_IO			401			// Error Reading/Writing Fits file

// The time - Time of first packet in file
//extern uint32_t starttime;

//  Storage for file names on command line
//char filenames[2][MAX_STRING_LENGTH];		

// Define arrays for test images
//uint16_t testAimage[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT];
//uint16_t testBimage[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT];
//uint16_t testAimageSW[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT];
//uint16_t testBimageSW[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT];

// Command line switches
//bool ImagesMade;
//bool doByteSwap, nocrc, noquicklook, nolog;
//bool nomegsa, nomegsb, noshk, nophoto, routineplots;

// User defined include files
#include "eve_structures.h"
//#include "eve_graphics.h"
//#include "rainbow_colors.h"
//#include "fits_rw.h"
//#include "eveplot.h"
//#include "filter_pos.h"
//#include "png.h"
//#include "logutilities.h"
//#include "ccsds_crc_tab.h"
//#include "gsl/gsl_statistics.h"
//#include "gsl/gsl_fft_real.h"


// +++++++++++++++++  The procedure prototypes  ++++++++++++++++++

int assemble_image( uint8_t * vcdu, struct MEGS_IMAGE_REC * ptr, int8_t *status);
int	processPHOTOpacket(uint32_t *esp_time_seconds, uint32_t *esp_index, uint8_t *vcdu_data);
//int	processSHKpacket(uint32_t *shk_time_seconds, int *shk_index, int *esp_index, uint8_t *vcdu_data, struct MEGS_IMAGE_REC *ptr_ma, struct MEGS_IMAGE_REC *ptr_mb);
//inline void read_header( uint8_t *vcdu );  
inline int WHERE( uint32_t tai_time, int shk_index );

uint16_t max( uint16_t, uint16_t, uint16_t, uint16_t);

void checkstring( char * teststring );
inline int tai_to_ydhms(uint32_t tai_in, uint16_t *year, uint16_t *doy, 
		 uint32_t *sod, uint16_t *hh, uint16_t *mm, uint16_t *ss);

// Math and pointer helper procedures
void realfft(float data [ ], unsigned long n, int isign);
void four32(float data [ ], unsigned long nn, int isign);

inline int 				byteswap16( uint16_t * p16bitdata, int sizeofarray );
inline uint32_t 		halfwordswap( uint16_t * u16p );
inline uint32_t 		halfword_no_swap( uint16_t * u16p );
inline void 			wordswap( uint32_t * );
inline unsigned char 	getbit8( uint8_t, uint8_t );
inline unsigned char 	getbit16( uint16_t, uint8_t );
inline uint8_t 			bitswap8( uint8_t inbits );
inline uint16_t 		bitswap16( uint16_t inbits );
// Swap bytes in 32 bit value.  MACRO
#define byteswap_32(x) \
     ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) |		      \
      (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))

void locatesam( uint16_t **image, int width, int height, int xstart, int xstop, int ystart, int ystop, int * xloc, int * yloc, int * radius );

int fillimagedata(uint16_t image[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT]);

int checkdir( char * filename );
//int ParseCommandLineBool( char *cval, int argc, char *argv[] );
//uint8_t * read_tlm(  char argv[2][MAX_STRING_LENGTH], int numfiles, uint32_t * memsize, uint32_t * packets, int8_t *status  );
//int	processMegsPacket( uint16_t MEGS_APID, struct MEGS_IMAGE_REC *ptr_megs, uint8_t * vcdu, int shk_index );


// **********  Quicklook prototypes  *******************
//void dumpshk( char * filename, int );
//void dumpesp( char * filename );
int plotspectra( uint16_t image[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT], char * filename, uint32_t tai_time_seconds, int mode, int termination_flag );
int write_quicklook(char *, char *, uint16_t image[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT], int, uint8_t, struct MEGS_IMAGE_REC * rec, int pck_recvd );
uint8_t write_megsa1(char *, uint16_t image[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT], uint8_t, char * );
uint8_t write_megsa2(char *, uint16_t image[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT], uint8_t, char * );
uint8_t write_megs_sam(char * , uint16_t image[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT], uint8_t, char * );
uint8_t write_megsb1(char *, uint16_t image[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT], uint8_t, char * );
int make_routine_plots(struct MEGS_IMAGE_REC *ma, struct MEGS_IMAGE_REC *mb, struct PHOTOMETER_PACKET photometer_data[512], struct SHK_PACKET shk_data[200], int p_counter);

#endif
