#include <stdlib.h>
#include <stdint.h>

#include <ti/drivers/net/wifi/simplelink.h>

#include <pthread.h>
#include <mqueue.h>
#include <semaphore.h>

#include "simplelink_structures.h"

void UART_PRINT(char* label, ...)
{
}

/*!
    \brief          SimpleLinkWlanEventHandler

    This handler gets called whenever a WLAN event is reported
    by the host driver / NWP. Here user can implement he's own logic
    for any of these events. This handler is used by 'network_terminal'
    application to show case the following scenarios:

    1. Handling connection / Disconnection.
    2. Handling Addition of station / removal.
    3. RX filter match handler.
    4. P2P connection establishment.

    \param         pWlanEvent       -   pointer to Wlan event data.

    \return        void

    \note          For more information, please refer to: user.h in the porting
                   folder of the host driver and the
                   CC31xx/CC32xx NWP programmer's guide (SWRU455)

    \sa            cmdWlanConnectCallback, cmdEnableFilterCallback,
                   cmdWlanDisconnectCallback, cmdP2PModecallback.

 */
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent)
{
    if(!pWlanEvent)
    {
        return;
    }

    switch(pWlanEvent->Id)
    {
    case SL_WLAN_EVENT_CONNECT:
    {
        SET_STATUS_BIT(app_CB.Status, STATUS_BIT_CONNECTION);

        /* Copy new connection SSID and BSSID to global parameters */
        memcpy(app_CB.CON_CB.ConnectionSSID, pWlanEvent->Data.Connect.SsidName,
               pWlanEvent->Data.Connect.SsidLen);
        memcpy(app_CB.CON_CB.ConnectionBSSID, pWlanEvent->Data.Connect.Bssid,
               SL_WLAN_BSSID_LENGTH);
        UART_PRINT(
            "\n\r[WLAN EVENT] STA Connected to the AP: %s , "
            "BSSID: %x:%x:%x:%x:%x:%x\n\r",
            app_CB.CON_CB.ConnectionSSID,
            app_CB.CON_CB.ConnectionBSSID[0],
            app_CB.CON_CB.ConnectionBSSID[1],
            app_CB.CON_CB.ConnectionBSSID[2],
            app_CB.CON_CB.ConnectionBSSID[3],
            app_CB.CON_CB.ConnectionBSSID[4],
            app_CB.CON_CB.ConnectionBSSID[5]);

        sem_post(&app_CB.CON_CB.connectEventSyncObj);
    }
    break;

    case SL_WLAN_EVENT_DISCONNECT:
    {
        SlWlanEventDisconnect_t  *pEventData = NULL;

        CLR_STATUS_BIT(app_CB.Status, STATUS_BIT_CONNECTION);
        CLR_STATUS_BIT(app_CB.Status, STATUS_BIT_IP_ACQUIRED);
        CLR_STATUS_BIT(app_CB.Status, STATUS_BIT_IPV6_ACQUIRED);

        /* If ping operation is running, release it. */
        if(IS_PING_RUNNING(app_CB.Status))
        {
            sem_post(&app_CB.CON_CB.eventCompletedSyncObj);
            UART_PRINT("\n\rPing failed, since device is no longer connected.\n\r");
        }

        pEventData = &pWlanEvent->Data.Disconnect;

        /* If the user has initiated 'Disconnect' request,
           'reason_code' is SL_WLAN_DISCONNECT_USER_INITIATED */
        if(SL_WLAN_DISCONNECT_USER_INITIATED == pEventData->ReasonCode)
        {
            UART_PRINT(
                "\n\r[WLAN EVENT] Device disconnected from the AP: %s,\n\r"
                "BSSID: %x:%x:%x:%x:%x:%x on application's request \n\r",
                app_CB.CON_CB.ConnectionSSID,
                app_CB.CON_CB.ConnectionBSSID[0],
                app_CB.CON_CB.ConnectionBSSID[1],
                app_CB.CON_CB.ConnectionBSSID[2],
                app_CB.CON_CB.ConnectionBSSID[3],
                app_CB.CON_CB.ConnectionBSSID[4],
                app_CB.CON_CB.ConnectionBSSID[5]);
        }
        else
        {
            UART_PRINT(
                "\n\r[WLAN ERROR] Device disconnected from the AP: %s,\n\r"
                "BSSID: %x:%x:%x:%x:%x:%x\n\r",
                app_CB.CON_CB.ConnectionSSID,
                app_CB.CON_CB.ConnectionBSSID[0],
                app_CB.CON_CB.ConnectionBSSID[1],
                app_CB.CON_CB.ConnectionBSSID[2],
                app_CB.CON_CB.ConnectionBSSID[3],
                app_CB.CON_CB.ConnectionBSSID[4],
                app_CB.CON_CB.ConnectionBSSID[5]);
        }
        memset(&(app_CB.CON_CB.ConnectionSSID), 0x0,
               sizeof(app_CB.CON_CB.ConnectionSSID));
        memset(&(app_CB.CON_CB.ConnectionBSSID), 0x0,
               sizeof(app_CB.CON_CB.ConnectionBSSID));
    }
    break;

    case SL_WLAN_EVENT_PROVISIONING_STATUS:
    {
        /* Do nothing, this suppress provisioning event is because
            simplelink is configured to default state. */
    }
    break;

    case SL_WLAN_EVENT_STA_ADDED:
    {
        memcpy(&(app_CB.CON_CB.ConnectionBSSID), pWlanEvent->Data.STAAdded.Mac,
               SL_WLAN_BSSID_LENGTH);
        UART_PRINT(
            "\n\r[WLAN EVENT] STA was added to AP:"
            " BSSID: %x:%x:%x:%x:%x:%x\n\r",
            app_CB.CON_CB.ConnectionBSSID[0],
            app_CB.CON_CB.ConnectionBSSID[1],
            app_CB.CON_CB.ConnectionBSSID[2],
            app_CB.CON_CB.ConnectionBSSID[3],
            app_CB.CON_CB.ConnectionBSSID[4],
            app_CB.CON_CB.ConnectionBSSID[5]);
    }
    break;

    case SL_WLAN_EVENT_STA_REMOVED:
    {
        memcpy(&(app_CB.CON_CB.ConnectionBSSID), pWlanEvent->Data.STAAdded.Mac,
               SL_WLAN_BSSID_LENGTH);
        UART_PRINT(
            "\n\r[WLAN EVENT] STA was removed "
            "from AP: BSSID: %x:%x:%x:%x:%x:%x\n\r",
            app_CB.CON_CB.ConnectionBSSID[0],
            app_CB.CON_CB.ConnectionBSSID[1],
            app_CB.CON_CB.ConnectionBSSID[2],
            app_CB.CON_CB.ConnectionBSSID[3],
            app_CB.CON_CB.ConnectionBSSID[4],
            app_CB.CON_CB.ConnectionBSSID[5]);

        memset(&(app_CB.CON_CB.ConnectionBSSID), 0x0,
               sizeof(app_CB.CON_CB.ConnectionBSSID));
    }
    break;

    case SL_WLAN_EVENT_RXFILTER:
    {
        SlWlanEventRxFilterInfo_t  *triggred_filter = NULL;

        triggred_filter = &(pWlanEvent->Data.RxFilterInfo);

        UART_PRINT(
            "\n\r[WLAN EVENT] Rx filter match triggered. Set filters in"
            "filter bitmap :0x%x.\n\r",
            triggred_filter->UserActionIdBitmap);

        /*
         *  User can write he's / her's rx filter match handler here.
         *  Be advised, you can use the 'triggred_filter'
         *    structure info to determine which filter
         *  has received a match.
         * (Bit X is set if user action id X was
         *  passed to a filter that matched a packet.)
         */
    }
    break;

    case SL_WLAN_EVENT_P2P_DEVFOUND:
    {
        UART_PRINT("\n\r[WLAN EVENT] P2P Remote device found\n\r");
        sem_post(&(app_CB.P2P_CB.DeviceFound));
    }
    break;

    case SL_WLAN_EVENT_P2P_REQUEST:
    {
        UART_PRINT("\n\r[WLAN EVENT] P2P Negotiation request received\n\r");

        /* This information is needed to create connection*/
        memset(&(app_CB.P2P_CB.p2pPeerDeviceName), '\0',
               sizeof(app_CB.P2P_CB.p2pPeerDeviceName));
        memcpy(&app_CB.P2P_CB.p2pPeerDeviceName,
               pWlanEvent->Data.P2PRequest.GoDeviceName,
               pWlanEvent->Data.P2PRequest.GoDeviceNameLen);

        sem_post(&app_CB.P2P_CB.RcvNegReq);
    }
    break;

    case SL_WLAN_EVENT_P2P_CONNECT:
    {
        UART_PRINT(
            "n\r[WLAN EVENT] P2P connection was successfully completed as "
            "CLIENT\n\r");
        UART_PRINT("n\rBSSID is %02x:%02x:%02x:%02x:%02x:%02x\n\r",
                   pWlanEvent->Data.STAAdded.Mac[0],
                   pWlanEvent->Data.STAAdded.Mac[1],
                   pWlanEvent->Data.STAAdded.Mac[2],
                   pWlanEvent->Data.STAAdded.Mac[3],
                   pWlanEvent->Data.STAAdded.Mac[4],
                   pWlanEvent->Data.STAAdded.Mac[5]);

        sem_post(&app_CB.P2P_CB.RcvConReq);
    }
    break;

    case SL_WLAN_EVENT_P2P_CLIENT_ADDED:
    {
        UART_PRINT(
            "n\r[WLAN EVENT] P2P connection was successfully completed "
            "as GO\n\r");
        UART_PRINT("n\rBSSID is %02x:%02x:%02x:%02x:%02x:%02x\n\r",
                   pWlanEvent->Data.P2PClientAdded.Mac[0],
                   pWlanEvent->Data.P2PClientAdded.Mac[1],
                   pWlanEvent->Data.P2PClientAdded.Mac[2],
                   pWlanEvent->Data.P2PClientAdded.Mac[3],
                   pWlanEvent->Data.P2PClientAdded.Mac[4],
                   pWlanEvent->Data.P2PClientAdded.Mac[5]);

        sem_post(&app_CB.P2P_CB.RcvConReq);
    }
    break;

    case SL_WLAN_EVENT_P2P_DISCONNECT:
    {
        UART_PRINT("\n\r[WLAN EVENT] STA disconnected from device.\n\r");
        CLR_STATUS_BIT(app_CB.Status,STATUS_BIT_CONNECTION);
    }
    break;

    case SL_WLAN_EVENT_LINK_QUALITY_TRIGGER:
    {
        int Rssi = (int)((signed char)pWlanEvent->Data.LinkQualityTrigger.Data);
        UART_PRINT((char*)"\n\r[WLAN EVENT] Link Quality Async Event,TriggerId"
                   " =%d Rssi detected =%d.\n\r ",
                   pWlanEvent->Data.LinkQualityTrigger.TriggerId, Rssi);
        if(pWlanEvent->Data.LinkQualityTrigger.TriggerId == 0)
        {
            UART_PRINT((char*)"\n\rThis trigger is host app trigger.\n\r ");
        }
        else
        {
            UART_PRINT(
                (char*)"\n\r[SoftRoaming] One-shot background scan "
                "is started, search for AP with RSSI upper than %d .\n\r",
                Rssi);
        }
    }
    break;
    default:
    {
        UART_PRINT("\n\r[WLAN EVENT] Unexpected event [0x%x]\n\r",
                   pWlanEvent->Id);
    }
    break;
    }
    //UART_PRINT(cmdPromptStr);
}

/*!
    \brief          SimpleLinkNetAppEventHandler

    This handler gets called whenever a Netapp event is reported
    by the host driver / NWP. Here user can implement he's own logic
    for any of these events. This handler is used by 'network_terminal'
    application to show case the following scenarios:

    1. Handling IPv4 / IPv6 IP address acquisition.
    2. Handling IPv4 / IPv6 IP address Dropping.

    \param          pNetAppEvent     -   pointer to Netapp event data.

    \return         void

    \note           For more information, please refer to:
                    user.h in the porting
                    folder of the host driver and the
                    CC31xx/CC32xx NWP programmer's
                    guide (SWRU455).

 */
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{
    if(!pNetAppEvent)
    {
        return;
    }

    switch(pNetAppEvent->Id)
    {
    case SL_NETAPP_EVENT_IPV4_ACQUIRED:
    {
        SlIpV4AcquiredAsync_t *pEventData = NULL;

        SET_STATUS_BIT(app_CB.Status, STATUS_BIT_IP_ACQUIRED);

        /* Ip Acquired Event Data */
        pEventData = &pNetAppEvent->Data.IpAcquiredV4;
        app_CB.CON_CB.IpAddr = pEventData->Ip;

        /* Gateway IP address */
        app_CB.CON_CB.GatewayIP = pEventData->Gateway;

        UART_PRINT("\n\r[NETAPP EVENT] IP set to: IPv4=%d.%d.%d.%d , "
                   "Gateway=%d.%d.%d.%d\n\r",

                   SL_IPV4_BYTE(app_CB.CON_CB.IpAddr,3),
                   SL_IPV4_BYTE(app_CB.CON_CB.IpAddr,2),
                   SL_IPV4_BYTE(app_CB.CON_CB.IpAddr,1),
                   SL_IPV4_BYTE(app_CB.CON_CB.IpAddr,0),

                   SL_IPV4_BYTE(app_CB.CON_CB.GatewayIP,3),
                   SL_IPV4_BYTE(app_CB.CON_CB.GatewayIP,2),
                   SL_IPV4_BYTE(app_CB.CON_CB.GatewayIP,1),
                   SL_IPV4_BYTE(app_CB.CON_CB.GatewayIP,0));

        sem_post(&(app_CB.CON_CB.ip4acquireEventSyncObj));
    }
    break;

    case SL_NETAPP_EVENT_IPV6_ACQUIRED:
    {
        uint32_t i = 0;

        SET_STATUS_BIT(app_CB.Status, STATUS_BIT_IPV6_ACQUIRED);

        for(i = 0; i < 4; i++)
        {
            app_CB.CON_CB.Ipv6Addr[i] =
                pNetAppEvent->Data.IpAcquiredV6.Ip[i];
        }

        UART_PRINT("\n\r[NETAPP EVENT] IP Acquired: IPv6=");

        for(i = 0; i < 3; i++)
        {
            UART_PRINT("%04x:%04x:",
                       ((app_CB.CON_CB.Ipv6Addr[i] >> 16) & 0xffff),
                       app_CB.CON_CB.Ipv6Addr[i] & 0xffff);
        }

        UART_PRINT("%04x:%04x", ((app_CB.CON_CB.Ipv6Addr[3] >> 16) & 0xffff),
                   app_CB.CON_CB.Ipv6Addr[3] & 0xffff);
        //UART_PRINT(lineBreak);
        sem_post(&app_CB.CON_CB.ip6acquireEventSyncObj);
    }
    break;

    case SL_NETAPP_EVENT_DHCPV4_LEASED:
    {
        SET_STATUS_BIT(app_CB.Status, STATUS_BIT_IP_LEASED);
        SET_STATUS_BIT(app_CB.Status, STATUS_BIT_IP_ACQUIRED);

        app_CB.CON_CB.StaIp = pNetAppEvent->Data.IpLeased.IpAddress;
        UART_PRINT(
            "\n\r[NETAPP EVENT] IP Leased to Client: IP=%d.%d.%d.%d \n\r",
            SL_IPV4_BYTE(app_CB.CON_CB.StaIp,
                         3), SL_IPV4_BYTE(app_CB.CON_CB.StaIp,2),
            SL_IPV4_BYTE(app_CB.CON_CB.StaIp,
                         1), SL_IPV4_BYTE(app_CB.CON_CB.StaIp,0));

        sem_post(&(app_CB.CON_CB.ip4acquireEventSyncObj));
    }
    break;

    case SL_NETAPP_EVENT_DHCPV4_RELEASED:
    {
        UART_PRINT("\n\r[NETAPP EVENT] IP is released.\n\r");
    }
    break;

    default:
    {
        UART_PRINT("\n\r[NETAPP EVENT] Unexpected event [0x%x] \n\r",
                   pNetAppEvent->Id);
    }
    break;
    }
    //UART_PRINT(cmdPromptStr);
}

/*!
    \brief          SimpleLinkHttpServerEventHandler

    This handler gets called whenever a HTTP event is reported
    by the NWP internal HTTP server.

    \param          pHttpEvent       -   pointer to http event data.

    \param          pHttpEvent       -   pointer to http response.

    \return         void

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC31xx/CC32xx NWP
                    programmer's guide (SWRU455).

 */
void SimpleLinkHttpServerEventHandler(SlNetAppHttpServerEvent_t *pHttpEvent,
                                      SlNetAppHttpServerResponse_t *
                                      pHttpResponse)
{
    /* Unused in this application */
}

/*!
    \brief          SimpleLinkGeneralEventHandler

    This handler gets called whenever a general error is reported
    by the NWP / Host driver. Since these errors are not fatal,
    application can handle them.

    \param          pDevEvent    -   pointer to device error event.

    \return         void

    \note           For more information, please refer to:
                    user.h in the porting
                    folder of the host driver and the
                    CC31xx/CC32xx NWP programmer's
                    guide (SWRU455).

 */
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
    if(!pDevEvent)
    {
        return;
    }
    /*
       Most of the general errors are not FATAL are are to be handled
       appropriately by the application
     */
    UART_PRINT("\n\r[GENERAL EVENT] - ID=[%d] Sender=[%d]\n\n",
               pDevEvent->Data.Error.Code,
               pDevEvent->Data.Error.Source);
}

/*!
    \brief          SimpleLinkSockEventHandler

    This handler gets called whenever a socket event is reported
    by the NWP / Host driver.

    \param          SlSockEvent_t    -   pointer to socket event data.

    \return         void

    \note           For more information, please refer to:
                    user.h in the porting
                    folder of the host driver and the
                    CC31xx/CC32xx NWP programmer's
                    guide (SWRU455).

 */
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
    /* Unused in this application */
}

/*!
    \brief          SimpleLinkFatalErrorEventHandler

    This handler gets called whenever a socket event is reported
    by the NWP / Host driver. After this routine is called, the user's
    application must restart the device in order to recover.

    \param          slFatalErrorEvent    -   pointer to fatal error event.

    \return         void

    \note           For more information, please refer to:
                    user.h in the porting
                    folder of the host driver and the
                    CC31xx/CC32xx NWP programmer's
                    guide (SWRU455).

 */
void SimpleLinkFatalErrorEventHandler(SlDeviceFatal_t *slFatalErrorEvent)
{
    switch(slFatalErrorEvent->Id)
    {
    case SL_DEVICE_EVENT_FATAL_DEVICE_ABORT:
    {
        UART_PRINT("\n\r[ERROR] - FATAL ERROR: Abort NWP event detected: "
                   "AbortType=%d, AbortData=0x%x\n\r",
                   slFatalErrorEvent->Data.DeviceAssert.Code,
                   slFatalErrorEvent->Data.DeviceAssert.Value);
    }
    break;

    case SL_DEVICE_EVENT_FATAL_DRIVER_ABORT:
    {
        UART_PRINT("\n\r[ERROR] - FATAL ERROR: Driver Abort detected. \n\r");
    }
    break;

    case SL_DEVICE_EVENT_FATAL_NO_CMD_ACK:
    {
        UART_PRINT("\n\r[ERROR] - FATAL ERROR: No Cmd Ack detected "
                   "[cmd opcode = 0x%x] \n\r",
                   slFatalErrorEvent->Data.NoCmdAck.Code);
    }
    break;

    case SL_DEVICE_EVENT_FATAL_SYNC_LOSS:
    {
        UART_PRINT("\n\r[ERROR] - FATAL ERROR: Sync loss detected n\r");
    }
    break;

    case SL_DEVICE_EVENT_FATAL_CMD_TIMEOUT:
    {
        UART_PRINT("\n\r[ERROR] - FATAL ERROR: Async event timeout detected "
                   "[event opcode =0x%x]  \n\r",
                   slFatalErrorEvent->Data.CmdTimeout.Code);
    }
    break;

    default:
        UART_PRINT("\n\r[ERROR] - FATAL ERROR:"
                   " Unspecified error detected \n\r");
        break;
    }
}

/*!
    \brief          SimpleLinkNetAppRequestEventHandler

    This handler gets called whenever a NetApp event is reported
    by the NWP / Host driver. User can write he's logic to handle
    the event here.

    \param          pNetAppRequest     -   Pointer to NetApp request structure.

    \param          pNetAppResponse    -   Pointer to NetApp request Response.

    \note           For more information, please refer to:
                    user.h in the porting
                    folder of the host driver and the
                    CC31xx/CC32xx NWP programmer's
                    guide (SWRU455).

    \return         void

 */
void SimpleLinkNetAppRequestEventHandler(SlNetAppRequest_t *pNetAppRequest,
                                         SlNetAppResponse_t *pNetAppResponse)
{
    /* Unused in this application */
}

/*!
    \brief          SimpleLinkNetAppRequestMemFreeEventHandler

    This handler gets called whenever the NWP is done handling with
    the buffer used in a NetApp request. This allows the use of
    dynamic memory with these requests.

    \param          pNetAppRequest     -   Pointer to NetApp request structure.

    \param          pNetAppResponse    -   Pointer to NetApp request Response.

    \note           For more information, please refer to:
                    user.h in the porting
                    folder of the host driver and the
                    CC31xx/CC32xx NWP programmer's
                    guide (SWRU455).

    \return         void

 */
void SimpleLinkNetAppRequestMemFreeEventHandler(uint8_t *buffer)
{
    /* Unused in this application */
}

/**
 *  Set a timeout while waiting on a semaphore for POSIX based OS API.
 *
 *  @param          sem     -   points to a semaphore object.
 *  @param          Timeout -   Timeout in mSec value.
 *
 *  @return         Upon successful completion,
 *                  the function shall return 0.
 *                  In case of failure,
 *                  this function would return negative value.
 *
 */
int32_t sem_wait_timeout(sem_t *sem, uint32_t Timeout)
{
    struct timespec abstime;
    abstime.tv_nsec = 0;
    abstime.tv_sec = 0;

    // Since POSIX timeout are relative and not absolute,
    // take the current timestamp.
    clock_gettime(CLOCK_REALTIME, &abstime);
    if(abstime.tv_nsec < 0)
    {
        abstime.tv_sec = Timeout;
        return (sem_timedwait(sem, &abstime));
    }

    // Add the amount of time to wait
    abstime.tv_sec += Timeout / 1000;
    abstime.tv_nsec += (Timeout % 1000) * 1000000;

    abstime.tv_sec += (abstime.tv_nsec / 1000000000);
    abstime.tv_nsec = abstime.tv_nsec % 1000000000;

    // Call the semaphore wait API
    return(sem_timedwait(sem, &abstime));
}

/*
 *  ======== SimpleLinkWifi_config ========
 */
const SlWifiCC32XXConfig_t SimpleLinkWifiCC32XX_config = 
{
    .Mode = ROLE_STA,
    .Ipv4Mode = SL_NETCFG_IPV4_STA_ADDR_MODE,
    .ConnectionPolicy = SL_WLAN_CONNECTION_POLICY(1,0,0,0),
    .PMPolicy = SL_WLAN_ALWAYS_ON_POLICY, //SL_WLAN_NORMAL_POLICY,
    .MaxSleepTimeMS = 0,
    .ScanPolicy = SL_WLAN_SCAN_POLICY(0,0),
    .ScanIntervalInSeconds = 0,
    .Ipv4Config = SL_NETCFG_ADDR_DHCP,
    .Ipv4 = 0,
    .IpMask = 0,
    .IpGateway = 0,
    .IpDnsServer = 0,
    .ProvisioningStop = 0,      // 1
    .DeleteAllProfile = 0
};