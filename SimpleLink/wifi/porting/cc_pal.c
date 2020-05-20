/*
 * Copyright (c) 2017-2018, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/******************************************************************************
*     cc_pal.c
*
*    Simplelink Wi-Fi platform abstraction file
******************************************************************************/



#include "boards.h"
#include "FreeRTOS.h"
#include "SIMPLELINKWIFI.h"
#include "../simplelink.h"
//#include "nrf_spi.h"
#include "nrf_drv_spi.h"
#include "nrf_gpio.h"
#include "nrf_drv_gpiote.h"
#include "nrfx_gpiote.h"

#include "cc_pal.h"



// defined in the project settings now.
#define SPI_DEFAULT_CONFIG_IRQ_PRIORITY 6


#define WIFI_SPI_SCK_PIN               33 // P1.01
#define WIFI_SPI_MOSI_PIN              34 // P1.02
#define WIFI_SPI_MISO_PIN              36 // P1.04
#define WIFI_SPI_SS_PIN                39 // P1.07
#define WIFI_SPI_IRQ_PRIORITY          NRFX_SPI_DEFAULT_CONFIG_IRQ_PRIORITY
#define WIFI_SPI_FREQ                  NRF_DRV_SPI_FREQ_4M
#define WIFI_SPI_MODE                  NRF_DRV_SPI_MODE_0
#define WIFI_SPI_BIT_ORDER             NRF_DRV_SPI_BIT_ORDER_MSB_FIRST
#define WIFI_DETECT_PIN                37 // P1.05

/* SX1261 NRESET (OUTPUT) */
#define WIFI_RADIO_NRESET_GPIO_PIN     43 // P1.11

/* SX1261 NSS (OUTPUT) */
#define WIFI_RADIO_NSS_GPIO_PIN        39 // P1.07

/* SX1261 DIO1 (IRQ INPUT) */
#define WIFI_RADIO_DIO1_GPIO_PIN       10 // P0.10

/* SX1261 ANTSW (ANT SW OUTPUT) */
#define WIFI_RADIO_ANTSW_GPIO_PIN      29 // P0.29

/* SX1261 BUSY (INPUT) */
#define WIFI_RADIO_BUSY_GPIO_PIN       42 // P1.10

#define WIFI_RADIO_NHIB_GPIO_PIN       35 // P1.03


#define SPI_INSTANCE                    0
static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE_0;

/****************************************************************************
   GLOBAL VARIABLES
****************************************************************************/
volatile Fd_t g_SpiFd = 0;
SL_P_EVENT_HANDLER g_Host_irq_Hndlr = NULL;

// indicate that SPI instance completed the transfer
static volatile bool spi_xfer_done;

/****************************************************************************
   CONFIGURATION VARIABLES
****************************************************************************/
extern const WiFi_Config WiFi_config[];
extern const uint_least8_t WiFi_count;
static SIMPLELINKWIFI_HWAttrsV1* curDeviceConfiguration;

const SIMPLELINKWIFI_HWAttrsV1 wifiSimplelinkHWAttrs =
{
    .spiIndex = 0,
    .hostIRQPin = WIFI_RADIO_DIO1_GPIO_PIN,
    .nHIBPin = WIFI_RADIO_NHIB_GPIO_PIN,
    .csPin = WIFI_SPI_SS_PIN,
    .maxDMASize = 1024,
    .spiBitRate = 3000000
};

const uint_least8_t WiFi_count = 1;

const WiFi_Config WiFi_config[1] =
{
    {
        .hwAttrs = &wifiSimplelinkHWAttrs,
    }
};

/****************************************************************************
   CONFIGURATION FUNCTION DEFINITION
****************************************************************************/
void WiFi_init()
{
    /* We need to have at least one WiFi module. */
    if(WiFi_count == 0)
    {
        return;
    }

    curDeviceConfiguration = (SIMPLELINKWIFI_HWAttrsV1*) WiFi_config[0].hwAttrs;
    
    // initialize the IO
    if (!nrf_drv_gpiote_is_init())
    {
       nrf_drv_gpiote_init();
    }
    
    nrf_drv_gpiote_in_config_t in_configII = GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
    nrf_drv_gpiote_in_init(curDeviceConfiguration->hostIRQPin, &in_configII, HostIrqGPIO_callback);
    
    //nrf_gpio_cfg_output(curDeviceConfiguration->nHIBPin);
    //nrf_gpio_cfg_output(curDeviceConfiguration->csPin);
    
    nrf_drv_gpiote_out_config_t out_config = GPIOTE_CONFIG_OUT_SIMPLE(false);
    nrf_drv_gpiote_out_init(curDeviceConfiguration->nHIBPin, &out_config);
    nrf_drv_gpiote_out_init(curDeviceConfiguration->csPin, &out_config);
    
    nrf_drv_gpiote_out_init(WIFI_RADIO_NRESET_GPIO_PIN, &out_config);
    
    // pull the chip out of reset?
    nrf_gpio_pin_write(WIFI_RADIO_NRESET_GPIO_PIN, 1);
    nrf_gpio_pin_write(WIFI_RADIO_NRESET_GPIO_PIN, 0);
}

/****************************************************************************
   LOCAL FUNCTION DEFINITIONS
****************************************************************************/
/**
 * @brief SPI user event handler.
 * @param event
 */
void spi_event_handler(nrf_drv_spi_evt_t const * p_event, void* p_context)
{
    spi_xfer_done = true;
}


Fd_t spi_Open(char *ifName, unsigned long flags)
{
   // Initialize the WiFi driver
   WiFi_init();
  
   // If we could not initialize the device bail out with an error code
   if(curDeviceConfiguration == NULL)
   {
      return (-1);
   }  
   
   nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
   spi_config.ss_pin   = WIFI_SPI_SS_PIN;
   spi_config.miso_pin = WIFI_SPI_MISO_PIN;
   spi_config.mosi_pin = WIFI_SPI_MOSI_PIN;
   spi_config.sck_pin  = WIFI_SPI_SCK_PIN;
   
   APP_ERROR_CHECK(nrf_drv_spi_init(&spi, &spi_config, spi_event_handler, NULL));
   
   // TODO: how to deal with errors that 
   return 1;
}

int spi_Close(Fd_t fd)
{
   nrf_drv_spi_uninit(&spi);
   return (0);
}

int spi_Read(Fd_t fd, unsigned char *pBuff, int len)
{
   int read_size = 0;
   
   ASSERT_CS();

   // check if the link SPI has been initialized successfully
   if(fd < 0)
   {
      DEASSERT_CS();
      return (-1);
   }
    
   uint16_t currentReadLength = len;
   while (len > 0)
   {
      if(len > curDeviceConfiguration->maxDMASize)
      {
         currentReadLength = curDeviceConfiguration->maxDMASize;
      }
      else
      {
         currentReadLength = len;
      }
      
      if (NRF_SUCCESS == nrf_drv_spi_transfer(&spi, NULL, 0, pBuff, len))
      {
         read_size += len;
         len = len - currentReadLength;
         pBuff = pBuff + currentReadLength;
      }
      else
      {
         DEASSERT_CS();
         read_size = -1; 
      }
      
      while (!spi_xfer_done)
      {
         __WFE();
      }
   }
   
   

   DEASSERT_CS();

   return (read_size);
}

int spi_Write(Fd_t fd, unsigned char *pBuff, int len)
{
   int write_size = 0;

   ASSERT_CS();

   // check if the link SPI has been initialized successfully
   if(fd < 0)
   {
      DEASSERT_CS();
      return (-1);
   }
    
   uint16_t currentWriteLength = len;
   while (len > 0)
   {
      if(len > curDeviceConfiguration->maxDMASize)
      {
         currentWriteLength = curDeviceConfiguration->maxDMASize;
      }
      else
      {
         currentWriteLength = len;
      }
    
      if (NRF_SUCCESS == nrf_drv_spi_transfer(&spi, pBuff, currentWriteLength, NULL, 0))
      {
         write_size += len;
         len = len - currentWriteLength;
         pBuff = pBuff + currentWriteLength;
      }
      else
      {
         DEASSERT_CS();
         write_size = -1;       
      }
       
      while (!spi_xfer_done)
      {
         __WFE();
      }
   }

   DEASSERT_CS();

   return (write_size);
}

int NwpRegisterInterruptHandler(P_EVENT_HANDLER InterruptHdl, void* pValue)
{
    // Check for unregister condition
    if(NULL == InterruptHdl)
    {
        /*GPIO_disableInt(curDeviceConfiguration->hostIRQPin);
        GPIO_clearInt(curDeviceConfiguration->hostIRQPin);*/
       
        nrf_drv_gpiote_in_event_disable(curDeviceConfiguration->hostIRQPin);
        nrf_gpiote_event_clear((nrf_gpiote_events_t)curDeviceConfiguration->hostIRQPin);
       
        g_Host_irq_Hndlr = NULL;
        return (0);
    }
    else if(NULL == g_Host_irq_Hndlr)
    {
        g_Host_irq_Hndlr = InterruptHdl;
        /*GPIO_setCallback(curDeviceConfiguration->hostIRQPin,
                         HostIrqGPIO_callback);
        GPIO_clearInt(curDeviceConfiguration->hostIRQPin);
        GPIO_enableInt(curDeviceConfiguration->hostIRQPin);*/
        
        nrf_drv_gpiote_in_config_t in_configII = GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
        nrf_drv_gpiote_in_init(curDeviceConfiguration->hostIRQPin, &in_configII, HostIrqGPIO_callback);
        
        nrf_gpiote_event_clear((nrf_gpiote_events_t)curDeviceConfiguration->hostIRQPin);
        nrf_drv_gpiote_in_event_enable(curDeviceConfiguration->hostIRQPin, true);
        
        
        return (0);
    }
    else
    {
        // An error occurred
        return (-1);
    }
}

void HostIrqGPIO_callback(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t polarity)
{   
   if((pin == curDeviceConfiguration->hostIRQPin) &&
      (NULL != g_Host_irq_Hndlr))
   {
      g_Host_irq_Hndlr(0);
   }
}

void NwpMaskInterrupt()
{
}

void NwpUnMaskInterrupt()
{
}

void NwpPowerOnPreamble(void)
{
    /* Maybe start timer here? */
}

void NwpPowerOn(void)
{
   nrf_gpio_pin_write(curDeviceConfiguration->nHIBPin, 1);
    
   // wait 5msec
   ClockP_usleep(5000);
}

void NwpPowerOff(void)
{
   nrf_gpio_pin_write(curDeviceConfiguration->nHIBPin, 0);
   
   // wait 5msec
   ClockP_usleep(5000);
}

int Semaphore_create_handle(SemaphoreP_Handle* pSemHandle)
{
    SemaphoreP_Params params;

    SemaphoreP_Params_init(&params);
    
#ifndef SL_PLATFORM_MULTI_THREADED
    params.callback = tiDriverSpawnCallback;
#endif
    
    (*(pSemHandle)) = SemaphoreP_create(1, &params);

    if(!(*(pSemHandle)))
    {
        return(-1);
    }

    return(SemaphoreP_OK);
}

int SemaphoreP_delete_handle(SemaphoreP_Handle* pSemHandle)
{
    SemaphoreP_delete(*(pSemHandle));
    return(SemaphoreP_OK);
}

int SemaphoreP_post_handle(SemaphoreP_Handle* pSemHandle)
{
    SemaphoreP_post(*(pSemHandle));
    return(SemaphoreP_OK);
}

int Mutex_create_handle(MutexP_Handle* pMutexHandle)
{
    MutexP_Params params;

    MutexP_Params_init(&params);
#ifndef SL_PLATFORM_MULTI_THREADED
    params.callback = tiDriverSpawnCallback;
#endif

    (*(pMutexHandle)) = MutexP_create(&params);

    if(!(*(pMutexHandle)))
    {
        return(MutexP_FAILURE);
    }

    return(MutexP_OK);
}

int MutexP_delete_handle(MutexP_Handle* pMutexHandle)
{
    MutexP_delete(*(pMutexHandle));
    return (MutexP_OK);
}

int Mutex_unlock(MutexP_Handle pMutexHandle)
{
    MutexP_unlock(pMutexHandle, 0);
    return (MutexP_OK);
}

int Mutex_lock(MutexP_Handle pMutexHandle)
{
    MutexP_lock(pMutexHandle);
    return (MutexP_OK);
}

unsigned long TimerGetCurrentTimestamp()
{
    return (ClockP_getSystemTicks());
}
