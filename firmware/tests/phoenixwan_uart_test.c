#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hardware/uart.h"
#include "phoenixwan_uart.h"
uart_inst_t fake_uart[2];
void uart_set_format(uart_inst_t *p,unsigned bits,unsigned stop,unsigned parity) {
  (void)p; assert(bits==8 && stop==1 && parity==UART_PARITY_NONE);
}
void uart_set_hw_flow(uart_inst_t *p,bool cts,bool rts) { (void)p; assert(!cts && !rts); }
bool uart_is_writable(uart_inst_t *p) { return p->writable; }
bool uart_is_readable(uart_inst_t *p) { return p->rx_head<p->rx_count; }
void uart_putc_raw(uart_inst_t *p,uint8_t byte) { assert(p->tx_count<sizeof(p->tx)); p->tx[p->tx_count++]=byte; }
uint8_t uart_getc(uart_inst_t *p) { return p->rx[p->rx_head++]; }
static void receive(uart_inst_t *p,const uint8_t *data,size_t size) {
  memcpy(p->rx,data,size);p->rx_count=size;p->rx_head=0;
}
static uint8_t cfg[139];
static uint32_t now=1;
static void task(bool enabled) { phoenixwan_uart_task(now++,cfg,0,1000,0,enabled); }
int main(void) {
  fake_uart[0].writable=fake_uart[1].writable=true;
  cfg[0]=42;cfg[61]=16;
  for(unsigned i=6;i<30;i++)cfg[i]=(uint8_t)(i*7u);
  phoenixwan_uart_init();
  const uint8_t tt_init[]={0x5a,0x7e,0x5f,0x78,0x55,0x74,0xaf,0xf8};
  const uint8_t bar_init[]={0xf0,0x5a};
  receive(uart0,tt_init,sizeof(tt_init));receive(uart1,bar_init,sizeof(bar_init));task(true);
  const uint8_t expected_tt[]={0x5a,0,0,0,0,0,0,0xa5,0x31};
  assert(uart0->tx_count==sizeof(expected_tt) && !memcmp(uart0->tx,expected_tt,sizeof(expected_tt)));
  assert(uart1->tx_count==2 && uart1->tx[0]==0xa5 && uart1->tx[1]==0x6f);
  const uint8_t effects[]={0,1,2,3,4,6,7};
  const uint8_t masks[]={7,1,6,6,6,3,3};
  for(unsigned mode=0;mode<7;mode++) {
    uart0->tx_count=0;cfg[2]=effects[mode];task(true);
    if(mode)assert(uart0->tx_count==1 && uart0->tx[0]==0x31+mode);
    else assert(uart0->tx_count==0);
    task(true);uart0->tx_count=0;task(true);assert(!uart0->tx_count);
    for(uint8_t channel=0;channel<3;channel++)for(unsigned up=0;up<2;up++) {
      uart0->tx_count=0;phoenixwan_uart_adjust_turntable_color(channel,up!=0);task(true);
      bool supported=(masks[mode]&(1u<<channel))!=0;
      assert(uart0->tx_count==(supported?1u:0u));
      if(supported)assert(uart0->tx[0]==(up?0x81u:0x91u)+channel);
    }
  }
  for(unsigned count=0;count<=16;count+=2) {
    uart1->tx_count=0;cfg[61]=(uint8_t)count;task(true);
    assert(uart1->tx_count==1 && uart1->tx[0]==(count?0x67u+count/2u:0x4cu));
  }
  for(uint8_t channel=0;channel<3;channel++)for(unsigned up=0;up<2;up++) {
    uart1->tx_count=0;phoenixwan_uart_adjust_centerbar_color(channel,up!=0);task(true);
    assert(uart1->tx_count==1 && uart1->tx[0]==(up?0x81u:0x91u)+channel);
  }
  uart1->tx_count=0;uart1->writable=false;
  phoenixwan_uart_adjust_centerbar_color(0,true);task(true);assert(!uart1->tx_count);
  uart1->writable=true;task(true);assert(uart1->tx_count==1 && uart1->tx[0]==0x81);
  uart0->tx_count=uart1->tx_count=0;
  phoenixwan_uart_adjust_turntable_color(0,true);phoenixwan_uart_adjust_centerbar_color(0,true);
  task(false);assert(!uart0->tx_count && !uart1->tx_count);
  task(true);uart0->tx_count=uart1->tx_count=0;task(true);
  assert(!uart0->tx_count && !uart1->tx_count);
  cfg[5]=1;task(true);assert(uart1->tx_count==1 && uart1->tx[0]==0x4c);
  puts("PASS Phoenixwan UART: handshakes, seven modes, per-mode HSB masks, no cross-mode color changes, count 0..16, direct steps, retry and disable");
}
