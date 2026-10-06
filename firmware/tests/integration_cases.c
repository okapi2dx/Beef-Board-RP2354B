static void mapping_tests(void) {
 usb_ready=true;cfg[85]=0;pending_tt_buttons=0;turntable.output.direction=0;
 for(unsigned physical=0;physical<11;physical++) {
  stable_buttons=1u<<physical;
  cfg[35+physical]=4+physical;
  for(unsigned destination=0;destination<12;destination++) {
   cfg[127+physical]=destination;cfg[32]=0;
   logical_buttons=controller_buttons();
   assert(send_controller_report());
   uint16_t actual=(uint16_t)report[2]|((uint16_t)report[3]<<8);
   unsigned bit=destination==11?7:destination<7?destination:destination+1;
   assert(actual==(1u<<bit));
   cfg[32]=1;logical_buttons=controller_buttons();
   assert(send_controller_report());
   assert(report_instance==BEEF_HID_INSTANCE_KEYBOARD && report[0]==4+physical && report[1]==0);
  }
 }
 cfg[32]=0;cfg[127]=11;cfg[128]=11;stable_buttons=3;
 logical_buttons=controller_buttons();assert(send_controller_report());
 assert(report[2]==128 && report[3]==0);
 stable_buttons=0;logical_buttons=controller_buttons();assert(send_controller_report());
 assert(report[2]==0 && report[3]==0);
 memset(cfg,0,sizeof(cfg));previous_input_mode=0xff;report_count=0;reported_buttons=0;
 puts("PASS mappings: all 11 physical inputs x 12 destinations, physical keyboard bindings, duplicate B8 and release");
}
static void digital_diagnostics_tests(void) {
 for(unsigned mode=0;mode<2;mode++) for(unsigned digital=0;digital<2;digital++) for(int direction=-1;direction<=1;direction++) {
  cfg[32]=mode;cfg[85]=digital;stable_buttons=0x401;turntable.output.direction=direction;
  button_debouncer.raw=0x203;
  update_diagnostics();
  uint16_t expected=stable_buttons;
  if(digital && direction<0)expected|=1u<<11;
  if(digital && direction>0)expected|=1u<<12;
  assert(diagnostics.buttons==expected);
  assert(diagnostics.raw_buttons==0x203);
 }
 memset(cfg,0,sizeof(cfg));stable_buttons=0;turntable.output.direction=0;
 puts("PASS digital diagnostics: physical buttons plus B13/B14 direction; keyboard/joystick, release and analog isolation");
}
int main(void){
 mapping_tests();
 digital_diagnostics_tests();
 cfg[30]=10;cfg[78]=60;cfg[79]=3;cfg[80]=24;cfg[138]=16;cfg[7]=255;cfg[8]=255;
 for(unsigned i=0;i<11;i++)cfg[127+i]=i;
 cfg[32]=1;cfg[85]=1;cfg[47]=0x52;pending_tt_buttons=1u<<12;
 usb_ready=false;send_controller_report();assert(pending_tt_buttons && report_count==0);
 usb_ready=true;send_controller_report();assert(report[0]==0x52 && pending_tt_buttons==0);
 send_controller_report();assert(report[0]==0);
 stable_buttons=1;logical_buttons=1;usb_ready=false;
 assert(!send_controller_report() && reported_buttons==0);
 usb_ready=true;assert(send_controller_report() && reported_buttons==1);
 stable_buttons=0;logical_buttons=0;assert(send_controller_report());
 cfg[32]=0;send_controller_report();assert(report_instance==BEEF_HID_INSTANCE_GAMEPAD);
 cfg[127]=4;stable_buttons=1;logical_buttons=controller_buttons();assert(logical_buttons==(1u<<4));
 update_diagnostics();assert(diagnostics.buttons==1); // No double remapping in UI.
 cfg[88]=100;cfg[125]=100;update_button_leds(0);assert(led_pwm_duty[0]==100);
 stable_buttons=0;update_button_leds(1);update_button_leds(51);assert(led_pwm_duty[0]==50);
 update_button_leds(101);assert(led_pwm_duty[0]==0);
 cfg[90]=1;cfg[102]=37;cfg[113]=1;update_button_leds(102);assert(led_pwm_duty[0]==37);
 cfg[5]=1;update_button_leds(103);assert(led_pwm_duty[0]==0);cfg[5]=0;
 beef_lighting_update(&lighting,cfg,0,127,0,0);
 fake_us=20000;update_rgb_leds(20);assert(dma_starts==1 && dma_count[0]==24);
 uint32_t old=ring_words[23];assert(old!=0);
 cfg[80]=1;fake_us=40000;update_rgb_leds(40);assert(dma_starts==1 && ring_words[23]==old);
 dma_busy=false;update_rgb_leds(40);assert(dma_starts==1);
 fake_us=41000;update_rgb_leds(41);assert(dma_starts==2 && dma_count[0]==24 && ring_words[23]==0);
 dma_busy=false;fake_us=60000;update_rgb_leds(60);fake_us=61000;update_rgb_leds(61);
 assert(dma_starts==3 && dma_count[0]==1);
 puts("PASS integration: USB backpressure/TT release, mode switching, physical diagnostics, PWM fade/inversion, nonblocking DMA and tail clearing");
 return 0;
}
