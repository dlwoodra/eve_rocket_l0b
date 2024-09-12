/* EVE_l0b.hpp */
#ifndef EVE_l0b_defined
#define EVE_l0b_defined

#define DEBUG

// absorbed eve_structures.h

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
#include <cstdint>

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
constexpr uint32_t MEGS_IMAGE_WIDTH = 2048;	// The CCD images are 2048 x 1024 pixels
constexpr uint32_t MEGS_IMAGE_HEIGHT = 1024;
//#define MEGS_IMAGE_WIDTH 		2048	// The CCD images are 2048 x 1024 pixels
//#define MEGS_IMAGE_HEIGHT 		1024
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
//#include "eve_structures.hpp" // the contents of eve_structures were pasted in below
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

//
//define structures
//

// contents from eve_structures.hpp

// struct Plot_spectra_data
// {
// 	char filename[256];
//     uint16_t image[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT];
// 	uint32_t time;
// 	int readout_mode;
// 	int aORb;
// }plot_spectra_structA, plot_spectra_structB;

struct PKT_COUNT_REC {
  long MA;
  long MB;
  long ESP;
  long MP;
  long SHK;
  long TEST_MA;
  long TEST_MB;
  long TEST_GENERIC;
}; //pkt_count;

extern PKT_COUNT_REC pkt_count;

struct MEGS_STATS_STRUCT 
{
	short a_image_count;
	short b_image_count;
	long last_a_sequence_counter;
	long last_b_sequence_counter;
	long a_sequence_number;
	long b_sequence_number;
	long goodpixelsA;
	long badpixelsA;
	long goodpixelsB;
	long badpixelsB;
	long badparityA;
	long badparityB;
	long totalCRCerrors;
	uint32_t last_megs_a_tai_seconds;
	uint32_t last_megs_b_tai_seconds;	
};

extern MEGS_STATS_STRUCT megs_stats;

struct HEADERS
{
	//uint8_t aos_version;
	//uint8_t sc_id;
	//uint8_t vcdu_id;
	uint32_t vcdu_counter;
	//bool replay_flag;
	//uint8_t im_pdu_id;
	//uint64_t im_pdu_seq_counter;
	uint16_t file_header_ptr;
	//uint8_t version;
	//bool type;
	//bool secondary_header_flag;
	uint16_t app_id;
	//uint8_t segmentation_flag;
	uint16_t source_sequence_counter;
	uint16_t packet_length;
	uint32_t tai_time_seconds;
	uint32_t tai_time_subseconds;
};

extern HEADERS ps_headers;

struct SHK_PACKET
{
	//uint16_t  	spare_mode_flags;					// 1
	//bool		load_shed;
	//bool		current_threshold;	
	//bool		safe_mode_entry;
	//bool		time_loss;
	//bool		inertial_mode;
	//bool		sun_acquisition_mode;
	//bool		science_mode;
	//bool		offpoint;
	//bool		roll_maneuver;						// 10
	//bool		start_slew;
	//bool		thruster_firing;
	//bool		eclipse;
	//bool		eop_temp_thresh;			
	//bool		ccd_temp_thresh;			
	//uint32_t  	spare_reserved;
	//bool 		megsa_power;
	//bool 		megsb_power;
	//bool 		resolver_power;
	//bool 		filter_wheels_power;				// 20
	//bool 		esp_power;
	//bool 		esp_repeller_grid;
	//uint8_t  	megsa_led_power;
	//uint8_t  	megsb_led_power;
	//uint8_t  	megsa_led0_current;					// 25
	//uint8_t  	megsa_led1_current;
	//uint8_t  	megsb_led0_current;
	//uint8_t  	megsb_led1_current;
	//bool 		megsa_filter_pos_known;
	//bool 		megsa_filter_mech_moving;			// 30
	//bool 		megsb_filter_pos_known;
	//bool 		megsb_filter_mech_moving;
	//bool 		sam_filter_pos_known;
	//bool 		sam_filter_mech_moving;
	//bool 		esp_filter_pos_known;				// 35
	//bool 		esp_filter_mech_moving;
	//uint16_t  	megsa_filter_step_number;		
	//uint16_t  	megsb_filter_step_number;
	//uint16_t  	sam_filter_step_number;
	//uint16_t  	esp_filter_step_number;				//40
	//uint16_t  	megsa_filter_position_number;
	//uint16_t  	megsb_filter_position_number;
	//uint16_t  	sam_filter_position_number;
	//uint16_t  	esp_filter_position_number;
	//uint16_t  	megsp_temperature_dn;				// 45
	//float	  	megsp_temperature;
	//uint16_t  	esp_temperature_dn;				
	//float  		esp_temperature;
	//uint16_t  	megsa_ccd_temperature_dn;
	//float		megsa_ccd_temperature;				// 50
	//uint16_t  	megsb_ccd_temperature_dn;		
	//float		megsb_ccd_temperature;
	//uint32_t  	met_seconds;					
	//uint32_t  	met_subseconds;
	//uint32_t  	stcf_seconds;						// 55
	//uint32_t  	stcf_subseconds;	
	//uint32_t   	leap_seconds;					
	//uint32_t 	time_status;
	uint32_t  	tai_time_seconds;
	uint32_t  	tai_time_subseconds;				// 60
	//uint16_t	megsa_resolver_number;
	//uint16_t	megsb_resolver_number;
	//uint16_t	sam_resolver_number;				
	//uint16_t	esp_resolver_number;
	//uint16_t  	sdn_core_temperature_dn;			// 65
	//float		sdn_core_temperature;
	//uint16_t  	power_services_temperature_dn;	
	//float		power_services_temperature;
	//uint16_t  	optics_posX_posY_temperature_dn;
	//float		optics_posX_posY_temperature;		// 70
	//uint16_t  	optics_negX_posY_temperature_dn;
	//float		optics_negX_posY_temperature;	
	//uint16_t  	megsa_ceb_temperature_dn;
	//float		megsa_ceb_temperature;		
	//uint16_t  	megsb_ceb_temperature_dn;			// 75
	//float		megsb_ceb_temperature;
	//uint16_t 	ma_integration_time;
	//bool 		ma_hw_test;
  	//bool 		ma_sw_test;
  	//bool 		ma_reverse_clock;
  	//bool 		ma_valid;
  	//bool 		ma_ram_bank;
  	//bool 		ma_int_time_warn;
  	//uint8_t 	ma_readout_mode;
	//uint16_t 	mb_integration_time;
	//bool 		mb_hw_test;
  	//bool 		mb_sw_test;
  	//bool 		mb_reverse_clock;
  	//bool 		mb_valid;
  	//bool 		mb_ram_bank;
  	//bool 		mb_int_time_warn;
  	//uint8_t 	mb_readout_mode;
} __attribute__ ((packed));

extern struct SHK_PACKET shk_data[200];

struct PHOTOMETER_PACKET
{
	uint8_t		ESP_xfer_cnt;
	bool		ESP_V_ref;
	bool		ESP_valid;
	uint16_t	ESP_q0;
	uint16_t	ESP_q1;
	uint16_t	ESP_q2;
	uint16_t	ESP_q3;
	uint16_t	ESP_171;
	uint16_t	ESP_257;
	uint16_t	ESP_304;
	uint16_t	ESP_366;
	uint16_t	ESP_dark;
	uint8_t		filter_position;
	uint16_t	resolver;
	float  		esp_temperature;
	uint32_t 	tai_time_seconds;  
	uint32_t 	tai_time_subseconds;
} __attribute__ ((packed));

extern struct PHOTOMETER_PACKET photometer_data[512];	// In 2 minutes there are 480 observations

struct MEGSP_PACKET
{
	uint8_t		ESP_xfer_cnt;
	uint8_t		MP_mode;
	bool		MP_V_ref;
	bool		MP_valid;
	uint16_t	MP_lya;
	uint16_t	MP_dark;
	uint16_t	resolver;
	float  		mp_temperature;
	uint32_t 	tai_time_seconds;  
	uint32_t 	tai_time_subseconds;
} __attribute__ ((packed));

extern struct MEGSP_PACKET megsp_data[512];

// struct DARK_STRUCT
// {
// 	uint32_t 	tai_time_seconds;
// 	uint8_t	 	int_time;
// 	float		mean;
// 	float		StdDev;
// 	float		min;
// 	float		max;
// } __attribute__ ((packed));
// struct DARK_STRUCT dark_photo_data;

// struct MEGS_DARK_STRUCT
// {
// 	uint32_t yyyydoy;
// 	uint32_t sod;
// 	uint32_t tai_time_seconds;
// 	uint32_t tai_time_subseconds;
// 	uint16_t vcdu_count;
// 	uint8_t integration_time;
// 	bool reverse_clock;
// 	bool ram_bank;
// 	bool int_time_warn;
// 	uint8_t readout_mode;
// 	float ccd_temp;
// 	bool led_on; 
// 	uint8_t led_0_level;
// 	uint8_t led_1_level;
// 	uint16_t 	num_pix_tl;
// 	uint32_t 	num_pix_tm;
// 	uint16_t 	num_pix_tr;
// 	uint16_t 	num_pix_bl;
// 	uint32_t 	num_pix_bm;
// 	uint16_t 	num_pix_br;
// 	float		tl_mean;
// 	float		tl_StdDev;
// 	float		tl_min;
// 	float		tl_max;
// 	float		tm_mean;
// 	float		tm_StdDev;
// 	float		tm_min;
// 	float		tm_max;
// 	float		tr_mean;
// 	float		tr_StdDev;
// 	float		tr_min;
// 	float		tr_max;
// 	float		bl_mean;
// 	float		bl_StdDev;
// 	float		bl_min;
// 	float		bl_max;
// 	float		bm_mean;
// 	float		bm_StdDev;
// 	float		bm_min;
// 	float		bm_max;
// 	float		br_mean;
// 	float		br_StdDev;
// 	float		br_min;
// 	float		br_max;
// } __attribute__ ((packed));

// struct MEGS_DARK_STRUCT megs_dark_data;

// struct MEGS_IMAGE_REC {
//   uint32_t yyyydoy;
//   uint32_t sod;
//   uint32_t tai_time_seconds;
//   uint32_t tai_time_subseconds;
//   uint16_t vcdu_count;
//   uint16_t integration_time;
//   bool hw_test;
//   bool sw_test;
//   bool reverse_clock;
//   bool valid;
//   bool ram_bank;
//   bool int_time_warn;
//   uint8_t filter_position;
//   uint8_t readout_mode;
//   float ccd_temp;
//   bool led_on; 
//   uint8_t led_0_level;
//   uint8_t led_1_level;
//   uint16_t resolver;
//   uint16_t sam_resolver;
//   uint16_t image[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT];
// } __attribute__ ((packed));

struct MEGS_IMAGE_REC {
  uint32_t yyyydoy;
  uint32_t sod;
  uint32_t tai_time_seconds;
  uint32_t tai_time_subseconds;
  uint32_t rec_tai_seconds;
  uint32_t rec_tai_subseconds;
  uint16_t vcdu_count;
  uint16_t image[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT];
}; // __attribute__ ((packed));

extern struct MEGS_IMAGE_REC megs_image_rec;

struct TLM_ERRORS
{
	uint16_t	crc;
	uint16_t	parity;
	uint16_t	sync;
};

extern TLM_ERRORS tlm_errors;

//// Compression file rec
//struct FILECOMP {
//	char filename[128];
//};
//extern FILECOMP filecomp;

// +++++++++++++++++  The procedure prototypes  ++++++++++++++++++

int assemble_image( uint8_t * vcdu,  MEGS_IMAGE_REC * ptr, uint16_t sourceSequenceCounter, bool testPattern, int8_t *status);
int	processPHOTOpacket(uint32_t *esp_time_seconds, uint32_t *esp_index, uint8_t *vcdu_data);
//int	processSHKpacket(uint32_t *shk_time_seconds, int *shk_index, int *esp_index, uint8_t *vcdu_data, struct MEGS_IMAGE_REC *ptr_ma, struct MEGS_IMAGE_REC *ptr_mb);
//inline void read_header( uint8_t *vcdu );
//inline int WHERE( uint32_t tai_time, int shk_index );

uint16_t max( uint16_t, uint16_t, uint16_t, uint16_t);

void checkstring( char * teststring );
extern int tai_to_ydhms(uint32_t tai_in, uint16_t *year, uint16_t *doy, 
      uint32_t *sod, uint16_t *hh, uint16_t *mm, uint16_t *ss);

// Math and pointer helper procedures
//void realfft(float data [ ], unsigned long n, int isign);
//void four32(float data [ ], unsigned long nn, int isign);

//inline int 				byteswap16( uint16_t * p16bitdata, int sizeofarray );
//inline uint32_t 		halfwordswap( uint16_t * u16p );
//inline uint32_t 		halfword_no_swap( uint16_t * u16p );
//inline void 			wordswap( uint32_t * );
inline unsigned char 	getbit8( uint8_t, uint8_t );
inline unsigned char 	getbit16( uint16_t, uint8_t );
inline uint8_t 			bitswap8( uint8_t inbits );
inline uint16_t 		bitswap16( uint16_t inbits );
// Swap bytes in 32 bit value.  MACRO
// replaced with commonFunctions.cpp
//#define byteswap_32(x) 
//     ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) |
//      (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))

//void locatesam( uint16_t **image, int width, int height, int xstart, int xstop, int ystart, int ystop, int * xloc, int * yloc, int * radius );

//int fillimagedata(uint16_t image[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT]);

int checkdir( char * filename );
//int ParseCommandLineBool( char *cval, int argc, char *argv[] );
//uint8_t * read_tlm(  char argv[2][MAX_STRING_LENGTH], int numfiles, uint32_t * memsize, uint32_t * packets, int8_t *status  );
//int	processMegsPacket( uint16_t MEGS_APID, struct MEGS_IMAGE_REC *ptr_megs, uint8_t * vcdu, int shk_index );


// **********  Quicklook prototypes  *******************
//void dumpshk( char * filename, int );
//void dumpesp( char * filename );
//int plotspectra( uint16_t image[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT], char * filename, uint32_t tai_time_seconds, int mode, int termination_flag );
//int write_quicklook(char *, char *, uint16_t image[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT], int, uint8_t, struct MEGS_IMAGE_REC * rec, int pck_recvd );
//uint8_t write_megsa1(char *, uint16_t image[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT], uint8_t, char * );
//uint8_t write_megsa2(char *, uint16_t image[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT], uint8_t, char * );
//uint8_t write_megs_sam(char * , uint16_t image[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT], uint8_t, char * );
//uint8_t write_megsb1(char *, uint16_t image[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT], uint8_t, char * );
//int make_routine_plots(struct MEGS_IMAGE_REC *ma, struct MEGS_IMAGE_REC *mb, struct PHOTOMETER_PACKET photometer_data[512], struct SHK_PACKET shk_data[200], int p_counter);


#endif
