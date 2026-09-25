#include "beef_config_store.h"

#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "pico/flash.h"

#define CONFIG_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define CONFIG_FLASH_ADDRESS (XIP_BASE + CONFIG_FLASH_OFFSET)
#define CONFIG_FLASH_PAGE_SIZE FLASH_SECTOR_SIZE
#define CONFIG_FLASH_MAGIC     0x32464642u
#define CONFIG_FLASH_FORMAT    2u

typedef struct __attribute__((packed, aligned(4))) {
  uint32_t magic;
  uint32_t format;
  uint32_t sequence;
  uint8_t data[BEEF_CONFIG_SIZE];
  uint8_t padding;
  uint32_t checksum;
} config_record_t;

_Static_assert((sizeof(config_record_t) % 4u) == 0u, "config record must be word aligned");

#define CONFIG_SLOT_COUNT (CONFIG_FLASH_PAGE_SIZE / FLASH_PAGE_SIZE)

static uint8_t current_config[BEEF_CONFIG_SIZE];
static uint32_t next_sequence = 1u;
static bool config_dirty;

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

static void set_defaults(uint8_t *c) {
  memset(c, 0, BEEF_CONFIG_SIZE);

  c[0] = 33;
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

  c[30] = 10;
  c[31] = 0;
  c[32] = 0;
  c[33] = 0;
  c[34] = 0;

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
  c[79] = 1;
  c[80] = 24;

  c[81] = 0; c[82] = 255; c[83] = 255;
  c[84] = 0;
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
  config_record_t const *rec = slot_ptr(slot);
  return rec->magic == 0xffffffffu && rec->format == 0xffffffffu;
}

static bool record_valid(config_record_t const *rec) {
  if (rec->magic != CONFIG_FLASH_MAGIC || rec->format != CONFIG_FLASH_FORMAT) return false;
  if (rec->data[0] < 11 || rec->data[0] > 33) return false;
  return rec->checksum == checksum_config(rec->sequence, rec->data);
}

static uint8_t flash_page[FLASH_PAGE_SIZE];
static uint32_t write_offset;
static bool write_erase;
static void flash_write_callback(void *arg) {
  (void)arg;
  if (write_erase) flash_range_erase(CONFIG_FLASH_OFFSET, FLASH_SECTOR_SIZE);
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
  uint32_t best_sequence = 0;

  for (uint32_t slot = 0; slot < CONFIG_SLOT_COUNT; ++slot) {
    config_record_t const *rec = slot_ptr(slot);
    if (!record_valid(rec)) continue;

    if (!found || rec->sequence > best_sequence) {
      memcpy(current_config, rec->data, BEEF_CONFIG_SIZE);
      best_sequence = rec->sequence;
      found = true;
    }
  }

  if (found) {
    next_sequence = best_sequence + 1u;
  } else {
    set_defaults(current_config);
    next_sequence = 1u;
  }

  config_dirty = false;
}

uint8_t const *beef_config_store_data(void) {
  return current_config;
}

bool beef_config_store_stage(uint8_t const data[BEEF_CONFIG_SIZE]) {
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
  rec.padding = 0xff;
  rec.checksum = checksum_config(rec.sequence, rec.data);

  uint32_t target_slot = CONFIG_SLOT_COUNT;
  for (uint32_t slot = 0; slot < CONFIG_SLOT_COUNT; ++slot) {
    if (slot_is_erased(slot)) {
      target_slot = slot;
      break;
    }
  }

  write_erase = target_slot == CONFIG_SLOT_COUNT;
  if (write_erase) target_slot = 0;
  bool ok = program_record(target_slot, &rec);

  if (ok) {
    ++next_sequence;
    config_dirty = false;
  }

  return ok;
}

bool beef_config_store_write(uint8_t const data[BEEF_CONFIG_SIZE]) {
  (void)beef_config_store_stage(data);
  return beef_config_store_flush();
}

void beef_config_store_reset(void) {
  uint8_t defaults[BEEF_CONFIG_SIZE];
  set_defaults(defaults);
  (void)beef_config_store_write(defaults);
}
