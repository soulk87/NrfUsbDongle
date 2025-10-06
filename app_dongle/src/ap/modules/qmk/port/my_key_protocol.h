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

void RfKeysReadBuf(uint8_t *buf, uint32_t len);

bool RfMotionRead(int32_t *x, int32_t *y);
#endif

#endif /* MY_KEY_PROTOCOL_H_ */
