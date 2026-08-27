#ifndef __BSP_W25Q128_H
#define __BSP_W25Q128_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define W25Q128_PAGE_SIZE          256U
#define W25Q128_SECTOR_SIZE        4096U
#define W25Q128_BLOCK_SIZE         65536U
#define W25Q128_SIZE_BYTES         (16U * 1024U * 1024U)
#define W25Q128_SECTOR_COUNT       (W25Q128_SIZE_BYTES / W25Q128_SECTOR_SIZE)
#define W25Q128_JEDEC_CAPACITY_ID  0x18U

#ifndef W25Q128_CS_GPIO_Port
#define W25Q128_CS_GPIO_Port GPIOC
#endif

#ifndef W25Q128_CS_Pin
#define W25Q128_CS_Pin GPIO_PIN_4
#endif

typedef enum
{
    BSP_W25Q128_OK = 0,
    BSP_W25Q128_ERROR,
    BSP_W25Q128_TIMEOUT,
    BSP_W25Q128_INVALID_PARAM,
    BSP_W25Q128_NOT_FOUND
} bsp_w25q128_status_t;

bsp_w25q128_status_t BSP_W25Q128_Init(void);
bsp_w25q128_status_t BSP_W25Q128_ReadJedecID(uint32_t *jedec_id);
bsp_w25q128_status_t BSP_W25Q128_ReadStatus1(uint8_t *status);
bsp_w25q128_status_t BSP_W25Q128_ReadStatus2(uint8_t *status);
bool BSP_W25Q128_IsBusy(void);
bsp_w25q128_status_t BSP_W25Q128_WaitReady(uint32_t timeout_ms);

bsp_w25q128_status_t BSP_W25Q128_Read(uint32_t addr, uint8_t *data, size_t len);
bsp_w25q128_status_t BSP_W25Q128_PageProgram(uint32_t addr, const uint8_t *data, size_t len);
bsp_w25q128_status_t BSP_W25Q128_Write(uint32_t addr, const uint8_t *data, size_t len);
bsp_w25q128_status_t BSP_W25Q128_EraseSector(uint32_t addr);
bsp_w25q128_status_t BSP_W25Q128_EraseBlock64K(uint32_t addr);
bsp_w25q128_status_t BSP_W25Q128_EraseChip(void);

bsp_w25q128_status_t BSP_W25Q128_PowerDown(void);
bsp_w25q128_status_t BSP_W25Q128_ReleasePowerDown(void);

#ifdef __cplusplus
}
#endif

#endif
