#include "app_system.h"
#include "app_gui.h"
#include "app_can.h"
#include <stdio.h> // 必须加上这个才能打印

SemaphoreHandle_t xGuiMutex = NULL;

void App_System_Init(void)
{
    // ==============================================================
    // 【第一阶段：纯硬件与 HAL 库初始化】
    // 此时 FreeRTOS 尚未屏蔽中断，TIM6 和 HAL_Delay 完美运行！
    // ==============================================================
    App_GUI_Init(); 

    // ==============================================================
    // 【第二阶段：FreeRTOS API 调用区】
    // 警告：一旦开始执行下面的代码，系统中断将被屏蔽！后续绝对不能再用 HAL_Delay！
    // ==============================================================
    xGuiMutex = xSemaphoreCreateMutex(); // 从这句开始，中断被锁死

    App_CAN_Init(); // 这里面调用了 xQueueCreate，也没有延时，放在这里很安全
    xTaskCreate(GUI_Task, "GUI_Task", 4096, NULL, 1, NULL);
    xTaskCreate(CAN_Tx_Task, "CAN_Tx_Task", 256, NULL, 2, NULL);
    xTaskCreate(CAN_Rx_Task, "CAN_Rx_Task", 512, NULL, 3, NULL);
}