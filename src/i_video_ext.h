#ifndef I_VIDEO_EXT_H
#define I_VIDEO_EXT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t* I_GetRGBABuffer(void);
size_t I_GetRGBABufferSize(void);

#ifdef __cplusplus
}
#endif

#endif
