.syntax unified
.cpu cortex-m7
.fpu fpv5-d16
.thumb

.global g_pfnVectors
.global Default_Handler

.word _sidata
.word _sdata
.word _edata
.word _sbss
.word _ebss

.section .text.Reset_Handler
.weak Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
  ldr   r0, =_estack
  mov   sp, r0

  bl    SystemInit

  ldr   r0, =_sdata
  ldr   r1, =_edata
  ldr   r2, =_sidata
  movs  r3, #0
LoopCopyData:
  adds  r4, r0, r3
  cmp   r4, r1
  bcc   CopyData
  b     LoopFillZerobss
CopyData:
  ldr   r4, [r2, r3]
  str   r4, [r0, r3]
  adds  r3, r3, #4
  b     LoopCopyData

LoopFillZerobss:
  ldr   r2, =_sbss
  ldr   r4, =_ebss
  movs  r3, #0
FillZerobss:
  cmp   r2, r4
  bcc   FillZero
  b     CallMain
FillZero:
  str   r3, [r2], #4
  b     FillZerobss

CallMain:
  bl    __libc_init_array
  bl    main
LoopForever:
  b     LoopForever

.size Reset_Handler, .-Reset_Handler

.section .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
  b Infinite_Loop
.size Default_Handler, .-Default_Handler

.macro IRQ handler
  .weak \handler
  .thumb_set \handler,Default_Handler
.endm

IRQ NMI_Handler
IRQ HardFault_Handler
IRQ MemManage_Handler
IRQ BusFault_Handler
IRQ UsageFault_Handler
IRQ SVC_Handler
IRQ DebugMon_Handler
IRQ PendSV_Handler
IRQ SysTick_Handler
IRQ WWDG_IRQHandler
IRQ PVD_AVD_IRQHandler
IRQ TAMP_STAMP_IRQHandler
IRQ RTC_WKUP_IRQHandler
IRQ FLASH_IRQHandler
IRQ RCC_IRQHandler
IRQ EXTI0_IRQHandler
IRQ EXTI1_IRQHandler
IRQ EXTI2_IRQHandler
IRQ EXTI3_IRQHandler
IRQ EXTI4_IRQHandler
IRQ DMA1_Stream0_IRQHandler
IRQ DMA1_Stream1_IRQHandler
IRQ DMA1_Stream2_IRQHandler
IRQ DMA1_Stream3_IRQHandler
IRQ DMA1_Stream4_IRQHandler
IRQ DMA1_Stream5_IRQHandler
IRQ DMA1_Stream6_IRQHandler
IRQ ADC_IRQHandler
IRQ FDCAN1_IT0_IRQHandler
IRQ FDCAN2_IT0_IRQHandler
IRQ FDCAN1_IT1_IRQHandler
IRQ FDCAN2_IT1_IRQHandler
IRQ EXTI9_5_IRQHandler
IRQ TIM1_BRK_IRQHandler
IRQ TIM1_UP_IRQHandler
IRQ TIM1_TRG_COM_IRQHandler
IRQ TIM1_CC_IRQHandler
IRQ TIM2_IRQHandler
IRQ TIM3_IRQHandler
IRQ TIM4_IRQHandler
IRQ I2C1_EV_IRQHandler
IRQ I2C1_ER_IRQHandler
IRQ I2C2_EV_IRQHandler
IRQ I2C2_ER_IRQHandler
IRQ SPI1_IRQHandler
IRQ SPI2_IRQHandler
IRQ USART1_IRQHandler
IRQ USART2_IRQHandler
IRQ USART3_IRQHandler
IRQ EXTI15_10_IRQHandler
IRQ RTC_Alarm_IRQHandler
IRQ TIM8_BRK_TIM12_IRQHandler
IRQ TIM8_UP_TIM13_IRQHandler
IRQ TIM8_TRG_COM_TIM14_IRQHandler
IRQ TIM8_CC_IRQHandler
IRQ DMA1_Stream7_IRQHandler
IRQ FMC_IRQHandler
IRQ SDMMC1_IRQHandler
IRQ TIM5_IRQHandler
IRQ SPI3_IRQHandler
IRQ UART4_IRQHandler
IRQ UART5_IRQHandler
IRQ TIM6_DAC_IRQHandler
IRQ TIM7_IRQHandler
IRQ DMA2_Stream0_IRQHandler
IRQ DMA2_Stream1_IRQHandler
IRQ DMA2_Stream2_IRQHandler
IRQ DMA2_Stream3_IRQHandler
IRQ DMA2_Stream4_IRQHandler
IRQ ETH_IRQHandler
IRQ ETH_WKUP_IRQHandler
IRQ FDCAN_CAL_IRQHandler
IRQ DMA2_Stream5_IRQHandler
IRQ DMA2_Stream6_IRQHandler
IRQ DMA2_Stream7_IRQHandler
IRQ USART6_IRQHandler
IRQ I2C3_EV_IRQHandler
IRQ I2C3_ER_IRQHandler
IRQ OTG_HS_EP1_OUT_IRQHandler
IRQ OTG_HS_EP1_IN_IRQHandler
IRQ OTG_HS_WKUP_IRQHandler
IRQ OTG_HS_IRQHandler
IRQ DCMI_IRQHandler
IRQ RNG_IRQHandler
IRQ FPU_IRQHandler
IRQ UART7_IRQHandler
IRQ UART8_IRQHandler
IRQ SPI4_IRQHandler
IRQ SPI5_IRQHandler
IRQ SPI6_IRQHandler
IRQ SAI1_IRQHandler
IRQ LTDC_IRQHandler
IRQ LTDC_ER_IRQHandler
IRQ DMA2D_IRQHandler
IRQ SAI2_IRQHandler
IRQ QUADSPI_IRQHandler
IRQ LPTIM1_IRQHandler
IRQ CEC_IRQHandler
IRQ I2C4_EV_IRQHandler
IRQ I2C4_ER_IRQHandler
IRQ SPDIF_RX_IRQHandler
IRQ OTG_FS_EP1_OUT_IRQHandler
IRQ OTG_FS_EP1_IN_IRQHandler
IRQ OTG_FS_WKUP_IRQHandler
IRQ OTG_FS_IRQHandler
IRQ DMAMUX1_OVR_IRQHandler
IRQ HRTIM1_Master_IRQHandler
IRQ HRTIM1_TIMA_IRQHandler
IRQ HRTIM1_TIMB_IRQHandler
IRQ HRTIM1_TIMC_IRQHandler
IRQ HRTIM1_TIMD_IRQHandler
IRQ HRTIM1_TIME_IRQHandler
IRQ HRTIM1_FLT_IRQHandler
IRQ DFSDM1_FLT0_IRQHandler
IRQ DFSDM1_FLT1_IRQHandler
IRQ DFSDM1_FLT2_IRQHandler
IRQ DFSDM1_FLT3_IRQHandler
IRQ SAI3_IRQHandler
IRQ SWPMI1_IRQHandler
IRQ TIM15_IRQHandler
IRQ TIM16_IRQHandler
IRQ TIM17_IRQHandler
IRQ MDIOS_WKUP_IRQHandler
IRQ MDIOS_IRQHandler
IRQ JPEG_IRQHandler
IRQ MDMA_IRQHandler
IRQ SDMMC2_IRQHandler
IRQ HSEM1_IRQHandler
IRQ ADC3_IRQHandler
IRQ DMAMUX2_OVR_IRQHandler
IRQ BDMA_Channel0_IRQHandler
IRQ BDMA_Channel1_IRQHandler
IRQ BDMA_Channel2_IRQHandler
IRQ BDMA_Channel3_IRQHandler
IRQ BDMA_Channel4_IRQHandler
IRQ BDMA_Channel5_IRQHandler
IRQ BDMA_Channel6_IRQHandler
IRQ BDMA_Channel7_IRQHandler
IRQ COMP_IRQHandler
IRQ LPTIM2_IRQHandler
IRQ LPTIM3_IRQHandler
IRQ LPTIM4_IRQHandler
IRQ LPTIM5_IRQHandler
IRQ LPUART1_IRQHandler

.section .isr_vector,"a",%progbits
.type g_pfnVectors, %object
g_pfnVectors:
  .word _estack
  .word Reset_Handler
  .word NMI_Handler
  .word HardFault_Handler
  .word MemManage_Handler
  .word BusFault_Handler
  .word UsageFault_Handler
  .word 0
  .word 0
  .word 0
  .word 0
  .word SVC_Handler
  .word DebugMon_Handler
  .word 0
  .word PendSV_Handler
  .word SysTick_Handler
  .word WWDG_IRQHandler
  .word PVD_AVD_IRQHandler
  .word TAMP_STAMP_IRQHandler
  .word RTC_WKUP_IRQHandler
  .word FLASH_IRQHandler
  .word RCC_IRQHandler
  .word EXTI0_IRQHandler
  .word EXTI1_IRQHandler
  .word EXTI2_IRQHandler
  .word EXTI3_IRQHandler
  .word EXTI4_IRQHandler
  .word DMA1_Stream0_IRQHandler
  .word DMA1_Stream1_IRQHandler
  .word DMA1_Stream2_IRQHandler
  .word DMA1_Stream3_IRQHandler
  .word DMA1_Stream4_IRQHandler
  .word DMA1_Stream5_IRQHandler
  .word DMA1_Stream6_IRQHandler
  .word ADC_IRQHandler
  .word FDCAN1_IT0_IRQHandler
  .word FDCAN2_IT0_IRQHandler
  .word FDCAN1_IT1_IRQHandler
  .word FDCAN2_IT1_IRQHandler
  .word EXTI9_5_IRQHandler
  .word TIM1_BRK_IRQHandler
  .word TIM1_UP_IRQHandler
  .word TIM1_TRG_COM_IRQHandler
  .word TIM1_CC_IRQHandler
  .word TIM2_IRQHandler
  .word TIM3_IRQHandler
  .word TIM4_IRQHandler
  .word I2C1_EV_IRQHandler
  .word I2C1_ER_IRQHandler
  .word I2C2_EV_IRQHandler
  .word I2C2_ER_IRQHandler
  .word SPI1_IRQHandler
  .word SPI2_IRQHandler
  .word USART1_IRQHandler
  .word USART2_IRQHandler
  .word USART3_IRQHandler
  .word EXTI15_10_IRQHandler
  .word RTC_Alarm_IRQHandler
  .word 0
  .word TIM8_BRK_TIM12_IRQHandler
  .word TIM8_UP_TIM13_IRQHandler
  .word TIM8_TRG_COM_TIM14_IRQHandler
  .word TIM8_CC_IRQHandler
  .word DMA1_Stream7_IRQHandler
  .word FMC_IRQHandler
  .word SDMMC1_IRQHandler
  .word TIM5_IRQHandler
  .word SPI3_IRQHandler
  .word UART4_IRQHandler
  .word UART5_IRQHandler
  .word TIM6_DAC_IRQHandler
  .word TIM7_IRQHandler
  .word DMA2_Stream0_IRQHandler
  .word DMA2_Stream1_IRQHandler
  .word DMA2_Stream2_IRQHandler
  .word DMA2_Stream3_IRQHandler
  .word DMA2_Stream4_IRQHandler
  .word ETH_IRQHandler
  .word ETH_WKUP_IRQHandler
  .word FDCAN_CAL_IRQHandler
  .word 0
  .word 0
  .word 0
  .word 0
  .word DMA2_Stream5_IRQHandler
  .word DMA2_Stream6_IRQHandler
  .word DMA2_Stream7_IRQHandler
  .word USART6_IRQHandler
  .word I2C3_EV_IRQHandler
  .word I2C3_ER_IRQHandler
  .word OTG_HS_EP1_OUT_IRQHandler
  .word OTG_HS_EP1_IN_IRQHandler
  .word OTG_HS_WKUP_IRQHandler
  .word OTG_HS_IRQHandler
  .word DCMI_IRQHandler
  .word 0
  .word RNG_IRQHandler
  .word FPU_IRQHandler
  .word UART7_IRQHandler
  .word UART8_IRQHandler
  .word SPI4_IRQHandler
  .word SPI5_IRQHandler
  .word SPI6_IRQHandler
  .word SAI1_IRQHandler
  .word LTDC_IRQHandler
  .word LTDC_ER_IRQHandler
  .word DMA2D_IRQHandler
  .word SAI2_IRQHandler
  .word QUADSPI_IRQHandler
  .word LPTIM1_IRQHandler
  .word CEC_IRQHandler
  .word I2C4_EV_IRQHandler
  .word I2C4_ER_IRQHandler
  .word SPDIF_RX_IRQHandler
  .word OTG_FS_EP1_OUT_IRQHandler
  .word OTG_FS_EP1_IN_IRQHandler
  .word OTG_FS_WKUP_IRQHandler
  .word OTG_FS_IRQHandler
  .word DMAMUX1_OVR_IRQHandler
  .word HRTIM1_Master_IRQHandler
  .word HRTIM1_TIMA_IRQHandler
  .word HRTIM1_TIMB_IRQHandler
  .word HRTIM1_TIMC_IRQHandler
  .word HRTIM1_TIMD_IRQHandler
  .word HRTIM1_TIME_IRQHandler
  .word HRTIM1_FLT_IRQHandler
  .word DFSDM1_FLT0_IRQHandler
  .word DFSDM1_FLT1_IRQHandler
  .word DFSDM1_FLT2_IRQHandler
  .word DFSDM1_FLT3_IRQHandler
  .word SAI3_IRQHandler
  .word SWPMI1_IRQHandler
  .word TIM15_IRQHandler
  .word TIM16_IRQHandler
  .word TIM17_IRQHandler
  .word MDIOS_WKUP_IRQHandler
  .word MDIOS_IRQHandler
  .word JPEG_IRQHandler
  .word MDMA_IRQHandler
  .word SDMMC2_IRQHandler
  .word HSEM1_IRQHandler
  .word 0
  .word ADC3_IRQHandler
  .word DMAMUX2_OVR_IRQHandler
  .word BDMA_Channel0_IRQHandler
  .word BDMA_Channel1_IRQHandler
  .word BDMA_Channel2_IRQHandler
  .word BDMA_Channel3_IRQHandler
  .word BDMA_Channel4_IRQHandler
  .word BDMA_Channel5_IRQHandler
  .word BDMA_Channel6_IRQHandler
  .word BDMA_Channel7_IRQHandler
  .word COMP_IRQHandler
  .word LPTIM2_IRQHandler
  .word LPTIM3_IRQHandler
  .word LPTIM4_IRQHandler
  .word LPTIM5_IRQHandler
  .word LPUART1_IRQHandler
.size g_pfnVectors, .-g_pfnVectors
