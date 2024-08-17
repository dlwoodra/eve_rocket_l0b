#ifndef EVE_STRUCTURES_defined
#define EVE_STRUCTURES_defined

struct Plot_spectra_data
{
	char filename[256];
    uint16_t image[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT];
	uint32_t time;
	int readout_mode;
	int aORb;
}plot_spectra_structA, plot_spectra_structB;

struct PKT_COUNT_REC 
{
  long MA;
  long MB;
  long ESP;
  long SHK;
  long TEST_MA;
  long TEST_MB;
  long TEST_GENERIC;
}pkt_count;

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
	uint8_t aos_version;
	uint8_t sc_id;
	uint8_t vcdu_id;
	uint32_t vcdu_counter;
	_Bool replay_flag;
	uint8_t im_pdu_id;
	uint64_t im_pdu_seq_counter;
	uint16_t file_header_ptr;
	uint8_t version;
	_Bool type;
	_Bool secondary_header_flag;
	uint16_t app_id;
	uint8_t segmentation_flag;
	uint16_t source_sequence_counter;
	uint16_t packet_length;
	uint32_t tai_time_seconds;
	uint32_t tai_time_subseconds;
}ps_headers;
 
struct SHK_PACKET
{
	uint16_t  	spare_mode_flags;					// 1
	_Bool		load_shed;
	_Bool		current_threshold;	
	_Bool		safe_mode_entry;
	_Bool		time_loss;
	_Bool		inertial_mode;
	_Bool		sun_acquisition_mode;
	_Bool		science_mode;
	_Bool		offpoint;
	_Bool		roll_maneuver;						// 10
	_Bool		start_slew;
	_Bool		thruster_firing;
	_Bool		eclipse;
	_Bool		eop_temp_thresh;			
	_Bool		ccd_temp_thresh;			
	uint32_t  	spare_reserved;
	_Bool 		megsa_power;
	_Bool 		megsb_power;
	_Bool 		resolver_power;
	_Bool 		filter_wheels_power;				// 20
	_Bool 		esp_power;
	_Bool 		esp_repeller_grid;
	uint8_t  	megsa_led_power;
	uint8_t  	megsb_led_power;
	uint8_t  	megsa_led0_current;					// 25
	uint8_t  	megsa_led1_current;
	uint8_t  	megsb_led0_current;
	uint8_t  	megsb_led1_current;
	_Bool 		megsa_filter_pos_known;
	_Bool 		megsa_filter_mech_moving;			// 30
	_Bool 		megsb_filter_pos_known;
	_Bool 		megsb_filter_mech_moving;
	_Bool 		sam_filter_pos_known;
	_Bool 		sam_filter_mech_moving;
	_Bool 		esp_filter_pos_known;				// 35
	_Bool 		esp_filter_mech_moving;
	uint16_t  	megsa_filter_step_number;		
	uint16_t  	megsb_filter_step_number;
	uint16_t  	sam_filter_step_number;
	uint16_t  	esp_filter_step_number;				//40
	uint16_t  	megsa_filter_position_number;
	uint16_t  	megsb_filter_position_number;
	uint16_t  	sam_filter_position_number;
	uint16_t  	esp_filter_position_number;
	uint16_t  	megsp_temperature_dn;				// 45
	float	  	megsp_temperature;
	uint16_t  	esp_temperature_dn;				
	float  		esp_temperature;
	uint16_t  	megsa_ccd_temperature_dn;
	float		megsa_ccd_temperature;				// 50
	uint16_t  	megsb_ccd_temperature_dn;		
	float		megsb_ccd_temperature;
	uint32_t  	met_seconds;					
	uint32_t  	met_subseconds;
	uint32_t  	stcf_seconds;						// 55
	uint32_t  	stcf_subseconds;	
	uint32_t   	leap_seconds;					
	uint32_t 	time_status;
	uint32_t  	tai_time_seconds;
	uint32_t  	tai_time_subseconds;				// 60
	uint16_t	megsa_resolver_number;
	uint16_t	megsb_resolver_number;
	uint16_t	sam_resolver_number;				
	uint16_t	esp_resolver_number;
	uint16_t  	sdn_core_temperature_dn;			// 65
	float		sdn_core_temperature;
	uint16_t  	power_services_temperature_dn;	
	float		power_services_temperature;
	uint16_t  	optics_posX_posY_temperature_dn;
	float		optics_posX_posY_temperature;		// 70
	uint16_t  	optics_negX_posY_temperature_dn;
	float		optics_negX_posY_temperature;	
	uint16_t  	megsa_ceb_temperature_dn;
	float		megsa_ceb_temperature;		
	uint16_t  	megsb_ceb_temperature_dn;			// 75
	float		megsb_ceb_temperature;
	uint16_t 	ma_integration_time;
	_Bool 		ma_hw_test;
  	_Bool 		ma_sw_test;
  	_Bool 		ma_reverse_clock;
  	_Bool 		ma_valid;
  	_Bool 		ma_ram_bank;
  	_Bool 		ma_int_time_warn;
  	uint8_t 	ma_readout_mode;
	uint16_t 	mb_integration_time;
	_Bool 		mb_hw_test;
  	_Bool 		mb_sw_test;
  	_Bool 		mb_reverse_clock;
  	_Bool 		mb_valid;
  	_Bool 		mb_ram_bank;
  	_Bool 		mb_int_time_warn;
  	uint8_t 	mb_readout_mode;
} __attribute__ ((packed));
struct SHK_PACKET shk_data[200];

struct PHOTOMETER_PACKET
{
	uint8_t		ESP_xfer_cnt;
	_Bool		ESP_V_ref;
	_Bool		ESP_valid;
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
	_Bool		MP_V_ref;
	_Bool		MP_valid;
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
	_Bool reverse_clock;
	_Bool ram_bank;
	_Bool int_time_warn;
	uint8_t readout_mode;
	float ccd_temp;
	_Bool led_on; 
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
  uint16_t integration_time;
  _Bool hw_test;
  _Bool sw_test;
  _Bool reverse_clock;
  _Bool valid;
  _Bool ram_bank;
  _Bool int_time_warn;
  uint8_t filter_position;
  uint8_t readout_mode;
  float ccd_temp;
  _Bool led_on; 
  uint8_t led_0_level;
  uint8_t led_1_level;
  uint16_t resolver;
  uint16_t sam_resolver;
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