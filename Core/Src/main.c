/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "can.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"
#include "fsmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "bsp_ili9806g_lcd.h"
#include "ui.h"
#include "lvgl.h"
#include "lv_port_disp.h"
// 把那两个变量引过来，让 main.c 能看到它们
extern volatile uint8_t dma_flush_flag;
extern lv_disp_drv_t * my_dma_disp_drv;
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

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int fputc(int ch, FILE *f)
{
    // 将 printf 的字符通过串口 1 (huart1) 发送出去
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}



/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_CAN1_Init();
  MX_USART1_UART_Init();
  MX_FSMC_Init();
  /* USER CODE BEGIN 2 */
  // 1. 底层屏幕初始化
	uint32_t *test_sram = (uint32_t *)0x6C000000;
  uint8_t sram_is_alive = 1; // 默认它活着
  

  ILI9806G_Init();
  ILI9806G_GramScan(5); // 跟随驱动默认扫描方向，避免强行写死
  ILI9806G_Clear(0, 0, 800, 480);

  GTP_Init_Panel(); // 初始化触摸屏
  // 2. LVGL 核心和接口初始化
  lv_init();
  lv_port_disp_init();
  lv_port_indev_init();
  // 3. UI 界面初始化
   ui_init();

  // 4. CAN 过滤器配置
  CAN_FilterTypeDef sFilterConfig;
  sFilterConfig.FilterBank = 0;
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  sFilterConfig.FilterIdHigh = 0x0000;
  sFilterConfig.FilterIdLow = 0x0000;
  sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  sFilterConfig.FilterActivation = ENABLE;
  sFilterConfig.SlaveStartFilterBank  = 14;

  HAL_CAN_ConfigFilter(&hcan1,&sFilterConfig);
  HAL_CAN_Start(&hcan1);
 // printf("\r\n---HAL 库 CAN 回环测试启动！---\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  uint32_t last_lvgl_time = 0;
  uint32_t last_can_tx_time = 0;
  uint32_t last_blink_time = 0;
  uint8_t blink_state = 0;

  while (1)
  {
      uint32_t current_tick = HAL_GetTick();

      // ==========================================
      // 任务 1：LVGL 心脏起搏器 (严格 5ms 跑一次)
      // ==========================================
        //1先算出真实过去了多少毫秒
        uint32_t delta_time = current_tick - last_lvgl_time;
        // 2. 如果时间差大于等于 5ms，才执行刷新
      if (delta_time >= 5)
      {
          // 3. 更新上一次的时间戳
          last_lvgl_time = current_tick;
          
          // 4. ? 灵魂所在：过去了多少时间，就加多少时间！
          lv_tick_inc(delta_time); 
          
          // 5. 让 LVGL 刷屏
          lv_timer_handler();
      }
			
        // ==========================================
        // 任务 2：CAN 发送任务 (严格 2000ms 发送一次)
        // ==========================================
        if (current_tick - last_can_tx_time >= 2000)
        {
          last_can_tx_time = current_tick;
          CAN_TxHeaderTypeDef TxHeader;
          uint32_t TxMailbox;
          uint8_t TxData[8] = {0x02, 0x01, 0x0c, 0x00, 0x00, 0x00,0x00, 0x00};
          
          TxHeader.StdId = 0x7DF;                       
          TxHeader.ExtId = 0X00;  
          TxHeader.RTR = CAN_RTR_DATA;     
          TxHeader.DLC = 8;     
          TxHeader.TransmitGlobalTime = DISABLE;
          
          if(HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox) == HAL_OK){
         //   printf("已发送 OBD 请求帧...\r\n");
          }
        }
        // ==========================================
     
        // ==========================================
        // 任务 4：CAN 接收任务 (有空就检测，无需延时)
        // ==========================================
        if (HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) > 0) 
        {
          CAN_RxHeaderTypeDef RxHeader;
          uint8_t RxData[8] = {0};
          
          HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &RxHeader, RxData);
        //  printf("收到回环数据！ID:0x%x,内容：", RxHeader.StdId);
          for(int i = 0; i < RxHeader.DLC; i++){
         //   printf("%02x ", RxData[i]);
          }
        //  printf("\r\n.............\r\n");
        }
  }
}

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
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

#ifdef  USE_FULL_ASSERT
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
