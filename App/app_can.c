#include "app_can.h"
#include "app_system.h" // 确保这里面 extern 了 xGuiMutex
#include "can.h"
#include "ui.h"
#include "lvgl.h"
#include <stdio.h>

//定义一个队列结构体, 用来把中断里的数据搬到任务里
typedef struct
{
    CAN_RxHeaderTypeDef header;
    uint8_t data[8];
} CAN_Msg_t; // 【修复 1】：加上了救命的分号！

QueueHandle_t xCanRxQueue;

uint16_t uds_dids[] = {0x010D, 0x0105, 0x0142, 0x0111, 0x0104, 0x010F, 0x0110};
uint8_t obd_pids[] = {0x0D, 0x05, 0x42, 0x11, 0x04, 0x0F, 0x10};
uint8_t current_pid_index = 0;
uint32_t current_poll_interval = 100;
uint8_t time_out_cnt = 0;
uint8_t device_offline = 0;
volatile uint8_t flag_bus_off_occurred = 0;

void App_CAN_Init(void)
{
    //创建一个能存 10 条报文的队列 
    xCanRxQueue = xQueueCreate(10, sizeof(CAN_Msg_t));

    //CAN 过滤器配置
    CAN_FilterTypeDef sFilterConfig = {0}; 
    sFilterConfig.FilterBank = 14;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
    sFilterConfig.FilterIdHigh = 0x7E8 << 5;
    sFilterConfig.FilterIdLow = 0x0000;
    sFilterConfig.FilterMaskIdHigh = 0x7FF << 5; 
    sFilterConfig.FilterMaskIdLow = 0x0000;  
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    sFilterConfig.FilterActivation = ENABLE;
    sFilterConfig.SlaveStartFilterBank  = 14;

    // STM32F4 必须借用老大哥 hcan2 去分配 CAN2 的过滤器！
    HAL_CAN_ConfigFilter(&hcan2, &sFilterConfig); 

    // 把 hcan2 也一并启动，防止 CAN2 罢工
    HAL_CAN_Start(&hcan2);

    // 【修复 4】：删除了重复调用的冗余代码，只保留完整的这句开启中断
    HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_ERROR | CAN_IT_BUSOFF);
}

extern CAN_HandleTypeDef hcan2; // 【修改 1】：引用 hcan2
extern SemaphoreHandle_t xGuiMutex;
extern volatile uint8_t flag_bus_off_occurred;

void CAN_Tx_Task(void *pvParameters)
{
    CAN_TxHeaderTypeDef TxHeader;
    uint32_t TxMailbox;
    uint8_t TxData[8] = {0x02, 0x01, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00};

    // 【修改 2】：删掉前面的类型前缀，直接赋值给全局变量！
    time_out_cnt = 0;
    current_poll_interval = 200; 
    device_offline = 0;

    TxHeader.StdId = 0x7DF;
    TxHeader.ExtId = 0x00;
    TxHeader.IDE = CAN_ID_STD;
    TxHeader.RTR = CAN_RTR_DATA;
    TxHeader.DLC = 8;
    TxHeader.TransmitGlobalTime = DISABLE;

    while (1)
    {
        if (flag_bus_off_occurred == 1) 
        {
            flag_bus_off_occurred = 0;
            printf("CRITICAL ERROR: 触发 CAN Bus-Off!\r\n");
        }

        time_out_cnt++; 
        if (time_out_cnt >= 3) 
        {
            current_poll_interval = 2000; 
            time_out_cnt = 3; 
            
            if (device_offline == 0)
            {
                device_offline = 1;
                printf ("设备离线，降低轮询频率\r\n"); 
                
                if (xSemaphoreTake(xGuiMutex, portMAX_DELAY) == pdTRUE) {
                    lv_arc_set_value(ui_Arc_Speed, 0);       
                    lv_label_set_text(ui_Label_Speed, "--"); 
                    xSemaphoreGive(xGuiMutex); 
                }
            }
        }

        // 【修改 3】：这里必须用 hcan2 进行发送！
        if(HAL_CAN_AddTxMessage(&hcan2, &TxHeader, TxData, &TxMailbox) == HAL_OK)
        {
            // 发送成功逻辑
        }

        vTaskDelay(pdMS_TO_TICKS(current_poll_interval));
    }
}

extern CAN_HandleTypeDef hcan1; // 确保能引用到底层句柄

extern uint8_t time_out_cnt;
extern uint32_t current_poll_interval;
extern uint8_t device_offline;
extern SemaphoreHandle_t xGuiMutex;

void CAN_Rx_Task(void *pvParameters)
{
    CAN_Msg_t rx_msg; // 使用你定义的结构体接收队列数据

    while (1)
    {
        // 1. 挂起任务，死等队列中的数据 (portMAX_DELAY 意味着没有数据就不消耗 CPU)
        if (xQueueReceive(xCanRxQueue, &rx_msg, portMAX_DELAY) == pdTRUE)
        {
            // printf("收到回环数据！ID:0x%x\r\n", rx_msg.header.StdId);
            
            // 2. 收到有效回复，立刻清除超时标记，恢复 200ms 高频刷新！
            time_out_cnt = 0;               
            current_poll_interval = 200;    
            
            if (device_offline == 1)
            {
                device_offline = 0;
                printf("设备重新上线，恢复高频通信！\r\n");
            }

            // 3. 将解析后的数据更新到 LVGL 屏幕上 (必须上锁)
            /*
            if (xSemaphoreTake(xGuiMutex, portMAX_DELAY) == pdTRUE) {
                // 示例：将收到的某个数据字节转成速度显示
                // lv_arc_set_value(ui_Arc_Speed, rx_msg.data[3]);
                // lv_label_set_text_fmt(ui_Label_Speed, "%d", rx_msg.data[3]);
                xSemaphoreGive(xGuiMutex);
            }
            */
        }
    }
}
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    if(hcan->Instance == CAN2)
    {
        CAN_Msg_t msg;
        if(HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &msg.header, msg.data) == HAL_OK)
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            // 把数据扔进队列，唤醒 CAN_Rx_Task
            xQueueSendFromISR(xCanRxQueue, &msg, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
    uint32_t errorcode = HAL_CAN_GetError(hcan);
    if (errorcode & HAL_CAN_ERROR_BOF)
    {
        //软重启 CAN
        HAL_CAN_Stop(hcan);
        HAL_CAN_Start(hcan);

        time_out_cnt = 3;

        //通知主循环打印 
        flag_bus_off_occurred = 1;
    }
}