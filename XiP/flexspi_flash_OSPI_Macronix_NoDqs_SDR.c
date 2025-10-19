/** Coremark for iMXRT-Family
Copyright (C) 2019-2024 Markus Klein
https://github.com/Masmiseim36/Coremark_iMXRT

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE. */

#include "flexspi_flash.h"
#if defined XIP_BOOT_OCTASPI	// Is defined in the iMXRT CPU Support package depended on the selected placement

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
#define DUMMY_CYCLES 12U
const flexspi_nor_config_t FlashBootHeader =
{
	.memConfig =
	{
		.tag                  = FLEXSPI_CFG_BLK_TAG,
		.version              = FLEXSPI_CFG_BLK_VERSION,
		.readSampleClkSrc     = kFlexSPIReadSampleClk_LoopbackInternally,
		.csHoldTime          = 3U,
		.csSetupTime         = 3U,
		.deviceModeCfgEnable = 1U,
		.deviceModeType      = kDeviceConfigCmdType_Generic,
		.waitTimeCfgCommands = 1U,
		.deviceModeSeq =
		{
			.seqNum   = 1U,
			.seqId    = 6, // See Lookup table for more details
			.reserved = 0U,
		},
		.deviceModeArg        = 0,
		.configCmdEnable      = 1,
		.configModeType       = {kDeviceConfigCmdType_Generic, kDeviceConfigCmdType_Spi2Xpi, kDeviceConfigCmdType_Generic},
		.configCmdSeqs        =
		{
			{
				.seqNum   = 1,
				.seqId    = 7,
				.reserved = 0,
			},
			{
				.seqNum   = 1,
				.seqId    = 10,
				.reserved = 0,
			}
		},
		.configCmdArgs        = {0x2, 0x1},
		.controllerMiscOption = 1u << kFlexSpiMiscOffset_SafeConfigFreqEnable,
		.deviceType           = kFlexSpiDeviceType_SerialNOR,
		.sflashPadType        = kSerialFlash_8Pads,
		.serialClkFreq        = kFlexSpiSerialClk_SDR_48MHz,
		.sflashA1Size         = 0,
		.sflashA2Size         = 0,
		.sflashB1Size         = 0x4000000U,
		.sflashB2Size         = 0,
		.lookupTable =
		{
			/* Read */
			[0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_8PAD, 0xEC, CMD_SDR, FLEXSPI_8PAD, 0x13),
			[1] = FLEXSPI_LUT_SEQ(RADDR_SDR, FLEXSPI_8PAD, 0x20, DUMMY_SDR, FLEXSPI_8PAD, 0x14),
			[2] = FLEXSPI_LUT_SEQ(READ_SDR, FLEXSPI_8PAD, 0x04, STOP, FLEXSPI_1PAD, 0x00),

			/* Read status SPI */
			[4 * 1 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x05, READ_SDR, FLEXSPI_1PAD, 0x04),

			/* Read Status OPI */
			[4 * 2 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_8PAD, 0x05, CMD_SDR, FLEXSPI_8PAD, 0xFA),
			[4 * 2 + 1] = FLEXSPI_LUT_SEQ(RADDR_SDR, FLEXSPI_8PAD, 0x20, DUMMY_SDR, FLEXSPI_8PAD, 0x14),
			[4 * 2 + 2] = FLEXSPI_LUT_SEQ(READ_SDR, FLEXSPI_8PAD, 0x04, STOP, FLEXSPI_1PAD, 0x00),

			/* Write Enable */
			[4 * 3 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x06, STOP, FLEXSPI_1PAD, 0x00),

			/* Write Enable - OPI */
			[4 * 4 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_8PAD, 0x06, CMD_SDR, FLEXSPI_8PAD, 0xF9),

			/* Erase Sector */
			[4 * 5 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_8PAD, 0x21, CMD_SDR, FLEXSPI_8PAD, 0xDE),
			[4 * 5 + 1] = FLEXSPI_LUT_SEQ(RADDR_SDR, FLEXSPI_8PAD, 0x20, STOP, FLEXSPI_1PAD, 0x00),

			/* Configure dummy cycles */
			[4 * 6 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x72, CMD_SDR, FLEXSPI_1PAD, 0x00),
			[4 * 6 + 1] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x00, CMD_SDR, FLEXSPI_1PAD, 0x03),
			[4 * 6 + 2] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x00, WRITE_SDR, FLEXSPI_1PAD, 0x01),

			/* Configure Register */
			[4 * 7 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x72, CMD_SDR, FLEXSPI_1PAD, 0x00),
			[4 * 7 + 1] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x00, CMD_SDR, FLEXSPI_1PAD, 0x02),
			[4 * 7 + 2] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x00, WRITE_SDR, FLEXSPI_1PAD, 0x01),

			/* Erase Block */
			[4 * 8 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_8PAD, 0xDC, CMD_SDR, FLEXSPI_8PAD, 0x23),
			[4 * 8 + 1] = FLEXSPI_LUT_SEQ(RADDR_SDR, FLEXSPI_8PAD, 0x20, STOP, FLEXSPI_1PAD, 0x00),

			/* Page program */
			[4 * 9 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_8PAD, 0x12, CMD_SDR, FLEXSPI_8PAD, 0xED),
			[4 * 9 + 1] = FLEXSPI_LUT_SEQ(RADDR_SDR, FLEXSPI_8PAD, 0x20, WRITE_SDR, FLEXSPI_8PAD, 0x04),

			/* Enable OPI STR mode */
			[4 * 10 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x72, CMD_SDR, FLEXSPI_1PAD, 0x00),
			[4 * 10 + 1] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x00, CMD_SDR, FLEXSPI_1PAD, 0x00),
			[4 * 10 + 2] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x00, WRITE_SDR, FLEXSPI_1PAD, 0x01),

			/* Erase Chip */
			[4 * 11 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_8PAD, 0x60, CMD_SDR, FLEXSPI_8PAD, 0x9F),
		},
	},
	.pageSize           = 0x100,
	.sectorSize         = 0x1000,
	.ipcmdSerialClkFreq = 1u,
	.serialNorType      = 2u,
	.blockSize          = 0x10000,
	.flashStateCtx      = 0x07008100u,
};

#endif // defined XIP_BOOT_OCTASPI