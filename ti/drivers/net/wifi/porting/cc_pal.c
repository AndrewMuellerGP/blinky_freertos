// Copyright (c) 2020 Confidential Information Georgia-Pacific Consumer Products
// Not for further distribution.  All rights reserved.

/**
 * Simplelink Wi-Fi platform abstraction layer for the Nordic NRF52840 host
 * micro-controller.
 */

#include <unistd.h>

#include "boards.h"
#include "FreeRTOS.h"
#include "SIMPLELINKWIFI.h"
#include "../simplelink.h"
#include "nrf_drv_spi.h"
#include "nrf_gpio.h"
#include "nrf_drv_gpiote.h"
#include "nrfx_gpiote.h"

#include "cc_pal.h"

/*
The following are the pins as defined in the LORA system.  Will likely migrate
to these or similar on the prototype hardware.
#define WIFI_SPI_SCK_PIN               33 // P1.01
#define WIFI_SPI_MOSI_PIN              34 // P1.02
#define WIFI_SPI_MISO_PIN              36 // P1.04
#define WIFI_SPI_SS_PIN                39 // P1.07
#define WIFI_DETECT_PIN                37 // P1.05
*/
#define WIFI_SPI_IRQ_PRIORITY          NRFX_SPI_DEFAULT_CONFIG_IRQ_PRIORITY
#define WIFI_SPI_FREQ                  NRF_DRV_SPI_FREQ_250K
#define WIFI_SPI_MODE                  NRF_DRV_SPI_MODE_0
#define WIFI_SPI_BIT_ORDER             NRF_DRV_SPI_BIT_ORDER_MSB_FIRST

#define WIFI_SPI_SCK_PIN                26  // P0.26
#define WIFI_SPI_MOSI_PIN               29  // P0.29
#define WIFI_SPI_MISO_PIN               30  // P0.30
#define WIFI_SPI_SS_PIN                 31  // P0.31

/* SX1261 NRESET (OUTPUT) */
#define WIFI_RADIO_NRESET_GPIO_PIN     43 // P1.11

/* SX1261 NSS (OUTPUT) */
#define WIFI_RADIO_NSS_GPIO_PIN        39 // P1.07

/* SX1261 DIO1 (IRQ INPUT) */
#define WIFI_RADIO_DIO1_GPIO_PIN       37 // P1.05

/* SX1261 ANTSW (ANT SW OUTPUT) */
#define WIFI_RADIO_ANTSW_GPIO_PIN      29 // P0.29

/* SX1261 BUSY (INPUT) */
#define WIFI_RADIO_BUSY_GPIO_PIN       42 // P1.10

#define WIFI_RADIO_NHIB_GPIO_PIN       35 // P1.03

// define the NRF52840 SPI interface to use.
#define SPI_INSTANCE                    0
static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE);

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
    // Verify we have at least one WiFi module.
    if(WiFi_count == 0)
    {
        return;
    }

    curDeviceConfiguration = (SIMPLELINKWIFI_HWAttrsV1*) WiFi_config[0].hwAttrs;
    
    // initialize the GPIO IO used for wifi interrupt generation
    if (!nrf_drv_gpiote_is_init())
    {
       nrf_drv_gpiote_init();
    }
    
    // interrupt line from the wifi chip back to the NRF52840
    nrf_drv_gpiote_in_config_t in_config = GPIOTE_CONFIG_IN_SENSE_LOTOHI(true);
    in_config.pull = NRF_GPIO_PIN_PULLUP;
    nrf_drv_gpiote_in_init(curDeviceConfiguration->hostIRQPin, &in_config, HostIrqGPIO_callback);
    nrf_drv_gpiote_in_event_enable(curDeviceConfiguration->hostIRQPin, true);
    
    // output lines to the chip select and the inhibit pins.
    nrf_drv_gpiote_out_config_t out_config = GPIOTE_CONFIG_OUT_SIMPLE(false);
    nrf_drv_gpiote_out_init(curDeviceConfiguration->nHIBPin, &out_config);
    nrf_drv_gpiote_out_init(curDeviceConfiguration->csPin, &out_config);
    
    // output line to the reset pin
    nrf_drv_gpiote_out_init(WIFI_RADIO_NRESET_GPIO_PIN, &out_config);
    
    // pull the chip out of reset?
    // NOTE: CC3135 dev kit does not route the reset pin on the socket to the
    //       micro.
    nrf_gpio_pin_write(WIFI_RADIO_NRESET_GPIO_PIN, 1);
    usleep(1000000 - 1);
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
   
   return nrf_drv_spi_init(&spi, &spi_config, spi_event_handler, NULL);   
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
        nrf_drv_gpiote_in_event_disable(curDeviceConfiguration->hostIRQPin);
        nrf_gpiote_event_clear((nrf_gpiote_events_t)curDeviceConfiguration->hostIRQPin);
       
        g_Host_irq_Hndlr = NULL;
        return (0);
    }
    else if(NULL == g_Host_irq_Hndlr)
    {
        g_Host_irq_Hndlr = InterruptHdl;
        
        nrf_drv_gpiote_in_config_t in_config = GPIOTE_CONFIG_IN_SENSE_HITOLO(true); //GPIOTE_CONFIG_IN_SENSE_TOGGLE(true); //GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
        in_config.pull = NRF_GPIO_PIN_PULLUP;
        nrf_drv_gpiote_in_init(curDeviceConfiguration->hostIRQPin, &in_config, HostIrqGPIO_callback);
        
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
   // TODO: decide if this is needed
}

void NwpUnMaskInterrupt()
{
   // TODO: decide if this is needed
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
