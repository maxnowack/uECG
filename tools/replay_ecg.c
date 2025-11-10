#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../uECG_v5/board_config.h"
#include "../uECG_v5/ecg_processor.h"
#include "../uECG_v5/mcp3911.h"
#include "../urf_lib/urf_timer.h"

/* -------------------------------------------------------------------------
 * Host stubs for firmware dependencies
 * ------------------------------------------------------------------------- */

sBoardConfig board_config = {0};
sDeviceState dev_state = {0};
sDeviceConfig dev_config = {
	.validation_word = 0,
	.led_enabled = 1,
	.ecg_advertising_enabled = 0,
	.led_color_r = 0,
	.led_color_g = 0,
	.led_color_b = 0,
	.gyro_enabled = 0,
	.radio_mode = radio_mode_off,
	.signal_measurement_mode = signal_measurement_ecg,
	.power_mode = device_power_normal
};

static double simulated_time_ms = 0.0;

void time_start(void) {}
void time_pause(void) {}
void time_resume(void) {}
void time_stop(void) {}
void time_adjust(int ms_shift)
{
	simulated_time_ms += ms_shift;
	if(simulated_time_ms < 0.0)
		simulated_time_ms = 0.0;
}
uint32_t micros(void)
{
	if(simulated_time_ms < 0.0) simulated_time_ms = 0.0;
	return (uint32_t)(simulated_time_ms * 1000.0);
}
uint32_t micros_count(void)
{
	return micros();
}
uint32_t millis(void)
{
	if(simulated_time_ms < 0.0) simulated_time_ms = 0.0;
	return (uint32_t)simulated_time_ms;
}
uint32_t seconds(void)
{
	if(simulated_time_ms < 0.0) simulated_time_ms = 0.0;
	return (uint32_t)(simulated_time_ms / 1000.0);
}
void delay_ms(uint32_t ms)
{
	simulated_time_ms += ms;
}
void delay_mcs(uint32_t mcs)
{
	simulated_time_ms += mcs / 1000.0;
}
void schedule_event(uint32_t steps_dt, void (*tm_event)(void), int repeated)
{
	(void)steps_dt;
	(void)tm_event;
	(void)repeated;
}
void schedule_subevent1(uint32_t steps_dt, void (*tm_event)(void))
{
	(void)steps_dt;
	(void)tm_event;
}
void schedule_subevent2(uint32_t steps_dt, void (*tm_event)(void))
{
	(void)steps_dt;
	(void)tm_event;
}
void schedule_event_delayed(uint32_t delay, uint32_t steps_dt, void (*tm_event)(void), int repeated)
{
	(void)delay;
	(void)steps_dt;
	(void)tm_event;
	(void)repeated;
}
void schedule_event_adjust(int dt)
{
	(void)dt;
}
void schedule_event_stop(void) {}
void schedule_event_cancel_sub1(void) {}
void schedule_event_cancel_sub2(void) {}

void leds_init(int pin_r, int pin_g, int pin_b, int pin_driver)
{
	(void)pin_r;
	(void)pin_g;
	(void)pin_b;
	(void)pin_driver;
}
void leds_set(int r, int g, int b)
{
	(void)r;
	(void)g;
	(void)b;
}
void leds_set_driver(int val)
{
	(void)val;
}
void leds_pulse(int r, int g, int b, int length)
{
	(void)r;
	(void)g;
	(void)b;
	(void)length;
}
void leds_set_default_color(int r, int g, int b)
{
	(void)r;
	(void)g;
	(void)b;
}
void leds_pulse_default(int length)
{
	(void)length;
}

static int latest_filtered_value = 0;

int mcp_get_filtered_value(void)
{
	return latest_filtered_value;
}
int mcp_get_filtered_skin(void)
{
	return 0;
}
void mcp_set_filter_mode(int use_filter)
{
	(void)use_filter;
}
void mcp_start_clock(void) {}
void mcp_stop_clock(void) {}
int mcp_fft_process(void)
{
	return 0;
}
float *mcp_fft_get_spectr(void)
{
	static float dummy[8] = {0};
	return dummy;
}

float sqrt_fast(float x)
{
	if(x <= 0.0f)
		return 0.0f;
	return sqrtf(x);
}

/* -------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

static double parse_timestamp_ms(const char *timestamp, double *base_ms)
{
	int year, month, day, hour, minute;
	double seconds;
	if(sscanf(timestamp, "%d-%d-%d %d:%d:%lf",
	          &year, &month, &day, &hour, &minute, &seconds) != 6)
		return -1.0;

	double total_ms = (((((day * 24.0 + hour) * 60.0 + minute) * 60.0) + seconds) * 1000.0);
	if(*base_ms < 0.0)
		*base_ms = total_ms;
	return total_ms - *base_ms;
}

static void usage(const char *prog)
{
	fprintf(stderr, "Usage: %s <input_csv> [output_csv]\n", prog);
	fprintf(stderr, "  Default input: data/ecg_5m.csv\n");
	fprintf(stderr, "  Default output: tools/replay_rr_output.csv\n");
}

/* -------------------------------------------------------------------------
 * Replay driver
 * ------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
	const char *input_path = "data/ecg_5m.csv";
	const char *output_path = "tools/replay_rr_output.csv";
	const char *debug_path = NULL;

	int positional = 0;
	for(int i = 1; i < argc; ++i)
	{
		if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
		{
			usage(argv[0]);
			return 0;
		}
		else if(strcmp(argv[i], "--debug") == 0 && (i + 1) < argc)
		{
			debug_path = argv[++i];
		}
		else if(positional == 0)
		{
			input_path = argv[i];
			positional++;
		}
		else if(positional == 1)
		{
			output_path = argv[i];
			positional++;
		}
		else
		{
			usage(argv[0]);
			return 1;
		}
	}

	FILE *input = fopen(input_path, "r");
	if(!input)
	{
		fprintf(stderr, "Failed to open %s: %s\n", input_path, strerror(errno));
		return 1;
	}
	FILE *output = fopen(output_path, "w");
	if(!output)
	{
		fprintf(stderr, "Failed to open %s for writing: %s\n", output_path, strerror(errno));
		fclose(input);
		return 1;
	}

	FILE *debug = NULL;
	if(debug_path)
	{
		debug = fopen(debug_path, "w");
		if(!debug)
		{
			fprintf(stderr, "Failed to open %s for debug output: %s\n", debug_path, strerror(errno));
			fclose(output);
			fclose(input);
			return 1;
		}
		fprintf(debug, "time_ms,value,avg_s,avg_l,avg_dv_p,avg_dv_n,dv_p_peak,dv_n_peak,p_peak_v,n_peak_v,dv_p_time,dv_n_time,R_detected,cur_peak_time,prev_peak_time\n");
	}

	fprintf(output, "rr_id,time_ms,rr_interval_ms,bpm_normal,bpm_momentary,sdrr,rmssd\n");

	ecg_processor_init();
	dev_state.battery_mv = 3700;

	char line[256];
	if(!fgets(line, sizeof(line), input))
	{
		fprintf(stderr, "Input file is empty\n");
		fclose(output);
		fclose(input);
		return 1;
	}

	double base_ms = -1.0;
	double prev_ts_ms = 0.0;
	uint32_t last_rr_id = ecg_params.rr_id;
	size_t sample_index = 0;
	double min_rr = 1e12;
	double max_rr = 0.0;
	size_t total_rr = 0;

	while(fgets(line, sizeof(line), input))
	{
		char *comma = strchr(line, ',');
		if(!comma)
			continue;
		*comma = '\0';
		const char *timestamp = line;
		const char *value_str = comma + 1;

		double ts_ms = parse_timestamp_ms(timestamp, &base_ms);
		if(ts_ms < 0.0)
			continue;

		if(sample_index == 0)
		{
			simulated_time_ms = ts_ms;
			prev_ts_ms = ts_ms;
		}
		else
		{
			double delta = ts_ms - prev_ts_ms;
			if(delta < 0.0)
				delta = 0.0;
			simulated_time_ms += delta;
			prev_ts_ms = ts_ms;
		}

		latest_filtered_value = (int)lrint(strtod(value_str, NULL));
		r_detector_step((float)latest_filtered_value);

		if(debug)
		{
			const sRdetector *rd = get_r_detector();
			fprintf(debug, "%.3f,%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%d,%d,%d,%u,%u\n",
			        simulated_time_ms,
			        latest_filtered_value,
			        rd->avg_s,
			        rd->avg_l,
			        rd->avg_dv_p,
			        rd->avg_dv_n,
			        rd->dv_p_peak,
			        rd->dv_n_peak,
			        rd->p_peak_v,
			        rd->n_peak_v,
			        rd->dv_p_peak_time,
			        rd->dv_n_peak_time,
			        rd->R_detected,
			        rd->cur_peak_time,
			        rd->prev_peak_time);
		}

		if(ecg_params.rr_id != last_rr_id)
		{
			uint16_t rr = get_RR(1);
			if(rr > max_rr)
				max_rr = rr;
			if(rr < min_rr)
				min_rr = rr;
			fprintf(output, "%u,%.3f,%u,%d,%d,%.2f,%.2f\n",
			        ecg_params.rr_id,
			        simulated_time_ms,
			        rr,
			        ecg_params.BPM_normal,
			        ecg_params.BPM_momentary,
			        hrv_params.sdrr,
			        hrv_params.rmssd);
			last_rr_id = ecg_params.rr_id;
			total_rr++;
		}

		sample_index++;
	}

	fprintf(output, "#total_rr=%zu,min_rr=%.1f,max_rr=%.1f\n",
	        total_rr,
	        (min_rr < 1e11) ? min_rr : 0.0,
	        max_rr);

fclose(output);
fclose(input);
if(debug)
	fclose(debug);

fprintf(stdout, "Replayed %zu samples, detected %zu RR intervals.\n", sample_index, total_rr);
return 0;
}
