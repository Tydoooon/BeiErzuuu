/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_led.h"
#include "app_lcd.h"
#include "app_lvgl.h"
#include "app_mouse.h"
#include "app_ui.h"
#include "app_w25q128.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
osThreadId_t startTaskHandle;
osThreadId_t keyTaskHandle;
osThreadId_t lcdTaskHandle;
osThreadId_t w25q128TaskHandle;
osThreadId_t usbinitTaskHandle;
const osThreadAttr_t startTask_attributes = {
  .name = "startTask",
#if APP_W25Q128_FONT_PROGRAMMER_ENABLE
  .stack_size = 1024,
#else
  .stack_size = 128 * 4,
#endif
  .priority = (osPriority_t) osPriorityNormal,
};
const osThreadAttr_t keyTask_attributes = {
  .name = "keyTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
const osThreadAttr_t lcdTask_attributes = {
  .name = "lcdTask",
  .stack_size = 4096,
  .priority = (osPriority_t) osPriorityNormal,
};
const osThreadAttr_t w25q128Task_attributes = {
  .name = "w25q128Task",
  .stack_size = 1024,
  .priority = (osPriority_t) osPriorityNormal,
};
const osThreadAttr_t usbinitTask_attributes = {
  .name = "usbinitTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartStartTask(void *argument);
void StartKeyTask(void *argument);
void StartLCDTask(void *argument);
void StartW25Q128Task(void *argument);
void StartUSBInitTask(void *argument);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

extern void MX_USB_HOST_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  startTaskHandle = osThreadNew(StartStartTask, NULL, &startTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  (void)argument;

  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void StartStartTask(void *argument)
{
  (void)argument;
#if APP_W25Q128_FONT_PROGRAMMER_ENABLE
  (void)app_w25q128_program_ui_font_image();
#endif
  keyTaskHandle = osThreadNew(StartKeyTask, NULL, &keyTask_attributes);
  lcdTaskHandle = osThreadNew(StartLCDTask, NULL, &lcdTask_attributes);
  w25q128TaskHandle = osThreadNew(StartW25Q128Task, NULL, &w25q128Task_attributes);
  usbinitTaskHandle = osThreadNew(StartUSBInitTask, NULL, &usbinitTask_attributes);
  
  osThreadExit();
}

void StartKeyTask(void *argument)
{
  (void)argument;
  g_key_queue = KeyQueueInit();

  for(;;)
  {
    key_scan();
    osDelay(10);
  }
}

void StartLCDTask(void *argument)
{
  uint8_t key_id;
  (void)argument;
  lv_init();
  lv_port_disp_init();
  app_ui_create();
  lv_timer_handler();
  osDelay(10);
  app_mouse_init();

  for(;;)
  {
    while (g_key_queue != NULL && xQueueReceive(g_key_queue, &key_id, 0) == pdPASS)
    {
      app_ui_key_push(key_id);
    }

    app_ui_process_keys();
    lv_timer_handler();
    osDelay(5);
  }
}
void StartW25Q128Task(void *argument)
{
  (void)argument;
  bsp_w25q128_status_t ret;

  ret = app_w25q128_init();
  if(ret == BSP_W25Q128_OK) {
    while(!app_ui_is_created()) {
      osDelay(10);
    }
    app_ui_load_w25q128_resources();
  } else {
    
  }

  osThreadExit();
}
void StartUSBInitTask(void *argument)
{
  (void)argument;
  MX_USB_HOST_Init();
  osThreadExit();
}
/* USER CODE END Application */

