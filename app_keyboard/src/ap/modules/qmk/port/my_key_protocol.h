#ifndef MY_KEY_PROTOCOL_H_
#define MY_KEY_PROTOCOL_H_

#include <stdint.h>
#include <stdbool.h>
#include QMK_KEYMAP_CONFIG_H

#ifdef RF_DONGLE_MODE_ENABLE
// Initialize the key protocol
bool key_protocol_init(void);

// Update the key protocol (call this in the main loop)
void key_protocol_update(void);

// RX related functions
void RfKeysReadBuf(uint8_t *buf, uint32_t len);
bool RfMotionRead(int32_t *x, int32_t *y);

// TX related functions
bool key_protocol_send_key_data(uint8_t device_id, uint8_t *key_matrix, uint8_t column_count);
bool key_protocol_send_trackball_data(uint8_t device_id, int16_t x, int16_t y);
bool key_protocol_send_system_data(uint8_t device_id, uint8_t *system_data, uint8_t length);
bool key_protocol_send_battery_data(uint8_t device_id, uint8_t battery_level);

#endif

#endif /* MY_KEY_PROTOCOL_H_ */
