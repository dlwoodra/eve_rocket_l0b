/* EVE_l0b.hpp */
#ifndef EVE_l0b_defined
#define EVE_l0b_defined

#define DEBUG

#include <atomic>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <time.h>
#include <math.h>
#include <mutex>
#include <byteswap.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/file.h>
#include <wchar.h>
#include <cstdint>
#include <thread>
#include <chrono>

// Constants
constexpr uint32_t SYNC_MARKER = 0x1ACFFC1D; // sync marker
constexpr uint32_t BSWAP_SYNC_MARKER = 0x1DFCCF1A;
constexpr size_t PACKET_HEADER_SIZE = 6;     // CCSDS packet primary header size in bytes
constexpr uint16_t STANDARD_MEGSAB_PACKET_LENGTH = 1761; // value from CCSDS packet (one less than size)
constexpr uint16_t STANDARD_MEGSP_PACKET_LENGTH = 1761; //25; // apid 604
constexpr uint16_t STANDARD_ESP_PACKET_LENGTH = 1761; //89; // apid 605
constexpr uint16_t STANDARD_SHK_PACKET_LENGTH = 1761; //265; // apid 606

// David's definitions of APIDs
constexpr uint16_t MEGSA_APID = 601; // same as SDO EVE
constexpr uint16_t MEGSB_APID = 602; // same as SDO EVE
constexpr uint16_t MEGSP_APID = 604; // was SHK in SDO EVE
constexpr uint16_t ESP_APID = 605; // not used in SDO EVE, MEGS-P and ESP were both in 603
constexpr uint16_t SHK_APID = 606; // rocket housekeeping only

constexpr int32_t MEGSP_INTEGRATIONS_PER_PACKET = 4;
constexpr int32_t SECONDS_PER_MEGSP_FILE = 10;
constexpr int32_t MEGSP_PACKETS_PER_FILE = SECONDS_PER_MEGSP_FILE;
constexpr int32_t MEGSP_INTEGRATIONS_PER_FILE = MEGSP_INTEGRATIONS_PER_PACKET * SECONDS_PER_MEGSP_FILE;

constexpr int32_t ESP_INTEGRATIONS_PER_PACKET = 4;
constexpr int32_t SECONDS_PER_ESP_FILE = 10;
constexpr int32_t ESP_PACKETS_PER_FILE = SECONDS_PER_ESP_FILE;
constexpr int32_t ESP_INTEGRATIONS_PER_FILE = ESP_INTEGRATIONS_PER_PACKET * SECONDS_PER_ESP_FILE;

constexpr int32_t SHK_INTEGRATIONS_PER_PACKET = 1;
constexpr int32_t SECONDS_PER_SHK_FILE = 10;
constexpr int32_t SHK_PACKETS_PER_FILE = SECONDS_PER_SHK_FILE;
constexpr int32_t SHK_INTEGRATIONS_PER_FILE = SHK_INTEGRATIONS_PER_PACKET * SECONDS_PER_SHK_FILE;

constexpr uint16_t IMAGE_UPDATE_INTERVAL = 114; //266; // best values are evenly divisible into 2394: 1,2,3,6,7,9,14,18,19,21,38,42,57,63,114,126,133,171,266,342,399,798,1197

#define BAD_PIXEL 16384 			// 2^14	- Image fill value

#define n_bytes_per_syncmarker 4		// The sync marker is 4 bytes
#define n_bytes_per_vcdu 1784			// Includes CRC 16bit word
#define n_bytes_per_vcdu_header 28		// The vcdu header is 28 bytes
#define n_bytes_per_vcdu_data 1756		// Each vcdu is 1756 bytes

#define N_PKT_PER_IMAGE 		2395	// There are 2395 packets in each image

constexpr uint32_t MEGS_IMAGE_WIDTH = 2048;	// The CCD images are 2048 x 1024 pixels
constexpr uint32_t MEGS_IMAGE_HEIGHT = 1024;
constexpr uint32_t MEGS_TOTAL_PIXELS = MEGS_IMAGE_WIDTH * MEGS_IMAGE_HEIGHT;

constexpr uint32_t MEGS_IMAGE_T_WIDTH = 1024; //transposed
constexpr uint32_t MEGS_IMAGE_T_HEIGHT = 2048; //transposed

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
#define W_INVALID_PARITY 	21			// MEGS image - Invalid Parity

// FITS Errors
#define E_FITS_IO			401			// Error Reading/Writing Fits file



//
//define structures
//

// contents from eve_structures.hpp


// define program state variables to use across both threads here

struct PKT_COUNT_REC {
  std::atomic<int64_t> MA{0};
  std::atomic<int64_t> MB{0};
  std::atomic<int64_t> ESP{0};
  std::atomic<int64_t> MP{0};
  std::atomic<int64_t> SHK{0};
  std::atomic<int64_t> Unknown{0};
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
	uint32_t vcdu_counter;
	uint16_t file_header_ptr;
	uint16_t app_id;
	uint16_t source_sequence_counter;
	uint16_t packet_length;
	uint32_t tai_time_seconds;
	uint32_t tai_time_subseconds;
};

extern HEADERS ps_headers;

struct SHK_PACKET
{
	uint32_t yyyydoy;
  	uint32_t sod;
	uint32_t tai_time_seconds;  
	uint32_t tai_time_subseconds;
	uint32_t rec_tai_seconds;
 	uint32_t rec_tai_subseconds; // 6 in fits file
// 10/22/2024 mode will be used for integration rate in seconds 1=1sec, 10 will be max seconds
	uint32_t mode[SHK_INTEGRATIONS_PER_FILE]; // only 16 bits but use 32 for alignment

	//uint32_t spare0; 						// 0
	uint32_t FPGA_Board_Temperature[SHK_INTEGRATIONS_PER_FILE]; // 8 in fits file
	uint32_t FPGA_Board_p5_0_Voltage[SHK_INTEGRATIONS_PER_FILE];
	uint32_t FPGA_Board_p3_3_Voltage[SHK_INTEGRATIONS_PER_FILE]; // 10 in fits file
	uint32_t FPGA_Board_p2_5_Voltage[SHK_INTEGRATIONS_PER_FILE];
	uint32_t FPGA_Board_p1_2_Voltage[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSA_CEB_Temperature[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSA_CPR_Temperature[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSA_p24_Voltage[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSA_p15_Voltage[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSA_m15_Voltage[SHK_INTEGRATIONS_PER_FILE];				// 10
	uint32_t MEGSA_p5_0_Analog_Voltage[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSA_m5_0_Voltage[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSA_p5_0_Digital_Voltage[SHK_INTEGRATIONS_PER_FILE]; // 20 in fits file
	uint32_t MEGSA_p2_5_Voltage[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSA_p24_Current[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSA_p15_Current[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSA_m15_Current[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSA_p5_0_Analog_Current[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSA_m5_0_Current[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSA_p5_0_Digital_Current[SHK_INTEGRATIONS_PER_FILE];	// 20
	uint32_t MEGSA_p2_5_Current[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSA_Integration_Register[SHK_INTEGRATIONS_PER_FILE];

	uint32_t MEGSA_Analog_Mux_Register[SHK_INTEGRATIONS_PER_FILE]; // DN // 30 in fits file
	uint32_t MEGSA_Digital_Status_Register[SHK_INTEGRATIONS_PER_FILE]; // DN
	uint32_t MEGSA_Integration_Timer_Register[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSA_Command_Error_Count_Register[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSA_CEB_FPGA_Version_Register[SHK_INTEGRATIONS_PER_FILE];
	//uint32_t spare28; // use (adcValue-0x8000) * 305.2 microVolts
	//uint32_t spare29;
	//uint32_t spare30;						// 30
	//uint32_t spare31;
	uint32_t MEGSB_CEB_Temperature[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSB_CPR_Temperature[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSB_p24_Voltage[SHK_INTEGRATIONS_PER_FILE]; 
	uint32_t MEGSB_p15_Voltage[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSB_m15_Voltage[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSB_p5_0_Analog_Voltage[SHK_INTEGRATIONS_PER_FILE]; // 40 in fits file
	uint32_t MEGSB_m5_0_Voltage[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSB_p5_0_Digital_Voltage[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSB_p2_5_Voltage[SHK_INTEGRATIONS_PER_FILE];			// 40
	uint32_t MEGSB_p24_Current[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSB_p15_Current[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSB_m15_Current[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSB_p5_0_Analog_Current[SHK_INTEGRATIONS_PER_FILE]; 
	uint32_t MEGSB_m5_0_Current[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSB_p5_0_Digital_Current[SHK_INTEGRATIONS_PER_FILE];	
	uint32_t MEGSB_p2_5_Current[SHK_INTEGRATIONS_PER_FILE]; // 50 in fits file

	uint32_t MEGSB_Integration_Register[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSB_Analog_Mux_Register[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSB_Digital_Status_Register[SHK_INTEGRATIONS_PER_FILE];		// 50
	uint32_t MEGSB_Integration_Timer_Register[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSB_Command_Error_Count_Register[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSB_CEB_FPGA_Version_Register[SHK_INTEGRATIONS_PER_FILE];

	uint32_t MEGSA_PRT[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSA_Thermistor_Diode[SHK_INTEGRATIONS_PER_FILE];  //special conversion 
	uint32_t MEGSB_PRT[SHK_INTEGRATIONS_PER_FILE]; // 60 in fits file
	uint32_t MEGSB_Thermistor_Diode[SHK_INTEGRATIONS_PER_FILE];

	uint32_t ESP_Detector_Temperature[SHK_INTEGRATIONS_PER_FILE];
	uint32_t ESP_Electrometer_Temperature[SHK_INTEGRATIONS_PER_FILE];
	uint32_t MEGSP_Temperature[SHK_INTEGRATIONS_PER_FILE]; 			// 60

	//uint32_t spare61;
	//uint32_t spare62;
	//uint32_t spare63;
	//uint32_t spare64;

	// converted values
	double cFPGA_Board_Temperature[SHK_INTEGRATIONS_PER_FILE];
	double cFPGA_Board_p5_0_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double cFPGA_Board_p3_3_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double cFPGA_Board_p2_5_Voltage[SHK_INTEGRATIONS_PER_FILE]; // group of 4
	double cFPGA_Board_p1_2_Voltage[SHK_INTEGRATIONS_PER_FILE];
	
	double cMEGSA_CEB_Temperature[SHK_INTEGRATIONS_PER_FILE];
	double cMEGSA_CPR_Temperature[SHK_INTEGRATIONS_PER_FILE]; // 70 in fits file
	double cMEGSA_p24_Voltage[SHK_INTEGRATIONS_PER_FILE]; // group of 4
	double cMEGSA_p15_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double cMEGSA_m15_Voltage[SHK_INTEGRATIONS_PER_FILE];				// 10
	double cMEGSA_p5_0_Analog_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double cMEGSA_m5_0_Voltage[SHK_INTEGRATIONS_PER_FILE]; // group of 4
	double cMEGSA_p5_0_Digital_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double cMEGSA_p2_5_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double cMEGSA_p24_Current[SHK_INTEGRATIONS_PER_FILE];
	double cMEGSA_p15_Current[SHK_INTEGRATIONS_PER_FILE]; // 16th converted var, group of 4
	double cMEGSA_m15_Current[SHK_INTEGRATIONS_PER_FILE]; // 80 in fits file
	double cMEGSA_p5_0_Analog_Current[SHK_INTEGRATIONS_PER_FILE];
	double cMEGSA_m5_0_Current[SHK_INTEGRATIONS_PER_FILE];
	double cMEGSA_p5_0_Digital_Current[SHK_INTEGRATIONS_PER_FILE];	// 20 // group of 4
	double cMEGSA_p2_5_Current[SHK_INTEGRATIONS_PER_FILE];

	double cMEGSB_CEB_Temperature[SHK_INTEGRATIONS_PER_FILE];
	double cMEGSB_CPR_Temperature[SHK_INTEGRATIONS_PER_FILE];
	double cMEGSB_p24_Voltage[SHK_INTEGRATIONS_PER_FILE]; //group of 4
	double cMEGSB_p15_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double cMEGSB_m15_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double cMEGSB_p5_0_Analog_Voltage[SHK_INTEGRATIONS_PER_FILE]; // 90 in fits file
	double cMEGSB_m5_0_Voltage[SHK_INTEGRATIONS_PER_FILE]; //group of 4
	double cMEGSB_p5_0_Digital_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double cMEGSB_p2_5_Voltage[SHK_INTEGRATIONS_PER_FILE];			// 30
	double cMEGSB_p24_Current[SHK_INTEGRATIONS_PER_FILE];
	double cMEGSB_p15_Current[SHK_INTEGRATIONS_PER_FILE]; // 32nd converted var // group of 4
	double cMEGSB_m15_Current[SHK_INTEGRATIONS_PER_FILE];
	double cMEGSB_p5_0_Analog_Current[SHK_INTEGRATIONS_PER_FILE]; 
	double cMEGSB_m5_0_Current[SHK_INTEGRATIONS_PER_FILE];
	double cMEGSB_p5_0_Digital_Current[SHK_INTEGRATIONS_PER_FILE];	 //group of 4
	double cMEGSB_p2_5_Current[SHK_INTEGRATIONS_PER_FILE]; // 100 in fits file

	double cMEGSA_Thermistor_Diode[SHK_INTEGRATIONS_PER_FILE];  //special conversion
	double cMEGSA_PRT[SHK_INTEGRATIONS_PER_FILE];
	double cMEGSB_Thermistor_Diode[SHK_INTEGRATIONS_PER_FILE]; //40 //group of 4
	double cMEGSB_PRT[SHK_INTEGRATIONS_PER_FILE];

	double cESP_Electrometer_Temperature[SHK_INTEGRATIONS_PER_FILE];
	double cESP_Detector_Temperature[SHK_INTEGRATIONS_PER_FILE];
	double cMEGSP_Temperature[SHK_INTEGRATIONS_PER_FILE]; // 44 // 107 in fits file
};

extern struct SHK_PACKET shk_data;

struct SHK_CONVERTED_PACKET
{
	//uint32_t yyyydoy;
  	//uint32_t sod;
	//uint32_t tai_time_seconds;  
	//uint32_t tai_time_subseconds;
	//uint32_t rec_tai_seconds;
 	//uint32_t rec_tai_subseconds;
// 10/22/2024 mode will be used for integration rate in seconds 1=1sec, 10 will be max seconds
	//uint32_t spare0; 						// 0
	double FPGA_Board_Temperature[SHK_INTEGRATIONS_PER_FILE];
	double FPGA_Board_p5_0_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double FPGA_Board_p3_3_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double FPGA_Board_p2_5_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double FPGA_Board_p1_2_Voltage[SHK_INTEGRATIONS_PER_FILE];

	double MEGSA_CEB_Temperature[SHK_INTEGRATIONS_PER_FILE];
	double MEGSA_CPR_Temperature[SHK_INTEGRATIONS_PER_FILE];
	double MEGSA_p24_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double MEGSA_p15_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double MEGSA_m15_Voltage[SHK_INTEGRATIONS_PER_FILE];				// 10
	double MEGSA_p5_0_Analog_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double MEGSA_m5_0_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double MEGSA_p5_0_Digital_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double MEGSA_p2_5_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double MEGSA_p24_Current[SHK_INTEGRATIONS_PER_FILE];
	double MEGSA_p15_Current[SHK_INTEGRATIONS_PER_FILE];
	double MEGSA_m15_Current[SHK_INTEGRATIONS_PER_FILE];
	double MEGSA_p5_0_Analog_Current[SHK_INTEGRATIONS_PER_FILE];
	double MEGSA_m5_0_Current[SHK_INTEGRATIONS_PER_FILE];
	double MEGSA_p5_0_Digital_Current[SHK_INTEGRATIONS_PER_FILE];	// 20
	double MEGSA_p2_5_Current[SHK_INTEGRATIONS_PER_FILE];

	double MEGSB_CEB_Temperature[SHK_INTEGRATIONS_PER_FILE];
	double MEGSB_CPR_Temperature[SHK_INTEGRATIONS_PER_FILE];
	double MEGSB_p24_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double MEGSB_p15_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double MEGSB_m15_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double MEGSB_p5_0_Analog_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double MEGSB_m5_0_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double MEGSB_p5_0_Digital_Voltage[SHK_INTEGRATIONS_PER_FILE];
	double MEGSB_p2_5_Voltage[SHK_INTEGRATIONS_PER_FILE];			// 40
	double MEGSB_p24_Current[SHK_INTEGRATIONS_PER_FILE];
	double MEGSB_p15_Current[SHK_INTEGRATIONS_PER_FILE];
	double MEGSB_m15_Current[SHK_INTEGRATIONS_PER_FILE];
	double MEGSB_p5_0_Analog_Current[SHK_INTEGRATIONS_PER_FILE];
	double MEGSB_m5_0_Current[SHK_INTEGRATIONS_PER_FILE];
	double MEGSB_p5_0_Digital_Current[SHK_INTEGRATIONS_PER_FILE];	
	double MEGSB_p2_5_Current[SHK_INTEGRATIONS_PER_FILE];

	double MEGSA_Thermistor_Diode[SHK_INTEGRATIONS_PER_FILE];  //special conversion
	double MEGSA_PRT[SHK_INTEGRATIONS_PER_FILE];
	double MEGSB_Thermistor_Diode[SHK_INTEGRATIONS_PER_FILE];
	double MEGSB_PRT[SHK_INTEGRATIONS_PER_FILE];

	double ESP_Electrometer_Temperature[SHK_INTEGRATIONS_PER_FILE];
	double ESP_Detector_Temperature[SHK_INTEGRATIONS_PER_FILE];
	double MEGSP_Temperature[SHK_INTEGRATIONS_PER_FILE]; 			// 60
};

extern struct SHK_CONVERTED_PACKET shk_converted_data;


struct ESP_PACKET
{
	uint32_t yyyydoy;
  	uint32_t sod;
	uint32_t tai_time_seconds;  
	uint32_t tai_time_subseconds;
	uint32_t rec_tai_seconds;
 	uint32_t rec_tai_subseconds;
	float FPGA_Board_Temperature;
	float ESP_Electrometer_Temperature;
	float ESP_Detector_Temperature;
	uint16_t ESP_xfer_cnt[ESP_INTEGRATIONS_PER_FILE];
	uint16_t ESP_q0[ESP_INTEGRATIONS_PER_FILE];
	uint16_t ESP_q1[ESP_INTEGRATIONS_PER_FILE];
	uint16_t ESP_q2[ESP_INTEGRATIONS_PER_FILE];
	uint16_t ESP_q3[ESP_INTEGRATIONS_PER_FILE];
	uint16_t ESP_171[ESP_INTEGRATIONS_PER_FILE];
	uint16_t ESP_257[ESP_INTEGRATIONS_PER_FILE];
	uint16_t ESP_304[ESP_INTEGRATIONS_PER_FILE];
	uint16_t ESP_366[ESP_INTEGRATIONS_PER_FILE];
	uint16_t ESP_dark[ESP_INTEGRATIONS_PER_FILE];
};

extern struct ESP_PACKET esp_data;

struct MEGSP_PACKET
{
  uint32_t yyyydoy;
  uint32_t sod;
  uint32_t tai_time_seconds;
  uint32_t tai_time_subseconds;
  uint32_t rec_tai_seconds;
  uint32_t rec_tai_subseconds;
  float FPGA_Board_Temperature;
  float MEGSP_Temperature;
  uint16_t MP_lya[MEGSP_INTEGRATIONS_PER_FILE];
  uint16_t MP_dark[MEGSP_INTEGRATIONS_PER_FILE];
};

extern struct MEGSP_PACKET megsp_data;

struct MEGS_IMAGE_REC {
  uint32_t yyyydoy;
  uint32_t sod;
  uint32_t tai_time_seconds;
  uint32_t tai_time_subseconds;
  uint32_t rec_tai_seconds;
  uint32_t rec_tai_subseconds;
  uint16_t vcdu_count;
  float FPGA_Board_Temperature;
  float CEB_Temperature;
  float CPR_Temperature;
  float PRT_Temperature;
  uint16_t image[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH];
};

extern struct MEGS_IMAGE_REC megs_image_rec;


int assemble_image( uint8_t * vcdu,  MEGS_IMAGE_REC * ptr, uint16_t sourceSequenceCounter, bool testPattern, int32_t& xpos, int32_t& ypos, int8_t *status);
int	processPHOTOpacket(uint32_t *esp_time_seconds, uint32_t *esp_index, uint8_t *vcdu_data);

uint16_t max( uint16_t, uint16_t, uint16_t, uint16_t);

void checkstring( char * teststring );
extern int tai_to_ydhms(uint32_t tai_in, uint16_t *year, uint16_t *doy, 
      uint32_t *sod, uint16_t *hh, uint16_t *mm, uint16_t *ss, std::string& iso8601);

extern std::string tai_to_iso8601(uint32_t tai);
extern std::string tai_to_iso8601_with_milliseconds(uint32_t tai_seconds, uint32_t tai_subseconds);

extern int convertToCString(const std::string& str, const char** cstr, size_t& size);

inline unsigned char 	getbit8( uint8_t, uint8_t );
inline unsigned char 	getbit16( uint16_t, uint8_t );
inline uint8_t 			bitswap8( uint8_t inbits );
inline uint16_t 		bitswap16( uint16_t inbits );

int checkdir( char * filename );

// imgui_thread.cpp function prototypes
//void histogramEqualization(uint16_t (*image)[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH], std::vector<uint8_t>& textureData);

#endif
