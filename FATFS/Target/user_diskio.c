/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * @file    user_diskio.c
  * @brief   This file includes a diskio driver skeleton to be completed by the user.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
 /* USER CODE END Header */

#ifdef USE_OBSOLETE_USER_CODE_SECTION_0
/*
 * Warning: the user section 0 is no more in use (starting from CubeMx version 4.16.0)
 * To be suppressed in the future.
 * Kept to ensure backward compatibility with previous CubeMx versions when
 * migrating projects.
 * User code previously added there should be copied in the new user sections before
 * the section contents can be deleted.
 */
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */
#endif

/* USER CODE BEGIN DECL */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "bsp_tfcard.h"
#include "ff_gen_drv.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define USER_TFCARD_LUN             0U
#define USER_TFCARD_SECTOR_SIZE     TFCARD_BLOCK_SIZE
#define USER_TFCARD_BLOCK_SIZE      1U

#define USER_CT_SD1                 0x02U
#define USER_CT_SD2                 0x04U
#define USER_CT_BLOCK               0x08U

/* Private variables ---------------------------------------------------------*/
/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;

static DWORD USER_get_sector_count(void);

/* USER CODE END DECL */

/* Private function prototypes -----------------------------------------------*/
DSTATUS USER_initialize (BYTE pdrv);
DSTATUS USER_status (BYTE pdrv);
DRESULT USER_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
#if _USE_WRITE == 1
  DRESULT USER_write (BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
#endif /* _USE_WRITE == 1 */
#if _USE_IOCTL == 1
  DRESULT USER_ioctl (BYTE pdrv, BYTE cmd, void *buff);
#endif /* _USE_IOCTL == 1 */

Diskio_drvTypeDef  USER_Driver =
{
  USER_initialize,
  USER_status,
  USER_read,
#if  _USE_WRITE
  USER_write,
#endif  /* _USE_WRITE == 1 */
#if  _USE_IOCTL == 1
  USER_ioctl,
#endif /* _USE_IOCTL == 1 */
};

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes a Drive
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_initialize (
	BYTE pdrv           /* Physical drive nmuber to identify the drive */
)
{
  /* USER CODE BEGIN INIT */
    if (pdrv != USER_TFCARD_LUN) {
      Stat = STA_NOINIT;
      return Stat;
    }

    if (BSP_TFCARD_Init() == BSP_TFCARD_OK) {
      Stat = 0;
    } else {
      Stat = STA_NOINIT | STA_NODISK;
    }

    return Stat;
  /* USER CODE END INIT */
}

/**
  * @brief  Gets Disk Status
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_status (
	BYTE pdrv       /* Physical drive number to identify the drive */
)
{
  /* USER CODE BEGIN STATUS */
    if (pdrv != USER_TFCARD_LUN) {
      Stat = STA_NOINIT;
      return Stat;
    }

    Stat = BSP_TFCARD_IsReady() ? 0 : (STA_NOINIT | STA_NODISK);
    return Stat;
  /* USER CODE END STATUS */
}

/**
  * @brief  Reads Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT USER_read (
	BYTE pdrv,      /* Physical drive nmuber to identify the drive */
	BYTE *buff,     /* Data buffer to store read data */
	DWORD sector,   /* Sector address in LBA */
	UINT count      /* Number of sectors to read */
)
{
  /* USER CODE BEGIN READ */
    if (pdrv != USER_TFCARD_LUN || buff == NULL || count == 0U) {
      return RES_PARERR;
    }

    if ((Stat & STA_NOINIT) != 0U || !BSP_TFCARD_IsReady()) {
      return RES_NOTRDY;
    }

    if (BSP_TFCARD_ReadBlocks((uint32_t)sector, buff, (uint32_t)count) == BSP_TFCARD_OK) {
      return RES_OK;
    }

    return RES_ERROR;
  /* USER CODE END READ */
}

/**
  * @brief  Writes Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
DRESULT USER_write (
	BYTE pdrv,          /* Physical drive nmuber to identify the drive */
	const BYTE *buff,   /* Data to be written */
	DWORD sector,       /* Sector address in LBA */
	UINT count          /* Number of sectors to write */
)
{
  /* USER CODE BEGIN WRITE */
    (void)pdrv;
    (void)buff;
    (void)sector;
    (void)count;
    return RES_WRPRT;
  /* USER CODE END WRITE */
}
#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O control operation
  * @param  pdrv: Physical drive number (0..)
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT USER_ioctl (
	BYTE pdrv,      /* Physical drive nmuber (0..) */
	BYTE cmd,       /* Control code */
	void *buff      /* Buffer to send/receive control data */
)
{
  /* USER CODE BEGIN IOCTL */
    DRESULT res = RES_ERROR;

    if (pdrv != USER_TFCARD_LUN || buff == NULL) {
      return RES_PARERR;
    }

    if ((Stat & STA_NOINIT) != 0U || !BSP_TFCARD_IsReady()) {
      return RES_NOTRDY;
    }

    switch (cmd) {
    case CTRL_SYNC:
      res = RES_OK;
      break;

    case GET_SECTOR_COUNT:
      *(DWORD *)buff = USER_get_sector_count();
      res = (*(DWORD *)buff != 0U) ? RES_OK : RES_ERROR;
      break;

    case GET_SECTOR_SIZE:
      *(WORD *)buff = USER_TFCARD_SECTOR_SIZE;
      res = RES_OK;
      break;

    case GET_BLOCK_SIZE:
      *(DWORD *)buff = USER_TFCARD_BLOCK_SIZE;
      res = RES_OK;
      break;

    case MMC_GET_TYPE:
      if (BSP_TFCARD_GetType() == BSP_TFCARD_TYPE_SDHC_SDXC) {
        *(BYTE *)buff = USER_CT_SD2 | USER_CT_BLOCK;
      } else {
        *(BYTE *)buff = USER_CT_SD1;
      }
      res = RES_OK;
      break;

    case MMC_GET_CSD:
      res = (BSP_TFCARD_ReadCSD((uint8_t *)buff) == BSP_TFCARD_OK) ? RES_OK : RES_ERROR;
      break;

    case MMC_GET_CID:
      res = (BSP_TFCARD_ReadCID((uint8_t *)buff) == BSP_TFCARD_OK) ? RES_OK : RES_ERROR;
      break;

    default:
      res = RES_PARERR;
      break;
    }

    return res;
  /* USER CODE END IOCTL */
}
#endif /* _USE_IOCTL == 1 */

/* USER CODE BEGIN USER_FUNCTIONS */
static DWORD USER_get_sector_count(void)
{
  uint8_t csd[16];
  uint8_t csd_version;

  if (BSP_TFCARD_ReadCSD(csd) != BSP_TFCARD_OK) {
    return 0U;
  }

  csd_version = (uint8_t)((csd[0] >> 6) & 0x03U);
  if (csd_version == 1U) {
    uint32_t c_size = ((uint32_t)(csd[7] & 0x3FU) << 16) |
                      ((uint32_t)csd[8] << 8) |
                      csd[9];
    return (DWORD)((c_size + 1U) * 1024U);
  }

  if (csd_version == 0U) {
    uint32_t read_bl_len = csd[5] & 0x0FU;
    uint32_t c_size = ((uint32_t)(csd[6] & 0x03U) << 10) |
                      ((uint32_t)csd[7] << 2) |
                      ((uint32_t)(csd[8] & 0xC0U) >> 6);
    uint32_t c_size_mult = ((uint32_t)(csd[9] & 0x03U) << 1) |
                           ((uint32_t)(csd[10] & 0x80U) >> 7);
    uint32_t block_count = (c_size + 1U) << (c_size_mult + 2U);
    uint32_t block_len = 1UL << read_bl_len;

    return (DWORD)((block_count * block_len) / USER_TFCARD_SECTOR_SIZE);
  }

  return 0U;
}
/* USER CODE END USER_FUNCTIONS */

