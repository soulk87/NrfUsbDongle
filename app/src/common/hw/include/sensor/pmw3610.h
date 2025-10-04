#ifndef PMW3610_H_
#define PMW3610_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "hw_def.h"


#ifdef _USE_HW_PMW3610

bool pmw3610_init(void);
bool pmw3610_motion_read(int32_t* x_out, int32_t* y_out);

#endif //_USE_HW_PMW3610


#ifdef __cplusplus
 }
#endif

#endif