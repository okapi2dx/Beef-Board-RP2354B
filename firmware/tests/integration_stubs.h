#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "beef_runtime.h"
#include "beef_lighting.h"
#include "button_debounce.h"
#include "button_chatter.h"
typedef unsigned uint;
static uint8_t cfg[139];
static const uint8_t *beef_config_store_data(void){return cfg;}
static beef_button_debouncer_t button_debouncer;
static beef_turntable_t turntable;
static beef_lighting_t lighting;
static uint16_t stable_buttons,logical_buttons,pending_tt_buttons,host_led_buttons;
static uint16_t reported_buttons;
static uint8_t previous_input_mode=0xff;
static bool host_lighting_seen,led_active[11],led_fading[11];
static uint32_t host_lighting_ms,led_released_ms[11];
static uint8_t led_pwm_duty[11];
static beef_button_chatter_t button_chatter;
static uint32_t save_and_disable_interrupts(void){return 0;}
static void restore_interrupts(uint32_t irq){(void)irq;}
static uint8_t host_tt_rgb[3],host_center_rgb[3];
static beef_rgb_t ring_pixels[255],bar_pixels[255];
static uint32_t ring_words[255],bar_words[255];
static uint8_t previous_tt_count,previous_bar_count;
static uint32_t rgb_last_frame,rgb_dma_done_us,fake_us;
static bool rgb_transmitting,rgb_latching,dma_busy,usb_ready;
static int dma_tt=0,dma_bar=1;
static unsigned dma_count[2],dma_starts;
static uint32_t time_us_32(void){return fake_us;}
static bool dma_channel_is_busy(int channel){(void)channel;return dma_busy;}
static void dma_channel_set_read_addr(int ch,const void *p,bool trigger){(void)ch;(void)p;(void)trigger;}
static void dma_channel_set_trans_count(int ch,uint count,bool trigger){(void)trigger;dma_count[ch]=count;}
static void dma_start_channel_mask(uint32_t mask){assert(mask==3);dma_busy=true;dma_starts++;}
static bool tud_mounted(void){return true;}
static bool tud_hid_n_ready(int instance){(void)instance;return usb_ready;}
static uint8_t report[13];static unsigned report_count,report_instance;
static bool tud_hid_n_report(int instance,int id,const void *data,unsigned size){
 (void)id;assert(size<=13);memset(report,0,sizeof(report));memcpy(report,data,size);
 report_count++;report_instance=instance;return true;
}
typedef struct __attribute__((packed)){uint8_t x,y;uint16_t buttons;} gamepad_payload_t;
typedef struct __attribute__((packed)){
 uint16_t buttons;uint8_t sensor_ab,raw_position,filtered_position,output_position;
 int8_t direction;uint32_t transitions;uint16_t last_transition_ms;uint8_t led_levels[11];
 uint16_t chatter_edges[7],raw_buttons,edge_sequence;
 uint8_t edge_events[BEEF_CHATTER_EVENT_COUNT * 3u];
} diagnostics_t;
static diagnostics_t diagnostics;
typedef struct __attribute__((packed)){uint16_t edges[7],detected[7];} chatter_counts_t;
static chatter_counts_t chatter_counts;
static uint32_t encoder_transitions,encoder_interval_ms;
static bool gpio_get(unsigned pin){(void)pin;return false;}
void beef_protocol_set_diagnostics(const uint8_t data[BEEF_DIAGNOSTICS_SIZE]){(void)data;}
void beef_protocol_set_chatter_counts(const uint8_t data[BEEF_CHATTER_COUNTS_SIZE]){(void)data;}

