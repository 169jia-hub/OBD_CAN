SET(SOURCES screens/ui_Screen1.c
    screens/ui_ScreenDiag.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img_antilock_brake_fault_png.c
    images/ui_img_235150827.c
    images/ui_img_icon_teld_engine_png.c)

add_library(ui ${SOURCES})
