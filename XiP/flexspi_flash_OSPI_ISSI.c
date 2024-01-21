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

#define DUMMY_CYCLES 16	/* Number of dummy cycles after Read Command for ISSI-Flash */

const flexspi_nor_config_t FlashBootHeader =
{
	.memConfig =
	{
		.tag                  = FLEXSPI_CFG_BLK_TAG,
		.version              = FLEXSPI_CFG_BLK_VERSION,
		.readSampleClkSrc     = kFlexSPIReadSampleClk_ExternalInputFromDqsPad,
		.csHoldTime           = 3U,
		.csSetupTime          = 3U,
		.columnAddressWidth   = 0U,
		.deviceModeCfgEnable  = 1U,
		.deviceModeType       = kDeviceConfigCmdType_Spi2Xpi,
		.waitTimeCfgCommands  = 1U,
		.deviceModeSeq        =
		{
			.seqNum   = 1U,
			.seqId    = 15U, // This will switch to Octal-DDR mode
			.reserved = 0U,
		},
		.deviceModeArg = 0xE7,
		.configCmdSeqs        =   // Sequences for configuration command, allow 3 separate configuration command sequences
		{
			{.seqId=13, .seqNum=1}, // This will modify the number of dummy cycles used
		},
		.configCmdArgs        =
		{
			DUMMY_CYCLES,
//			0xE7,
		},
		// Enable DDR mode, Safe configuration
		.controllerMiscOption = (1u << kFlexSpiMiscOffset_SafeConfigFreqEnable) | (1u << kFlexSpiMiscOffset_DdrModeEnable),
		.deviceType           = kFlexSpiDeviceType_SerialNOR, // serial NOR
		.sflashPadType        = kSerialFlash_8Pads,
		.serialClkFreq        = kFlexSpiSerialClk_133MHz,
		.sflashA1Size         = 8 * 1024U * 1024U,
		.sflashA2Size         = 0U,
		.sflashB1Size         = 0U, // for iMXRT500/600
		.sflashB2Size         = 0U,
		.dataValidTime =
		{
//			[0] = {.time_100ps = 16},
			[0] = 16,
			[1] = 0
		},
		.busyOffset           = 0U, // busy bit in bit 0
		.busyBitPolarity      = 0U, // busy bit is 1 when device is busy

		.csPadSettingOverride   = 0xA,
		.sclkPadSettingOverride = 0xA,
		.dataPadSettingOverride = 0xA,
		.dqsPadSettingOverride  = 0xA,

		.lookupTable =
		{
			// (0) Read
			[ 0] = FLEXSPI_LUT_SEQ (CMD_SDR,        FLEXSPI_8PAD, 0xFD, RADDR_DDR, FLEXSPI_8PAD, 32),
			[ 1] = FLEXSPI_LUT_SEQ (DUMMY_RWDS_DDR, FLEXSPI_8PAD, DUMMY_CYCLES, READ_DDR,    FLEXSPI_8PAD, 128),

			// (1) Read Status (byte 1)
			[ 4] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_8PAD, 0x05, DUMMY_DDR, FLEXSPI_8PAD, DUMMY_CYCLES),
			[ 5] = FLEXSPI_LUT_SEQ (READ_DDR,  FLEXSPI_8PAD, 0x01, STOP,      FLEXSPI_1PAD, 0x0),

			// (3) Write Enable
			[12] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x06, STOP,      FLEXSPI_1PAD, 0x0),

			// (4) Page Program
			[16] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_8PAD, 0x8E, RADDR_DDR, FLEXSPI_8PAD, 32),
			[17] = FLEXSPI_LUT_SEQ (WRITE_DDR, FLEXSPI_8PAD, 128,  STOP,      FLEXSPI_1PAD, 0),

			// (13) Write to non volatile control register at address 1 to set the dummy cycles - Octa + DDR
			[52] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_8PAD, 0x81, CMD_DDR,   FLEXSPI_8PAD, 0x00), // Command + Address [24-16]
			[53] = FLEXSPI_LUT_SEQ (CMD_DDR,   FLEXSPI_8PAD, 0x00, CMD_DDR,   FLEXSPI_8PAD, 0x01), // Address [15-8] + Address [7-0]
			[54] = FLEXSPI_LUT_SEQ (WRITE_DDR, FLEXSPI_8PAD, 0x01, STOP,      FLEXSPI_8PAD, 0x00), // Data --> Dummy Cycles
			[55] = 0,

			// (14) Write to non volatile control register at address 1 to set the dummy cycles - SPI
			[56] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_8PAD, 0x81, CMD_SDR,   FLEXSPI_1PAD, 0x00), // Command + Address [24-16]
			[57] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_8PAD, 0x00, CMD_SDR,   FLEXSPI_1PAD, 0x01), // Address [15-8] + Address [7-0]
			[58] = FLEXSPI_LUT_SEQ (WRITE_SDR, FLEXSPI_8PAD, 0x01, STOP,      FLEXSPI_1PAD, 0x00), // Data --> Dummy Cycles
			[59] = 0,
			// (15) Write to non volatile control register at address 0 to switch to chosen interface-Type - SPI
			[60] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x81, CMD_SDR,   FLEXSPI_1PAD, 0x00), // Command + Address [24-16]
			[61] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x00, CMD_SDR,   FLEXSPI_1PAD, 0x00), // Address [15-8] + Address [7-0]
			[62] = FLEXSPI_LUT_SEQ (WRITE_SDR, FLEXSPI_1PAD, 0x01, STOP,      FLEXSPI_1PAD, 0x00), // Data --> Octa SPI + DDR Mode
			[63] = 0
		},
	},
	.pageSize           = 256U,
	.sectorSize         = 4U * 1024U, // 4K
	.ipcmdSerialClkFreq = 1, // 30MHz
	.blockSize          = 64U * 1024U,
	.isUniformBlockSize = true,
};

#endif	// #if defined XIP_BOOT_OCTASPI
