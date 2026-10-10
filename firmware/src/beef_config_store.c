#include "beef_config_store.h"
#include "beef_runtime.h"
#include "turntable_sensitivity.h"

#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "pico/flash.h"

#define CONFIG_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - 2u * FLASH_SECTOR_SIZE)
#define CONFIG_FLASH_ADDRESS (XIP_BASE + CONFIG_FLASH_OFFSET)
#define CONFIG_FLASH_PAGE_SIZE (2u * FLASH_SECTOR_SIZE)
#define CONFIG_FLASH_MAGIC     0x32464642u
#define CONFIG_FLASH_FORMAT    3u

typedef struct __attribute__((packed, aligned(4))) {
  uint32_t magic;
  uint32_t format;
  uint32_t sequence;
  uint8_t data[BEEF_CONFIG_SIZE];
  uint8_t legacy_phoenixwan_enabled;
  uint32_t checksum;
} config_record_t;

_Static_assert((sizeof(config_record_t) % 4u) == 0u, "config record must be word aligned");
_Static_assert(sizeof(config_record_t) <= FLASH_PAGE_SIZE, "config must fit one flash page");

#define CONFIG_SLOT_COUNT (CONFIG_FLASH_PAGE_SIZE / FLASH_PAGE_SIZE)

static uint8_t current_config[BEEF_CONFIG_SIZE];
static uint32_t next_sequence = 1u;
static bool config_dirty;
static bool legacy_phoenixwan_enabled = true;
static uint32_t newest_slot;

static uint32_t checksum_config(uint32_t sequence, uint8_t const *data) {
  uint32_t hash = 2166136261u;

  for (uint32_t i = 0; i < sizeof(sequence); ++i) {
    hash ^= (uint8_t)(sequence >> (i * 8u));
    hash *= 16777619u;
  }

  for (uint32_t i = 0; i < BEEF_CONFIG_SIZE; ++i) {
    hash ^= data[i];
    hash *= 16777619u;
  }

  return hash;
}

// Format 2 did not protect the former padding byte. Keep it readable, but
// include the legacy UART enable flag in all newly written records.
static uint32_t checksum_record(config_record_t const *rec) {
  uint32_t hash = checksum_config(rec->sequence, rec->data);
  if (rec->format >= 3u) {
    hash ^= rec->legacy_phoenixwan_enabled;
    hash *= 16777619u;
  }
  return hash;
}

static void set_defaults(uint8_t *c) {
  memset(c, 0, BEEF_CONFIG_SIZE);

  c[0] = 45;
  c[1] = 0;
  c[2] = 0;
  c[3] = 0;
  c[4] = 8;
  c[5] = 0;

  for (uint32_t off = 6; off <= 27; off += 3) {
    c[off + 0] = 0;
    c[off + 1] = 255;
    c[off + 2] = 255;
  }

  c[30] = 6;
  c[71] = 1000 & 255; c[72] = 1000 >> 8;
  c[31] = 0;
  c[32] = 0;
  c[33] = 0;
  c[34] = 0;
  for (uint32_t i = 0; i < 7; ++i) c[48 + i] = 0;
  for (uint32_t i = 0; i < 7; ++i) c[64 + i] = 0;

  static uint8_t const iidx_keys[13] = {
    0x16, 0x07, 0x09, 0x2c, 0x0d, 0x0e, 0x0f,
    0x1e, 0x1f, 0x20, 0x21, 0x51, 0x52
  };
  memcpy(&c[35], iidx_keys, sizeof(iidx_keys));

  c[55] = 0x07; c[56] = 0x09; c[57] = 0x0d; c[58] = 0x0e;
  c[59] = 0x06; c[60] = 0x10;
  c[63] = 0x28;

  c[75] = 0;
  c[76] = 4;
  c[77] = 0;
  c[78] = 60;
  c[79] = 3;
  c[80] = 24;

  c[81] = 0; c[82] = 255; c[83] = 255;
  c[84] = 0;
  c[61] = 16;
  c[85] = 0;
  c[86] = 0;

  c[87] = 0;
  c[88] = 100;
  c[89] = 0;
  c[90] = 0;

  for (uint32_t i = 0; i < 11; ++i) c[91 + i] = 0;
  for (uint32_t i = 0; i < 11; ++i) c[102 + i] = 100;
  for (uint32_t i = 0; i < 11; ++i) c[113 + i] = 0;

  c[124] = 0;
  c[125] = 0; c[126] = 0;

  for (uint32_t i = 0; i < 11; ++i) c[127 + i] = (uint8_t)i;
  c[138] = 16;
}

static config_record_t const *slot_ptr(uint32_t slot) {
  return (config_record_t const *)(CONFIG_FLASH_ADDRESS + slot * FLASH_PAGE_SIZE);
}

static bool slot_is_erased(uint32_t slot) {
  const uint8_t *page = (const uint8_t *)slot_ptr(slot);
  for (unsigned i=0;i<FLASH_PAGE_SIZE;++i) if(page[i]!=0xff) return false;
  return true;
}

static bool record_valid(config_record_t const *rec) {
  if (rec->magic != CONFIG_FLASH_MAGIC || (rec->format != 2u && rec->format != CONFIG_FLASH_FORMAT)) return false;
  if (rec->legacy_phoenixwan_enabled != 0xffu && rec->legacy_phoenixwan_enabled > 1u) return false;
  uint8_t validated[BEEF_CONFIG_SIZE];
  memcpy(validated, rec->data, sizeof(validated));
  // Read previous count-range records without losing unrelated settings.
  if (validated[0] >= 43 && validated[3] > 50) validated[3] = 50;
  if (!beef_config_valid(validated)) return false;
  return rec->checksum == checksum_record(rec);
}

static uint8_t flash_page[FLASH_PAGE_SIZE];
static uint32_t write_offset;
static bool write_erase;
static void flash_write_callback(void *arg) {
  (void)arg;
  if (write_erase) flash_range_erase(CONFIG_FLASH_OFFSET + (write_offset / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
  flash_range_program(CONFIG_FLASH_OFFSET + write_offset, flash_page, FLASH_PAGE_SIZE);
}
static bool program_record(uint32_t slot, config_record_t const *rec) {
  uint32_t page_base = slot * FLASH_PAGE_SIZE;
  memset(flash_page, 0xff, sizeof(flash_page));
  memcpy(flash_page, rec, sizeof(*rec));
  write_offset = page_base;
  int result = flash_safe_execute(flash_write_callback, NULL, 1000);
  return result == PICO_OK && record_valid(slot_ptr(slot));
}

void beef_config_store_init(void) {
  bool found = false;
  bool migrate = false;
  uint32_t best_sequence = 0;
  newest_slot = 0;
  legacy_phoenixwan_enabled = true;

  for (uint32_t slot = 0; slot < CONFIG_SLOT_COUNT; ++slot) {
    config_record_t const *rec = slot_ptr(slot);
    if (!record_valid(rec)) continue;

    if (!found || (int32_t)(rec->sequence - best_sequence) > 0) {
      memcpy(current_config, rec->data, BEEF_CONFIG_SIZE);
      legacy_phoenixwan_enabled = rec->legacy_phoenixwan_enabled == 0xffu ||
                                  rec->legacy_phoenixwan_enabled != 0u;
      best_sequence = rec->sequence;
      newest_slot = slot;
      migrate = rec->format != CONFIG_FLASH_FORMAT;
      found = true;
    }
  }

  if (found) {
    next_sequence = best_sequence + 1u;
    if (current_config[0] == 33) {
      uint8_t const legacy_window = current_config[75] > 50 ? 50 : current_config[75];
      for (uint32_t i = 0; i < 7; ++i) current_config[48 + i] = legacy_window;
      current_config[0] = 34;
      migrate = true;
    }
    if (current_config[0] == 34) {
      current_config[0] = 35;
      migrate = true;
    }
    if (current_config[0] == 35) {
      for (uint32_t i = 0; i < 7; ++i) current_config[64 + i] = current_config[48 + i];
      current_config[0] = 36;
      migrate = true;
    }
    if (current_config[0] == 36) {
      for (uint32_t i = 0; i < 7; ++i) {
        current_config[48 + i] = (uint8_t)(10u * (current_config[48 + i] > 16 ? 16u : current_config[48 + i]));
        current_config[64 + i] = (uint8_t)(10u * (current_config[64 + i] > 16 ? 16u : current_config[64 + i]));
      }
      current_config[0] = 37;
      migrate = true;
    }
    if (current_config[0] == 37) {
      current_config[0] = 38;
      migrate = true;
    }
    if (current_config[0] == 38) {
      current_config[0] = 39;
      migrate = true;
    }
    if (current_config[0] == 39) {
      current_config[0] = 40;
      migrate = true;
    }
    if (current_config[0] == 40) {
      current_config[0] = 41;
      migrate = true;
    }
    if (current_config[0] == 41) {
      current_config[61] = 16;
      current_config[0] = 42;
      migrate = true;
    }
    if (current_config[0] == 42) {
      current_config[3] = 0;
      current_config[0] = 43;
      migrate = true;
    }
    if (current_config[0] >= 43 && current_config[3] > 50) {
      current_config[3] = 50;
      migrate = true;
    }
    if (current_config[0] == 43) {
      current_config[30] = beef_migrate_sensitivity(current_config[30]);
      current_config[0] = 44;
      migrate = true;
    }
    if (current_config[0] == 44) { uint16_t milli = beef_sensitivity_milli(current_config[30]); current_config[71] = milli; current_config[72] = milli >> 8; current_config[0] = 45; migrate = true; }
  } else {
    set_defaults(current_config);
    next_sequence = 1u;
  }

  config_dirty = false;
  if (migrate) {
    config_dirty = true;
    (void)beef_config_store_flush();
  }
}

uint8_t const *beef_config_store_data(void) {
  return current_config;
}

bool beef_config_store_legacy_phoenixwan_enabled(void) {
  return legacy_phoenixwan_enabled;
}

bool beef_config_store_set_legacy_phoenixwan_enabled(bool enabled) {
  if (legacy_phoenixwan_enabled == enabled) return false;
  legacy_phoenixwan_enabled = enabled;
  config_dirty = true;
  return true;
}

bool beef_config_store_stage(uint8_t const data[BEEF_CONFIG_SIZE]) {
  uint8_t migrated[BEEF_CONFIG_SIZE];
  if (data[0] == 33) {
    memcpy(migrated, data, sizeof(migrated));
    migrated[0] = 34;
    uint8_t const legacy_window = migrated[75] > 50 ? 50 : migrated[75];
    for (uint32_t i = 0; i < 7; ++i) migrated[48 + i] = legacy_window;
    data = migrated;
  }
  if (data[0] == 34) {
    memcpy(migrated, data, sizeof(migrated));
    migrated[0] = 35;
    data = migrated;
  }
  if (data[0] == 35) {
    memcpy(migrated, data, sizeof(migrated));
    for (uint32_t i = 0; i < 7; ++i) migrated[64 + i] = migrated[48 + i];
    migrated[0] = 36;
    data = migrated;
  }
  if (data[0] == 36) {
    if (data != migrated) memcpy(migrated, data, sizeof(migrated));
    for (uint32_t i = 0; i < 7; ++i) {
      migrated[48 + i] = (uint8_t)(10u * (migrated[48 + i] > 16 ? 16u : migrated[48 + i]));
      migrated[64 + i] = (uint8_t)(10u * (migrated[64 + i] > 16 ? 16u : migrated[64 + i]));
    }
    migrated[0] = 37;
    data = migrated;
  }
  if (data[0] == 37) {
    if (data != migrated) memcpy(migrated, data, sizeof(migrated));
    migrated[0] = 38;
    data = migrated;
  }
  if (data[0] == 38) {
    if (data != migrated) memcpy(migrated, data, sizeof(migrated));
    migrated[0] = 39;
    data = migrated;
  }
  if (data[0] == 39) {
    if (data != migrated) memcpy(migrated, data, sizeof(migrated));
    migrated[0] = 40;
    data = migrated;
  }
  if (data[0] == 40) {
    if (data != migrated) memcpy(migrated, data, sizeof(migrated));
    migrated[0] = 41;
    data = migrated;
  }
  if (data[0] == 41) {
    if (data != migrated) memcpy(migrated, data, sizeof(migrated));
    migrated[61] = 16;
    migrated[0] = 42;
    data = migrated;
  }
  if (data[0] == 42) {
    if (data != migrated) memcpy(migrated, data, sizeof(migrated));
    migrated[3] = 0;
    migrated[0] = 43;
    data = migrated;
  }
  if (data != migrated) memcpy(migrated, data, sizeof(migrated));
  if (migrated[0] == 43) { migrated[30] = beef_migrate_sensitivity(migrated[30]); migrated[0] = 44; }
  if (migrated[0] == 44) { uint16_t milli = beef_sensitivity_milli(migrated[30]); migrated[71] = milli; migrated[72] = milli >> 8; migrated[0] = 45; }
  data = migrated;
  if (data[0] != 45 || !beef_config_valid(data)) return false;
  if (memcmp(current_config, data, BEEF_CONFIG_SIZE) == 0) return false;

  memcpy(current_config, data, BEEF_CONFIG_SIZE);
  config_dirty = true;
  return true;
}

bool beef_config_store_is_dirty(void) {
  return config_dirty;
}

bool beef_config_store_flush(void) {
  if (!config_dirty) return true;

  config_record_t rec;
  memset(&rec, 0xff, sizeof(rec));
  rec.magic = CONFIG_FLASH_MAGIC;
  rec.format = CONFIG_FLASH_FORMAT;
  rec.sequence = next_sequence;
  memcpy(rec.data, current_config, BEEF_CONFIG_SIZE);
  rec.legacy_phoenixwan_enabled = legacy_phoenixwan_enabled ? 1u : 0u;
  rec.checksum = checksum_record(&rec);

  uint32_t target_slot = CONFIG_SLOT_COUNT;
  for (uint32_t slot = 0; slot < CONFIG_SLOT_COUNT; ++slot) {
    if (slot_is_erased(slot)) {
      target_slot = slot;
      break;
    }
  }

  write_erase = target_slot == CONFIG_SLOT_COUNT;
  if (write_erase) {
    // Never erase the sector containing the newest committed record. A power
    // loss during erase/program leaves the other sector available at boot.
    uint32_t slots_per_sector = FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE;
    target_slot = newest_slot < slots_per_sector ? slots_per_sector : 0;
  }
  bool ok = program_record(target_slot, &rec);

  if (ok) {
    newest_slot = target_slot;
    ++next_sequence;
    config_dirty = false;
  }

  return ok;
}

bool beef_config_store_write(uint8_t const data[BEEF_CONFIG_SIZE]) {
  if (!beef_config_valid(data)) return false;
  (void)beef_config_store_stage(data);
  return beef_config_store_flush();
}

bool beef_config_store_reset(void) {
  uint8_t defaults[BEEF_CONFIG_SIZE];
  set_defaults(defaults);
  legacy_phoenixwan_enabled = true;
  config_dirty = true;
  return beef_config_store_write(defaults);
}
