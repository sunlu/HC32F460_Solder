/**
 * @file    storage.h
 * @brief   Flash storage interface for settings persistence
 *
 * Uses internal Flash sector at EEPROM_BASE_ADDR for settings storage.
 * Simple append-based scheme with magic number validation.
 */

#ifndef SOURCE_STORAGE_H
#define SOURCE_STORAGE_H

#include "hc32_ll.h"
#include "main.h"  

void storage_init(void);
void storage_save(void);
uint8_t storage_load(void);
void storage_reset_defaults(void);

#endif /* SOURCE_STORAGE_H */
