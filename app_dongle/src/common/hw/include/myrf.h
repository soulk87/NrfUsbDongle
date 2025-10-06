#ifndef SRC_COMMON_HW_INCLUDE_MYRF_H_
#define SRC_COMMON_HW_INCLUDE_MYRF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"


#ifdef _USE_HW_RF


bool rfInit(void);
uint32_t rfAvailable(void);
uint32_t rfWrite(uint8_t *p_data, uint32_t length);
uint32_t rfRead(uint8_t *p_data, uint32_t length);


/*
bool rfSetTxPower(int8_t power);
int8_t rfGetTxPower(void);
bool rfSetChannel(uint8_t channel);
uint8_t rfGetChannel(void);
bool rfSetAddress(uint8_t *p_address, uint8_t length);
bool rfGetAddress(uint8_t *p_address, uint8_t length);
void rfSleep(void);
void rfWakeUp(void);
*/

#endif

#ifdef __cplusplus
}
#endif

#endif /* SRC_COMMON_HW_INCLUDE_RF_H_ */
