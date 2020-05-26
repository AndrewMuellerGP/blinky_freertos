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
*     SIMPLELINKWIFI.h
*
*    Simplelink Wi-Fi platform specific information for MSP432P4 and MSP432E4
******************************************************************************/
#ifndef __SIMPLELINK_MSP432P4_H__
#define __SIMPLELINK_MSP432P4_H__

#ifdef  __cplusplus
extern "C" {
#endif

#include "cc_pal.h"

typedef struct SIMPLELINKWIFI_HWAttrsV1
{
    uint_least8_t spiIndex;         /* Index of the SPI driver to use */
    uint_least8_t hostIRQPin;       /* Host IRQ Pin */
    uint_least8_t nHIBPin;          /* nHIBPint */
    uint_least8_t csPin;            /* Chip Select Pin */
    uint_least16_t maxDMASize;      /* Maximum DMA size (default 1024) */
    uint32_t spiBitRate;            /* Bitrate of SPI */
} SIMPLELINKWIFI_HWAttrsV1;

#define ASSERT_CS()         (nrf_gpio_pin_write(curDeviceConfiguration->csPin, 0))
#define DEASSERT_CS()       (nrf_gpio_pin_write(curDeviceConfiguration->csPin, 1))

#ifdef  __cplusplus
}
#endif

#endif /*__SIMPLELINK_MSP432P4_H__*/
