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

#if LEDS_NUMBER <= 2
#error "Board is not equipped with enough amount of LEDs"
#endif

#define TASK_DELAY        200           /**< Task delay. Delays a LED0 task for 200 ms */
#define TIMER_PERIOD      1000          /**< Timer period. LED1 timer will expire after 1000 ms */

#define MAIN_STACK_SIZE         (4096)
#define TASK_STACK_SIZE         (2048)
#define SPAWN_TASK_PRIORITY     (9)

#define TARGET_SSID             "Galios"
#define SEC_KEY                 "Gen2WiFiPW"

#define WLAN_EVENT_TOUT             (6000)
#define TIMEOUT_SEM                 (-1)

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
   
   // Yield control of this tasks until the synchronization Task Control block 
   // is setup by the Spawn task.
   taskYIELD();
   
   sl_Stop(0);
   
   // Simple Link Library Documentation:
   // http://software-dl.ti.com/ecs/SIMPLELINK_CC3220_SDK/1_50_00_06/exports/docs/wifi_host_driver_api/html/group___wlan.html#gab8ba00f95398b5dccd80550ab3fc17e5
   
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
   //app_CB.Role = isSuccess;
   
   SlDeviceVersion_t ver;
   uint16_t pConfigLen = sizeof(ver);
   uint8_t pConfigOpt = SL_DEVICE_GENERAL_VERSION;
   if (sl_DeviceGet(SL_DEVICE_GENERAL, &pConfigOpt, &pConfigLen, (uint8_t*)(&ver)) < 0)
   {
      return NULL;
   }

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
   
   // TODO: scan on both 2.4Ghz and 5Ghz every x Seconds
   

   // Make sure no connection policy is not set
   //   (so no scan is run in the background)
   SetPolicyCmd_t SetPolicyParams;
   SetPolicyParams.hiddenSsid = 0;
   SetPolicyParams.ScanParamConfig.ChannelsMask = 0xFFFFFFFF;
   SetPolicyParams.ScanParamConfig.RssiThreshold = -80;
   SetPolicyParams.ScanParamConfig5G.ChannelsMask = 0xFFFFFFFF;
   SetPolicyParams.ScanParamConfig5G.RssiThreshold = -80;
   SetPolicyParams.ScanIntervalinSec = 10;
   uint8_t policyOpt = SL_WLAN_CONNECTION_POLICY(0, 0, 0, 0);

   if (sl_WlanPolicySet(SL_WLAN_POLICY_CONNECTION,policyOpt, NULL, 0) < 0)
   {
      return NULL;
   }

   // Set scan parameters for 2.4Gz
   if (sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
                  SL_WLAN_GENERAL_PARAM_OPT_SCAN_PARAMS,
                  sizeof(SetPolicyParams.ScanParamConfig),
                  (uint8_t *)(&SetPolicyParams.ScanParamConfig)) < 0)
   {
      return NULL;
   }
   
   // Set scan parameters for 5Ghz
   uint8_t mode = 0x01;
   uint16_t option = SL_WLAN_GENERAL_PARAM_OPT_ENABLE_5G;
   if (sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
                  option,
                  sizeof(mode),
                  (uint8_t*)&mode) < 0)
   {
      return NULL;
   }
   
   
   SlWlanScanParam5GCommand_t ScanParamConfig5G;
   option = SL_WLAN_GENERAL_PARAM_OPT_SCAN_PARAMS_5G;
   
   // 5.0G channels bits order: 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132,
   //                          136, 140, 144, 149, 153, 157, 161, 165, 169, 184, 188, 192, 196 
        
   ScanParamConfig5G.ChannelsMask = 0x3FFFFFFF; // Select ChannelsMask for channels 36, 40, 44, 48 
   ScanParamConfig5G.RssiThreshold = -70;
   if (sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, option, sizeof(SlWlanScanParam5GCommand_t), (_u8 *)&ScanParamConfig5G) < 0)
   {
      return NULL;
   }
   
   
   uint8_t enabled5Ghz = 0;
   option = SL_WLAN_GENERAL_PARAM_OPT_ENABLE_5G;
   uint16_t optionLen = sizeof(SlWlanScanParamCommand_t);
   
   sl_WlanGet(SL_WLAN_CFG_GENERAL_PARAM_ID, &option, &optionLen, (uint8_t*)&enabled5Ghz);
   
   // Enable scan
   policyOpt = SL_WLAN_SCAN_POLICY(1, SetPolicyParams.hiddenSsid);

   // Set scan policy - this API starts the scans
   if (sl_WlanPolicySet(SL_WLAN_POLICY_SCAN, policyOpt,
                        (uint8_t*)(&SetPolicyParams.ScanIntervalinSec),
                        sizeof(SetPolicyParams.ScanIntervalinSec)) < 0)
   {
      return NULL;
   }
   
   
   // scan and attempt to connect until the desired AP is found and connected to.
   while(1)
   {
      // Scan for available access points
      ScanCmd_t scanParams;
      scanParams.extendedRes = false;
      scanParams.index = 0;
      scanParams.numOfentries = 30;
      
      memset(&app_CB.gDataBuffer, 0x0, sizeof(app_CB.gDataBuffer));
      uint8_t triggeredScanTrials = 0;
      bool scanComplete = false;
      while((triggeredScanTrials++ < 30) && (!scanComplete))
      {
         /*if (sl_WlanGetNetworkList(scanParams.index, scanParams.numOfentries, &app_CB.gDataBuffer.netEntries[index]) == 0)
         {
            scanComplete = true;
         }*/
         
         if (sl_WlanGetExtNetworkList(scanParams.index, scanParams.numOfentries, &app_CB.gDataBuffer.extNetEntries[scanParams.index]) > 0)
         {
            scanComplete = true;
         }
             
         // We wait for one second for the NWP to complete
         // the initiated scan and collect results
         usleep(1000000 - 1);
      }
      

      
      for (uint8_t i = 0; i < 30; i++)
      {
         // find the desired access point in the Access Point list returned from the scan.
         if ((app_CB.gDataBuffer.extNetEntries[i].SsidLen != 0) &&
             (0 == strncmp((const char*)TARGET_SSID, (const char*)app_CB.gDataBuffer.extNetEntries[i].Ssid, sizeof(TARGET_SSID))))
         {
            // connect to the desired access point.
            SlWlanSecParams_t connectionSecurity;
            connectionSecurity.Type = SL_WLAN_SEC_TYPE_WPA_WPA2;
            connectionSecurity.Key = SEC_KEY;
            connectionSecurity.KeyLen = sizeof(SEC_KEY);
            if (0 == sl_WlanConnect((signed char const*)app_CB.gDataBuffer.extNetEntries[i].Ssid, 
                                    app_CB.gDataBuffer.extNetEntries[i].SsidLen, 
                                    app_CB.gDataBuffer.extNetEntries[i].Bssid, 
                                    &connectionSecurity, 
                                    NULL))
            {
               
               ConnectCmd_t ConnectParams;
               memset(&ConnectParams, 0, sizeof(ConnectParams));
               
               /* Wait for connection events:
                * In order to verify that connection was successful,
                * we pend on two incoming events: Connected and Ip acquired.
                * The semaphores below are pend by this (Main) context.
                * They will be signaled once an asynchronous event
                * Indicating that the NWP has connected and acquired IP address is raised.
                * For further information, see this application read me file.
                */
                if(!IS_CONNECTED(app_CB.Status))
                {
                    if(sem_wait_timeout(&app_CB.CON_CB.connectEventSyncObj, WLAN_EVENT_TOUT) == TIMEOUT_SEM)
                    {
                        UART_PRINT("\n\r[wlanconnect] : Failed to connect to AP: %s\n\r", ConnectParams.ssid);
                        //FreeConnectCmd(&ConnectParams);
                        //return(-1);
                    }
                }

                if(!IS_IP_ACQUIRED(app_CB.Status))
                {
                    if(sem_wait_timeout(&app_CB.CON_CB.ip4acquireEventSyncObj, WLAN_EVENT_TOUT) == TIMEOUT_SEM)
                    {
                        // In next step try to get IPv6, may be router/AP doesn't support IPv4
                        UART_PRINT("\n\r[wlanconnect] : Failed to acquire IPv4 address.\n\r");
                    }
                }

                if(!IS_IPV6G_ACQUIRED(app_CB.Status))
                {
                    if(sem_wait_timeout(&app_CB.CON_CB.ip6acquireEventSyncObj, WLAN_EVENT_TOUT) == TIMEOUT_SEM)
                    {
                        UART_PRINT("\n\r[wlanconnect] : Failed to acquire IPv6 address.\n\r");
                    }
                }

                if(!IS_IPV6G_ACQUIRED(app_CB.Status) &&
                   !IS_IPV6L_ACQUIRED(app_CB.Status) && !IS_IP_ACQUIRED(app_CB.Status))
                {
                    UART_PRINT("\n\r[line:%d, error:%d] %s\n\r", __LINE__, -1, "Network Error");
                }

                //FreeConnectCmd(&ConnectParams);
       
       
               // light up LED 0 to show connected
               bsp_board_led_on(BSP_BOARD_LED_0);
               
               while(1)
               {
                  // TODO: ping Polka Palace every 10ish seconds
               
                  // flash LED 1 indicating the ping went out.
                  bsp_board_led_invert(BSP_BOARD_LED_1);
                  usleep(100000 - 1);
                  taskYIELD();
               }
            }
         }
      }   
   
   
      // setup, scanning, or connection failed ..
      // pause for a beat, then try again.
      usleep(100000 - 1);
   }
}

/**
 *@}
 **/
