#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "beef_protocol.h"

void beef_config_store_init(void);
void beef_config_store_reset(void);

bool beef_config_store_stage(uint8_t const data[BEEF_CONFIG_SIZE]);
bool beef_config_store_flush(void);
bool beef_config_store_is_dirty(void);

bool beef_config_store_write(uint8_t const data[BEEF_CONFIG_SIZE]);
uint8_t const *beef_config_store_data(void);
