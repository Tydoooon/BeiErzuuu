#ifndef __BSP_TFCARD_H
#define __BSP_TFCARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TFCARD_BLOCK_SIZE 512U

typedef enum
{
    BSP_TFCARD_OK = 0,
    BSP_TFCARD_ERROR,
    BSP_TFCARD_TIMEOUT,
    BSP_TFCARD_INVALID_PARAM,
    BSP_TFCARD_NOT_FOUND,
    BSP_TFCARD_UNSUPPORTED
} bsp_tfcard_status_t;

typedef enum
{
    BSP_TFCARD_TYPE_UNKNOWN = 0,
    BSP_TFCARD_TYPE_SDSC,
    BSP_TFCARD_TYPE_SDHC_SDXC
} bsp_tfcard_type_t;

bsp_tfcard_status_t BSP_TFCARD_Init(void);
bsp_tfcard_status_t BSP_TFCARD_ReadBlock(uint32_t block, uint8_t data[TFCARD_BLOCK_SIZE]);
bsp_tfcard_status_t BSP_TFCARD_ReadBlocks(uint32_t start_block, uint8_t *data, uint32_t block_count);
bsp_tfcard_status_t BSP_TFCARD_ReadCSD(uint8_t csd[16]);
bsp_tfcard_status_t BSP_TFCARD_ReadCID(uint8_t cid[16]);
bsp_tfcard_type_t BSP_TFCARD_GetType(void);
bool BSP_TFCARD_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif
