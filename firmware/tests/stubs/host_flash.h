#pragma once
#include <stdint.h>
#include <stddef.h>
#define PICO_FLASH_SIZE_BYTES 16384u
#define FLASH_SECTOR_SIZE 4096u
#define FLASH_PAGE_SIZE 256u
#define PICO_OK 0
extern uint8_t fake_flash[PICO_FLASH_SIZE_BYTES];
#define XIP_BASE ((uintptr_t)fake_flash)
void flash_range_erase(uint32_t offset,size_t count);
void flash_range_program(uint32_t offset,const uint8_t *data,size_t count);
int flash_safe_execute(void (*callback)(void *),void *arg,uint32_t timeout);
