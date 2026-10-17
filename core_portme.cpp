/* Copyright 2021 Markus Klein

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include <stdio.h>
#include <stdlib.h>
#include <debugio.h>
#include "board.h"

#include "pin_mux.h"
#include "clock_config.h"
#include "fsl_clock.h"
#if defined LPUART_BASE_PTRS
	#include "fsl_lpuart.h"
#elif defined USART_BASE_PTRS
	#include "fsl_usart.h"
#else
	#error "unknown serial port driver"
#endif
#include "fsl_common.h"
#if (defined(MIMXRT118C_cm33_SERIES) || defined(MIMXRT1185_cm33_SERIES) || defined(MIMXRT1186_cm33_SERIES) || defined(MIMXRT1187_cm33_SERIES) || defined(MIMXRT1189_cm33_SERIES))
	#include "fsl_ele_base_api.h"
#endif

extern "C"
{
	#include "core_portme.h"
	#include "coremark.h"
	#include <STDARG.H>
	#include <stdio.h>
	using namespace std;

	ee_u32 default_num_contexts=1;
	#if VALIDATION_RUN
		volatile ee_s32 seed1_volatile=0x3415;
		volatile ee_s32 seed2_volatile=0x3415;
		volatile ee_s32 seed3_volatile=0x66;
	#endif
	#if PERFORMANCE_RUN
		volatile ee_s32 seed1_volatile=0x0;
		volatile ee_s32 seed2_volatile=0x0;
		volatile ee_s32 seed3_volatile=0x66;
	#endif
	#if PROFILE_RUN
		volatile ee_s32 seed1_volatile=0x8;
		volatile ee_s32 seed2_volatile=0x8;
		volatile ee_s32 seed3_volatile=0x8;
	#endif
	#if __CORTEX_M == 7
		volatile ee_s32 seed4_volatile = 50000;
	#else
		volatile ee_s32 seed4_volatile = 15000;
	#endif
	volatile ee_s32 seed5_volatile=0;




	volatile uint32_t TickCounter = 0;
	uint32_t TickLength = 0;
	uint32_t StartTick = 0;
	uint32_t StopTick = 0;


	void SysTick_Handler (void)
	{
		TickCounter++;
	}

	bool InitializeSystemTick (uint32_t SysTickInMS)
	{
		if (SysTickInMS == 0)
			return false;

		TickLength = SysTickInMS;


		SysTick->CTRL	= 0;
		SysTick->LOAD	= (SystemCoreClock / (1000/SysTickInMS)) -1;
		SysTick->VAL	= 0;
		SysTick->CTRL	= SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

		__enable_irq ();
		return true;
	}



	#if defined LPUART_BASE_PTRS
		static LPUART_Type * const uart [] = LPUART_BASE_PTRS;
	#elif defined USART_BASE_PTRS
		static USART_Type * const uart [] = USART_BASE_PTRS;
	#else
		#error "unknown serial port driver"
	#endif

	#if (defined(MIMXRT118C_cm33_SERIES) || defined(MIMXRT1185_cm33_SERIES) || defined(MIMXRT1186_cm33_SERIES) || defined(MIMXRT1187_cm33_SERIES) || defined(MIMXRT1189_cm33_SERIES))
		// Step 8 of the CM7 kick-off: ensure execution access of the CM7 to its image in the flash.
		// The CM7 runs XiP from the same flash as the CM33. After boot the TRDC denies the CM7 the access, the first vector
		// fetch ends with a bus error (CM7 CFSR.IBUSERR, HardFault).
		// - FlexSPI2 is checked by the memory region checker MRC_A1 of TRDC1 (AON domain, ELE RDC ID 0x74).
		//   FlexSPI1 is checked by the memory region checker MRC_W1 of TRDC2 (WAKEUP domain, ELE RDC ID 0x78).
		// - The TRDCs are owned by the ELE after boot and have to be released to the CM33 before they can be written.
		//   A repeated release request (e.g. after a debugger reset without power cycle) may be refused by the ELE.
		//   TRDC3 (MEGA) must be released before TRDC2 (WAKEUP), otherwise the release of TRDC3 fails.
		// - The CM7 has the domain ID 4 (TRDC2 MDAC_W0/W1).
		// - Although the CM7 uses the non-secure address 0x04xxxxxx, its accesses arrive as secure privileged at the TRDC
		//   (MRC_A1 error capture: EDID 4, EATR "secure privileged data"). The region must therefore have NSE = 0.
		// - If several regions of a domain hit, a denying region wins. So the CM7 gets exactly one valid region.
		// - The MRC only sees accesses to its FlexSPI, therefore the region can cover the whole address range.
		static void GrantCm7FlashAccess ()
		{
			#if defined XIP_BOOT_FLEXSPI1
				TRDC_Type *const trdc               = TRDC2;
				static constexpr uint32_t EleRdcId  = 0x78;
				static constexpr char     MrcName[] = "TRDC2 MRC_W1";
			#elif defined XIP_BOOT_FLEXSPI2
				TRDC_Type *const trdc               = TRDC1;
				static constexpr uint32_t EleRdcId  = 0x74;
				static constexpr char     MrcName[] = "TRDC1 MRC_A1";
			#else
				#error "Only Boot from FlexSPI"
			#endif
			static constexpr uint32_t EleCoreCm33Id = 0x01;
			static constexpr uint32_t FlexSpiMrcIdx = 1;
			static constexpr uint32_t MrcRegions    = 8;
			static constexpr uint32_t MrcAccessSets = 8;

			if (ELE_BaseAPI_ReleaseRDC (MU_RT_S3MUA, EleRdcId, EleCoreCm33Id) != kStatus_Success)
			{
				MyPrintf ("CM7 kick-off: %s not released by the ELE\r\n", MrcName);
				return;
			}

			auto &mrc = trdc->MRC_INDEX[FlexSpiMrcIdx];

			// Use the highest access control policy which is not locked
			uint32_t accessSet = MrcAccessSets;
			while ((accessSet > 0) && ((mrc.MRC_GLBAC[accessSet - 1] & TRDC_MRC_GLBAC_LK_MASK) != 0UL))
			{
				accessSet--;
			}

			if (accessSet == 0)
			{
				MyPrintf ("CM7 kick-off: all access control policies of %s are locked\r\n", MrcName);
				return;
			}

			accessSet--;
			mrc.MRC_GLBAC[accessSet] = TRDC_MRC_GLBAC_NUX_MASK | TRDC_MRC_GLBAC_NUW_MASK | TRDC_MRC_GLBAC_NUR_MASK |
									   TRDC_MRC_GLBAC_NPX_MASK | TRDC_MRC_GLBAC_NPW_MASK | TRDC_MRC_GLBAC_NPR_MASK |
									   TRDC_MRC_GLBAC_SUX_MASK | TRDC_MRC_GLBAC_SUW_MASK | TRDC_MRC_GLBAC_SUR_MASK |
									   TRDC_MRC_GLBAC_SPX_MASK | TRDC_MRC_GLBAC_SPW_MASK | TRDC_MRC_GLBAC_SPR_MASK;

			for (uint32_t region = 0; region < MrcRegions; region++)
			{
				mrc.MRC_DOM4_RGD_W[region][1] = 0;
			}
			mrc.MRC_DOM4_RGD_W[0][0] = TRDC_MRC_DOM0_RGD_W_MRACSEL(accessSet);
			mrc.MRC_DOM4_RGD_W[0][1] = TRDC_MRC_DOM0_RGD_W_END_ADDR_MASK | TRDC_MRC_DOM0_RGD_W_NSE(0) | TRDC_MRC_DOM0_RGD_W_VLD(1);
		}
	#endif

	void portable_init (core_portable *p, int *argc, char *argv[])
	{
		// Initialize board hardware.
		BOARD_ConfigMPU       ();
		BOARD_InitPins        ();
		BOARD_BootClockRUN    ();
		SystemCoreClockUpdate ();

		#if defined LPUART_BASE_PTRS
			#if (defined(MIMXRT1011_SERIES) || defined(MIMXRT1015_SERIES) || defined(MIMXRT1021_SERIES) || defined(MIMXRT1024_SERIES) || \
				 defined(MIMXRT1041_SERIES) || defined(MIMXRT1042_SERIES) || defined(MIMXRT1051_SERIES) || defined(MIMXRT1052_SERIES) || \
				 defined(MIMXRT1061_SERIES) || defined(MIMXRT1062_SERIES) || defined(MIMXRT1064_SERIES))
				uint32_t ClockFrequency = 0;
				if (CLOCK_GetMux (kCLOCK_UartMux) == 0) // --> PLL3 div6 80M
					ClockFrequency = (CLOCK_GetPllFreq (kCLOCK_PllUsb1) / 6U) / (CLOCK_GetDiv(kCLOCK_UartDiv) + 1U);
				else
					ClockFrequency = CLOCK_GetOscFreq() / (CLOCK_GetDiv(kCLOCK_UartDiv) + 1U);
			#elif (defined(MIMXRT1165_cm7_SERIES) || defined(MIMXRT1166_cm7_SERIES) || defined(MIMXRT1165_cm4_SERIES) || defined(MIMXRT1166_cm4_SERIES) || \
				   defined(MIMXRT1171_SERIES)     || defined(MIMXRT1172_SERIES)     || \
				   defined(MIMXRT1173_cm7_SERIES) || defined(MIMXRT1175_cm7_SERIES) || defined(MIMXRT1176_cm7_SERIES) || \
				   defined(MIMXRT1173_cm4_SERIES) || defined(MIMXRT1175_cm4_SERIES) || defined(MIMXRT1176_cm4_SERIES))
				// Configure Lpuartx using SysPll2
				static const clock_root_t RootClocks [] = {static_cast<clock_root_t>(0x7F), kCLOCK_Root_Lpuart1, kCLOCK_Root_Lpuart2, kCLOCK_Root_Lpuart3, kCLOCK_Root_Lpuart4, kCLOCK_Root_Lpuart5, kCLOCK_Root_Lpuart6, kCLOCK_Root_Lpuart7, kCLOCK_Root_Lpuart8, kCLOCK_Root_Lpuart9, kCLOCK_Root_Lpuart10, kCLOCK_Root_Lpuart11, kCLOCK_Root_Lpuart12};
				static const _clock_lpcg clocks [] = LPUART_CLOCKS;

				clock_root_config_t rootCfg {};
				rootCfg.mux = 6;
				rootCfg.div = 21;
				CLOCK_SetRootClock (RootClocks[BOARD_DEBUG_UART_INSTANCE], &rootCfg);
				CLOCK_ControlGate  (clocks[BOARD_DEBUG_UART_INSTANCE], kCLOCK_On);

				uint32_t ClockFrequency = CLOCK_GetRootClockFreq (RootClocks[BOARD_DEBUG_UART_INSTANCE]);
			#elif (defined(MIMXRT1181_SERIES)      || defined(MIMXRT1182_SERIES)     || defined(MIMXRT118C_cm33_SERIES) || defined(MIMXRT118C_cm7_SERIES) || \
				   defined(MIMXRT1185_cm33_SERIES) || defined(MIMXRT1185_cm7_SERIES) || defined(MIMXRT1186_cm33_SERIES) || defined(MIMXRT1186_cm7_SERIES) || \
				   defined(MIMXRT1187_cm33_SERIES) || defined(MIMXRT1187_cm7_SERIES) || defined(MIMXRT1189_cm33_SERIES) || defined(MIMXRT1189_cm7_SERIES))
				// Configure Lpuartx using SysPll2
				static const clock_root_t RootClocks [] 
				{
	 				static_cast<clock_root_t>(0x7F), kCLOCK_Root_Lpuart0102, kCLOCK_Root_Lpuart0102, kCLOCK_Root_Lpuart0304, kCLOCK_Root_Lpuart0304,
					kCLOCK_Root_Lpuart0506, kCLOCK_Root_Lpuart0506, kCLOCK_Root_Lpuart0708, kCLOCK_Root_Lpuart0708, kCLOCK_Root_Lpuart0910,
					kCLOCK_Root_Lpuart0910, kCLOCK_Root_Lpuart1112, kCLOCK_Root_Lpuart1112
				};
				static const clock_ip_name_t clocks [] = LPUART_CLOCKS;

				clock_root_config_t rootCfg {};
				rootCfg.mux = 6;
				rootCfg.div = 21;
				CLOCK_SetRootClock (RootClocks[BOARD_DEBUG_UART_INSTANCE], &rootCfg);
				CLOCK_ControlGate  (clocks[BOARD_DEBUG_UART_INSTANCE], kCLOCK_On);

				uint32_t ClockFrequency = CLOCK_GetRootClockFreq (RootClocks[BOARD_DEBUG_UART_INSTANCE]);
			#else
				#error "unknon controller family"
			#endif

			lpuart_config_t config;
			LPUART_GetDefaultConfig (&config);
			config.baudRate_Bps = 115200U;
			config.enableTx     = true;
			config.enableRx     = true;
			[[maybe_unused]] status_t status = LPUART_Init (uart[BOARD_DEBUG_UART_INSTANCE], &config, ClockFrequency);
		#elif defined USART_BASE_PTRS
			const clock_frg_clk_config_t ClockConfig = 
			{
				0, _clock_frg_clk_config::kCLOCK_FrgPllDiv, 255, 0
			};

			CLOCK_SetFRGClock (&ClockConfig);
			const clock_attach_id_t Clock[] = {kFRG_to_FLEXCOMM0, kFRG_to_FLEXCOMM1, kFRG_to_FLEXCOMM2, kFRG_to_FLEXCOMM3, kFRG_to_FLEXCOMM4, kFRG_to_FLEXCOMM5, kFRG_to_FLEXCOMM6, kFRG_to_FLEXCOMM7};
			CLOCK_AttachClk   (Clock[BOARD_DEBUG_UART_INSTANCE]);


			usart_config_t config;
			USART_GetDefaultConfig (&config);
			config.baudRate_Bps     = 115200U;
			config.enableTx         = true;
			config.enableRx         = true;
			[[maybe_unused]] status_t status         = USART_Init (uart[BOARD_DEBUG_UART_INSTANCE], &config, CLOCK_GetFlexCommClkFreq(BOARD_DEBUG_UART_INSTANCE));
		#else
			#error "unknown serial port driver"
		#endif


		InitializeSystemTick (1);

		MyPrintf ("Hello Coremark on M%d core\r\n", __CORTEX_M);
		MyPrintf ("Compiled at " __DATE__ " - " __TIME__ "\r\n");
		#if defined GCC_OPTIONS
			MyPrintf ("\r\n");
			MyPrintf (GCC_OPTIONS);
			MyPrintf ("\r\n");
		#endif

		#if defined MIMXRT1166_cm7_SERIES || defined MIMXRT1165_cm7_SERIES || \
			defined MIMXRT1173_cm7_SERIES || defined MIMXRT1175_cm7_SERIES || defined MIMXRT1176_cm7_SERIES
			// start the M4 core
			extern uint32_t __FlexSPI_segment_end__;
			const uint32_t bootAddress = (uint32_t)&__FlexSPI_segment_end__;
			IOMUXC_LPSR_GPR->GPR0 = IOMUXC_LPSR_GPR_GPR0_CM4_INIT_VTOR_LOW ((uint32_t)bootAddress >> 3);
			IOMUXC_LPSR_GPR->GPR1 = IOMUXC_LPSR_GPR_GPR1_CM4_INIT_VTOR_HIGH((uint32_t)bootAddress >> 16);
			SRC->SCR |= SRC_SCR_BT_RELEASE_M4_MASK;
		#elif (defined(MIMXRT118C_cm33_SERIES) || defined(MIMXRT1185_cm33_SERIES) || defined(MIMXRT1186_cm33_SERIES) || defined(MIMXRT1187_cm33_SERIES) || defined(MIMXRT1189_cm33_SERIES))
			// Start the M7 core
			// Compare Chapter 12.10 "Cortex-M7 kick-off procedure" of the reference manual

			// Step 1: After POR the CM7 is held in reset and CPUWAIT is set.
			// The debugger script (Release_118x_M7) already performs the kick-off and finally clears CPUWAIT.
			// SRC_GENERAL_REG->SCR is no indicator: with fuse POR_PRELOAD_CM7_TCM_ECC the ROM already releases the CM7 reset.
			if ((BLK_CTRL_S_AONMIX->M7_CFG & BLK_CTRL_S_AONMIX_M7_CFG_WAIT_MASK) == 0UL)
			{
				// The debugger script doesn't configure the TRDC
				GrantCm7FlashAccess ();
			}
			else
			{
				const uint32_t coreClockFreq = CLOCK_GetCpuClkFreq ();

				// Preparation: core voltage and CM7 clock
				// DCDC_SetVDD1P0BuckModeTargetVoltage(DCDC, kDCDC_1P0Target1P1V);
				DCDC->REG3                 = DCDC_REG3_REG_FBK_SEL(2) | DCDC_REG3_DISABLE_IDLE_SKIP_MASK | DCDC_REG3_DISABLE_PULSE_SKIP_MASK;
				DCDC->TRG_SW_0             = DCDC_TRG_SW_0_VDD1P0CTRL_LP_TRG(0x10) | DCDC_TRG_SW_0_VDD1P8CTRL_TRG(0xC) | DCDC_TRG_SW_0_VDD1P0CTRL_TRG(0x14);

				// M7 clock from the ARM PLL, which is already configured by BOARD_BootClockRUN.
				// Unlike the debugger script, don't switch the M33 clock root to RC24M: running XiP from FlexSPI2 requires
				// the M33 clock root to be higher than 1/4 of the FlexSPI2 clock root.
				CCM->CLOCK_ROOT[kCLOCK_Root_M7].CONTROL = CCM_CLOCK_ROOT_CONTROL_MUX(kCLOCK_M7_ClockRoot_MuxArmPllOut) | CCM_CLOCK_ROOT_CONTROL_DIV(1);
				CCM->LPCG[kCLOCK_M7].DIRECT             = CCM_LPCG_DIRECT_ON_MASK;
				SDK_DelayAtLeastUs (10, coreClockFreq);

				// Step 2: Set initial VTOR (address >> 7)
				// The CM7 image follows directly the CM33 image in the flash. __FlexSPIx_segment_end__ is the secure CM33 address,
				// the CM7 sees the flash at the non-secure address.
				static constexpr uint32_t SecureAdressOffset = 0x10000000;
				#if defined XIP_BOOT_FLEXSPI1
					extern uint32_t __FlexSPI_segment_end__[];
					const uint32_t *const cm7Vectors = __FlexSPI_segment_end__;
				#elif defined XIP_BOOT_FLEXSPI2
					extern uint32_t __FlexSPI2_segment_end__[];
					const uint32_t *const cm7Vectors = __FlexSPI2_segment_end__;
				#else
					#error "Only Boot from FlexSPI"
				#endif
				const uint32_t bootaddr = (reinterpret_cast<uint32_t>(cm7Vectors) - SecureAdressOffset) >> 7;
				BLK_CTRL_S_AONMIX->M7_CFG = BLK_CTRL_S_AONMIX_M7_CFG_INITVTOR(bootaddr) | BLK_CTRL_S_AONMIX_M7_CFG_WAIT(1);

				// Step 3: The default TCM size (256 KB ITCM, 256 KB DTCM) is used

				// Step 4: Release Cortex-M7 reset
				SRC_GENERAL_REG->SCR = SRC_GENERAL_SCR_BT_RELEASE_M7_MASK;

				// Step 5: Initialize the TCM ECC by 64-bit writes. The written data is irrelevant.
				// The source must be reachable by the eDMA4. The stack is located in the M33 System TCM at its CPU-local
				// address 0x30000000, which ends with a source bus error. The DMA reads the M33 System TCM via its bus alias.
				uint32_t dmaError = 0;
				auto InitM7Mem = [&dmaError](uint32_t targetAddr)
				{
					static constexpr uint32_t M33SysTcmBusAlias = 0x20200000;
					DMA4->TCD[0].SADDR          = M33SysTcmBusAlias;
					DMA4->TCD[0].DADDR          = targetAddr;
					DMA4->TCD[0].NBYTES_MLOFFNO = DMA4_NBYTES_MLOFFNO_NBYTES(0x20000); // Number of Bytes to transfer per service request
					DMA4->TCD[0].CITER_ELINKNO  = DMA4_CITER_ELINKNO_CITER(0x1);       // Current Major Iteration Count
					DMA4->TCD[0].BITER_ELINKNO  = DMA4_BITER_ELINKNO_BITER(0x1);       // Starting Major Iteration Count
					DMA4->TCD[0].ATTR           = DMA4_ATTR_SSIZE(3) | DMA4_ATTR_DSIZE(3); // 0x303 - Source Size and and destinatiuon offset ==> 64-Bit
					DMA4->TCD[0].SOFF           = DMA4_SOFF_SOFF(0);   // 0x0 . Source Offset => 0
					DMA4->TCD[0].DOFF           = DMA4_DOFF_DOFF(0x8); // Destination Offset 8-Bytes
					DMA4->TCD[0].CH_CSR         = DMA4_CH_CSR_EEI_MASK | DMA4_CH_CSR_EARQ_MASK | DMA4_CH_CSR_ERQ_MASK; // 0x7;
					DMA4->TCD[0].CSR            = DMA4_CSR_DREQ_MASK;  // 0x8
					DMA4->TCD[0].CSR            = DMA4_CSR_DREQ_MASK | DMA4_CSR_START_MASK; // 0x9

					while (((DMA4->TCD[0].CH_CSR & DMA4_CH_CSR_DONE_MASK) == 0UL) && ((DMA4->TCD[0].CH_ES & DMA4_CH_ES_ERR_MASK) == 0UL))
					{
					}

					dmaError |= DMA4->TCD[0].CH_ES;
					DMA4->TCD[0].CH_ES  = DMA4_CH_ES_ERR_MASK;
					DMA4->TCD[0].CH_CSR = DMA4_CH_CSR_DONE_MASK | DMA4_CH_CSR_EEI_MASK | DMA4_CH_CSR_EARQ_MASK; // 0x40000006;
				};

				InitM7Mem (0x303C0000); // M7 ITCM
				InitM7Mem (0x303E0000);
				InitM7Mem (0x30400000); // M7 DTCM
				InitM7Mem (0x30420000);
				SDK_DelayAtLeastUs (200, coreClockFreq);

				if (dmaError != 0UL)
				{
					MyPrintf ("CM7 kick-off: TCM initialization failed, DMA4 CH_ES 0x%X\r\n", dmaError);
				}

				// With fuse POR_PRELOAD_CM7_TCM_ECC programmed the ROM already powered up the CM7, its VTOR is fixed to 0 and
				// INITVTOR is ignored. The initial SP and reset vector at ITCM address 0 let the CM7 start in both cases.
				// The CM7 startup code sets VTOR to its own vector table.
				volatile uint32_t *const itcmAlias = reinterpret_cast<volatile uint32_t *>(0x303C0000);
				itcmAlias[0] = cm7Vectors[0];
				itcmAlias[1] = cm7Vectors[1];
				__DSB();

				// Step 6 and 7: The CM7 image is already located in the flash, it isn't authenticated by the ELE

				// Step 8: Check TRDC configuration
				GrantCm7FlashAccess ();

				// Step 9: Send "Enable APC request" to the ELE
				while ((MU_RT_S3MUA->TSR & MU_TSR_TE0_MASK) == 0)
					; // Wait TR empty
				MU_RT_S3MUA->TR[0] = 0x17d20106;
				while ((MU_RT_S3MUA->RSR & MU_RSR_RF0_MASK) == 0)
					; // Wait RR Full
				while ((MU_RT_S3MUA->RSR & MU_RSR_RF1_MASK) == 0)
					; // Wait RR Full

				// The response from the ELE must always be read
				static constexpr uint32_t EleApcResponseHeader = 0xE1D20206;
				static constexpr uint32_t EleResponseSuccess   = 0xD6;
				const uint32_t eleResponse = MU_RT_S3MUA->RR[0];
				const uint32_t eleStatus   = MU_RT_S3MUA->RR[1];
				if ((eleResponse != EleApcResponseHeader) || (eleStatus != EleResponseSuccess))
				{
					MyPrintf ("CM7 kick-off: ELE APC response 0x%X 0x%X\r\n", eleResponse, eleStatus);
				}

				// Step 10-12: Gate off CM7 clock, deassert CPUWAIT, gate on CM7 clock
				CCM->LPCG[kCLOCK_M7].DIRECT = 0;
				while ((CCM->LPCG[kCLOCK_M7].STATUS0 & CCM_LPCG_STATUS0_ON_MASK) != 0UL)
				{
				}
				BLK_CTRL_S_AONMIX->M7_CFG   = BLK_CTRL_S_AONMIX_M7_CFG_INITVTOR(bootaddr);
				CCM->LPCG[kCLOCK_M7].DIRECT = CCM_LPCG_DIRECT_ON_MASK;
			}
		#endif
	}

	/* Function : portable_fini
	Target specific final code */
	void portable_fini (core_portable *p)
	{
		p->portable_id=0;
		MyPrintf ("Coremark done - M%d\r\n", __CORTEX_M);
	}


	extern "C" void MyPrintf (const char *Message, ...)
	{
		static char Buffer [128];
		va_list ArgPtr;
		va_start(ArgPtr, Message);
		size_t Length = vsnprintf (Buffer, sizeof(Buffer), Message, ArgPtr);
		va_end(ArgPtr);

		if (Length > 0 && Length <= sizeof(Buffer))
		{
			#if defined LPUART_BASE_PTRS
				LPUART_WriteBlocking (uart[BOARD_DEBUG_UART_INSTANCE], reinterpret_cast<const uint8_t *>(Buffer), Length);
			#elif defined USART_BASE_PTRS
				USART_WriteBlocking (uart[BOARD_DEBUG_UART_INSTANCE], reinterpret_cast<const uint8_t *>(Buffer), Length);
			#endif
		}
	}

	/* Function : start_time
	This function will be called right before starting the timed portion of the benchmark.

	Implementation may be capturing a system timer (as implemented in the example code)
	or zeroing some system parameters - e.g. setting the cpu clocks cycles to 0.	*/
	void start_time (void)
	{
		StartTick = TickCounter;
	}


	/* Function : stop_time
	This function will be called right after ending the timed portion of the benchmark.

	Implementation may be capturing a system timer (as implemented in the example code)
	or other system parameters - e.g. reading the current value of cpu cycles counter.	*/
	void stop_time (void)
	{
		StopTick = TickCounter;
	}


	/* Function : get_time
	Return an abstract "ticks" number that signifies time on the system.
	
	Actual value returned may be cpu cycles, milliseconds or any other value,
	as long as it can be converted to seconds by <time_in_secs>.
	This methodology is taken to accomodate any hardware or simulated platform.
	The sample implementation returns millisecs by default,
	and the resolution is controlled by <TIMER_RES_DIVIDER> */
	CORE_TICKS get_time (void)
	{
		return (StopTick - StartTick);
	}
	/* Function : time_in_secs
	Convert the value returned by get_time to seconds.

	The <secs_ret> type is used to accomodate systems with no support for floating point.
	Default implementation implemented by the EE_TICKS_PER_SEC macro above.*/
	secs_ret time_in_secs (CORE_TICKS ticks)
	{
		double Time = ticks;
		Time /= 1000.0;
		return Time;
	}
};
