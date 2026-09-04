/**
  ******************************************************************************
  * @file    usbh_hid_mouse.c
  * @author  MCD Application Team
  * @brief   This file is the application layer for USB Host HID Mouse Handling.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2015 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* BSPDependencies
- "stm32xxxxx_{eval}{discovery}{nucleo_144}.c"
- "stm32xxxxx_{eval}{discovery}_io.c"
- "stm32xxxxx_{eval}{discovery}{adafruit}_lcd.c"
- "stm32xxxxx_{eval}{discovery}_sdram.c"
EndBSPDependencies */

/* Includes ------------------------------------------------------------------*/
#include "usbh_hid_mouse.h"
#include "usbh_hid_parser.h"


/** @addtogroup USBH_LIB
  * @{
  */

/** @addtogroup USBH_CLASS
  * @{
  */

/** @addtogroup USBH_HID_CLASS
  * @{
  */

/** @defgroup USBH_HID_MOUSE
  * @brief    This file includes HID Layer Handlers for USB Host HID class.
  * @{
  */

/** @defgroup USBH_HID_MOUSE_Private_TypesDefinitions
  * @{
  */
typedef struct
{
  uint16_t bit_offset;
  uint8_t size;
  uint8_t sign;
  uint8_t report_id;
  uint8_t valid;
} HID_MOUSE_FieldTypeDef;

typedef struct
{
  HID_MOUSE_FieldTypeDef x;
  HID_MOUSE_FieldTypeDef y;
  HID_MOUSE_FieldTypeDef wheel;
  HID_MOUSE_FieldTypeDef button1;
  uint8_t has_report_id;
  uint8_t valid;
} HID_MOUSE_ReportLayoutTypeDef;

/**
  * @}
  */


/** @defgroup USBH_HID_MOUSE_Private_Defines
  * @{
  */
#define HID_MOUSE_USAGE_PAGE_GENERIC_DESKTOP 0x01U
#define HID_MOUSE_USAGE_PAGE_BUTTON          0x09U
#define HID_MOUSE_USAGE_X                    0x30U
#define HID_MOUSE_USAGE_Y                    0x31U
#define HID_MOUSE_USAGE_WHEEL                0x38U
#define HID_MOUSE_USAGE_BUTTON_1             0x01U
#define HID_MOUSE_INPUT_CONSTANT             0x01U

/**
  * @}
  */


/** @defgroup USBH_HID_MOUSE_Private_Macros
  * @{
  */
/**
  * @}
  */

/** @defgroup USBH_HID_MOUSE_Private_FunctionPrototypes
  * @{
  */
static USBH_StatusTypeDef USBH_HID_MouseDecode(USBH_HandleTypeDef *phost);
static void USBH_HID_MouseParseReportDesc(uint8_t *desc, uint16_t len);
static int32_t USBH_HID_MouseItemValue(const uint8_t *data, uint8_t size);
static uint32_t USBH_HID_MouseItemUnsigned(const uint8_t *data, uint8_t size);
static void USBH_HID_MouseClearLocal(uint32_t *usages, uint8_t *usage_count,
                                     uint32_t *usage_min, uint32_t *usage_max);
static void USBH_HID_MouseSetField(HID_MOUSE_FieldTypeDef *field, uint16_t bit_offset,
                                   uint8_t size, uint8_t sign, uint8_t report_id);
static uint8_t USBH_HID_MouseFieldFits(const HID_MOUSE_FieldTypeDef *field,
                                       uint16_t report_len);
static uint32_t USBH_HID_MouseReadUnsignedField(const uint8_t *data,
                                                const HID_MOUSE_FieldTypeDef *field);
static int16_t USBH_HID_MouseReadSignedField(const uint8_t *data,
                                             const HID_MOUSE_FieldTypeDef *field);

/**
  * @}
  */


/** @defgroup USBH_HID_MOUSE_Private_Variables
  * @{
  */
HID_MOUSE_Info_TypeDef   mouse_info;
uint8_t                  mouse_report_data[USBH_HID_MOUSE_REPORT_SIZE];
uint8_t                  mouse_rx_report_buf[USBH_HID_MOUSE_REPORT_SIZE];
static HID_MOUSE_ReportLayoutTypeDef mouse_layout;

/* Structures defining how to access items in a HID mouse report */
/* Access button 1 state. */
static const HID_Report_ItemTypedef prop_b1 =
{
  mouse_report_data, /*data*/
  1,     /*size*/
  0,     /*shift*/
  0,     /*count (only for array items)*/
  0,     /*signed?*/
  0,     /*min value read can return*/
  1,     /*max value read can return*/
  0,     /*min value device can report*/
  1,     /*max value device can report*/
  1      /*resolution*/
};

/* Access button 2 state. */
static const HID_Report_ItemTypedef prop_b2 =
{
  mouse_report_data, /*data*/
  1,     /*size*/
  1,     /*shift*/
  0,     /*count (only for array items)*/
  0,     /*signed?*/
  0,     /*min value read can return*/
  1,     /*max value read can return*/
  0,     /*min value device can report*/
  1,     /*max value device can report*/
  1      /*resolution*/
};

/* Access button 3 state. */
static const HID_Report_ItemTypedef prop_b3 =
{
  mouse_report_data, /*data*/
  1,     /*size*/
  2,     /*shift*/
  0,     /*count (only for array items)*/
  0,     /*signed?*/
  0,     /*min value read can return*/
  1,     /*max value read can return*/
  0,     /*min vale device can report*/
  1,     /*max value device can report*/
  1      /*resolution*/
};

/* Access x coordinate change. */
static const HID_Report_ItemTypedef prop_x =
{
  mouse_report_data + 1U, /*data*/
  8,     /*size*/
  0,     /*shift*/
  0,     /*count (only for array items)*/
  1,     /*signed?*/
  0,     /*min value read can return*/
  0xFFFF,/*max value read can return*/
  0,     /*min vale device can report*/
  0xFFFF,/*max value device can report*/
  1      /*resolution*/
};

/* Access y coordinate change. */
static const HID_Report_ItemTypedef prop_y =
{
  mouse_report_data + 2U, /*data*/
  8,     /*size*/
  0,     /*shift*/
  0,     /*count (only for array items)*/
  1,     /*signed?*/
  0,     /*min value read can return*/
  0xFFFF,/*max value read can return*/
  0,     /*min vale device can report*/
  0xFFFF,/*max value device can report*/
  1      /*resolution*/
};


/**
  * @}
  */


/** @defgroup USBH_HID_MOUSE_Private_Functions
  * @{
  */

/**
  * @brief  USBH_HID_MouseInit
  *         The function init the HID mouse.
  * @param  phost: Host handle
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_HID_MouseInit(USBH_HandleTypeDef *phost)
{
  uint32_t i;
  HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *) phost->pActiveClass->pData;

  mouse_info.x = 0U;
  mouse_info.y = 0U;
  mouse_info.wheel = 0;
  mouse_info.buttons[0] = 0U;
  mouse_info.buttons[1] = 0U;
  mouse_info.buttons[2] = 0U;

  USBH_HID_MouseParseReportDesc(phost->device.Data, HID_Handle->HID_Desc.wItemLength);

  for (i = 0U; i < sizeof(mouse_report_data); i++)
  {
    mouse_report_data[i] = 0U;
    mouse_rx_report_buf[i] = 0U;
  }

  if (HID_Handle->length > sizeof(mouse_report_data))
  {
    HID_Handle->length = (uint16_t)sizeof(mouse_report_data);
  }
  HID_Handle->pData = mouse_rx_report_buf;

  if ((HID_QUEUE_SIZE * sizeof(mouse_report_data)) > sizeof(phost->device.Data))
  {
    return USBH_FAIL;
  }
  else
  {
    USBH_HID_FifoInit(&HID_Handle->fifo, phost->device.Data, (uint16_t)(HID_QUEUE_SIZE * sizeof(mouse_report_data)));
  }

  return USBH_OK;
}

/**
  * @brief  USBH_HID_GetMouseInfo
  *         The function return mouse information.
  * @param  phost: Host handle
  * @retval mouse information
  */
HID_MOUSE_Info_TypeDef *USBH_HID_GetMouseInfo(USBH_HandleTypeDef *phost)
{
  if (USBH_HID_MouseDecode(phost) == USBH_OK)
  {
    return &mouse_info;
  }
  else
  {
    return NULL;
  }
}

/**
  * @brief  USBH_HID_MouseDecode
  *         The function decode mouse data.
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_HID_MouseDecode(USBH_HandleTypeDef *phost)
{
  HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *) phost->pActiveClass->pData;

  if ((HID_Handle->length == 0U) || (HID_Handle->fifo.buf == NULL))
  {
    return USBH_FAIL;
  }
  /*Fill report */
  if (USBH_HID_FifoRead(&HID_Handle->fifo, &mouse_report_data, HID_Handle->length) == HID_Handle->length)
  {
    /*Decode report */
    mouse_info.x = (uint8_t)HID_ReadItem((HID_Report_ItemTypedef *) &prop_x, 0U);
    mouse_info.y = (uint8_t)HID_ReadItem((HID_Report_ItemTypedef *) &prop_y, 0U);
    mouse_info.wheel = 0;

    if (mouse_layout.wheel.valid != 0U)
    {
      uint8_t report_id = mouse_layout.wheel.report_id;

      if ((mouse_layout.has_report_id == 0U) || (mouse_report_data[0] == report_id))
      {
        if (USBH_HID_MouseFieldFits(&mouse_layout.wheel, HID_Handle->length) != 0U)
        {
          mouse_info.wheel = (int8_t)USBH_HID_MouseReadSignedField(mouse_report_data,
                                                                   &mouse_layout.wheel);
        }
      }
    }

    mouse_info.buttons[0] = (uint8_t)HID_ReadItem((HID_Report_ItemTypedef *) &prop_b1, 0U);
    mouse_info.buttons[1] = (uint8_t)HID_ReadItem((HID_Report_ItemTypedef *) &prop_b2, 0U);
    mouse_info.buttons[2] = (uint8_t)HID_ReadItem((HID_Report_ItemTypedef *) &prop_b3, 0U);

    if ((mouse_layout.wheel.valid == 0U) && (HID_Handle->length > 3U))
    {
      mouse_info.wheel = (int8_t)mouse_report_data[3U];
    }

    return USBH_OK;
  }
  return   USBH_FAIL;
}

static void USBH_HID_MouseParseReportDesc(uint8_t *desc, uint16_t len)
{
  uint16_t pos = 0U;
  uint16_t bit_offset = 0U;
  uint16_t usage_page = 0U;
  uint32_t usages[HID_MAX_USAGE];
  uint8_t usage_count = 0U;
  uint32_t usage_min = 1U;
  uint32_t usage_max = 0U;
  int32_t logical_min = 0;
  uint8_t report_size = 0U;
  uint8_t report_count = 0U;
  uint8_t report_id = 0U;

  (void)USBH_memset(&mouse_layout, 0, sizeof(mouse_layout));

  while ((desc != NULL) && (pos < len))
  {
    uint8_t prefix = desc[pos++];
    uint8_t size_code;
    uint8_t size;
    uint8_t type;
    uint8_t tag;
    uint32_t value;
    int32_t signed_value;

    if (prefix == HID_ITEM_LONG)
    {
      if ((pos + 1U) > len)
      {
        break;
      }
      size = desc[pos++];
      pos++;
      if ((uint16_t)(pos + size) > len)
      {
        break;
      }
      pos = (uint16_t)(pos + size);
      continue;
    }

    size_code = (uint8_t)(prefix & 0x03U);
    size = (size_code == 3U) ? 4U : size_code;
    type = (uint8_t)((prefix >> 2U) & 0x03U);
    tag = (uint8_t)((prefix >> 4U) & 0x0FU);

    if ((uint16_t)(pos + size) > len)
    {
      break;
    }

    value = USBH_HID_MouseItemUnsigned(&desc[pos], size);
    signed_value = USBH_HID_MouseItemValue(&desc[pos], size);
    pos = (uint16_t)(pos + size);

    if (type == HID_ITEM_TYPE_GLOBAL)
    {
      if (tag == HID_GLOBAL_ITEM_TAG_USAGE_PAGE)
      {
        usage_page = (uint16_t)value;
      }
      else if (tag == HID_GLOBAL_ITEM_TAG_LOG_MIN)
      {
        logical_min = signed_value;
      }
      else if (tag == HID_GLOBAL_ITEM_TAG_REPORT_SIZE)
      {
        report_size = (uint8_t)value;
      }
      else if (tag == HID_GLOBAL_ITEM_TAG_REPORT_COUNT)
      {
        report_count = (uint8_t)value;
      }
      else if (tag == HID_GLOBAL_ITEM_TAG_REPORT_ID)
      {
        report_id = (uint8_t)value;
        mouse_layout.has_report_id = 1U;
        bit_offset = 8U;
      }
    }
    else if (type == HID_ITEM_TYPE_LOCAL)
    {
      if (tag == HID_LOCAL_ITEM_TAG_USAGE)
      {
        if (usage_count < HID_MAX_USAGE)
        {
          usages[usage_count++] = value;
        }
      }
      else if (tag == HID_LOCAL_ITEM_TAG_USAGE_MIN)
      {
        usage_min = value;
      }
      else if (tag == HID_LOCAL_ITEM_TAG_USAGE_MAX)
      {
        usage_max = value;
      }
    }
    else if (type == HID_ITEM_TYPE_MAIN)
    {
      if (tag == HID_MAIN_ITEM_TAG_INPUT)
      {
        uint8_t flags = (uint8_t)value;

        if ((report_size != 0U) && (report_count != 0U))
        {
          if ((flags & HID_MOUSE_INPUT_CONSTANT) == 0U)
          {
            uint8_t i;

            for (i = 0U; i < report_count; i++)
            {
              uint32_t usage = 0U;
              uint16_t field_offset = (uint16_t)(bit_offset + ((uint16_t)i * report_size));

              if (i < usage_count)
              {
                usage = usages[i];
              }
              else if (usage_min <= usage_max)
              {
                usage = usage_min + i;
              }

              if (usage_page == HID_MOUSE_USAGE_PAGE_GENERIC_DESKTOP)
              {
                if (usage == HID_MOUSE_USAGE_X)
                {
                  USBH_HID_MouseSetField(&mouse_layout.x, field_offset, report_size,
                                         (uint8_t)(logical_min < 0), report_id);
                }
                else if (usage == HID_MOUSE_USAGE_Y)
                {
                  USBH_HID_MouseSetField(&mouse_layout.y, field_offset, report_size,
                                         (uint8_t)(logical_min < 0), report_id);
                }
                else if (usage == HID_MOUSE_USAGE_WHEEL)
                {
                  USBH_HID_MouseSetField(&mouse_layout.wheel, field_offset, report_size,
                                         (uint8_t)(logical_min < 0), report_id);
                }
              }
              else if ((usage_page == HID_MOUSE_USAGE_PAGE_BUTTON) &&
                       (usage == HID_MOUSE_USAGE_BUTTON_1))
              {
                USBH_HID_MouseSetField(&mouse_layout.button1, field_offset, report_size,
                                       0U, report_id);
              }
            }
          }

          bit_offset = (uint16_t)(bit_offset + ((uint16_t)report_size * report_count));
        }
      }

      USBH_HID_MouseClearLocal(usages, &usage_count, &usage_min, &usage_max);
    }
  }

  mouse_layout.valid = (uint8_t)((mouse_layout.x.valid != 0U) &&
                                 (mouse_layout.y.valid != 0U));
}

static int32_t USBH_HID_MouseItemValue(const uint8_t *data, uint8_t size)
{
  uint32_t value = USBH_HID_MouseItemUnsigned(data, size);

  if (size == 1U)
  {
    return (int32_t)(int8_t)value;
  }
  if (size == 2U)
  {
    return (int32_t)(int16_t)value;
  }
  if (size == 4U)
  {
    return (int32_t)value;
  }

  return 0;
}

static uint32_t USBH_HID_MouseItemUnsigned(const uint8_t *data, uint8_t size)
{
  uint32_t value = 0U;
  uint8_t i;

  for (i = 0U; i < size; i++)
  {
    value |= (uint32_t)data[i] << (8U * i);
  }

  return value;
}

static void USBH_HID_MouseClearLocal(uint32_t *usages, uint8_t *usage_count,
                                     uint32_t *usage_min, uint32_t *usage_max)
{
  if (usages != NULL)
  {
    (void)USBH_memset(usages, 0, sizeof(uint32_t) * HID_MAX_USAGE);
  }
  *usage_count = 0U;
  *usage_min = 1U;
  *usage_max = 0U;
}

static void USBH_HID_MouseSetField(HID_MOUSE_FieldTypeDef *field, uint16_t bit_offset,
                                   uint8_t size, uint8_t sign, uint8_t report_id)
{
  if ((field != NULL) && (size > 0U) && (size <= 16U))
  {
    field->bit_offset = bit_offset;
    field->size = size;
    field->sign = sign;
    field->report_id = report_id;
    field->valid = 1U;
  }
}

static uint8_t USBH_HID_MouseFieldFits(const HID_MOUSE_FieldTypeDef *field,
                                       uint16_t report_len)
{
  uint16_t end_bit;

  if ((field == NULL) || (field->valid == 0U))
  {
    return 0U;
  }

  end_bit = (uint16_t)(field->bit_offset + field->size);
  return (uint8_t)(((end_bit + 7U) / 8U) <= report_len);
}

static uint32_t USBH_HID_MouseReadUnsignedField(const uint8_t *data,
                                                const HID_MOUSE_FieldTypeDef *field)
{
  uint32_t value = 0U;
  uint8_t i;

  for (i = 0U; i < field->size; i++)
  {
    uint16_t bit = (uint16_t)(field->bit_offset + i);

    if ((data[bit / 8U] & (uint8_t)(1U << (bit % 8U))) != 0U)
    {
      value |= (uint32_t)1U << i;
    }
  }

  return value;
}

static int16_t USBH_HID_MouseReadSignedField(const uint8_t *data,
                                             const HID_MOUSE_FieldTypeDef *field)
{
  uint32_t value = USBH_HID_MouseReadUnsignedField(data, field);

  if ((field->sign != 0U) && ((value & ((uint32_t)1U << (field->size - 1U))) != 0U))
  {
    value |= 0xFFFFFFFFU << field->size;
  }

  return (int16_t)(int32_t)value;
}

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */


/**
  * @}
  */
