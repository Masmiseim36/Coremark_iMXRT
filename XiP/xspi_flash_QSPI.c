/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "xspi_flash.h"


/*******************************************************************************
 * Code
 ******************************************************************************/
#if defined(BOOT_HEADER_ENABLE) && (BOOT_HEADER_ENABLE == 1) && defined XIP_BOOT_QSPI
	#if defined(__ARMCC_VERSION) || defined(__GNUC__) || defined __CROSSWORKS_ARM
		__attribute__((section(".flash_conf"), used))
	#elif defined(__ICCARM__)
		#pragma location = ".flash_conf"
	#endif

	/*W25Q25PW @ QSPI / SDR / 133MHz / Read=0xEC (1-4-4, 4-Byte Address) */
	const fc_static_platform_config_t flash_config =
	{
		.xspi_fcb_block =
		{
			.memConfig =
			{
				.tag                  = FC_XSPI_CFG_BLK_TAG,
				.version              = FC_XSPI_CFG_BLK_VERSION,
				.readSampleClkSrc     = kXSPIReadSampleClk_LoopbackFromDqsPad,
				.csHoldTime           = 3U,
				.csSetupTime          = 3U,
				.controllerMiscOption = (1U << Fc_XspiMiscOffset_SafeConfigFreqEnable),
				.deviceType           = 1U,
				.sflashPadType        = 4U,
				.serialClkFreq        = Fc_XspiSerialClk_166MHz,
				.sflashA1Size         = 32UL * 1024U * 1024U,
				.lutCustomSeqEnable   = 0U,
				.lookupTable =
				{
					/*Read*/
					[0] = FC_XSPI_LUT_SEQ (FC_CMD_SDR,      FC_XSPI_1PAD, 0xEC, FC_CMD_RADDR_SDR, FC_XSPI_4PAD, 0x20),
					[1] = FC_XSPI_LUT_SEQ (FC_CMD_MODE_SDR, FC_XSPI_4PAD, 0x00, FC_CMD_DUMMY_SDR, FC_XSPI_4PAD, 0x04),
					[2] = FC_XSPI_LUT_SEQ (FC_CMD_READ_SDR, FC_XSPI_4PAD, 0x04, FC_CMD_STOP,      FC_XSPI_1PAD, 0x0),

					/*Read status SPI*/
					[5 * 1 + 0] = FC_XSPI_LUT_SEQ (FC_CMD_SDR, FC_XSPI_1PAD, 0x05, FC_CMD_READ_SDR, FC_XSPI_1PAD, 0x04),

					/*Write enable*/
					[5 * 3 + 0] = FC_XSPI_LUT_SEQ (FC_CMD_SDR, FC_XSPI_1PAD, 0x06, FC_CMD_STOP, FC_XSPI_1PAD, 0x04),

					/* Erase Sector */
					[5 * 5 + 0] = FC_XSPI_LUT_SEQ (FC_CMD_SDR, FC_XSPI_4PAD, 0x21, FC_CMD_RADDR_SDR, FC_XSPI_4PAD, 0x20),

					/* Page program */
					[5 * 9 + 0] = FC_XSPI_LUT_SEQ (FC_CMD_SDR,       FC_XSPI_4PAD, 0x12, FC_CMD_RADDR_SDR, FC_XSPI_4PAD, 0x20),
					[5 * 9 + 1] = FC_XSPI_LUT_SEQ (FC_CMD_WRITE_SDR, FC_XSPI_4PAD, 0x4, FC_CMD_STOP, FC_XSPI_4PAD, 0x0),

					/* Erase Chip */
					[5 * 13 + 0] = FC_XSPI_LUT_SEQ (FC_CMD_SDR, FC_XSPI_4PAD, 0x60, FC_CMD_STOP, FC_XSPI_4PAD, 0x0),
				},
			},
			.pageSize           = 256u,
			.sectorSize         = 4u * 1024u,
			.ipcmdSerialClkFreq = 1u,
			.blockSize          = 64u * 1024u,
			.flashStateCtx      = 0x06004100u,
		},
		#ifdef BOOT_ENABLE_XSPI1_PSRAM
			.psram_config_block =
			{
				.xmcdHeader = 0xC0010008,
				.xmcdOpt0   = 0xC0000700,
			},
		#endif
	};

#endif /* BOOT_HEADER_ENABLE */
