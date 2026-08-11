/**
 * @file lv_port_disp_templ.c
 *
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp.h"
#include <stdbool.h>
#include "bsp_ili9806g_lcd.h"
//引入DMA句柄
extern DMA_HandleTypeDef hdma_memtomem_dma2_stream0;

//定义全局指针来存放屏幕信息
lv_disp_drv_t * my_dma_disp_drv = NULL ;
//设置回调函数
//参数格式必须是DMA_HandleTypeDef *hdma

void My_LVGL_DMA_TxCpltCallback(DMA_HandleTypeDef *hdma)
	{

			if(my_dma_disp_drv != NULL)
			{
					lv_disp_flush_ready(my_dma_disp_drv);
			}
	}
/*********************
 *      DEFINES
 *********************/
 
#define MY_DISP_HOR_RES    800
#define MY_DISP_VER_RES    480
 
 
#ifndef MY_DISP_HOR_RES
    #warning Please define or replace the macro MY_DISP_HOR_RES with the actual screen width, default value 320 is used for now.
    #define MY_DISP_HOR_RES    320
#endif

#ifndef MY_DISP_VER_RES
    #warning Please define or replace the macro MY_DISP_HOR_RES with the actual screen height, default value 240 is used for now.
    #define MY_DISP_VER_RES    240
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);
//static void gpu_fill(lv_disp_drv_t * disp_drv, lv_color_t * dest_buf, lv_coord_t dest_width,
//        const lv_area_t * fill_area, lv_color_t color);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    /*-----------------------------
     * Create a buffer for drawing
     *----------------------------*/

    /**
     * LVGL requires a buffer where it internally draws the widgets.
     * Later this buffer will passed to your display driver's `flush_cb` to copy its content to your display.
     * The buffer has to be greater than 1 display row
     *
     * There are 3 buffering configurations:
     * 1. Create ONE buffer:
     *      LVGL will draw the display's content here and writes it to your display
     *
     * 2. Create TWO buffer:
     *      LVGL will draw the display's content to a buffer and writes it your display.
     *      You should use DMA to write the buffer's content to the display.
     *      It will enable LVGL to draw the next part of the screen to the other buffer while
     *      the data is being sent form the first buffer. It makes rendering and flushing parallel.
     *
     * 3. Double buffering
     *      Set 2 screens sized buffers and set disp_drv.full_refresh = 1.
     *      This way LVGL will always provide the whole rendered screen in `flush_cb`
     *      and you only need to change the frame buffer's address.
     */

    /* Example for 1) */
//    static lv_disp_draw_buf_t draw_buf_dsc_1;
//    static lv_color_t buf_1[MY_DISP_HOR_RES * 10];                          /*A buffer for 10 rows*/
//    lv_disp_draw_buf_init(&draw_buf_dsc_1, buf_1, NULL, MY_DISP_HOR_RES * 10);   /*Initialize the display buffer*/

    /* Example for 2) */
lv_color_t *sram_buf = (lv_color_t *)0x6C000000;
	static lv_disp_draw_buf_t draw_buf_dsc_2;

    // 改成这行，把 768KB 的航母挂载上去！（注意第三个参数是 NULL）
    lv_disp_draw_buf_init(&draw_buf_dsc_2, sram_buf, NULL, 800 * 480);
    /* Example for 3) also set disp_drv.full_refresh = 1 below*/
//    static lv_disp_draw_buf_t draw_buf_dsc_3;
//    static lv_color_t buf_3_1[MY_DISP_HOR_RES * MY_DISP_VER_RES];            /*A screen sized buffer*/
//    static lv_color_t buf_3_2[MY_DISP_HOR_RES * MY_DISP_VER_RES];            /*Another screen sized buffer*/
//    lv_disp_draw_buf_init(&draw_buf_dsc_3, buf_3_1, buf_3_2,
//                          MY_DISP_VER_RES * LV_VER_RES_MAX);   /*Initialize the display buffer*/

    /*-----------------------------------
     * Register the display in LVGL
     *----------------------------------*/

    static lv_disp_drv_t disp_drv;                   /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv);                    /*Basic initialization*/

    /*Set up the functions to access to your display*/

    /*Set the resolution of the display*/
    disp_drv.hor_res = MY_DISP_HOR_RES;
    disp_drv.ver_res = MY_DISP_VER_RES;
    disp_drv.sw_rotate = 0;            // 开启软件旋转功能
    disp_drv.rotated = LV_DISP_ROT_NONE; // 顺时针旋转 90 度
    /*Used to copy the buffer's content to the display*/
    disp_drv.flush_cb = disp_flush;

    /*Set a display buffer*/
    disp_drv.draw_buf = &draw_buf_dsc_2;

    /*Required for Example 3)*/
    //disp_drv.full_refresh = 1;

    /* Fill a memory array with a color if you have GPU.
     * Note that, in lv_conf.h you can enable GPUs that has built-in support in LVGL.
     * But if you have a different GPU you can use with this callback.*/
    //disp_drv.gpu_fill_cb = gpu_fill;

    /*Finally register the driver*/
    lv_disp_drv_register(&disp_drv);
    // 我们手动把自己的回调函数地址，塞给 DMA 句柄里专门用来存放“完成回调”的指针
    hdma_memtomem_dma2_stream0.XferCpltCallback = My_LVGL_DMA_TxCpltCallback;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
	/*You code here*/
	// NT35510_Init ();
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/*Flush the content of the internal buffer the specific area on the display
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_disp_flush_ready()' has to be called when finished.*/

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL */



static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    if(disp_flush_enabled) {    
        uint16_t phy_y1 = area->y1; 
        uint32_t width = area->x2 - area->x1 + 1;
        uint32_t height = area->y2 - area->y1 + 1;

        // 1. 设置 LCD 开窗
        ILI9806G_OpenWindow(area->x1, phy_y1, width, height);
        // 2. 发送写显存命令 (0x2C)
        ILI9806G_Write_Cmd(CMD_SetPixel);

        // 3. 【读写分离隔离法】
        uint16_t *src = (uint16_t *)color_p; 
        volatile uint16_t *dst = (volatile uint16_t *)0x68000002; 
        
        // 申请一个极其安全的 1 行缓冲区 (800 像素 = 1.6 KB，内部 RAM 毫无压力)
        static uint16_t line_buf[800]; 

        // 按行进行搬运
        for(uint32_t y = 0; y < height; y++) {
            // 第一阶段：纯读操作 (只激活 NE4 SRAM)
            for(uint32_t x = 0; x < width; x++) {
                line_buf[x] = *src++;
            }
            
            // 第二阶段：纯写操作 (只激活 NE3 LCD屏幕)
            for(uint32_t x = 0; x < width; x++) {
                *dst = line_buf[x];
            }
        }

        // 4. 通知 LVGL 刷新完成
        lv_disp_flush_ready(disp_drv);
    }
}
    /*IMPORTANT!!!
     *Inform the graphics library that you are ready with the flushing*/
    
//        /*The most simple case (but also the slowest) to put all pixels to the screen one-by-one*/

//        int32_t x;
//        int32_t y;
//        for(y = area->y1; y <= area->y2; y++) {
//            for(x = area->x1; x <= area->x2; x++) {
//                /*Put a pixel to the display. For example:*/
//                /*put_px(x, y, *color_p)*/
//                color_p++;
//            }
//        }
  

    /*IMPORTANT!!!
     *Inform the graphics library that you are ready with the flushing*/



/*OPTIONAL: GPU INTERFACE*/

/*If your MCU has hardware accelerator (GPU) then you can use it to fill a memory with a color*/
//static void gpu_fill(lv_disp_drv_t * disp_drv, lv_color_t * dest_buf, lv_coord_t dest_width,
//                    const lv_area_t * fill_area, lv_color_t color)
//{
//    /*It's an example code which should be done by your GPU*/
//    int32_t x, y;
//    dest_buf += dest_width * fill_area->y1; /*Go to the first line*/
//
//    for(y = fill_area->y1; y <= fill_area->y2; y++) {
//        for(x = fill_area->x1; x <= fill_area->x2; x++) {
//            dest_buf[x] = color;
//        }
//        dest_buf+=dest_width;    /*Go to the next line*/
//    }
//}


#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif