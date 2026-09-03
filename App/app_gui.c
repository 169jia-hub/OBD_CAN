#include "app_gui.h"
#include "app_system.h"
#include "bsp_ili9806g_lcd.h"
#include "gt9xx.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "ui.h"
#include "gpio.h"
#include <stdio.h> // 【新增】引入头文件以支持 printf

void arc_speed_drag_cb(lv_event_t * e)
{
    lv_obj_t * arc = lv_event_get_target(e);      // 获取被拖动的对象 (仪表盘)
    int16_t speed = lv_arc_get_value(arc);        // 获取当前手滑到的速度值
    
    // 1. 同步更新中间的速度数字
    lv_label_set_text_fmt(ui_Label_Speed, "%d", speed);
    
    // 2. 复用超速报警逻辑
    if (speed > 120) 
    {
        // 速度变红，亮起 ABS 警告
        lv_obj_clear_flag(ui_ImgWarnABS, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_set_style_text_color(ui_Label_Speed, lv_color_hex(0xFF0000), LV_PART_MAIN); 
        
        // 联动第二页诊断
        lv_label_set_text(ui_LabelDTCStatus, "DTC Status: ERROR");
        lv_label_set_text(ui_LabelDTCCode, "U0415\nSpeed Limit"); 
        lv_obj_set_style_text_color(ui_LabelDTCCode, lv_color_hex(0xFF0000), LV_PART_MAIN);

        //打开蜂鸣器
        HAL_GPIO_WritePin(GPIOG,GPIO_PIN_7,GPIO_PIN_SET);
    }
    else 
    {
        // 降速后自动恢复白色，并熄灭 ABS 警告 (增加交互丝滑感)
        lv_obj_add_flag(ui_ImgWarnABS, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_set_style_text_color(ui_Label_Speed, lv_color_hex(0xFFFFFF), LV_PART_MAIN); 

        // 【新增】：划回到120及以下时，强制关闭蜂鸣器！
        HAL_GPIO_WritePin(GPIOG, GPIO_PIN_7, GPIO_PIN_RESET);
    }
}

void App_GUI_Init(void)
{
    printf("    -> [GUI-1] ILI9806G_Init...\r\n");
    ILI9806G_Init();

    printf("    -> [GUI-2] ILI9806G_GramScan & Clear...\r\n");
    ILI9806G_GramScan(5); 
    ILI9806G_Clear(0, 0, 800, 480);

    printf("    -> [GUI-3] GTP_Init_Panel...\r\n");
    GTP_Init_Panel();

    printf("    -> [GUI-4] lv_init...\r\n");
    lv_init();

    printf("    -> [GUI-5] lv_port_disp_init...\r\n");
    lv_port_disp_init();

    printf("    -> [GUI-6] lv_port_indev_init...\r\n");
    lv_port_indev_init();

    printf("    -> [GUI-7] ui_init...\r\n");
    ui_init();

    printf("    -> [GUI-8] Configuring GUI Flags...\r\n");
    // 1. 默认强制隐藏三个警告灯
    lv_obj_add_flag(ui_ImgWarnEngine, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ImgWarnABS, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ImgWarnTire, LV_OBJ_FLAG_HIDDEN);
    
    // 2. 解锁仪表盘的“可点击/可拖动”属性 (防止 SquareLine 默认锁死)
    lv_obj_add_flag(ui_Arc_Speed, LV_OBJ_FLAG_CLICKABLE);
    
    // 3. 将我们上面写的回调函数，死死绑定到仪表盘的“值改变”事件上
    lv_obj_add_event_cb(ui_Arc_Speed, arc_speed_drag_cb, LV_EVENT_VALUE_CHANGED, NULL);

    printf("    -> [GUI-9] App_GUI_Init DONE!\r\n");
}

void GUI_Task(void *pvParameters)
{
    while (1)
    {
        // 核心：上锁保护 LVGL 渲染，防止和 CAN 任务冲突
        if (xSemaphoreTake(xGuiMutex, portMAX_DELAY) == pdTRUE) {
            lv_timer_handler();
            xSemaphoreGive(xGuiMutex);
        }
        
        lv_tick_inc(5);
        vTaskDelay(pdMS_TO_TICKS(5)); // 严格 5ms 释放一次 CPU 让给其他任务
    }
}