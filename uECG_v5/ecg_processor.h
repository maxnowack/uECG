#include <stdint.h>
#include "r_detector.h"

typedef struct sECGparams
{
	uint8_t emg_mode;
	int BPM_normal;
	int BPM_momentary;
	uint32_t rr_id;
	uint32_t data_id;
	uint32_t ble_data_id;

	int buf_len;
	int buf_pos;
	int data_buffer[64];

	int ble_buf_pos;
	int ble_buf_len;
	int data_buffer_ble[64];

	int skin_parameter;

	uint8_t led_enabled;

	uint32_t unsent_RR_cnt;
	uint32_t unsent_data_cnt;
}sECGparams;

typedef struct sEMGparams
{
	float value; //strength of muscle signal
	uint16_t level; //scaled and limited value
	uint16_t spectr_scale;
	uint8_t spectr_buf[8];
	uint32_t data_id;
	uint8_t led_enabled;
}sEMGparams;

typedef struct sHRVparams
{
	float avg_speed; //averaging speed
	float sdrr2;
	float sdrr;
	float rmssd2;
	float rmssd;
	int pNN_bins; //must be not more than following arrays size
	float pNN[16];
	uint8_t pNN_norm[16];
	float pnn_avg_speed;
}sHRVparams;


extern sHRVparams hrv_params;
extern sEMGparams emg_params;
extern sECGparams ecg_params;

int get_RR(int hist_depth);
int process_mcp_data();
void ecg_processor_init();
void set_emg_mode(int use_emg_mode);
void set_led_indication(int use_leds);

sRdetector *get_r_detector();
sHRVparams *get_hrv_params();
sEMGparams *get_emg_params();
sECGparams *get_ecg_params();
