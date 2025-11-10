#include "r_detector.h"
#include "ecg_processor.h"
#include "leds.h"
#include "urf_timer.h"

sRdetector r_detector;

static const uint8_t use_lemur_variant = 0;

static float r_fit_coeff()
{
	uint32_t ms = millis();
	int dt = ms - r_detector.cur_peak_time;
	int prev_rr = r_detector.cur_peak_time - r_detector.prev_peak_time;
	float fit_c = (float)dt / (float)(prev_rr + 1.0f); //avoiding zero check
	float dd = fit_c - 1;
	dd *= dd*10.0f;
	dd = 0.6 + dd; //thresholds would be lower to 0.6 of normal value at exact RR position, going up to 1.0 at <0.8 and above 1.2 of exact RR position
	if(dd > 1.3f) dd = 1.3f;
	if(use_lemur_variant && dd > 1.0f) dd = 1.0f;
	return dd;
}

void r_detector_init()
{
	r_detector.avg_s = 0;
	r_detector.avg_l = 0;
	r_detector.avg_dv_p = 0;
	r_detector.avg_dv_n = 0;
	r_detector.dv_p_peak = 0;
	r_detector.dv_n_peak = 0;
	r_detector.dv_p_peak_time = 0;
	r_detector.dv_n_peak_time = 0;
	r_detector.R_time = 0;
	r_detector.R_detected = 0;

	r_detector.v_dec_avg = 0;
	r_detector.v_dec = 0;
	r_detector.dec_l = 8;
	r_detector.dec_p = 0;
}

void r_detector_step(float vraw)
{
	static float lp_filtered_v = 0;
	if(use_lemur_variant)
	{
		lp_filtered_v *= 0.98; //lemur variant
		lp_filtered_v += 0.02*vraw;
	}
	else
	{
		lp_filtered_v *= 0.99; //human variant
		lp_filtered_v += 0.01*vraw;
	}
	float v = vraw - lp_filtered_v;
	//calculate short average and speed of signal change
	if(use_lemur_variant)
	{
		r_detector.avg_s *= 0.7;
		r_detector.avg_s += 0.3*v;
		r_detector.avg_l *= 0.85;
		r_detector.avg_l += 0.15*v;
	}
	else
	{
		r_detector.avg_s *= 0.8;
		r_detector.avg_s += 0.2*v;
		r_detector.avg_l *= 0.9;
		r_detector.avg_l += 0.1*v;
	}

	float dv = r_detector.avg_s - r_detector.avg_l;
	//fast average of current speed of signal change in positive and negative directions
	if(use_lemur_variant)
	{
		r_detector.avg_dv_p *= 0.8;
		r_detector.avg_dv_n *= 0.8;
		if(dv > 0)
			r_detector.avg_dv_p += 0.2 * dv;
		else
			r_detector.avg_dv_n -= 0.2 * dv; //minus sign because dv is negative but we want positive value
	}
	else
	{
		r_detector.avg_dv_p *= 0.9;
		r_detector.avg_dv_n *= 0.9;
		if(dv > 0)
			r_detector.avg_dv_p += 0.1 * dv;
		else
			r_detector.avg_dv_n -= 0.1 * dv; //minus sign because dv is negative but we want positive value
	}

	//slowly reduce peak value over time, so that next R beat definitely will be higher than previous
	//absolute peak value isn't very stable, but with this method it works well
	if(use_lemur_variant)
	{
		r_detector.dv_p_peak *= 0.997; //lemur variant
		r_detector.dv_n_peak *= 0.997;
	}
	else
	{
		r_detector.dv_p_peak *= 0.9992; //human variant
		r_detector.dv_n_peak *= 0.9992;
	}

	//by default assuming we are not in R peak, but that might change in following lines
	r_detector.R_detected = 0;

	//each R peak consists of high increase in positive and negative signal changes, but we
	//don't know their order in advance - it depends on sensor placement. So criteria is based
	//on detecting peak in one direction, and if opposite direction had a peak just before that -
	//then it's our R peak
	float fit_coeff = r_fit_coeff();
	static uint32_t last_p_triggered = 0;
	static uint32_t last_n_triggered = 0;
	uint32_t ms = millis();
	float fit_coeff_p = fit_coeff;
	float fit_coeff_n = fit_coeff;
	if(ms - last_p_triggered < 100 - 70*use_lemur_variant) fit_coeff_p = 1.01;
	if(ms - last_n_triggered < 100 - 70*use_lemur_variant) fit_coeff_n = 1.01;
	int max_peak_width = 40;
	if(use_lemur_variant) max_peak_width = 20;
	if(r_detector.avg_dv_p > r_detector.dv_p_peak*fit_coeff_p)
	{
		//update positive peak
		last_p_triggered = ms;
		r_detector.dv_p_peak_time = 0;
		r_detector.dv_p_peak = r_detector.avg_dv_p;
//		if(r_detector.dv_p_peak > 5 * r_detector.p_peak_v) r_detector.dv_p_peak = 5 * r_detector.p_peak_v; //limit effects of false detections due to signal spikes
		r_detector.p_peak_v = r_detector.avg_dv_p;
		r_detector.p_peak_vraw = vraw;
		if(r_detector.dv_n_peak_time < max_peak_width && r_detector.p_peak_v > r_detector.n_peak_v/3 && r_detector.p_peak_v < 3*r_detector.n_peak_v) //lemur variant
		{
			r_detector.R_detected = 1;
			r_detector.R_time = 0;
		}
	}
	if(r_detector.avg_dv_n > r_detector.dv_n_peak*fit_coeff_n)
	{
		//update negative peak
		last_n_triggered = ms;
		r_detector.dv_n_peak_time = 0;
		r_detector.dv_n_peak = r_detector.avg_dv_n;
//		if(r_detector.dv_n_peak > 5 * r_detector.n_peak_v) r_detector.dv_n_peak = 5 * r_detector.n_peak_v; //limit effects of false detections due to signal spikes
		r_detector.n_peak_v = r_detector.avg_dv_n;
		r_detector.n_peak_vraw = vraw;
		if(r_detector.dv_n_peak_time < max_peak_width && r_detector.p_peak_v > r_detector.n_peak_v/3 && r_detector.p_peak_v < 3*r_detector.n_peak_v) //lemur variant
		{
			r_detector.R_detected = 1;
			r_detector.R_time = 0;
		}
	}

	if(r_detector.R_time == 30) //~30 milliseconds after R peak - long enough
	{ //to make sure detected point is stable, so we can push new RR interval
		int time_threshold = 150;
		if(use_lemur_variant) time_threshold = 60;
		if(ms - r_detector.cur_peak_time > time_threshold) //detector is not perfect, so need to ignore multiple detections
		{
			r_detector.prev_peak_time = r_detector.cur_peak_time;
			r_detector.cur_peak_time = ms;
			int RR = r_detector.cur_peak_time - r_detector.prev_peak_time;
			if(RR < 2000) //if RR is too long - it means that we missed some previous R peak(s),
				push_RR(RR); //so we won't be adding RR if that was the case

			//indicate detected beat with led
			if(ecg_params.led_enabled)
				leds_pulse_default(32); //lemur variant
		}
	}

	//these times are in ADC data points, not milliseconds!
	//that is used only to indicate that some time has passed
	r_detector.dv_n_peak_time++;
	r_detector.dv_p_peak_time++;
	r_detector.R_time++;

	//that part is not exactly R peak processing, it is averaging of data for sending via BLE
	//at reduced data rate. But when we have an R peak, average value is not good enough: peak is
	//too fast, and we won't always see actual peak value on averaged stream. So when we are in
	//peak state, we use peak value instead of average value - that's why processing is
	//added into this function
	r_detector.dec_p++;
	r_detector.v_dec_avg += vraw;
	if(r_detector.dec_p >= r_detector.dec_l)
	{
		r_detector.dec_p = 0;
		r_detector.v_dec = r_detector.v_dec_avg / r_detector.dec_l; //normally use average value
		r_detector.v_dec_avg = 0;
		//but if peak was within our averaging interval, use it instead
		//that somewhat deforms ECG shape around the peak, but since low frequency
		//data stream isn't good for R shape analysis anyway, we chose to preserve
		//R peak amplitude instead of its shape
		if(r_detector.dv_n_peak_time <= r_detector.dec_l)
			r_detector.v_dec = r_detector.n_peak_vraw;
		if(r_detector.dv_p_peak_time <= r_detector.dec_l)
			r_detector.v_dec = r_detector.p_peak_vraw;
	}
}
