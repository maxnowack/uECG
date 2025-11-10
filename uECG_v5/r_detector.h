#pragma once

#include <stdint.h>

/*
 * R detector interface
 *
 * How to implement a detector variant:
 * 1. Keep the global r_detector structure in sync with every new ADC sample: update the short/long averages,
 *    differential peaks, timestamps, and decimation accumulators so downstream code can read them at any time.
 * 2. Accept filtered ECG samples through r_detector_step(float sample). Callers feed the raw stream at the ADC rate
 *    and expect the function to run comfortably on the NRF52 MCU: keep per-sample work bounded and memory accesses
 *    predictable so that timing is deterministic and battery usage stays low.
 * 3. Whenever a full RR interval is confirmed, call push_RR(uint32_t rr_ms) exactly once (declared below, implemented
 *    in ecg_processor.c). This hands the interval to BPM/HRV logic and bumps the RR history.
 * 4. Keep r_detector.v_dec, r_detector.dec_p, and r_detector.dec_l coherent. process_mcp_data() uses these fields to
 *    downsample ECG data for BLE; if the detector wants to override the decimated value (e.g., inject peak amplitude),
 *    it should do so before dec_p resets to 0.
 * 5. When visual feedback is enabled (ecg_params.led_enabled == 1 from ecg_processor), indicate R detections by calling
 *    leds_pulse_default() or a similar helper, mirroring the current behaviour so users see beat flashes.
 * 6. Balance correctness with efficiency: the detector should reject noise and avoid double-counting beats, but must
 *    also avoid heavy floating-point math or long critical sections so overall battery consumption remains minimal.
 *
 * By honouring the contract above, alternative detector implementations can slot in without touching the rest of the
 * ecg_processor module.
 */

typedef struct sRdetector
{
	float avg_s;
	float avg_l;
	float avg_dv_p;
	float avg_dv_n;
	float dv_p_peak;
	float dv_n_peak;
	float p_peak_vraw;
	float n_peak_vraw;
	float p_peak_v;
	float n_peak_v;

	//WARNING: all times here are in ADC steps, which runs at 976 Hz - correction
	//is required if they are translated into milliseconds
	int dv_p_peak_time;
	int dv_n_peak_time;
	int R_time;
	int R_detected;

	float v_dec_avg;
	float v_dec;
	int dec_l;
	int dec_p;

	//those times are in milliseconds
	uint32_t prev_peak_time;
	uint32_t cur_peak_time;
} sRdetector;

extern sRdetector r_detector;

void push_RR(uint32_t RR);

void r_detector_init(void);
void r_detector_step(float vraw);
