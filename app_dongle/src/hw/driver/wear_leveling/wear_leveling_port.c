// Copyright 2022 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdbool.h>
#include "wear_leveling.h"
#include "wear_leveling_config.h"
#include "wear_leveling_internal.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <stdio.h>

#define KEY_PARTITION_OFFSET	FIXED_PARTITION_OFFSET(keymap_partition)
#define KEY_PARTITION_DEVICE	FIXED_PARTITION_DEVICE(keymap_partition)
#define FLASH_PAGE_SIZE   4096

const struct device *flash_dev = KEY_PARTITION_DEVICE;

bool backing_store_init(void) {
    bs_dprintf("Init\n");

    
	if (!device_is_ready(flash_dev)) {
		bs_dprintf("Internal storage device not ready\n");
		return false;
	}
    
    return true;
}

bool backing_store_unlock(void) {
    bs_dprintf("Unlock\n");
    // FLASH_Unlock();
    return true;
}

bool backing_store_erase(void) {
#ifdef WEAR_LEVELING_DEBUG_OUTPUT
    uint32_t start = timer_read32();
#endif
  

    bool         ret = true;

    flash_erase(flash_dev, KEY_PARTITION_OFFSET, FLASH_PAGE_SIZE*2);
    // FLASH_Status status;
    // for (int i = 0; i < (WEAR_LEVELING_LEGACY_EMULATION_PAGE_COUNT); ++i) {
    //     status = FLASH_ErasePage(WEAR_LEVELING_LEGACY_EMULATION_BASE_PAGE_ADDRESS + (i * (WEAR_LEVELING_LEGACY_EMULATION_PAGE_SIZE)));
    //     if (status != FLASH_COMPLETE) {
    //         ret = false;
    //     }
    // }

    // bs_dprintf("Backing store erase took %ldms to complete\n", ((long)(timer_read32() - start)));
    return ret;
}

bool backing_store_write(uint32_t address, backing_store_int_t value) {
    // uint32_t offset = ((WEAR_LEVELING_LEGACY_EMULATION_BASE_PAGE_ADDRESS) + address);
    // bs_dprintf("Write ");
    // wl_dump(offset, &value, sizeof(backing_store_int_t));
    // return FLASH_ProgramHalfWord(offset, ~value) == FLASH_COMPLETE;
    static uint32_t buffer;

    buffer = ~value;

    if(flash_write(flash_dev, KEY_PARTITION_OFFSET + address, &buffer, sizeof(backing_store_int_t)) != 0) {
        printf("   Write failed!\n");
        return false;
    }
    volatile static uint32_t test_counter = 0;
    test_counter++;
    return true;
}

bool backing_store_lock(void) {
    bs_dprintf("Lock  \n");
    // FLASH_Lock();
    return true;
}

bool backing_store_read(uint32_t address, backing_store_int_t* value) {
    // uint32_t             offset = ((WEAR_LEVELING_LEGACY_EMULATION_BASE_PAGE_ADDRESS) + address);
    // backing_store_int_t* loc    = (backing_store_int_t*)offset;
    // *value                      = ~(*loc);
    // bs_dprintf("Read  ");
    // wl_dump(offset, loc, sizeof(backing_store_int_t));
    static uint32_t buf_word;

    if (flash_read(flash_dev, KEY_PARTITION_OFFSET + address, &buf_word,
			sizeof(uint32_t)) != 0) {
		printf("   Read failed!\n");
		return false;
	}

    *value = ~buf_word;

    return true;
}
