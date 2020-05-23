
#ifndef SIMPLELINK_STRUCTURES_H
#define SIMPLELINK_STRUCTURES_H

#define CMD_BUFFER_LEN          (256)
#define MAX_BUF_SIZE            (1400)
#define MAX_TEXT_PAD_SIZE       (256)
#define WLAN_SCAN_COUNT         (30)

typedef union
{
    uint8_t nwData[MAX_BUF_SIZE];
    int8_t textPad[MAX_TEXT_PAD_SIZE];
    SlWlanNetworkEntry_t netEntries[WLAN_SCAN_COUNT];
    SlWlanExtNetworkEntry_t extNetEntries[WLAN_SCAN_COUNT];
}gDataBuffer_t;

typedef struct cmdAction
{
    char        *cmd;
    int32_t (*callback)(void *);
    int32_t (*printusagecallback)(void *);
}cmdAction_t;

typedef struct p2p_ControlBlock_t
{
    uint8_t p2pPeerDeviceName[32];
    uint8_t p2pDeviceName[32];
    sem_t DeviceFound;
    sem_t RcvNegReq;
    sem_t RcvConReq;
    SlWlanSecParams_t P2PsecParams;
}p2p_CB;

typedef struct connectionControlBlock_t
{
    sem_t connectEventSyncObj;
    sem_t ip4acquireEventSyncObj;
    sem_t ip6acquireEventSyncObj;
    sem_t eventCompletedSyncObj;
    uint32_t GatewayIP;
    uint8_t ConnectionSSID[SL_WLAN_SSID_MAX_LENGTH + 1];
    uint8_t ConnectionBSSID[SL_WLAN_BSSID_LENGTH];
    uint32_t DestinationIp;
    uint32_t IpAddr;
    uint32_t StaIp;
    uint32_t Ipv6Addr[4];
}connection_CB;

typedef struct appControlBlock_t
{
    /* Status Variables */
    /* This bit-wise status variable shows the state of the NWP */
    uint32_t Status;
    /* This field keeps the device's role (STA, P2P or AP) */
    uint32_t Role;
    /* This flag lets the application to exit */
    uint32_t Exit;
    /* Sets the number of Ping attempts to send */
    uint32_t PingAttempts;
    /* Data & Network entry Union */
    gDataBuffer_t gDataBuffer;

    /* P2P mode CB */
    p2p_CB P2P_CB;

    /* STA/AP mode CB */
    connection_CB CON_CB;

    /* Cmd Prompt buffer */
    uint8_t CmdBuffer[CMD_BUFFER_LEN];

    /* WoWLAN semaphore */
    sem_t WowlanSleepSem;
}appControlBlock;

extern appControlBlock app_CB;

typedef enum
{
/* This bit is set: Network Processor is powered up */
    STATUS_BIT_NWP_INIT = 0,
/* This bit is set: the device is connected to the AP or
   client is connected to device (AP) */
    STATUS_BIT_CONNECTION,
/* This bit is set: the device has leased IP to  any connected client */
    STATUS_BIT_IP_LEASED,
/* This bit is set: the device has acquired an IP */
    STATUS_BIT_IP_ACQUIRED,
/* If this bit is set: the device (P2P mode)
   found any p2p-device in scan */
    STATUS_BIT_P2P_DEV_FOUND,
/* If this bit is set: the device (P2P mode)
   found any p2p-negotiation request */
    STATUS_BIT_P2P_REQ_RECEIVED,
/* If this bit is set: the device(P2P mode)
   connection to client(or reverse way) is failed */
    STATUS_BIT_CONNECTION_FAILED,
/* This bit is set: device is undergoing ping operation */
    STATUS_BIT_PING_STARTED,
/* This bit is set: Scan is running is background */
    STATUS_BIT_SCAN_RUNNING,
/* If this bit is set: the device
   has acquired an IPv6 address */
    STATUS_BIT_IPV6_ACQUIRED,
/* If this bit is set: the device has acquired
   an IPv6 address */
    STATUS_BIT_IPV6_GLOBAL_ACQUIRED,
/* If this bit is set: the device has acquired
   an IPv6 address */
    STATUS_BIT_IPV6_LOCAL_ACQUIRED,

/* If this bit is set: Authentication with ENT AP failed. */
    STATUS_BIT_AUTHENTICATION_FAILED,

    STATUS_BIT_RESET_REQUIRED,

    STATUS_BIT_TX_STARED
}e_StatusBits;

#define MAX_SERVICE_NAME_LENGTH     (63)

#define CHANNEL_MASK_ALL            (0x1FFF)

/* without channel 140 */
#define CHANNEL_MASK_ALL_5G         (0x1F7FFFF)

#define RSSI_TH_MAX                 (-95)

typedef struct ScanCmd
{
   // Number of Scan Entries to retrieve from the NWP
   uint8_t numOfentries;
    
   // The netEntries array position to start write results from
   uint8_t index;
    
   // Retrieve extended results
   uint8_t extendedRes;
}ScanCmd_t;

typedef struct SetPolicyCmd
{
   // State of the scans - ON or OFF
   uint8_t turnOff;
   
   // Default is set to 10 Seconds
   uint32_t ScanIntervalinSec;
   
   /* Displays hidden SSID's - Yes or No */
   uint8_t hiddenSsid;
   
   /* Scan parameters configurations -
      RSSI threshold and channel mask for 2.4Ghz */
   SlWlanScanParamCommand_t ScanParamConfig;
   
   // Scan parameters configurations -
   // RSSI threshold and channel mask for 5Ghz
   SlWlanScanParam5GCommand_t ScanParamConfig5G;
} SetPolicyCmd_t;

typedef struct ConnectCmd
{
   // Ap's SSID
   uint8_t                 *ssid;
   
   // Static IP address (for static configuration)
   uint8_t                 *ip;
    
   // Default gateway IP address (for static configuration)
   uint8_t                 *gw;
    
   // Dns IP address (for static configuration)
   uint8_t                 *dns;
    
   // Enterprise user name
   uint8_t                 *entUserName;
   
   // Device Date and Time - IMPORTANT: Date and time
   // should match the certificate expiration date
   SlDateTime_t dateTime;
    
   // Security parameters - Security Type and Password
   SlWlanSecParams_t secParams;
    
   // Enterprise parameters - Security Type and Password
   SlWlanSecParamsExt_t secParamsEnt;
}ConnectCmd_t;

typedef struct StartApCmd 
{
   // AP's SSID
   uint8_t *ssid;
 
   // Determine if AP has hidden SSID
   uint8_t hidden;
 
   // 802.11 WLAN channel [1-12]
   uint8_t channel;
 
   // The AP's TX power
   uint8_t tx_pow;
   
   // Limits the number of stations that the AP's has
   uint8_t sta_limit;
 
   // Security parameters - Security Type and Password
   SlWlanSecParams_t secParams;
} StartApCmd_t;

typedef struct CreateFilterCmd 
{
   // Header or combination filter
   SlWlanRxFilterRuleType_t ruleType;
 
   // Returned value for 'sl_WlanRxFilterAdd()'
   SlWlanRxFilterID_t filterID;
   
   // Dictates filter behavior
   SlWlanRxFilterFlags_u flags;
   
   // Match criteria
   SlWlanRxFilterRule_u rule;
 
   // What are the preconditions to trigger the filter
   SlWlanRxFilterTrigger_t trigger;
 
   // Operation that execute upon a filter match
   SlWlanRxFilterAction_t action;
} CreateFilterCmd_t;

typedef struct WoWLANEnableCmd 
{
   // Header or combination filter
   SlWlanRxFilterRuleType_t ruleType;
   
   // Returned value for 'sl_WlanRxFilterAdd()'
   SlWlanRxFilterID_t filterID;
 
   // Dictates filter behavior
   SlWlanRxFilterFlags_u flags;
 
   // Match criteria
   SlWlanRxFilterRule_u rule;
   
   // What are the preconditions to trigger the filter
   SlWlanRxFilterTrigger_t trigger;
 
   // Operation that execute upon a filter match
   SlWlanRxFilterAction_t action;
} WoWLANEnableCmd_t;

// Host name or address
/* SimpleLink Ping command stracture */
typedef struct PingCmd
{
   uint8_t               *host;
 
 SlNetAppPingCommand_t pingCmd;
}PingCmd_t;

/* Status keeping MACROS */

#define SET_STATUS_BIT(status_variable, bit) status_variable |= (1 << (bit))

#define CLR_STATUS_BIT(status_variable, bit) status_variable &= ~(1 << (bit))

#define GET_STATUS_BIT(status_variable, bit) \
    (0 != (status_variable & (1 << (bit))))

#define IS_NW_PROCSR_ON(status_variable)     \
    GET_STATUS_BIT(status_variable, STATUS_BIT_NWP_INIT)

#define IS_CONNECTED(status_variable)        \
    GET_STATUS_BIT(status_variable, STATUS_BIT_CONNECTION)

#define IS_IP_LEASED(status_variable)        \
    GET_STATUS_BIT(status_variable, STATUS_BIT_IP_LEASED)

#define IS_IP_ACQUIRED(status_variable)      \
    GET_STATUS_BIT(status_variable, STATUS_BIT_IP_ACQUIRED)

#define IS_IP6_ACQUIRED(status_variable)     \
    GET_STATUS_BIT(status_variable, \
                   (STATUS_BIT_IPV6_LOCAL_ACQUIRED | \
                    STATUS_BIT_IPV6_GLOBAL_ACQUIRED))

#define IS_IPV6L_ACQUIRED(status_variable)   \
    GET_STATUS_BIT(status_variable, STATUS_BIT_IPV6_LOCAL_ACQUIRED)

#define IS_IPV6G_ACQUIRED(status_variable)   \
    GET_STATUS_BIT(status_variable, STATUS_BIT_IPV6_GLOBAL_ACQUIRED)

#define IS_PING_RUNNING(status_variable)     \
    GET_STATUS_BIT(status_variable, STATUS_BIT_PING_STARTED)

#define IS_TX_ON(status_variable)            \
    GET_STATUS_BIT(status_variable, STATUS_BIT_TX_STARED)

#define ASSERT_ON_ERROR(ret, errortype) \
    { \
        if(ret < 0) \
        { \
            SHOW_WARNING(ret, errortype); \
            return -1; \
        } \
    }

#define ASSERT_AND_CLEAN_CONNECT(ret, errortype, ConnectParams) \
    { \
        if(ret < 0) \
        { \
            SHOW_WARNING(ret, errortype); \
            FreeConnectCmd(ConnectParams); \
            return -1; \
        } \
    }

#define ASSERT_AND_CLEAN_STARTAP(ret, errortype, StartApParams) \
    { \
        if(ret < 0) \
        { \
            SHOW_WARNING(ret, errortype); \
            FreeStartApCmd(StartApParams); \
            return -1; \
        } \
    }

#define ASSERT_AND_CLEAN_PING(ret, errortype, pingParams) \
    { \
        if(ret < 0) \
        { \
            SHOW_WARNING(ret, errortype); \
            FreePingCmd(pingParams); \
            return -1; \
        } \
    }

#define ASSERT_AND_CLEAN_MDNS_ADV(ret, errortype, mDNSAdvertiseParams) \
    { \
        if(ret < 0) \
        { \
            SHOW_WARNING(ret, errortype); \
            FreemDNSAdvertiseCmd(mDNSAdvertiseParams); \
            return -1; \
        } \
    }

#define ASSERT_AND_CLEAN_RECV(ret, errortype, RecvCmdParams) \
    { \
        if(ret < 0) \
        { \
            SHOW_WARNING(ret, errortype); \
            FreeRecvCmd(RecvCmdParams); \
            return -1; \
        } \
    }

#define OS_ERROR ("")
#define SHOW_WARNING(ret, errortype)        UART_PRINT( \
        "\n\r[line:%d, error code:%d] %s\n\r", __LINE__, ret, errortype);

void gpioButtonFxn1(uint8_t index);

void PrintIPAddress(unsigned char ipv6,
                    void *ip);

int32_t sem_wait_timeout(sem_t *sem,
                         uint32_t Timeout);

int32_t ipv6AddressParse(char *str,
                         uint8_t *ipv6ip);

int32_t ipv4AddressParse(char *str,
                         uint32_t *ipv4ip);

int32_t hexbyteStrtoASCII(char *str,
                          uint8_t *ascii);

int32_t macAddressParse(char *str,
                        uint8_t *mac);
#endif