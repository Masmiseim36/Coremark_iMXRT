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
		.csHoldTime           = 3U,
		.csSetupTime          = 3U,
		.deviceModeCfgEnable  = 1U,
		.deviceModeType       = kDeviceConfigCmdType_Generic,
		.waitTimeCfgCommands  = 1U,
		.deviceModeSeq =
		{
			.seqNum           = 1U,
			.seqId            = 6, // See Lookup table for more details
			.reserved         = 0U,
		},
		.deviceModeArg        = DUMMY_CYCLES,
		.configCmdEnable      = 1,
		.configModeType       = {kDeviceConfigCmdType_Generic, kDeviceConfigCmdType_Spi2Xpi,kDeviceConfigCmdType_Generic},
		.configCmdSeqs        = 
		{
			{
				.seqNum       = 1,
				.seqId        = 7,
				.reserved     = 0,
			},
			{
				.seqNum       = 1,
				.seqId        = 10,
				.reserved     = 0,
			}
		},
		.configCmdArgs        = {0x2, 0x2},
		.controllerMiscOption = (1u << kFlexSpiMiscOffset_SafeConfigFreqEnable) | (1u << kFlexSpiMiscOffset_DdrModeEnable),
		.deviceType           = kFlexSpiDeviceType_SerialNOR,
		.sflashPadType        = kSerialFlash_8Pads,
		.serialClkFreq        = kFlexSpiSerialClk_50MHz,
		.sflashA1Size         = 0U,
		.sflashA2Size         = 0U,
		.sflashB1Size         = 64UL * 1024U * 1024U,
		.sflashB2Size         = 0U,
		.busyOffset           = 0U,
		.busyBitPolarity      = 0U,
		.lookupTable =
		{
			// (0) Read
			[ 0] = FLEXSPI_LUT_SEQ (CMD_DDR,   FLEXSPI_8PAD, 0xEE, CMD_DDR,   FLEXSPI_8PAD, 0x11),
			[ 1] = FLEXSPI_LUT_SEQ (RADDR_DDR, FLEXSPI_8PAD, 0x20, DUMMY_DDR, FLEXSPI_8PAD, (DUMMY_CYCLES * 2) +1),
			[ 2] = FLEXSPI_LUT_SEQ (READ_DDR,  FLEXSPI_8PAD, 0x04, STOP,      FLEXSPI_1PAD, 0x00),
			[ 3] = 0,

			// (1) Read Status - SPI
			[ 4] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x05, READ_SDR,  FLEXSPI_1PAD, 0x04),

			// (2) Read Status - OPI
			[ 8] = FLEXSPI_LUT_SEQ (CMD_DDR,   FLEXSPI_8PAD, 0x05, CMD_DDR,   FLEXSPI_8PAD, 0xFA),
			[ 9] = FLEXSPI_LUT_SEQ (RADDR_DDR, FLEXSPI_8PAD, 0x20, DUMMY_DDR, FLEXSPI_8PAD, (DUMMY_CYCLES * 2) +1),
			[10] = FLEXSPI_LUT_SEQ (READ_DDR,  FLEXSPI_8PAD, 0x04, STOP,      FLEXSPI_1PAD, 0x00),

			// (3) Write Enable - SPI
			[12] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x06, STOP,      FLEXSPI_1PAD, 0x00),

			// (4) Write Enable - OPI
			[16] = FLEXSPI_LUT_SEQ (CMD_DDR,   FLEXSPI_8PAD, 0x06, CMD_DDR,   FLEXSPI_8PAD, 0xF9),

			// (5) Erase Sector - OPI
			[20] = FLEXSPI_LUT_SEQ (CMD_DDR,   FLEXSPI_8PAD, 0x21, CMD_DDR,   FLEXSPI_8PAD, 0xDE),
			[21] = FLEXSPI_LUT_SEQ (RADDR_DDR, FLEXSPI_8PAD, 0x20, STOP,      FLEXSPI_1PAD, 0x00),

			// (6) Configure Register - Set dummy cycles - SPI
			[24] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x72, CMD_SDR,   FLEXSPI_1PAD, 0x00),
			[25] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x00, CMD_SDR,   FLEXSPI_1PAD, 0x03),
			[26] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x00, WRITE_SDR, FLEXSPI_1PAD, 0x01), // --> 6 Dummy Cycles

			// (7) Configure Register - Disable DQS on STR mode - SPI
			[28] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x72, CMD_SDR,   FLEXSPI_1PAD, 0x00),
			[29] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x00, CMD_SDR,   FLEXSPI_1PAD, 0x02),
			[30] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x00, WRITE_SDR, FLEXSPI_1PAD, 0x01),

			// (8) Erase Block - OPI
			[32] = FLEXSPI_LUT_SEQ (CMD_DDR,   FLEXSPI_8PAD, 0xDC, CMD_DDR,   FLEXSPI_8PAD, 0x23),
			[33] = FLEXSPI_LUT_SEQ (RADDR_DDR, FLEXSPI_8PAD, 0x20, STOP,      FLEXSPI_1PAD, 0x00),

			// (9) Page program - OPI
			[36] = FLEXSPI_LUT_SEQ (CMD_DDR,   FLEXSPI_8PAD, 0x12, CMD_DDR,   FLEXSPI_8PAD, 0xED),
			[37] = FLEXSPI_LUT_SEQ (RADDR_DDR, FLEXSPI_8PAD, 0x20, WRITE_DDR, FLEXSPI_8PAD, 0x04),

			// (10) Configure Register - Enable OPI DDR mode - SPI
			[40] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x72, CMD_SDR,   FLEXSPI_1PAD, 0x00),
			[41] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x00, CMD_SDR,   FLEXSPI_1PAD, 0x00),
			[42] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x00, WRITE_SDR, FLEXSPI_1PAD, 0x01), // --> 2 = OPI + DDR - 1 = OPI + SDR

			// (11) Erase Chip - OPI
			[44] = FLEXSPI_LUT_SEQ (CMD_DDR,   FLEXSPI_8PAD, 0x60, CMD_DDR,   FLEXSPI_8PAD, 0x9F),
		},
	},
	.pageSize           = 256U,
	.sectorSize         = 4U * 1024U,
	.ipcmdSerialClkFreq = 1U,
	.serialNorType      = SerialNorType_XPI,
	.blockSize          = 64U * 1024U,
	.flashStateCtx      = 0x07008200U
};

#endif // defined XIP_BOOT_OCTASPI