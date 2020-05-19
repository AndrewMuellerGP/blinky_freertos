
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "external/cJSON/cJSON.h"
#include "external/JSMN/jsmn.h"
#include <ti/utils/json/json.h>

#include "json_test.h"

//struct record {const char *precision;double lat,lon;const char *address,*city,*state,*zip,*country; };
const char* EXAMPLE_TEMPLATE = "{\n\"method\": string,"  \
                               "\"gatewayId\": int32,"   \
                               "\"collectorId\": int32," \
                               "\"deviceId\": boolean"   \
                               "\"eventList\": [string, string, string]" \
                               "}";

static char* jsonText = "{\n\"method\": \"event\", " \
                         "\n\"gatewayId\": \"0123456\", " \
                         "\n\"collectorId\": \"78910\", " \
                         "\n\"deviceId\": \"ABCDE\", " \
                         "\n\"eventList\": [\"7E0155FF14167E\", \"7E814101EE2A7E\",\"7E0146014CE77E\"\n]\n}";

struct record
{
   char* method;
   char* gatewayId;
   char* collectorId;
   char* deviceId;
   char** eventList;
};

// example dispenser events
const char* dispenserEvents[] =
   {
      "7E0155FF14167E",
      "7E814101EE2A7E",
      "7E0146014CE77E",
      "7E815603548C7E",
      "7E81430098697E",
      "7E814103CE687E"
   };

void cJSON_Test()
{
   // 1. Parse the JSON from the text
   cJSON* inJson = cJSON_Parse(jsonText);
   
   if (NULL != inJson)
   {
      //printf("cJSON Parsed Success\n");
      cJSON_Delete(inJson);
   }
   
   // 2. Parse from object into JSON string
   cJSON *outJson = cJSON_CreateObject();
   cJSON_AddItemToObject(outJson, "method", cJSON_CreateString("event"));
   cJSON_AddItemToObject(outJson, "gatewayId", cJSON_CreateString("98765"));
   cJSON_AddItemToObject(outJson, "collectorId", cJSON_CreateString("1234"));
   cJSON_AddItemToObject(outJson, "deviceId", cJSON_CreateString("554"));
   cJSON* eventList = cJSON_CreateObject();
   cJSON_AddItemToObject(outJson, "eventList", eventList);
   
   
   /* TODO: adding the list of ascii-hex strings is not straight forward
     cJSON_AddStringToObject(eventList, 
     7E 81 56 03 54 8C 7E 
     7E 81 43 00 98 69 7E 
     7E 81 41 03 CE 68 7E*/
   
   cJSON_AddStringToObject(eventList, "", "7E814103CE687E");
   
   char* strJson = cJSON_Print(outJson);
   //printf("%s\n",strJson);
   cJSON_Delete(outJson);
   free(strJson);
}

void JSMN_Test()
{
   jsmn_parser parser;
   jsmntok_t tokens[128];
   
   // 1. Parse the JSON from the text
   jsmn_init(&parser);
   int r = jsmn_parse(&parser, jsonText, strlen(jsonText), tokens, sizeof(tokens) / sizeof(tokens[0]));
   
   if (r < 0)
   {
      return;
   }
   
   if (r < 1 || tokens[0].type != JSMN_OBJECT)
   {
      //printf("Object expected\n");
      return;
   }
   
   for (uint8_t i = 0; i < r; i++)
   {
      if (tokens[i].type == JSMN_STRING && 
          (int)strlen("deviceId") == tokens[i].end - tokens[i].start &&
          strncmp(jsonText + tokens[i].start, "deviceId", tokens[i].end - tokens[i].start) == 0)
      {
         return;
      }
   }
   
   // 2. Parse from API object into JSON string
   // JSMN does not include data for serializing a data structure into JSON string
   
}

void TISL_JSON_TEST()
{
   // 1. Parse The JSON from text
   Json_Handle templateHandle;
   Json_Handle objectHandle;
   uint16_t    bufsize = 32;
   char        outputBuffer[32];
   char*       fieldName = "\"deviceId\"";
   char        deviceIdBuffer[32];
   uint16_t    deviceIdBufferSize = sizeof(deviceIdBuffer);
    
   // Create an internal representation of the template for the library's use
   Json_createTemplate(&templateHandle, EXAMPLE_TEMPLATE, strlen(EXAMPLE_TEMPLATE));
    
   // Allocate memory needed for an object matching the generated template
   Json_createObject(&objectHandle, templateHandle, 0);
    
   // Parse the JSON and fill in the object
   Json_parse(objectHandle, jsonText, strlen(jsonText));
    
   // Retrieve a value from the parsed json
   Json_getValue(objectHandle, fieldName, outputBuffer, &bufsize);
   fieldName = "\"method\"";
   Json_getValue(objectHandle, fieldName, &deviceIdBuffer, &deviceIdBufferSize);
}
