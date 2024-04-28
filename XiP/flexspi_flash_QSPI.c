/*
 * The Clear BSD License
 * Copyright 2017 NXP
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted (subject to the limitations in the disclaimer below) provided
 * that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY THIS LICENSE.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "flexspi_flash.h"
#if defined XIP_BOOT_QSPI	/* Is defined in the iMXRT CPU Support package depended on the selected placement */

/*******************************************************************************
 * Code
 ******************************************************************************/
#if defined(__CC_ARM) || defined(__ARMCC_VERSION) || defined(__GNUC__) || defined __SES_ARM || defined __CROSSWORKS_ARM
	__attribute__((section(".boot_hdr.conf")))
#elif defined(__ICCARM__)
	#pragma section="app_image"
	#pragma location=".boot_hdr.conf"
	__root
#else
	#error "Unknown Compiler"
#endif

//
// QSPI boot header
//
const flexspi_nor_config_t FlashBootHeader =
{
	.memConfig =
	{
		.tag                  = FLEXSPI_CFG_BLK_TAG,
		.version              = FLEXSPI_CFG_BLK_VERSION,
		#if defined MIMXRT1024_SERIES
			.readSampleClkSrc  = kFlexSPIReadSampleClk_LoopbackInternally,
		#else
			.readSampleClkSrc = kFlexSPIReadSampleClk_LoopbackFromDqsPad,
		#endif
		.csHoldTime           = 3U,
		.csSetupTime          = 3U,
		#if defined MIMXRT1176_cm7_SERIES || defined MIMXRT1175_cm7_SERIES || defined MIMXRT1173_cm7_SERIES || defined MIMXRT1172_SERIES || defined MIMXRT1171_SERIES || \
			defined MIMXRT1166_cm7_SERIES || defined MIMXRT1165_cm7_SERIES
			.controllerMiscOption = (1u << kFlexSpiMiscOffset_SafeConfigFreqEnable),
		#endif
		.deviceType           = kFlexSpiDeviceType_SerialNOR,
		.sflashPadType        = kSerialFlash_4Pads,
		.serialClkFreq        = kFlexSpiSerialClk_100MHz,
		#if defined MIMXRT1024_SERIES
			.sflashA1Size     = 4U * 1024U * 1024U,
		#else
			.sflashA1Size     = 16U * 1024U * 1024U,
		#endif
		.lookupTable =
		{
			// (0) Read Array
			FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0xEB, RADDR_SDR, FLEXSPI_4PAD, 0x18),
			FLEXSPI_LUT_SEQ (DUMMY_SDR, FLEXSPI_4PAD, 0x06, READ_SDR,  FLEXSPI_4PAD, 0x04),
			0,
			0,
			// (1) Read Status
			FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x05, READ_SDR,  FLEXSPI_1PAD, 0x04),
			0,
			0,
			0,
			// (2) Write Status/Control Registers
			FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x01, WRITE_SDR, FLEXSPI_1PAD, 0x04),
			0,
			0,
			0,
			// (3) Write Enable
			FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x06, STOP,      FLEXSPI_1PAD, 0x0),
			0,
			0,
			0,
			// (4) empty
			0,
			0,
			0,
			0,
			// (5) Erase Sector
			FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x20, RADDR_SDR, FLEXSPI_1PAD, 0x18),
			0,
			0,
			0,
			// (6) empty
			0,
			0,
			0,
			0,
			// (7) empty
			0,
			0,
			0,
			0,
			// (8) Erase Block
			FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0xD8, RADDR_SDR, FLEXSPI_1PAD, 0x18),
			0,
			0,
			0,
			// (9) Page Program
			FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x02, RADDR_SDR, FLEXSPI_1PAD, 0x18),
			FLEXSPI_LUT_SEQ (WRITE_SDR, FLEXSPI_1PAD, 0x04, STOP,      FLEXSPI_1PAD, 0x0),
			0,
			0,
			// (10) empty
			0,
			0,
			0,
			0,
			// (11) Erase Chip
			FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x60, STOP, FLEXSPI_1PAD, 0x0),
			0,
			0,
			0,
		},
	},
	.pageSize           = 256U,
	.sectorSize         = 4U * 1024U,
	.ipcmdSerialClkFreq = 1U,
	.blockSize          = 64U * 1024U,
	.isUniformBlockSize = false,
};

#endif // XIP_BOOT_QSPI
