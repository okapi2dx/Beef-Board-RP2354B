from pathlib import Path
import subprocess, tempfile, re
root=Path("firmware")
config_source=(root/"at90usb1286/config.cpp").read_text(encoding="utf-8")
begin=config_source.index("void reset_config(config* self)")
end=config_source.index("\nvoid config_init",begin)
reset=config_source[begin:end]
migration_hold=re.search(r'case 11:\s*(self->tt_sustain_ms = \d+;)',config_source).group(1).replace('self->','c->')
command=(root/"at90usb1286/beef.cpp").read_text(encoding="utf-8")
begin=command.index("case Command::ResetConfig:")
end=command.index("      break;",begin)
body=command[begin+len("case Command::ResetConfig:"):end]
source=r"""
#include <stdint.h>
#include <cstring>
#include <cassert>
struct config { uint8_t version,tt_ratio,tt_sustain_ms; struct {uint8_t padding[4];} sdvx_keys; };
static config current_config,saved,eeprom;
#define CONFIG_BASE_ADDR (&eeprom)
static void config_update(config* c) { c->version=47;c->tt_ratio=3;c->sdvx_keys.padding[0]=500&255;c->sdvx_keys.padding[1]=500>>8; HOLD_MIGRATION }
static void eeprom_update_block(const void* from,void* to,unsigned length){memcpy(to,from,length);}
static void config_save(const config &c){saved=c;eeprom=c;}
static bool rebooted=false;
static void reboot(){rebooted=true;}
""".replace('HOLD_MIGRATION',migration_hold)+reset+"\nstatic void command_reset(){"+body+"}\n"+r"""
int main(){
 for(unsigned initial=0;initial<=2000;initial++){
 config factory={};factory.sdvx_keys.padding[0]=initial;factory.sdvx_keys.padding[1]=initial>>8;
 reset_config(&factory);
 assert(factory.tt_sustain_ms==0 && eeprom.tt_sustain_ms==0);
 assert((eeprom.sdvx_keys.padding[0]+256u*eeprom.sdvx_keys.padding[1])==1000);
 current_config={};current_config.sdvx_keys.padding[0]=initial;current_config.sdvx_keys.padding[1]=initial>>8;
 rebooted=false;command_reset();
 assert(rebooted);
 assert((saved.sdvx_keys.padding[0]+256u*saved.sdvx_keys.padding[1])==1000);
 assert((eeprom.sdvx_keys.padding[0]+256u*eeprom.sdvx_keys.padding[1])==1000);
 assert(saved.tt_ratio==6 && saved.tt_sustain_ms==0);
 }
}
"""
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/"reset.cpp").write_text(source,encoding="utf-8")
 subprocess.run(["g++","-std=c++11","-Wall","-Wextra","-Werror",str(p/"reset.cpp"),"-o",str(p/"reset.exe")],check=True)
 subprocess.run([str(p/"reset.exe")],check=True)
print("PASS actual AT90 factory/reset paths: every gain 0..2000 resets/saves 1.000 and hold 0ms")
