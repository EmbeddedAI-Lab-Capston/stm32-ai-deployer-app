/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>

#include "npu_cache.h"
#include "mcu_cache.h"
#include "ll_aton_rt_user_api.h"
#include "network.h"
#include "bme280.h"
#include "telemetry.h"

/* NOTE: ll_aton_runtime.c already defines NPU0_IRQHandler (ATON_STD_IRQ_LINE is
   0). The application must not provide one - it only has to unmask the line in
   the NVIC, which LL_ATON_OSAL_INSTALL_IRQ does not do on bare metal. */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Defines NN_Instance_network / NN_Interface_network from network.c. */
LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(network)

/* NOTE: these globals exist so a host-side debugger can observe the run over
   SWD without any UART. They are the only inference telemetry this firmware
   exposes, and the STM32 AI Deployer's Variable Watcher reads them by symbol
   name. Keep them volatile and at file scope: a local would have no stable
   address for the watcher to sample. */
/* Bisect aid: every init step bumps this, so a fault that happens before the
   main loop can be located from the host with a single memory read. */
volatile uint32_t g_boot_stage;

volatile uint32_t g_ai_infer_count;
volatile uint32_t g_ai_last_inference_us;
volatile uint32_t g_ai_last_cycles;
volatile uint32_t g_ai_last_class;
volatile uint32_t g_ai_last_confidence_pct;
volatile uint32_t g_ai_status;          /* 0 = idle, 1 = running, 2 = done, 0xE = error */
volatile uint32_t g_ai_cpu_clock_hz;
volatile uint32_t g_probe_ramweights; /* first word after the copy, expect 0xE6DEE804 */
volatile uint32_t g_led_active;      /* 1 once the LED is owned by the tick hook */

/* Probes. The host cannot answer these questions itself: a debugger read of
   the external flash is filtered differently from the firmware's own access,
   so "the debugger cannot read 0x71000000" does not prove the NPU cannot. */
volatile uint32_t g_probe_weights;   /* first word of the weight blob, expect 0xE6DEE804 */
volatile uint32_t g_probe_aton_id;   /* ATON ID register, 0 would mean the NPU is dark  */

/* BME280 on I2C1. A missing or failing sensor must never stop the inference
   loop, so its state is reported here instead of going to Error_Handler(). */
I2C_HandleTypeDef hi2c1;
volatile uint32_t g_sensor_i2c_ok;     /* 1 = HAL_I2C_Init succeeded */
volatile uint32_t g_sensor_read_ok;    /* 1 = last BME280 read succeeded */
volatile uint32_t g_sensor_read_count;
volatile uint32_t g_sensor_fail_count;

static uint8_t *buffer_in;
static uint8_t *buffer_out;
static float    s_sensor[3];           /* temperature C, humidity %, pressure hPa */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MPU_Config(void);
/* USER CODE BEGIN PFP */
static void SystemInit_Post(void);
static void NPU_Config(void);
static void SetClockSleepMode(void);
static void DWT_Init(void);
static void FillStaticInput(uint8_t *buf, uint32_t len);
static void ReadClassification(const uint8_t *buf, uint32_t len);
static void MX_I2C1_Init(void);
static void Sensor_Poll(void);
static void Telemetry_Publish(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* System clock already configured, simply SystemCoreClock init */
  SystemCoreClockUpdate();
  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* Initialize LED1 */
  BSP_LED_Init(LED_GREEN);
  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */
  /* Power up the NPU RAMs and let the caches activate. Skipping this makes the
     inference hang rather than fail, because the NPU's activation buffer lives
     in AXISRAM5 (see the generate report). */
  g_boot_stage = 1U;
  SystemInit_Post();
  g_boot_stage = 2U;
  NPU_Config();
  g_boot_stage = 25U;
  /* NOTE: no RISAF programming here on purpose. With the RISAFs left at their
     reset state the hardware still allows accesses that are secure, privileged
     and CID=1 - which is exactly how NPU_Config() tags the NPU's own
     transactions - so the accelerator already reaches the SRAMs and the
     memory-mapped weights. Re-programming the filters from an Appli that is
     itself executing out of FLEXMEM resets the board instead. */
  SetClockSleepMode();
  g_boot_stage = 3U;
  DWT_Init();
  g_boot_stage = 4U;
/* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  /* USER CODE BEGIN 2 */
  g_ai_cpu_clock_hz = HAL_RCC_GetCpuClockFreq();

  /* This build keeps the whole network inside internal RAM: the neural-art
     compiler was pointed at a memory pool with no external memory, so the
     weights belong at 0x342E0000 (npuRAM5) rather than being read in place
     from the memory-mapped flash. They still *ship* in the flash, so copy them
     across once, after SystemInit_Post() has powered those RAMs up. */
  memcpy((void *)0x342E0000UL, (const void *)0x71000000UL, 229601U);
  g_probe_ramweights = *(volatile uint32_t *)0x342E0000UL;

  g_boot_stage = 41U;
  g_probe_aton_id  = *(volatile uint32_t *)0x480E0000UL;   /* NPU register space */
  g_boot_stage = 42U;
  g_probe_weights  = *(volatile uint32_t *)0x71000000UL;   /* memory-mapped weights */
  g_boot_stage = 5U;

  const LL_Buffer_InfoTypeDef *in_info  = LL_ATON_Input_Buffers_Info(&NN_Instance_network);
  const LL_Buffer_InfoTypeDef *out_info = LL_ATON_Output_Buffers_Info(&NN_Instance_network);
  buffer_in  = (uint8_t *)LL_Buffer_addr_start(&in_info[0]);
  buffer_out = (uint8_t *)LL_Buffer_addr_start(&out_info[0]);

  g_boot_stage = 6U;
  LL_ATON_RT_RuntimeInit();
  g_boot_stage = 7U;
  LL_ATON_RT_Init_Network(&NN_Instance_network);

  /* ATON_STD_IRQ_LINE is 0, i.e. NPU0. Unmasking it must happen *after* the
     runtime is initialised: a stale completion flag left in INTCTRL.INTREG by
     an earlier run fires the handler immediately, and the handler dereferences
     runtime state that does not exist yet. */
  HAL_NVIC_SetPriority(NPU0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(NPU0_IRQn);
  g_boot_stage = 8U;

  /* From here the LED is driven by App_SysTickHook(), not by this loop. In
     flash-boot mode the ROM closes debug memory access, so the LED is the only
     channel left - and driving it from the tick means it keeps signalling even
     while the inference loop is stuck, which is exactly the case we need to
     tell apart:
       dark / frozen  -> the image never got here
       slow blink     -> running, but no inference has ever completed
       fast blink     -> inferences are completing */
  g_led_active = 1U;

  /* Static input: the point of this build is a repeatable measurement, not a
     real classification, so the tensor is a fixed deterministic pattern. */
  FillStaticInput(buffer_in, LL_ATON_NETWORK_IN_1_SIZE_BYTES);

  g_boot_stage = 9U;
  MX_I2C1_Init();
  if (g_sensor_i2c_ok != 0U)
    Sensor_Init(&hi2c1);
  g_boot_stage = 10U;
/* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    LL_ATON_RT_RetValues_t rt = LL_ATON_RT_DONE;
    uint32_t start_cycles;

    g_ai_status = 1U;

    /* The NPU reads the input through its own path, so the CPU's dirty cache
       lines have to reach memory first. */
    mcu_cache_clean_range((uint32_t)buffer_in,
                          (uint32_t)buffer_in + LL_ATON_NETWORK_IN_1_SIZE_BYTES);

    LL_ATON_RT_Reset_Network(&NN_Instance_network);
    start_cycles = DWT->CYCCNT;
    do {
      rt = LL_ATON_RT_RunEpochBlock(&NN_Instance_network);
      if (rt == LL_ATON_RT_WFE)
        LL_ATON_OSAL_WFE();
    } while (rt != LL_ATON_RT_DONE);
    g_ai_last_cycles = DWT->CYCCNT - start_cycles;

    /* Mirror of the clean above: the NPU wrote the output behind the CPU's
       back, so the stale lines must go before the result is read. */
    mcu_cache_invalidate_range((uint32_t)buffer_out,
                               (uint32_t)buffer_out + LL_ATON_NETWORK_OUT_1_SIZE_BYTES);
    ReadClassification(buffer_out, LL_ATON_NETWORK_OUT_1_SIZE_BYTES);

    if (g_ai_cpu_clock_hz != 0U)
      g_ai_last_inference_us =
          (uint32_t)(((uint64_t)g_ai_last_cycles * 1000000ULL) / g_ai_cpu_clock_hz);

    g_ai_infer_count++;
    g_ai_status = 2U;

    Sensor_Poll();
    Telemetry_Publish();

    HAL_Delay(120);
/* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/* USER CODE BEGIN 4 */
/**
  * @brief Powers up the RAMs the NPU uses and re-allows the caches.
  * @note  Mirrors system_init_post() in ST Edge AI's hello_world reference.
  *        The boot ROM leaves MSCR's cache-active bits cleared, so this is not
  *        optional even though the caches were already "enabled" above.
  */
static void SystemInit_Post(void)
{
  g_boot_stage = 11U;
  __HAL_RCC_SYSCFG_CLK_ENABLE();

  g_boot_stage = 12U;
  /* NPU RAMs (4 x 448 KB) + the AXI cache RAM. */
  RCC->MEMENR |= RCC_MEMENR_AXISRAM3EN | RCC_MEMENR_AXISRAM4EN
               | RCC_MEMENR_AXISRAM5EN | RCC_MEMENR_AXISRAM6EN;
  RCC->MEMENR |= RCC_MEMENR_CACHEAXIRAMEN;

  g_boot_stage = 13U;
  /* Take the AXI SRAMs out of shutdown. */
  RAMCFG_SRAM2_AXI->CR &= ~RAMCFG_CR_SRAMSD;
  RAMCFG_SRAM3_AXI->CR &= ~RAMCFG_CR_SRAMSD;
  RAMCFG_SRAM4_AXI->CR &= ~RAMCFG_CR_SRAMSD;
  RAMCFG_SRAM5_AXI->CR &= ~RAMCFG_CR_SRAMSD;
  RAMCFG_SRAM6_AXI->CR &= ~RAMCFG_CR_SRAMSD;

  g_boot_stage = 14U;
  MEMSYSCTL->MSCR |= MEMSYSCTL_MSCR_DCACTIVE_Msk | MEMSYSCTL_MSCR_ICACTIVE_Msk;
}

/**
  * @brief Clocks, resets and un-isolates the Neural-ART accelerator.
  * @note  The RIF calls are the part that is easy to miss: without them the NPU
  *        is clocked but cannot reach memory, which looks like a hang.
  */
static void NPU_Config(void)
{
  RIMC_MasterConfig_t master_conf;

  g_boot_stage = 21U;
  __HAL_RCC_NPU_CLK_ENABLE();
  __HAL_RCC_NPU_FORCE_RESET();
  __HAL_RCC_NPU_RELEASE_RESET();

  g_boot_stage = 22U;
  /* FIXME(test): the NPU cache is off for now. Its RAM is enabled through a
     different RCC bit (NPUCACHERAMEN) than the AXI cache RAM the reference
     switches on, and a cache whose RAM is unpowered stalls the very bus
     transfer we see hanging. */
  npu_cache_disable();
  g_boot_stage = 23U;

  master_conf.MasterCID = RIF_CID_1;
  master_conf.SecPriv   = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_NPU, &master_conf);
  g_boot_stage = 24U;
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_NPU,
                                        RIF_ATTRIBUTE_PRIV | RIF_ATTRIBUTE_SEC);
}



/**
  * @brief Keeps the clocks the accelerator depends on running in sleep mode.
  * @note  This is the difference between a working inference and a silent hang.
  *        The epoch loop parks the CPU in WFE while the NPU works; if the NPU,
  *        its cache or the RAMs holding the activations lose their clock in
  *        that sleep, the accelerator never finishes and the loop waits for an
  *        event that can no longer arrive.
  */
static void SetClockSleepMode(void)
{
  __HAL_RCC_DBG_CLK_SLEEP_ENABLE();
  __HAL_RCC_XSPIPHYCOMP_CLK_SLEEP_ENABLE();

  __HAL_RCC_AXISRAM1_MEM_CLK_SLEEP_ENABLE();
  __HAL_RCC_AXISRAM2_MEM_CLK_SLEEP_ENABLE();
  __HAL_RCC_AXISRAM3_MEM_CLK_SLEEP_ENABLE();
  __HAL_RCC_AXISRAM4_MEM_CLK_SLEEP_ENABLE();
  __HAL_RCC_AXISRAM5_MEM_CLK_SLEEP_ENABLE();
  __HAL_RCC_AXISRAM6_MEM_CLK_SLEEP_ENABLE();
  __HAL_RCC_FLEXRAM_MEM_CLK_SLEEP_ENABLE();
  __HAL_RCC_CACHEAXIRAM_MEM_CLK_SLEEP_ENABLE();

  __HAL_RCC_RIFSC_CLK_SLEEP_ENABLE();
  __HAL_RCC_RISAF_CLK_SLEEP_ENABLE();
  __HAL_RCC_IAC_CLK_SLEEP_ENABLE();

  __HAL_RCC_XSPI1_CLK_SLEEP_ENABLE();
  __HAL_RCC_XSPI2_CLK_SLEEP_ENABLE();
  __HAL_RCC_CACHEAXI_CLK_SLEEP_ENABLE();
  __HAL_RCC_NPU_CLK_SLEEP_ENABLE();
}

/**
  * @brief Blinks the green LED from the SysTick interrupt.
  * @note  Deliberately interrupt-driven: the main loop can be parked inside the
  *        NPU epoch loop for good, and a LED driven from there would simply
  *        freeze, which is indistinguishable from a board that never booted.
  *        Slow = no inference has ever finished, fast = they are finishing.
  */
void App_SysTickHook(void)
{
  static uint32_t ticks;

  if (g_led_active == 0U)
    return;

  const uint32_t period = (g_ai_infer_count > 0U) ? 100U : 500U;

  if (++ticks >= period)
  {
    ticks = 0U;
    BSP_LED_Toggle(LED_GREEN);
  }
}

/** @brief Starts the cycle counter used to time one inference. */
static void DWT_Init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

/** @brief Fills the input tensor with a fixed pattern so runs are comparable. */
static void FillStaticInput(uint8_t *buf, uint32_t len)
{
  for (uint32_t i = 0U; i < len; ++i)
    buf[i] = (uint8_t)(i & 0xFFU);
}

/** @brief Picks the argmax of the quantised output vector. */
static void ReadClassification(const uint8_t *buf, uint32_t len)
{
  uint32_t best_index = 0U;
  uint8_t  best_value = 0U;

  for (uint32_t i = 0U; i < len; ++i)
  {
    if (buf[i] > best_value)
    {
      best_value = buf[i];
      best_index = i;
    }
  }
  g_ai_last_class          = best_index;
  g_ai_last_confidence_pct = ((uint32_t)best_value * 100U) / 255U;
}

/**
  * @brief I2C1 at ~100 kHz for the BME280.
  * @note  PCLK1 is 200 MHz here (PLL1 1200 MHz / IC2 3 / AHB 2 / APB1 1, set by
  *        the FSBL). PRESC=15 gives an 80 ns tick: SCLDEL=15 (1280 ns),
  *        SDADEL=6 (560 ns), SCLH=0x31 (4.0 us), SCLL=0x3D (5.0 us) - standard mode, so
  *        the breakout's own 10k pull-ups are comfortably fast enough.
  */
static void MX_I2C1_Init(void)
{
  hi2c1.Instance              = I2C1;
  hi2c1.Init.Timing           = 0xF0F6313DU;
  hi2c1.Init.OwnAddress1      = 0U;
  hi2c1.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2      = 0U;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;

  g_sensor_i2c_ok = (HAL_I2C_Init(&hi2c1) == HAL_OK &&
                     HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) == HAL_OK)
                    ? 1U : 0U;
}

/**
  * @brief Reads the BME280 twice a second, re-initialising it while it fails.
  * @note  The sensor runs in normal mode with a 1 s standby, so polling faster
  *        than this only re-reads the same conversion. A read that fails keeps
  *        the previous values and says so through g_sensor_read_ok - the host
  *        must not mistake a failed read for a real zero.
  */
static void Sensor_Poll(void)
{
  static uint32_t last_read_ms;
  static uint32_t last_init_ms;
  const uint32_t now = HAL_GetTick();

  if (g_sensor_i2c_ok == 0U || (now - last_read_ms) < 500U)
    return;
  last_read_ms = now;

  if (Sensor_Read(s_sensor, 3U) == HAL_OK)
  {
    g_sensor_read_ok = 1U;
    g_sensor_read_count++;
    return;
  }

  g_sensor_read_ok = 0U;
  g_sensor_fail_count++;
  if ((now - last_init_ms) >= 2000U)
  {
    last_init_ms = now;
    Sensor_Init(&hi2c1);     /* sensor plugged in late, or recovering */
  }
}

/** @brief Mirrors the latest sensor and inference results into g_telemetry. */
static void Telemetry_Publish(void)
{
  Telemetry_BeginWrite();
  g_telemetry.sensor[0]      = s_sensor[0];
  g_telemetry.sensor[1]      = s_sensor[1];
  g_telemetry.sensor[2]      = s_sensor[2];
  g_telemetry.sensor_count   = 3U;
  g_telemetry.sensor_ok      = g_sensor_read_ok;
  g_telemetry.inf_us         = g_ai_last_inference_us;
  g_telemetry.infer_count    = g_ai_infer_count;
  g_telemetry.cycle++;
  g_telemetry.class_id       = g_ai_last_class;
  g_telemetry.confidence_pct = g_ai_last_confidence_pct;
  Telemetry_EndWrite();
}
/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};
  MPU_Attributes_InitTypeDef MPU_AttributesInit = {0};
  uint32_t primask_bit = __get_PRIMASK();
  __disable_irq();

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region 0 and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = __NON_CACHEABLE_SECTION_BEGIN;
  MPU_InitStruct.LimitAddress = __NON_CACHEABLE_SECTION_END;
  MPU_InitStruct.AttributesIndex = MPU_ATTRIBUTES_NUMBER0;
  MPU_InitStruct.AccessPermission = MPU_REGION_ALL_RW;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.DisablePrivExec = MPU_PRIV_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Attribute 0 and the memory to be protected
  */
  MPU_AttributesInit.Number = MPU_ATTRIBUTES_NUMBER0;
  MPU_AttributesInit.Attributes = INNER_OUTER(MPU_NOT_CACHEABLE);

  HAL_MPU_ConfigMemoryAttributes(&MPU_AttributesInit);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_HFNMI_PRIVDEF);

  /* Exit critical section to lock the system and avoid any issue around MPU mechanism */
  __set_PRIMASK(primask_bit);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
