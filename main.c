/**
 * Copyright (c) 2015 - 2019, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/** @file
 * @defgroup blinky_example_main main.c
 * @{
 * @ingroup blinky_example_freertos
 *
 * @brief Blinky FreeRTOS Example Application main file.
 *
 * This file contains the source code for a sample application using FreeRTOS to blink LEDs.
 *
 */

#include <stdbool.h>
#include <stdint.h>

#include <ti/drivers/net/wifi/simplelink.h>

/*#include <ti/drivers/dpl/SemaphoreP.h>
#include <ti/drivers/dpl/MutexP.h>
#include <ti/drivers/dpl/ClockP.h>
#include <time.h>*/

#include <pthread.h>
#include <mqueue.h>
#include <semaphore.h>

#include "simplelink_structures.h"
    
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "bsp.h"
#include "nordic_common.h"
#include "nrf_drv_clock.h"
#include "sdk_errors.h"
#include "app_error.h"
    
#include "json_test.h"

#if LEDS_NUMBER <= 2
#error "Board is not equipped with enough amount of LEDs"
#endif

#define TASK_DELAY        200           /**< Task delay. Delays a LED0 task for 200 ms */
#define TIMER_PERIOD      1000          /**< Timer period. LED1 timer will expire after 1000 ms */

#define MAIN_STACK_SIZE         (4096)
#define TASK_STACK_SIZE         (2048)
#define SPAWN_TASK_PRIORITY     (9)

TaskHandle_t  led_toggle_task_handle;   /**< Reference to LED0 toggling FreeRTOS task. */
TimerHandle_t led_toggle_timer_handle;  /**< Reference to LED1 toggling FreeRTOS timer. */

TaskHandle_t gSpawn_thread = NULL;
appControlBlock app_CB;

extern void UART_PRINT(char* label, ...);

void* mainThread(void* arg);

/**@brief LED0 task entry function.
 *
 * @param[in] pvParameter   Pointer that will be used as the parameter for the task.
 */
/*static void led_toggle_task_function (void * pvParameter)
{
    UNUSED_PARAMETER(pvParameter);
    while (true)
    {
        bsp_board_led_invert(BSP_BOARD_LED_0);

        // Delay a task for a given number of ticks
        vTaskDelay(TASK_DELAY);

        // Tasks must be implemented to never return...
    }
}*/

/**@brief The function to call when the LED1 FreeRTOS timer expires.
 *
 * @param[in] pvParameter   Pointer that will be used as the parameter for the timer.
 */
/*static void led_toggle_timer_callback (void * pvParameter)
{
    UNUSED_PARAMETER(pvParameter);
    bsp_board_led_invert(BSP_BOARD_LED_1);
}*/

int main(void)
{
   //cJSON_Test();
   
   //JSMN_Test();
   
   //TISL_JSON_TEST();
   
   ret_code_t err_code;

   /* Initialize clock driver for better time accuracy in FREERTOS */
   err_code = nrf_drv_clock_init();
   APP_ERROR_CHECK(err_code);

   /* Configure LED-pins as outputs */
   bsp_board_init(BSP_INIT_LEDS);

   // Create task for LED0 blinking with priority set to 2
   /*UNUSED_VARIABLE(xTaskCreate(led_toggle_task_function, "LED0", configMINIMAL_STACK_SIZE + 200, NULL, 2, &led_toggle_task_handle));

   // Start timer for LED1 blinking
   led_toggle_timer_handle = xTimerCreate( "LED1", TIMER_PERIOD, pdTRUE, NULL, led_toggle_timer_callback);
   UNUSED_VARIABLE(xTimerStart(led_toggle_timer_handle, 0));
   */
   
   
   
   // create sl_Task spawn thread
   /* Create the sl_Task internal spawn thread */
    /*pthread_attr_init(&pAttrs_spawn);
    priParam.sched_priority = SPAWN_TASK_PRIORITY;
    RetVal = pthread_attr_setschedparam(&pAttrs_spawn, &priParam);
    RetVal |= pthread_attr_setstacksize(&pAttrs_spawn, TASK_STACK_SIZE);*/

    /* The SimpleLink host driver architecture mandate spawn
       thread to be created prior to calling Sl_start (turning the NWP on). */
    /* The purpose of this thread is to handle
       asynchronous events sent from the NWP.
     * Every event is classified and later handled
       by the Host driver event handlers. */
    //RetVal = pthread_create(&gSpawn_thread, &pAttrs_spawn, sl_Task, NULL);
    
   pthread_attr_t pAttrs;
   int detachState = PTHREAD_CREATE_DETACHED;
   struct sched_param priParam;
   
   pthread_attr_init(&pAttrs);
   priParam.sched_priority = 1;
   
   if (pthread_attr_setdetachstate(&pAttrs, detachState) != 0)
   {
      return(NULL);
   }
   
   pthread_attr_setschedparam(&pAttrs, &priParam);
   
   if (pthread_attr_setstacksize(&pAttrs, MAIN_STACK_SIZE) != 0)
   {
      return(NULL);
   }
   
   if(pthread_create(&gSpawn_thread, &pAttrs, mainThread, NULL) != 0)
   {
      // failed to create the simple link thread.
      return(NULL);
   }

    
      
   // Activate deep sleep mode
   SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

   // Start FreeRTOS scheduler.
   vTaskStartScheduler();

   while (true)
   {
        /* FreeRTOS should not be here... FreeRTOS goes back to the start of stack
         * in vTaskStartScheduler function. */
   }
}

int32_t initAppVariables(void)
{
    app_CB.Status = 0;
    app_CB.Role = ROLE_RESERVED;
    app_CB.Exit = FALSE;

    memset(&app_CB.CmdBuffer, 0x0, CMD_BUFFER_LEN);
    memset(&app_CB.gDataBuffer, 0x0, sizeof(app_CB.gDataBuffer));
    memset(&app_CB.CON_CB,0x0, sizeof(app_CB.CON_CB));

    int ret = sem_init(&app_CB.CON_CB.connectEventSyncObj, 0, 0);
    if(ret != 0)
    {
       SHOW_WARNING(ret, OS_ERROR);
       return(-1);
    }

    ret = sem_init(&app_CB.CON_CB.eventCompletedSyncObj,  0, 0);
    if(ret != 0)
    {
       SHOW_WARNING(ret, OS_ERROR);
       return(-1);
    }

    ret = sem_init(&app_CB.CON_CB.ip4acquireEventSyncObj, 0, 0);
    if(ret != 0)
    {
       SHOW_WARNING(ret, OS_ERROR);
       return(-1);
    }

    ret = sem_init(&app_CB.CON_CB.ip6acquireEventSyncObj, 0, 0);
    if(ret != 0)
    {
        SHOW_WARNING(ret, OS_ERROR);
        return(-1);
    }

    memset(&app_CB.P2P_CB, 0x0, sizeof(app_CB.P2P_CB));

    ret = sem_init(&app_CB.P2P_CB.DeviceFound, 0, 0);
    if(ret != 0)
    {
        SHOW_WARNING(ret, OS_ERROR);
        return(-1);
    }

    ret = sem_init(&app_CB.P2P_CB.RcvConReq, 0, 0);
    if(ret != 0)
    {
        SHOW_WARNING(ret, OS_ERROR);
        return(-1);
    }

    ret = sem_init(&app_CB.P2P_CB.RcvNegReq, 0, 0);
    if(ret != 0)
    {
        SHOW_WARNING(ret, OS_ERROR);
        return(-1);
    }

    ret = sem_init(&app_CB.WowlanSleepSem, 0, 0);
    if(ret != 0)
    {
        SHOW_WARNING(ret, OS_ERROR);
        return(-1);
    }

    return(ret);
}

void* mainThread(void* arg)
{
   int32_t RetVal;
   pthread_attr_t pAttrs_spawn;
   struct sched_param priParam;
   struct timespec ts = {0};

   /* Initializes the SPI interface to the Network
      Processor and peripheral SPI (if defined in the board file) */
   //SPI_init();
   //GPIO_init();
   //WiFi_init();

   /* Init Application variables */
   RetVal = initAppVariables();

   // initialize the realtime clock
   clock_settime(CLOCK_REALTIME, &ts);

   // Create the sl_Task internal spawn thread
   pthread_attr_init(&pAttrs_spawn);
   priParam.sched_priority = SPAWN_TASK_PRIORITY;
   RetVal = pthread_attr_setschedparam(&pAttrs_spawn, &priParam);
   RetVal |= pthread_attr_setstacksize(&pAttrs_spawn, TASK_STACK_SIZE);

   /* The SimpleLink host driver architecture mandate spawn
      thread to be created prior to calling Sl_start (turning the NWP on). */
   /* The purpose of this thread is to handle
      asynchronous events sent from the NWP.
    * Every event is classified and later handled
      by the Host driver event handlers. */
   RetVal = pthread_create(&gSpawn_thread, &pAttrs_spawn, sl_Task, NULL);
   if(RetVal < 0)
   {
      UART_PRINT("Network Terminal - Unable to create spawn thread \n");
      return(NULL);
   }
   
   // TODO: why is this needed?
   taskYIELD();
   
   // Before turning on the NWP on,
   // reset any previously configured parameters
   //
   //   IMPORTANT NOTE - This is an example reset function,
   //   user must update this function to match the application settings.
   int isSuccess = sl_WifiConfig();
   if(isSuccess < 0)
   {
     // Handle Error
     UART_PRINT(
         "Network Terminal - Couldn't configure Network Processor - %d\n",
         isSuccess);
     return(NULL);
   }

   // Turn NWP on
   isSuccess = sl_Start(NULL, NULL, NULL);
   if(isSuccess < 0)
   {
     // Handle Error
     UART_PRINT("sl_start failed - %d\n", isSuccess);
     return(NULL);
   }

   // sl_Start returns on success the role that device started on
   // TODO: RetVal needs to be the value of sl_Start()
   //app_CB.Role = RetVal;

   // disable the soft-roaming
   SlWlanRegisterLinkQualityEvents_t RegisterLinkQuality;
   RegisterLinkQuality.Enable = 0;

   // trigger Id 1 is used for soft roaming trigger id 0 is for the host app usage.
   RegisterLinkQuality.TriggerId = 1;
   RegisterLinkQuality.Metric = SL_WLAN_METRIC_EVENT_RSSI_BEACON;
   RegisterLinkQuality.Direction = SL_WLAN_RSSI_EVENT_DIR_LOW;
   RegisterLinkQuality.Threshold = 0;
   RegisterLinkQuality.Hysteresis = 0;

   // SL_WLAN_RX_QUALITY_EVENT_EDGE;
   RegisterLinkQuality.Type = SL_WLAN_RX_QUALITY_EVENT_LEVEL;
   RegisterLinkQuality.Pacing = 0;

   (void)sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
                 SL_WLAN_GENERAL_PARAM_REGISTER_LINK_QUALITY_EVENT,
                 sizeof(SlWlanRegisterLinkQualityEvents_t),
                 (uint8_t*)&RegisterLinkQuality);

   // Unregister mDNS services
   isSuccess = sl_NetAppMDNSUnRegisterService(0, 0, 0);
   if(isSuccess < 0)
   {
   // Handle Error
   UART_PRINT("sl_NetAppMDNSUnRegisterService failed - %d\n", isSuccess);
   return(NULL);
   }

   // TODO: scan for access points
   
   while(1);
}

/**
 *@}
 **/
