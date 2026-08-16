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
#include "can.h"
#include "lv_port_indev.h"
#include "gt9xx.h"
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
  MX_USART1_UART_Init();
  MX_FSMC_Init();
  MX_CAN2_Init();
  /* USER CODE BEGIN 2 */
  // 1. 底层屏幕初始化

  
  ILI9806G_Init();
  ILI9806G_GramScan(5); // 跟随驱动默认扫描方向，避免强制写错
  ILI9806G_Clear(0, 0, 800, 480);

  GTP_Init_Panel(); // 初始化触摸屏
  // 2. LVGL 核心和接口初始化
  lv_init();
  lv_port_disp_init();
  lv_port_indev_init();
  // 3. UI 界面初始化
  ui_init();
  // 4. CAN 过滤器配置
  CAN_FilterTypeDef sFilterConfig = {0}; 
  sFilterConfig.FilterBank = 14;
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  sFilterConfig.FilterIdHigh = 0x0000;
  sFilterConfig.FilterIdLow = 0x0000;
  sFilterConfig.FilterMaskIdHigh = 0x0000; 
  sFilterConfig.FilterMaskIdLow = 0x0000;  
  sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  sFilterConfig.FilterActivation = ENABLE;
  sFilterConfig.SlaveStartFilterBank  = 14;

  // STM32F4 必须借用老大哥 hcan2 去分配 CAN2 的过滤器！
  HAL_CAN_ConfigFilter(&hcan2, &sFilterConfig); 

  // 把 hcan2 也一并启动，防止 CAN2 罢工

  HAL_CAN_Start(&hcan2);

  // ========================================================
  // 【关键修复】：轮询变量必须放在 while(1) 外部，否则每次循环都会清零！
  // ========================================================
  uint32_t last_lvgl_time = 0;
  uint32_t last_can_tx_time = 0;
  uint8_t obd_pids[] = {0x0D, 0x05, 0x42, 0x11, 0x04, 0x0F, 0X10};
  uint8_t current_pid_index = 0; 
  // ========================================================
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
        uint32_t current_tick = HAL_GetTick();

        // ==========================================
        // 任务 1：LVGL 心脏起搏器 (严格 5ms 跑一次)
        // ==========================================
        uint32_t delta_time = current_tick - last_lvgl_time;
        if (delta_time >= 5)
        {
            last_lvgl_time = current_tick;
            lv_tick_inc(delta_time); 
            lv_timer_handler();
        }
         
        // ==========================================
        // 任务 2：CAN 轮询发送任务 (每 100ms 切换发送一个 PID)
        // ==========================================
        if (current_tick - last_can_tx_time >= 100)
        {
            last_can_tx_time = current_tick;

            CAN_TxHeaderTypeDef TxHeader = {0}; 
            uint32_t TxMailbox;
            
            uint8_t target_pid = obd_pids[current_pid_index];
            uint8_t TxData[8] = {0x02, 0x01, target_pid, 0x00, 0x00, 0x00, 0x00, 0x00};

            TxHeader.StdId = 0x7DF;
            TxHeader.ExtId = 0x00;
            TxHeader.IDE = CAN_ID_STD;  
            TxHeader.RTR = CAN_RTR_DATA;
            TxHeader.DLC = 8;
            TxHeader.TransmitGlobalTime = DISABLE;
            
            if (HAL_CAN_AddTxMessage(&hcan2, &TxHeader, TxData, &TxMailbox) == HAL_OK)
            {
                printf("TX Success! 发送成功\r\n"); 
                current_pid_index++;
                if (current_pid_index >= sizeof(obd_pids)) 
                {
                  current_pid_index = 0; 
                }
            } 
            else 
            {
                printf("TX Error! 发送邮箱被堵死\r\n"); 
            }   
        }
        // ==========================================
     
        // ==========================================
        // 任务 3: CAN 接收与 UI 渲染任务 (无阻塞检测)
        // ==========================================
        if (HAL_CAN_GetRxFifoFillLevel(&hcan2, CAN_RX_FIFO0) > 0) 
        {
            CAN_RxHeaderTypeDef RxHeader;
            uint8_t RxData[8] = {0};

            if(HAL_CAN_GetRxMessage(&hcan2, CAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK)
            {
                printf("Got Data! 收到数据\r\n");
                
                if (RxHeader.StdId == 0x7E8) 
                {
                    if (RxData[1] == 0x41) 
                    {
                        uint8_t pid = RxData[2];
                        uint8_t A = RxData[3];
                        uint8_t B = RxData[4];

                        switch (pid) 
                        {
                            case 0x0D: // 车速 (Speed)
                            {
                                uint8_t speed = A;
                                lv_arc_set_value(ui_Arc_Speed, speed);
                                lv_label_set_text_fmt(ui_Label_Speed, "%d", speed);
                                break;
                            }
                            case 0x05: // 水温 (Temp)
                            {
                                int8_t temp = A - 40;
                                lv_bar_set_value(ui_BarTemp, temp, LV_ANIM_ON);
                                lv_label_set_text_fmt(ui_Label_Temp, "Temp: %dC", temp);
                                break;
                            }
                            case 0x42: // 控制模块电压 (Volt)
                            {
                                float volt = ((A * 256.0f) + B) / 1000.0f;
                                // 【核心修改】：拆出整数和小数
                                int volt_int = (int)volt;                          // 整数部分
                                int volt_frac = (int)((volt - volt_int) * 10);     // 提取 1 位小数 (乘10)
                                
                                lv_bar_set_value(ui_BarVolt, (int32_t)(volt * 10), LV_ANIM_ON); 
                                // 用 %d.%d 组合起来
                                lv_label_set_text_fmt(ui_Label_Volt, "Volt: %d.%dV", volt_int, volt_frac);
                                break;
                            }
                            case 0x11: // 节气门位置 (Throttle Pos)
                            {
                                float throttle = (A * 100.0f) / 255.0f;
                                int th_int = (int)throttle;
                                int th_frac = (int)((throttle - th_int) * 10);
                                
                                lv_label_set_text_fmt(ui_Label_Throttle_Val, "%d.%d %%", th_int, th_frac);
                                break;
                            }
                            case 0x04: // 发动机负荷 (Engine Load)
                            {
                                float load = (A * 100.0f) / 255.0f;
                                int load_int = (int)load;
                                int load_frac = (int)((load - load_int) * 10);
                                
                                lv_label_set_text_fmt(ui_Label_Engine_Load_, "%d.%d %%", load_int, load_frac);
                                break;
                            }
                            case 0x0F: // 进气温度 (Intake Temp) - 整数，不用改
                            {
                                int8_t intake_temp = A - 40;
                                lv_label_set_text_fmt(ui_Label_Intake_Temp, "%d C", intake_temp);
                                break;
                            }
                            case 0x10: // 空气流量 (MAF)
                            {
                                float maf = ((A * 256.0f) + B) / 100.0f;
                                int maf_int = (int)maf;
                                int maf_frac = (int)((maf - maf_int) * 10);
                                
                                lv_label_set_text_fmt(ui_Label_MAF_Air_Flow_, "%d.%d g/s", maf_int, maf_frac);
                                break;
                            }
                        } 
                    }
                }
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
