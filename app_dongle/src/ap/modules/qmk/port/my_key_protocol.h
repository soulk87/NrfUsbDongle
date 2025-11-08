#ifndef MY_KEY_PROTOCOL_H_
#define MY_KEY_PROTOCOL_H_

#include <stdint.h>
#include <stdbool.h>
#include QMK_KEYMAP_CONFIG_H

#define DEVICE_ID_LEFT 0x01u
#define DEVICE_ID_RIGHT 0x02u

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
bool key_protocol_send_heartbeat(uint8_t device_id, uint8_t status_flag, uint8_t battery_level);

bool key_protocol_is_connected(uint8_t device_id);
uint8_t key_protocol_get_battery_level(uint8_t device_id);
uint8_t key_protocol_get_status_flag(uint8_t device_id);
uint32_t key_protocol_get_last_heartbeat_elapsed(uint8_t device_id);

// Get all device states at once (thread-safe)
void key_protocol_get_all_states(bool *left_connected, uint8_t *left_battery,
                                  bool *right_connected, uint8_t *right_battery);

#endif /* MY_KEY_PROTOCOL_H_ */
