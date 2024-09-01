#ifndef EVE_STRUCTURES_defined
#define EVE_STRUCTURES_defined

#include <cstdint>

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
}megs_stats;

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
}ps_headers;
 
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
struct SHK_PACKET shk_data[200];

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
struct PHOTOMETER_PACKET photometer_data[512];	// In 2 minutes there are 480 observations

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
struct MEGSP_PACKET megsp_data[512];

struct DARK_STRUCT
{
	uint32_t 	tai_time_seconds;
	uint8_t	 	int_time;
	float		mean;
	float		StdDev;
	float		min;
	float		max;
} __attribute__ ((packed));
struct DARK_STRUCT dark_photo_data;

struct MEGS_DARK_STRUCT
{
	uint32_t yyyydoy;
	uint32_t sod;
	uint32_t tai_time_seconds;
	uint32_t tai_time_subseconds;
	uint16_t vcdu_count;
	uint8_t integration_time;
	bool reverse_clock;
	bool ram_bank;
	bool int_time_warn;
	uint8_t readout_mode;
	float ccd_temp;
	bool led_on; 
	uint8_t led_0_level;
	uint8_t led_1_level;
	uint16_t 	num_pix_tl;
	uint32_t 	num_pix_tm;
	uint16_t 	num_pix_tr;
	uint16_t 	num_pix_bl;
	uint32_t 	num_pix_bm;
	uint16_t 	num_pix_br;
	float		tl_mean;
	float		tl_StdDev;
	float		tl_min;
	float		tl_max;
	float		tm_mean;
	float		tm_StdDev;
	float		tm_min;
	float		tm_max;
	float		tr_mean;
	float		tr_StdDev;
	float		tr_min;
	float		tr_max;
	float		bl_mean;
	float		bl_StdDev;
	float		bl_min;
	float		bl_max;
	float		bm_mean;
	float		bm_StdDev;
	float		bm_min;
	float		bm_max;
	float		br_mean;
	float		br_StdDev;
	float		br_min;
	float		br_max;
} __attribute__ ((packed));

struct MEGS_DARK_STRUCT megs_dark_data;

struct MEGS_IMAGE_REC 
{
  uint32_t yyyydoy;
  uint32_t sod;
  uint32_t tai_time_seconds;
  uint32_t tai_time_subseconds;
  uint16_t vcdu_count;
  //uint16_t integration_time;
  //bool hw_test;
  //bool sw_test;
  //bool reverse_clock;
  //bool valid;
  //bool ram_bank;
  //bool int_time_warn;
  //uint8_t filter_position;
  //uint8_t readout_mode;
  //float ccd_temp;
  //bool led_on; 
  //uint8_t led_0_level;
  //uint8_t led_1_level;
  //uint16_t resolver;
  //uint16_t sam_resolver;
  uint16_t image[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT];
} __attribute__ ((packed));

struct TLM_ERRORS
{
	uint16_t	crc;
	uint16_t	parity;
	uint16_t	sync;
}tlm_errors;

// Compression file rec
struct FILECOMP {
	char filename[128];
}filecomp;

#endif