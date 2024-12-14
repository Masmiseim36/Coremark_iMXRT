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

	void portable_init (core_portable *p, int *argc, char *argv[])
	{
		// Initialize board hardware.
		BOARD_ConfigMPU        ();
		BOARD_InitPins         ();
		BOARD_BootClockRUN     ();
//		BOARD_InitDebugConsole ();
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
			#elif (defined(MIMXRT1181_SERIES)     || defined(MIMXRT1182_SERIES)     || defined(MIMXRT1187_cm7_SERIES) || defined(MIMXRT1187_cm33_SERIES) ||\
				   defined(MIMXRT1189_cm7_SERIES) || defined(MIMXRT1189_cm33_SERIES))
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

		#if __CORTEX_M == 4
			MyPrintf ("Hello Coremark on M4 core\r\n");
		#elif __CORTEX_M == 7
			MyPrintf ("Hello Coremark on M7 core\r\n");
		#elif __CORTEX_M == 33
			MyPrintf ("Hello Coremark on M33 core\r\n");
		#else
			#error "unknown core"
		#endif

		#if defined MIMXRT1166_cm7_SERIES || defined MIMXRT1165_cm7_SERIES || \
			defined MIMXRT1173_cm7_SERIES || defined MIMXRT1175_cm7_SERIES || defined MIMXRT1176_cm7_SERIES
			// start the M4 core
			extern uint32_t __FlexSPI_segment_end__;
			uint32_t bootAddress = (uint32_t)&__FlexSPI_segment_end__;
			IOMUXC_LPSR_GPR->GPR0 = IOMUXC_LPSR_GPR_GPR0_CM4_INIT_VTOR_LOW ((uint32_t)bootAddress >> 3);
			IOMUXC_LPSR_GPR->GPR1 = IOMUXC_LPSR_GPR_GPR1_CM4_INIT_VTOR_HIGH((uint32_t)bootAddress >> 16);
			SRC->SCR |= SRC_SCR_BT_RELEASE_M4_MASK;
		#elif defined MIMXRT1189_cm33_SERIES || defined MIMXRT1187_cm33_SERIES
			// Start the M7 core
			extern uint32_t __ITCM_M7_segment_start__ [];
			extern uint32_t __FlexSPI_segment_end__[];
			// Copy the vector table of the M7 core from the flash to the ITCM of the M7:
/*			memcpy (__ITCM_M7_segment_start__, __FlexSPI_segment_end__, 1020);

			// Trigger S401
			while ((MU_RT_S3MUA->TSR & MU_TSR_TE0_MASK) == 0)
				; // Wait TR empty
			MU_RT_S3MUA->TR[0] = 0x17d20106;
			while ((MU_RT_S3MUA->RSR & MU_RSR_RF0_MASK) == 0)
				; // Wait RR Full
			while ((MU_RT_S3MUA->RSR & MU_RSR_RF1_MASK) == 0)
				; // Wait RR Full

			// Response from ELE must be always read 
			[[maybe_unused]]volatile uint32_t result1 = MU_RT_S3MUA->RR[0];
			[[maybe_unused]]volatile uint32_t result2 = MU_RT_S3MUA->RR[1];

			// Deassert Wait
			BLK_CTRL_S_AONMIX->M7_CFG = (BLK_CTRL_S_AONMIX->M7_CFG & (~BLK_CTRL_S_AONMIX_M7_CFG_WAIT_MASK)) | BLK_CTRL_S_AONMIX_M7_CFG_WAIT(0);
			*/
		#endif
	}

	/* Function : portable_fini
	Target specific final code */
	void portable_fini (core_portable *p)
	{
		p->portable_id=0;
		#if __CORTEX_M == 7
			MyPrintf ("Coremark done - M7\r\n");
		#elif __CORTEX_M == 4
			MyPrintf ("Coremark done - M4\r\n");
		#elif __CORTEX_M == 33
			MyPrintf ("Coremark done - M33\r\n");
		#else
			#error "unknown core"
		#endif
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
