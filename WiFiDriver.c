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

#include "MQTTDriver.h"

#define TASK_STACK_SIZE             (2048)
#define SPAWN_TASK_PRIORITY         (9)
#define WLAN_EVENT_TOUT             (6000)
#define TIMEOUT_SEM                 (-1)



extern void UART_PRINT(char* label, ...);

TaskHandle_t gSpawn_thread = NULL;

static int32_t WiFiDriver_SimpleLink_Init()
{
   WiFi_init();
   
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

static bool WiFiDriver_SpawnThread()
{
   bool success = true;
   pthread_attr_t pAttrs_spawn;
   struct sched_param priParam;
   struct timespec ts = {0};
   
   // initialize the realtime clock
   clock_settime(CLOCK_REALTIME, &ts);

   // Create the sl_Task internal spawn thread
   pthread_attr_init(&pAttrs_spawn);
   priParam.sched_priority = SPAWN_TASK_PRIORITY;
   uint32_t RetVal = pthread_attr_setschedparam(&pAttrs_spawn, &priParam);
   RetVal |= pthread_attr_setstacksize(&pAttrs_spawn, TASK_STACK_SIZE);

   /* The SimpleLink host driver architecture mandate spawn
      thread to be created prior to calling Sl_start (turning the NWP on). */
   /* The purpose of this thread is to handle
      asynchronous events sent from the NWP.
    * Every event is classified and later handled
      by the Host driver event handlers. */
   
   if(pthread_create(&gSpawn_thread, &pAttrs_spawn, sl_Task, NULL) < 0)
   {
      UART_PRINT("Network Terminal - Unable to create spawn thread \n");
      success = false;
   }
   
   // Yield control of this tasks until the synchronization Task Control block 
   // is setup by the Spawn task.
   taskYIELD();
   
   return success;
}

bool WiFiDriver_StartSimpleLink()
{
   if (WiFiDriver_SimpleLink_Init() < 0)
   {
      return false;
   }
   
   if (!WiFiDriver_SpawnThread())
   {
      return false;
   }
   
   return true;
}

bool WiFiDriver_Init()
{
   // TODO: start timer to catch if init hangs so we can retry
   
   // ensure the CC3135 is stopped before re-initializing.
   //sl_Stop(0);
   
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
     UART_PRINT("Network Terminal - Couldn't configure Network Processor - %d\n",
                isSuccess);
     return false;
   }

   // Turn NWP on
   isSuccess = sl_Start(NULL, NULL, NULL);
   if(isSuccess < 0)
   {
     // Handle Error
     UART_PRINT("sl_start failed - %d\n", isSuccess);
     return false;
   }
   
   // sl_Start returns on success the role that device started on
   //app_CB.Role = isSuccess;
   
   SlDeviceVersion_t ver;
   uint16_t pConfigLen = sizeof(ver);
   uint8_t pConfigOpt = SL_DEVICE_GENERAL_VERSION;
   if (sl_DeviceGet(SL_DEVICE_GENERAL, &pConfigOpt, &pConfigLen, (uint8_t*)(&ver)) < 0)
   {
      return false;
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
      return false;
   }
   
   // setup to scan on both 2.4Ghz and 5Ghz every 10 Seconds
   

   // TODO: Make sure no connection policy is not set
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
      return false;
   }

   // Set scan parameters for 2.4Gz
   if (sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
                  SL_WLAN_GENERAL_PARAM_OPT_SCAN_PARAMS,
                  sizeof(SetPolicyParams.ScanParamConfig),
                  (uint8_t *)(&SetPolicyParams.ScanParamConfig)) < 0)
   {
      return false;
   }
   
   // Set scan parameters for 5Ghz
   uint8_t mode = 0x01;
   uint16_t option = SL_WLAN_GENERAL_PARAM_OPT_ENABLE_5G;
   if (sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
                  option,
                  sizeof(mode),
                  (uint8_t*)&mode) < 0)
   {
      return false;
   }
   
   
   SlWlanScanParam5GCommand_t ScanParamConfig5G;
   option = SL_WLAN_GENERAL_PARAM_OPT_SCAN_PARAMS_5G;
   
   // 5.0G channels bits order: 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132,
   //                          136, 140, 144, 149, 153, 157, 161, 165, 169, 184, 188, 192, 196 
        
   ScanParamConfig5G.ChannelsMask = 0x3FFFFFFF; // Select ChannelsMask for channels 36, 40, 44, 48 
   ScanParamConfig5G.RssiThreshold = -70;
   if (sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, option, sizeof(SlWlanScanParam5GCommand_t), (_u8 *)&ScanParamConfig5G) < 0)
   {
      return false;
   }
   
   
   uint8_t enabled5Ghz = 0;
   option = SL_WLAN_GENERAL_PARAM_OPT_ENABLE_5G;
   uint16_t optionLen = sizeof(SlWlanScanParamCommand_t);
   
   sl_WlanGet(SL_WLAN_CFG_GENERAL_PARAM_ID, &option, &optionLen, (uint8_t*)&enabled5Ghz);
   
   
   /*uint8_t macAddressVal[SL_MAC_ADDR_LEN];
   uint8_t macAddressLen = SL_MAC_ADDR_LEN;
   sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL,&macAddressLen,(_u8 *)macAddressVal);
   */
   uint8_t macAddressVal[SL_MAC_ADDR_LEN];
   uint16_t macAddressLen = SL_MAC_ADDR_LEN;
   uint16_t ConfigOpt = 0;
   sl_NetCfgGet(SL_NETCFG_MAC_ADDRESS_GET,&ConfigOpt,&macAddressLen,(_u8 *)macAddressVal);
   
   // initialize the message queue telemetry transport pub/sub system.
   //MQTT_Init(macAddressVal, macAddressLen);
          
   return true;
}

void WiFiDriver_ScanStart(uint8_t scanSeconds, bool scanHidden)
{
   // Set Scan policy - this API starts the scans
   uint8_t policyOpt = SL_WLAN_SCAN_POLICY(1, scanHidden);

   if (sl_WlanPolicySet(SL_WLAN_POLICY_SCAN, policyOpt,
                        (uint8_t*)(&scanSeconds),
                        sizeof(scanSeconds)) < 0)
   {
      return;
   }
}

void WiFiDriver_ScanStop()
{
   uint8_t policyOpt = SL_WLAN_SCAN_POLICY(0, 0);
   uint8_t scanSeconds = 0;

   if (sl_WlanPolicySet(SL_WLAN_POLICY_SCAN, 
                        policyOpt,
                        (uint8_t*)(&scanSeconds),
                        sizeof(scanSeconds)) < 0)
   {
      return;
   }
}

int16_t WiFiDriver_CollectScanResults(uint8_t numScanEntries)
{
   // Scan for available access points
   ScanCmd_t scanParams;
   scanParams.extendedRes = false;
   scanParams.index = 0;
   scanParams.numOfentries = numScanEntries;
   
   memset(&app_CB.gDataBuffer, 0x0, sizeof(app_CB.gDataBuffer));
   
   int16_t numAPs = sl_WlanGetExtNetworkList(scanParams.index, scanParams.numOfentries, &app_CB.gDataBuffer.extNetEntries[scanParams.index]);
   
   return numAPs;
}

bool WiFiDriver_Connect(const uint8_t* ssid, const uint8_t* key)
{
   bool connected = false;
   
   for (uint8_t i = 0; i < 30; i++)
   {
      // find the desired access point in the Access Point list returned from the scan.
      if ((app_CB.gDataBuffer.extNetEntries[i].SsidLen != 0) &&
          (0 == strncmp((const char*)ssid, (const char*)app_CB.gDataBuffer.extNetEntries[i].Ssid, sizeof(ssid))))
      {
         // connect to the desired access point.
         SlWlanSecParams_t connectionSecurity;
         connectionSecurity.Type = SL_WLAN_SEC_TYPE_WPA_WPA2;
         connectionSecurity.Key = (signed char*)key;
         connectionSecurity.KeyLen = sizeof(key);
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
                  //break;
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
             
            connected = true;
            break;
         }
      }
   }
   
   return connected;
}

void WiFiDriver_Send(uint8_t* buffer)
{
   
}


