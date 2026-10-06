#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host_flash.h"
uint8_t fake_flash[PICO_FLASH_SIZE_BYTES];
static int fail_program;
void flash_range_erase(uint32_t offset,size_t count){assert(offset>=8192);memset(fake_flash+offset,255,count);}
void flash_range_program(uint32_t offset,const uint8_t *data,size_t count){
 if(fail_program)count=32;
 for(size_t i=0;i<count;i++)fake_flash[offset+i]&=data[i];
}
int flash_safe_execute(void (*callback)(void *),void *arg,uint32_t timeout){(void)timeout;callback(arg);return 0;}
#include "../src/beef_config_store.c"
int main(void){
 memset(fake_flash,255,sizeof(fake_flash));beef_config_store_init();
 uint8_t c[BEEF_CONFIG_SIZE];memcpy(c,beef_config_store_data(),sizeof(c));
 assert(beef_config_valid(c)); assert(c[30]==6); assert(((unsigned)c[71]+256u*c[72])==1000);
 c[0]=43;c[30]=10;c[3]=5;assert(beef_config_store_write(c));beef_config_store_init();
 assert(beef_config_store_data()[0]==45 && beef_config_store_data()[30]==6 && beef_config_store_data()[3]==5);
 memcpy(c,beef_config_store_data(),sizeof(c));
 for(unsigned gain=0;gain<=2000;gain++) { memcpy(c,beef_config_store_data(),sizeof(c));c[0]=45;c[71]=gain;c[72]=gain>>8;assert(beef_config_valid(c)); }
 c[71]=2001&255;c[72]=2001>>8;assert(!beef_config_store_stage(c));memcpy(c,beef_config_store_data(),sizeof(c));
 // Encoder thresholds are stored exactly; legacy angles reset only byte 3.
 for(unsigned count=0;count<=50;count++){
  c[3]=count;c[0]=44;assert(beef_config_store_write(c));beef_config_store_init();
  assert(beef_config_store_data()[0]==45 && beef_config_store_data()[3]==count);
 }
 c[0]=42;c[3]=10;c[6]=77;c[30]=5;
 assert(beef_config_store_write(c));beef_config_store_init();
 assert(beef_config_store_data()[0]==45 && beef_config_store_data()[3]==0);
 assert(beef_config_store_data()[6]==77 && beef_config_store_data()[30]==3);
 memcpy(c,beef_config_store_data(),sizeof(c));
 c[3]=51;assert(!beef_config_store_stage(c));c[3]=0;
 // Fill both sectors, then fail the erase+program rollover.
 for(unsigned i=0;i<32;i++){c[6]=i+1;assert(beef_config_store_write(c));}
 c[6]=90;fail_program=1;assert(!beef_config_store_write(c));
 beef_config_store_init();assert(beef_config_store_data()[6]==32);
 fail_program=0;c[6]=91;assert(beef_config_store_write(c));beef_config_store_init();assert(beef_config_store_data()[6]==91);
 // Multiple rotations must continue choosing the latest committed record.
 for(unsigned i=0;i<100;i++){c[6]=i;assert(beef_config_store_write(c));beef_config_store_init();assert(beef_config_store_data()[6]==i);}
 c[78]=0;assert(!beef_config_store_stage(c));
 assert(beef_config_store_data()[80]==24);
 assert(beef_config_store_reset());beef_config_store_init();assert(beef_config_store_data()[6]==0);
 // v36 millisecond settings migrate to v43 tenths without changing duration.
 memcpy(c,beef_config_store_data(),sizeof(c));c[0]=36;c[48]=3;c[64]=7;
 assert(beef_config_store_write(c));beef_config_store_init();
 assert(beef_config_store_data()[0]==45 && beef_config_store_data()[48]==30 && beef_config_store_data()[64]==70);
 memcpy(c,beef_config_store_data(),sizeof(c));c[0]=37;c[48]=17;c[64]=1;
 assert(beef_config_store_write(c));beef_config_store_init();
 assert(beef_config_store_data()[0]==45 && beef_config_store_data()[48]==17 && beef_config_store_data()[64]==1);
 memcpy(c,beef_config_store_data(),sizeof(c));c[0]=38;
 assert(beef_config_store_write(c));beef_config_store_init();
 assert(beef_config_store_data()[0]==45 && beef_config_store_data()[48]==17 && beef_config_store_data()[64]==1);
 memcpy(c,beef_config_store_data(),sizeof(c));c[0]=39;
 assert(beef_config_store_write(c));beef_config_store_init();
 assert(beef_config_store_data()[0]==45 && beef_config_store_data()[48]==17 && beef_config_store_data()[64]==1);
 memcpy(c,beef_config_store_data(),sizeof(c));c[0]=40;
 assert(beef_config_store_write(c));beef_config_store_init();assert(beef_config_store_data()[0]==45);
 memcpy(c,beef_config_store_data(),sizeof(c));c[48]=1;c[64]=159;
 assert(beef_config_store_write(c));beef_config_store_init();
 assert(beef_config_store_data()[48]==1 && beef_config_store_data()[64]==159);
 c[48]=161;assert(!beef_config_store_stage(c));
 // Existing releases used the last sector only. Import that format in place.
 memset(fake_flash,255,sizeof(fake_flash));config_record_t rec;
 memset(&rec,255,sizeof(rec));rec.magic=CONFIG_FLASH_MAGIC;rec.format=CONFIG_FLASH_FORMAT;
 rec.sequence=42;set_defaults(rec.data);rec.data[6]=77;rec.data[3]=200;rec.checksum=checksum_record(&rec);
 memcpy(fake_flash+12288,&rec,sizeof(rec));beef_config_store_init();assert(beef_config_store_data()[6]==77 && beef_config_store_data()[3]==50);
 memset(fake_flash,255,sizeof(fake_flash));rec.sequence=0xfffffffeu;rec.checksum=checksum_record(&rec);
 memcpy(fake_flash+12288,&rec,sizeof(rec));beef_config_store_init();memcpy(c,beef_config_store_data(),sizeof(c));c[6]=78;assert(beef_config_store_write(c));
 c[6]=79;assert(beef_config_store_write(c));beef_config_store_init();assert(beef_config_store_data()[6]==79);
 // UART flag and center-bar count survive reboot; odd counts are rejected.
 memcpy(c,beef_config_store_data(),sizeof(c));c[61]=6;
 assert(beef_config_store_set_legacy_phoenixwan_enabled(false));
 assert(beef_config_store_write(c));beef_config_store_init();
 assert(!beef_config_store_legacy_phoenixwan_enabled() && beef_config_store_data()[61]==6);
 c[61]=3;assert(!beef_config_store_write(c));
 // A corrupted enable flag must fail checksum validation.
 config_record_t corrupt=*slot_ptr(newest_slot);
 corrupt.legacy_phoenixwan_enabled ^= 1u;
 assert(!record_valid(&corrupt));
 // Original format 2 records remain readable and migrate to protected format 3.
 memset(fake_flash,255,sizeof(fake_flash));memset(&rec,255,sizeof(rec));
 rec.magic=CONFIG_FLASH_MAGIC;rec.format=2;rec.sequence=9;set_defaults(rec.data);
 rec.data[0]=41;rec.data[61]=0;rec.data[6]=88;
 rec.checksum=checksum_config(rec.sequence,rec.data);
 memcpy(fake_flash+12288,&rec,sizeof(rec));beef_config_store_init();
 assert(beef_config_store_data()[0]==45 && beef_config_store_data()[61]==16);
 assert(beef_config_store_data()[6]==88 && beef_config_store_legacy_phoenixwan_enabled());
 assert(slot_ptr(newest_slot)->format==3);
 // Adjustable LED counts persist, including disabled outputs and 255 pixels.
 for(unsigned count=0;count<=50;++count){
  memcpy(c,beef_config_store_data(),sizeof(c));c[80]=count;c[138]=255-count;
  assert(beef_config_store_write(c));beef_config_store_init();
  assert(beef_config_store_data()[80]==count && beef_config_store_data()[138]==255-count);
 }
 assert(beef_config_store_reset());
 assert(beef_config_store_data()[80]==24 && beef_config_store_data()[138]==16);
 puts("PASS storage: journal recovery, reset defaults and LED counts 0..255");
 return 0;
}
