#ifndef __APP_SYSTEM_H
#define __APP_SYSTEM_H

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

//声明全局的 LVGL 互斥锁 
extern SemaphoreHandle_t xGuiMutex;

void App_System_Init(void);

#endif