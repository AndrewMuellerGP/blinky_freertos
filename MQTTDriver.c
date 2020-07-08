// Copyright (c) 2020 Confidential Information Georgia-Pacific Consumer Products
// Not for further distribution.  All rights reserved.

#include "pthread.h"
#include "mqueue.h"

#include <ti/drivers/net/wifi/simplelink.h>
#include <ti/net/mqtt/mqttclient.h>

#include "MQTTDriver.h"

#define MQTT_3_1_1               false
#define MQTT_3_1                 true

//#define SERVER_ADDRESS           "messagesight.demos.ibm.com"
#define SERVER_ADDRESS           "mqtt.eclipse.org"
#define SERVER_IP_ADDRESS        "192.168.178.67"
#define PORT_NUMBER              1883
#define SECURED_PORT_NUMBER      8883
#define LOOPBACK_PORT            1882

#define PUB_URL                 "https:\/\/polkapalace-test.migidae.com\/securehome\/devices\/3403de11d3ba"
#define BROKER_ADDY             "ap14lclokbtpv-ats.iot.us-east-1.amazonaws.com"
#define TEST_MQTT               "{" \
                                "   \"method\": \"event\"," \
                                "   \"collectorGroupName\": \"WiFiTest\"," \
                                "   \"collectorId\": \"000102030405\"," \
                                "   \"deviceId\": \"3403de11d3ba\"," \
                                "   \"eventList\": [," \
                                "      \"eec,6200229606.940,7E0146014CE77E,37660\"," \
                                "   ]," \
                                "   \"gatewayGroupName\": \"WiFiTest\"," \
                                "   \"gatewayId\": \"441a667801ff\"," \
                                "}"

#define PORT_NUM                1883
#define SECURED_PORT_NUM        8883

#define CLEAN_SESSION            true
#define RETAIN_ENABLE            1
#define SUBSCRIPTION_TOPIC_COUNT 4

#define MSG_RECV_BY_CLIENT          11

/* secured client requires time configuration, in order to verify server     */
/* certificate validity (date).                                              */

/* Day of month (DD format) range 1-31                                       */
#define DAY                      1
/* Month (MM format) in the range of 1-12                                    */
#define MONTH                    5
/* Year (YYYY format)                                                        */
#define YEAR                     2017
/* Hours in the range of 0-23                                                */
#define HOUR                     12
/* Minutes in the range of 0-59                                              */
#define MINUTES                  33
/* Seconds in the range of 0-59                                              */
#define SEC                      21

/* Number of files used for secure connection                                */
#define CLIENT_NUM_SECURE_FILES  3

/* Expiration value for the timer that is being used to toggle the Led.      */
#define TIMER_EXPIRATION_VALUE   100 * 1000000

extern void UART_PRINT(char* label, ...);



char *Mqtt_Client_secure_files[CLIENT_NUM_SECURE_FILES] = {"AmazonRootCA1.pem",
                                                           "b9b883faea-certificate.pem.crt",
                                                           "b9b883faea-private.pem.key"};

// Client ID
// If ClientId isn't set, the MAC address of the device will be copied into
// the ClientID parameter.
char ClientId[13] = {'\0'};

static MQTTClient_Handle gMqttClient;
MQTTClient_Params MqttClientExmple_params;

void MqttClientCallback(int32_t event,
                        void * metaData,
                        uint32_t metaDateLen,
                        void *data,
                        uint32_t dataLen);

MQTTClient_ConnParams Mqtt_ClientCtx =
{
    MQTTCLIENT_NETCONN_IP4 | MQTTCLIENT_NETCONN_SEC,
    BROKER_ADDY,  //SERVER_ADDRESS,
    SECURED_PORT_NUMBER, //  PORT_NUMBER
    SLNETSOCK_SEC_METHOD_SSLv3_TLSV1_2,
    SLNETSOCK_SEC_CIPHER_FULL_LIST,
    CLIENT_NUM_SECURE_FILES,
    Mqtt_Client_secure_files
};

// Initialize the will_param structure to the default will parameters
#define WILL_TOPIC               "Client"
#define WILL_MSG                 "Client Stopped"
#define WILL_QOS                 MQTT_QOS_2
#define WILL_RETAIN              false

MQTTClient_Will will_param =
{
    WILL_TOPIC,
    WILL_MSG,
    WILL_QOS,
    WILL_RETAIN
};

// Publishing topics and messages
// Defining Publish Topic Values
#define PUBLISH_TOPIC0          "/cc32xx/ButtonPressEvtSw2"
#define PUBLISH_TOPIC0_DATA     "Push Button SW2 has been pressed on CC32xx device"
const char *publish_topic = { PUBLISH_TOPIC0 };
const char *publish_data = { PUBLISH_TOPIC0_DATA };

mqd_t g_PBQueue;
pthread_t mqttThread = (pthread_t) NULL;
pthread_t appThread = (pthread_t) NULL;
timer_t g_timer;

// Subscription topics and qos values
// Defining Subscription Topic Values
#define SUBSCRIPTION_TOPIC0      "/Broker/To/cc32xx"
#define SUBSCRIPTION_TOPIC1      "/cc3200/ToggleLEDCmdL1"
#define SUBSCRIPTION_TOPIC2      "/cc3200/ToggleLEDCmdL2"
#define SUBSCRIPTION_TOPIC3      "/cc3200/ToggleLEDCmdL3"
char *topic[SUBSCRIPTION_TOPIC_COUNT] = { SUBSCRIPTION_TOPIC0, SUBSCRIPTION_TOPIC1, SUBSCRIPTION_TOPIC2, SUBSCRIPTION_TOPIC3 };


typedef struct 
{
    uint32_t topicLen;
    uint32_t payLen;
    bool retain;
    bool dup;
    unsigned char qos;
} publishMsgHeader_t;

typedef struct
{
    int32_t event;
    void        *msgPtr;
    int32_t topLen;
} msgQueue_t;

extern int32_t MQTT_SendMsgToQueue(msgQueue_t *queueElement);

void MQTT_setTime()
{
    SlDateTime_t dateTime = {0};
    dateTime.tm_day = (uint32_t)DAY;
    dateTime.tm_mon = (uint32_t)MONTH;
    dateTime.tm_year = (uint32_t)YEAR;
    dateTime.tm_hour = (uint32_t)HOUR;
    dateTime.tm_min = (uint32_t)MINUTES;
    dateTime.tm_sec = (uint32_t)SEC;
    sl_DeviceSet(SL_DEVICE_GENERAL, SL_DEVICE_GENERAL_DATE_TIME,
                 sizeof(SlDateTime_t), (uint8_t *)(&dateTime));
}

int MQTT_Init(uint8_t* macAddress, uint16_t length)
{
   uint8_t Client_Mac_Name[2];
   
   for(uint8_t i = 0; i < SL_MAC_ADDR_LEN; i++)
   {
      // Each mac address byte contains two hexadecimal characters
      // Copy the 4 MSB - the most significant character
      Client_Mac_Name[0] = (macAddress[i] >> 4) & 0xf;
      
      // Copy the 4 LSB - the least significant character
      Client_Mac_Name[1] = macAddress[i] & 0xf;

      if (Client_Mac_Name[0] > 9)
      {
          // Converts and copies from number that is greater than 9 in
          // hexadecimal representation (a to f) into ascii character
          ClientId[2 * i] = Client_Mac_Name[0] + 'a' - 10;
      }
      else
      {
          // Converts and copies from number 0 - 9 in hexadecimal
          // representation into ascii character
          ClientId[2 * i] = Client_Mac_Name[0] + '0';
      }
      if (Client_Mac_Name[1] > 9)
      {
          // Converts and copies from number that is greater than 9 in
          // hexadecimal representation (a to f) into ascii character
          ClientId[2 * i + 1] = Client_Mac_Name[1] + 'a' - 10;
      }
      else
      {
          // Converts and copies from number 0 - 9 in hexadecimal
          // representation into ascii character
          ClientId[2 * i + 1] = Client_Mac_Name[1] + '0';
      }
   }
   
   MqttClientExmple_params.clientId = ClientId;
   MqttClientExmple_params.connParams = &Mqtt_ClientCtx;
   MqttClientExmple_params.mqttMode31 = MQTT_3_1;
   MqttClientExmple_params.blockingSend = true;

   gMqttClient = MQTTClient_create(MqttClientCallback, &MqttClientExmple_params);

   if(gMqttClient == NULL)
   {
      // lib initialization failed
      //gInitState &= ~CLIENT_INIT_STATE;
      return(-1);
   }
   
   MQTT_setTime();
   
   // TODO:
   MQTTClient_set(gMqttClient, MQTTClient_WILL_PARAM, &will_param, sizeof(will_param));
   
   bool clean = CLEAN_SESSION;
   MQTTClient_set(gMqttClient, MQTTClient_CLEAN_CONNECT, (void *)&clean, sizeof(bool));
   
   // The return code of MQTTClient_connect is the ConnACK value that returns from the server
   int16_t lRetVal = MQTTClient_connect(gMqttClient);

   /*negative lRetVal means error,
     0 means connection successful without session stored by the server,
     greater than 0 means successful connection with session stored by
     the server */
   if(0 > lRetVal)
   {
      // lib initialization failed
      UART_PRINT("Connection to broker failed, Error code: %d\n\r", lRetVal);
   }
   
   return lRetVal;
}

void MQTT_Publish()
{
   int16_t lRetVal = MQTTClient_publish(gMqttClient, 
                                        (char*) publish_topic, 
                                        strlen((char*)publish_topic),
                                        (char*)TEST_MQTT,
                                        strlen((char*) TEST_MQTT), 
                                        MQTT_QOS_2 | ((RETAIN_ENABLE) ? MQTT_PUBLISH_RETAIN : 0));
}

//*****************************************************************************
//
//! MQTT_SendMsgToQueue - Utility function that receive msgQueue parameter and
//! tries to push it the queue with minimal time for timeout of 0.
//! If the queue isn't full the parameter will be stored and the function
//! will return 0.
//! If the queue is full and the timeout expired (because the timeout parameter
//! is 0 it will expire immediately), the parameter is thrown away and the
//! function will return -1 as an error for full queue.
//!
//! \param[in] struct msgQueue *queueElement
//!
//! \return 0 on success, -1 on error
//
//*****************************************************************************
int32_t MQTT_SendMsgToQueue(msgQueue_t *queueElement)
{   
   struct timespec abstime = {0};

   clock_gettime(CLOCK_REALTIME, &abstime);

   if(g_PBQueue)
   {
      // send message to the queue
      if(mq_timedsend(g_PBQueue, (char *) queueElement, sizeof(msgQueue_t), 0, &abstime) == 0)
      {
         return(0);
      }
   }
   
   return(-1);
}

//*****************************************************************************
//
//! Callback in case of various event (for clients connection with remote
//! broker)
//!
//! \param[in]  event       - is a event occurred
//! \param[in]  metaData    - is the pointer for the message buffer
//!                           (for this event)
//! \param[in]  metaDateLen - is the length of the message buffer
//! \param[in]  data        - is the pointer to the buffer for data
//!                           (for this event)
//! \param[in]  dataLen     - is the length of the buffer data
//!
//! return none
//
//*****************************************************************************
void MqttClientCallback(int32_t event,
                        void * metaData,
                        uint32_t metaDateLen,
                        void *data,
                        uint32_t dataLen)
{
   int32_t i = 0;

   switch((MQTTClient_EventCB)event)
   {
      case MQTTClient_OPERATION_CB_EVENT:
      {
         switch(((MQTTClient_OperationMetaDataCB *)metaData)->messageType)
         {
            case MQTTCLIENT_OPERATION_CONNACK:
            {
               uint16_t *ConnACK = (uint16_t*) data;
               UART_PRINT("CONNACK:\n\r");
               
               // Check if Conn Ack return value is Success (0) or
               // Error - Negative value
               if(0 == ((*ConnACK) & 0xff))
               {
                  UART_PRINT("Connection Success\n\r");
               }
               else
               {
                  UART_PRINT("Connection Error: %d\n\r", *ConnACK);
               }
               break;
        }

        case MQTTCLIENT_OPERATION_EVT_PUBACK:
        {
            char *PubAck = (char *) data;
            UART_PRINT("PubAck:\n\r");
            UART_PRINT("%s\n\r", PubAck);
            break;
        }

        case MQTTCLIENT_OPERATION_SUBACK:
        {
            UART_PRINT("Sub Ack:\n\r");
            UART_PRINT("Granted QoS Levels are:\n\r");
            for(i = 0; i < dataLen; i++)
            {
                UART_PRINT("%s :QoS %d\n\r", topic[i],
                          ((unsigned char*) data)[i]);
            }
            break;
        }

        case MQTTCLIENT_OPERATION_UNSUBACK:
        {
            char *UnSub = (char *) data;
            UART_PRINT("UnSub Ack \n\r");
            UART_PRINT("%s\n\r", UnSub);
            break;
        }

        default:
            break;
        }
        break;
    }
    case MQTTClient_RECV_CB_EVENT:
    {
        MQTTClient_RecvMetaDataCB *recvMetaData =
            (MQTTClient_RecvMetaDataCB *)metaData;
        uint32_t bufSizeReqd = 0;
        uint32_t topicOffset;
        uint32_t payloadOffset;

        publishMsgHeader_t msgHead;

        char *pubBuff = NULL;
        msgQueue_t queueElem;

        topicOffset = sizeof(publishMsgHeader_t);
        payloadOffset = sizeof(publishMsgHeader_t) +
                        recvMetaData->topLen + 1;

        bufSizeReqd += sizeof(publishMsgHeader_t);
        bufSizeReqd += recvMetaData->topLen + 1;
        bufSizeReqd += dataLen + 1;
        pubBuff = (char *) malloc(bufSizeReqd);

        if(pubBuff == NULL)
        {
            UART_PRINT("malloc failed: recv_cb\n\r");
            return;
        }

        msgHead.topicLen = recvMetaData->topLen;
        msgHead.payLen = dataLen;
        msgHead.retain = recvMetaData->retain;
        msgHead.dup = recvMetaData->dup;
        msgHead.qos = recvMetaData->qos;
        memcpy((void*) pubBuff, &msgHead, sizeof(publishMsgHeader_t));

        /* copying the topic name into the buffer                        */
        memcpy((void*) (pubBuff + topicOffset),
               (const void*)recvMetaData->topic,
               recvMetaData->topLen);
        memset((void*) (pubBuff + topicOffset + recvMetaData->topLen),'\0',1);

        /* copying the payload into the buffer                           */
        memcpy((void*) (pubBuff + payloadOffset), (const void*) data, dataLen);
        memset((void*) (pubBuff + payloadOffset + dataLen), '\0', 1);

        UART_PRINT("\n\rMsg Recvd. by client\n\r");
        UART_PRINT("TOPIC: %s\n\r", pubBuff + topicOffset);
        UART_PRINT("PAYLOAD: %s\n\r", pubBuff + payloadOffset);
        UART_PRINT("QOS: %d\n\r", recvMetaData->qos);

        if(recvMetaData->retain)
        {
            UART_PRINT("Retained\n\r");
        }

        if(recvMetaData->dup)
        {
            UART_PRINT("Duplicate\n\r");
        }

        /* filling the queue element details                              */
        queueElem.event = MSG_RECV_BY_CLIENT;
        queueElem.msgPtr = pubBuff;
        queueElem.topLen = recvMetaData->topLen;

        /* signal to the main task                                        */
        if(MQTT_SendMsgToQueue(&queueElem))
        {
            UART_PRINT("\n\n\rQueue is full\n\n\r");
        }
        break;
    }
    case MQTTClient_DISCONNECT_CB_EVENT:
    {
        //gResetApplication = true;
        UART_PRINT("BRIDGE DISCONNECTION\n\r");
        break;
    }
    }
}
