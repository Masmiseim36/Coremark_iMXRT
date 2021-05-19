#include "flexspi_flash.h"
#if defined XIP_BOOT_OCTASPI	// Is defined in the iMXRT CPU Support package depended on the selected placement

/*******************************************************************************
 * Code
 ******************************************************************************/
#if defined(__CC_ARM) || defined(__GNUC__) || defined __SES_ARM || defined __CROSSWORKS_ARM
	__attribute__((section(".boot_hdr.conf")))
#elif defined(__ICCARM__)
	#pragma section="app_image"
	#pragma location=".boot_hdr.conf"
	__root
#else
	#error "Unknown Compiler"
#endif


const flexspi_nor_config_t FlashBootHeader =
{
	.memConfig =
	{
		.tag                  = FLEXSPI_CFG_BLK_TAG,
		.version              = FLEXSPI_CFG_BLK_VERSION,
		.readSampleClkSrc     = kFlexSPIReadSampleClk_ExternalInputFromDqsPad,
		.csHoldTime           = 3U,
		.csSetupTime          = 3U,
		.deviceModeCfgEnable  = 1U,
		.deviceModeType       = kDeviceConfigCmdType_Generic,
		.waitTimeCfgCommands  = 1U,
		.deviceModeSeq =
		{
			.seqNum   = 1U,
			.seqId    = 6U, // See Lookup table for more details
			.reserved = 0U,
		},
		.deviceModeArg        = 0U,
		.configCmdEnable      = 1U,
		.configModeType       = {kDeviceConfigCmdType_Generic, kDeviceConfigCmdType_Spi2Xpi, kDeviceConfigCmdType_Generic},
		.configCmdSeqs        =
		{
			{
				.seqNum       = 1U,
				.seqId        = 7U,
				.reserved     = 0U,
			},
			{
				.seqNum       = 1U,
				.seqId        = 10U,
				.reserved     = 0U,
			}
		},
		.configCmdArgs   = {0x2, 0x1},
		.controllerMiscOption = (1u << kFlexSpiMiscOffset_SafeConfigFreqEnable) | (1u << kFlexSpiMiscOffset_DdrModeEnable),
		.deviceType           = kFlexSpiDeviceType_SerialNOR,
		.sflashPadType        = kSerialFlash_8Pads,
		.serialClkFreq        = kFlexSpiSerialClk_100MHz,
		.sflashA1Size         = 64UL * 1024U * 1024U, // for iMXRT10xx / 11xx
		.sflashA2Size         = 0U,
		.sflashB1Size         = 0U, // for iMXRT500/600
		.sflashB2Size         = 0U,
		.busyOffset           = 0U,
		.busyBitPolarity      = 0U,
		.lookupTable =
		{
			// (0) Read
			[ 0] = FLEXSPI_LUT_SEQ (CMD_DDR,   FLEXSPI_8PAD, 0xEE, CMD_DDR,   FLEXSPI_8PAD, 0x11),
			[ 1] = FLEXSPI_LUT_SEQ (RADDR_DDR, FLEXSPI_8PAD, 0x20, DUMMY_DDR, FLEXSPI_8PAD, 0x04),
			[ 2] = FLEXSPI_LUT_SEQ (READ_DDR,  FLEXSPI_8PAD, 0x04, STOP,      FLEXSPI_1PAD, 0x00),
			[ 3] = 0,

			// (1) Read Status
			[ 4] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x05, READ_SDR,  FLEXSPI_1PAD, 0x04),

			// (3) Write Enable
			[12] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x06, STOP,      FLEXSPI_1PAD, 0x00),

			// (4) Page Program
			[16] = FLEXSPI_LUT_SEQ (CMD_DDR,   FLEXSPI_8PAD, 0x12, CMD_DDR,   FLEXSPI_8PAD, 0xED),
			[17] = FLEXSPI_LUT_SEQ (RADDR_DDR, FLEXSPI_8PAD, 32,   WRITE_DDR, FLEXSPI_8PAD, 128),

			// (6) Configure dummy cycles
			[24] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x72, CMD_SDR,   FLEXSPI_1PAD, 0x00),
			[25] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x00, CMD_SDR,   FLEXSPI_1PAD, 0x03),
			[26] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x00, WRITE_SDR, FLEXSPI_1PAD, 0x01),
			[27] = 0,

			// (7) Configure Register
			[28] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x72, CMD_SDR,   FLEXSPI_1PAD, 0x00),
			[29] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x00, CMD_SDR,   FLEXSPI_1PAD, 0x02),
			[30] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x00, WRITE_SDR, FLEXSPI_1PAD, 0x01),
			[31] = 0,

			// (10) Enable OPI STR mode
			[40] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x72, CMD_SDR,   FLEXSPI_1PAD, 0x00),
			[41] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x00, CMD_SDR,   FLEXSPI_1PAD, 0x00),
			[42] = FLEXSPI_LUT_SEQ (CMD_SDR,   FLEXSPI_1PAD, 0x00, WRITE_SDR, FLEXSPI_1PAD, 0x01),
		},
	},
	.pageSize    = 256U,
	.sectorSize  =  4U * 1024U,
	.blockSize   = 64U * 1024U,
};

#endif // defined XIP_BOOT_OCTASPI