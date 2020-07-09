/**
 * Demonstrate the test usage of the CC3135
 *
 *  TI-POSIX
 *  http://software-dl.ti.com/simplelink/esd/simplelink_msp432e4_sdk/2.40.00.11/docs/tiposix/Users_Guide.html
 *
 *  LLNL POSIX and pThreads
 *  https://computing.llnl.gov/tutorials/pthreads/
 *
 *  http://e2e.ti.com/support/wireless-connectivity/wifi/f/968/t/835623

 *  http://software-dl.ti.com/simplelink/esd/plugins/simplelink_sdk_wifi_plugin/2_40_00_22/exports/docs/users_guide_simplelink_sdk_wifi_plugin.html#uniflash-cc31xxemu-boost
 *
 *  https://e2e.ti.com/support/wireless-connectivity/wifi/f/968/t/876603

 *  Hang on SL Start
 *  https://e2e.ti.com/support/wireless-connectivity/wifi/f/968/t/707537
 *
 *  SL Start returns Aborted error
 *  https://e2e.ti.com/support/wireless-connectivity/wifi/f/968/t/776858
 *  
 *  SL STOP might need additional SPI_read after reading the header
 *  http://e2e.ti.com/support/wireless-connectivity/wifi/f/968/p/858459/3176981#3176981?jktype=e2e
 * * https://e2e.ti.com/support/wireless-connectivity/wifi/f/968/t/877877?tisearch=e2e-sitesearch&keymatch=cc3135
 * * sl_WifiConfig behaves better when allowed a 5s delay?
 * https://e2e.ti.com/support/wireless-connectivity/wifi/f/968/t/874487
 */

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include <ti/drivers/net/wifi/simplelink.h>

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

#include "WiFiDriver.h"
#include "AWSDriver.h"

#if LEDS_NUMBER <= 2
#error "Board is not equipped with enough amount of LEDs"
#endif

#define TASK_DELAY        200           /**< Task delay. Delays a LED0 task for 200 ms */
#define TIMER_PERIOD      1000          /**< Timer period. LED1 timer will expire after 1000 ms */

#define MAIN_STACK_SIZE           (8192)

#define TARGET_SSID             "Galios"
#define SEC_KEY                 "Gen2WiFiPW"

TaskHandle_t  led_toggle_task_handle;   /**< Reference to LED0 toggling FreeRTOS task. */
TimerHandle_t led_toggle_timer_handle;  /**< Reference to LED1 toggling FreeRTOS timer. */

TaskHandle_t gMain_thread = NULL;

appControlBlock app_CB;

extern void UART_PRINT(char* label, ...);

void* mainThread(void* arg);

int main(void)
{
   // Uncomment one or more of the following to demonstrate the use of the 
   // different JSON libraries.
   //cJSON_Test();
   
   //JSMN_Test();
   
   //TISL_JSON_TEST();
   
   ret_code_t err_code;

   /* Initialize clock driver for better time accuracy in FREERTOS */
   err_code = nrf_drv_clock_init();
   APP_ERROR_CHECK(err_code);

   // Configure LED-pins as outputs
   // Uncomment the following 4 lines to demonstrate creating a task that blinks the on board led.
   bsp_board_init(BSP_INIT_LEDS);

   // Create task for LED0 blinking with priority set to 2
   /*UNUSED_VARIABLE(xTaskCreate(led_toggle_task_function, "LED0", configMINIMAL_STACK_SIZE + 200, NULL, 2, &led_toggle_task_handle));

   // Start timer for LED1 blinking
   led_toggle_timer_handle = xTimerCreate( "LED1", TIMER_PERIOD, pdTRUE, NULL, led_toggle_timer_callback);
   UNUSED_VARIABLE(xTimerStart(led_toggle_timer_handle, 0));
   */
   
   // spin up the simple link main thread and start the RTOS scheduler
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
   
   if(pthread_create(&gMain_thread, &pAttrs, mainThread, NULL) != 0)
   {
      // failed to create the simple link thread.
      return(NULL);
   }
   
   pthread_attr_destroy(&pAttrs);
      
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

void* mainThread(void* arg)
{   
   /* Initializes the SPI interface to the Network
      Processor and peripheral SPI (if defined in the board file) */
   //WiFi_init();
   bsp_board_led_on(BSP_BOARD_LED_0);
   if (WiFiDriver_StartSimpleLink())
   { 
      while(false == WiFiDriver_Init())
      {
         NwpPowerOff();
         
         // if the initialization fails, just wait for a beat and then try again.
         usleep(1000000 - 1);
      }
   }
   
  
   bsp_board_led_on(BSP_BOARD_LED_1);
   
   AWSDriver_Init();
   
   // scan and attempt to connect until the desired AP is found and connected to.
   WiFiDriver_ScanStart(10, true);
   usleep(1000000 - 1);
   bsp_board_led_off(BSP_BOARD_LED_1);
   
   while(1)
   {
      if (WiFiDriver_CollectScanResults(10) > 0)
      {
         WiFiDriver_ScanStop();
         
         //bsp_board_led_off(BSP_BOARD_LED_2);
         
         if (WiFiDriver_Connect(TARGET_SSID, SEC_KEY))
         {
            // light up LED to show connected
            bsp_board_led_on(BSP_BOARD_LED_2);
            
            while(1)
            {
               // TODO: ping Polka Palace every 10ish seconds
               //WiFiDriver_Send(TEST_MQTT);
               AWSDriver_Run(NULL);
            
               // flash LED indicating the ping went out.
               bsp_board_led_invert(BSP_BOARD_LED_3);
               usleep(100000 - 1);
               taskYIELD();
            }
         }
      }
      
      bsp_board_led_on(BSP_BOARD_LED_1);
      WiFiDriver_ScanStart(10, true);
      
          
      // setup, scanning, or connection failed ..
      // pause for a beat, then try again.
      usleep(1000000 - 1);
      
      bsp_board_led_off(BSP_BOARD_LED_1);
   }
}
