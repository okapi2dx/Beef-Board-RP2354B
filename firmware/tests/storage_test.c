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
 assert(beef_config_valid(c));
 // Fill both sectors, then fail the erase+program rollover.
 for(unsigned i=0;i<32;i++){c[6]=i+1;assert(beef_config_store_write(c));}
 c[6]=90;fail_program=1;assert(!beef_config_store_write(c));
 beef_config_store_init();assert(beef_config_store_data()[6]==32);
 fail_program=0;c[6]=91;assert(beef_config_store_write(c));beef_config_store_init();assert(beef_config_store_data()[6]==91);
 // Multiple rotations must continue choosing the latest committed record.
 for(unsigned i=0;i<100;i++){c[6]=i;assert(beef_config_store_write(c));beef_config_store_init();assert(beef_config_store_data()[6]==i);}
 c[80]=0;assert(!beef_config_store_stage(c));assert(beef_config_store_data()[80]==24);
 assert(beef_config_store_reset());beef_config_store_init();assert(beef_config_store_data()[6]==0);
 // Existing releases used the last sector only. Import that format in place.
 memset(fake_flash,255,sizeof(fake_flash));config_record_t rec;
 memset(&rec,255,sizeof(rec));rec.magic=CONFIG_FLASH_MAGIC;rec.format=CONFIG_FLASH_FORMAT;
 rec.sequence=42;set_defaults(rec.data);rec.data[6]=77;rec.checksum=checksum_config(rec.sequence,rec.data);
 memcpy(fake_flash+12288,&rec,sizeof(rec));beef_config_store_init();assert(beef_config_store_data()[6]==77);
 memset(fake_flash,255,sizeof(fake_flash));rec.sequence=0xfffffffeu;rec.checksum=checksum_config(rec.sequence,rec.data);
 memcpy(fake_flash+12288,&rec,sizeof(rec));beef_config_store_init();memcpy(c,rec.data,sizeof(c));c[6]=78;assert(beef_config_store_write(c));
 c[6]=79;assert(beef_config_store_write(c));beef_config_store_init();assert(beef_config_store_data()[6]==79);
 puts("PASS storage: interrupted rollover preserves prior settings, retries, rotation, invalid config, reset, legacy import and sequence wrap");
 return 0;
}
