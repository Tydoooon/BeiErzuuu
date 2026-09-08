cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME BeiErZu)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    Core/Src
    app/app_backlight.c
    app/app_led.c
    app/app_lcd.c
    app/app_mouse.c
    app/app_w25q128.c
    app/app_w25q128_font_image.S
    app/app_ui_font.c
    bsp/bsp_led.c
    bsp/bsp_lcd.c
    bsp/bsp_tfcard.c
    bsp/bsp_w25q128.c
    bsp/bsp_max98357a.c
    bsp/bsp_buzzer.c
    dev/dev_head.h
    dev/dev_lcd.c
    dev/dev_led.c
    app/app_lvgl.c
    app/app_ui.c
    app/app_ui_model.c
    app/app_ui_common.c
    app/app_ui_login.c
    app/app_ui_home.c
    app/app_ui_file.c
    app/app_ui_draw.c
    app/app_ui_music.c
    app/app_ui_log.c
    app/app_ui_monitor.c
    app/app_ui_setting.c
    app/app_event.c
    app/app_music_player.c
    app/app_wav.c
    app/app_power.c
    gui/pic.c
    LVGL/GUI/lvgl/src/core/lv_disp.c
    LVGL/GUI/lvgl/src/core/lv_event.c
    LVGL/GUI/lvgl/src/core/lv_group.c
    LVGL/GUI/lvgl/src/core/lv_indev.c
    LVGL/GUI/lvgl/src/core/lv_indev_scroll.c
    LVGL/GUI/lvgl/src/core/lv_obj.c
    LVGL/GUI/lvgl/src/core/lv_obj_class.c
    LVGL/GUI/lvgl/src/core/lv_obj_draw.c
    LVGL/GUI/lvgl/src/core/lv_obj_pos.c
    LVGL/GUI/lvgl/src/core/lv_obj_scroll.c
    LVGL/GUI/lvgl/src/core/lv_obj_style.c
    LVGL/GUI/lvgl/src/core/lv_obj_style_gen.c
    LVGL/GUI/lvgl/src/core/lv_obj_tree.c
    LVGL/GUI/lvgl/src/core/lv_refr.c
    LVGL/GUI/lvgl/src/core/lv_theme.c
    LVGL/GUI/lvgl/src/draw/lv_draw.c
    LVGL/GUI/lvgl/src/draw/lv_draw_arc.c
    LVGL/GUI/lvgl/src/draw/lv_draw_img.c
    LVGL/GUI/lvgl/src/draw/lv_draw_label.c
    LVGL/GUI/lvgl/src/draw/lv_draw_line.c
    LVGL/GUI/lvgl/src/draw/lv_draw_mask.c
    LVGL/GUI/lvgl/src/draw/lv_draw_rect.c
    LVGL/GUI/lvgl/src/draw/lv_draw_triangle.c
    LVGL/GUI/lvgl/src/draw/lv_img_buf.c
    LVGL/GUI/lvgl/src/draw/lv_img_cache.c
    LVGL/GUI/lvgl/src/draw/lv_img_decoder.c
    LVGL/GUI/lvgl/src/draw/sw/lv_draw_sw.c
    LVGL/GUI/lvgl/src/draw/sw/lv_draw_sw_arc.c
    LVGL/GUI/lvgl/src/draw/sw/lv_draw_sw_blend.c
    LVGL/GUI/lvgl/src/draw/sw/lv_draw_sw_dither.c
    LVGL/GUI/lvgl/src/draw/sw/lv_draw_sw_gradient.c
    LVGL/GUI/lvgl/src/draw/sw/lv_draw_sw_img.c
    LVGL/GUI/lvgl/src/draw/sw/lv_draw_sw_letter.c
    LVGL/GUI/lvgl/src/draw/sw/lv_draw_sw_line.c
    LVGL/GUI/lvgl/src/draw/sw/lv_draw_sw_polygon.c
    LVGL/GUI/lvgl/src/draw/sw/lv_draw_sw_rect.c
    LVGL/GUI/lvgl/src/extra/lv_extra.c
    LVGL/GUI/lvgl/src/extra/layouts/flex/lv_flex.c
    LVGL/GUI/lvgl/src/extra/layouts/grid/lv_grid.c
    LVGL/GUI/lvgl/src/extra/libs/bmp/lv_bmp.c
    LVGL/GUI/lvgl/src/extra/libs/ffmpeg/lv_ffmpeg.c
    LVGL/GUI/lvgl/src/extra/libs/freetype/lv_freetype.c
    LVGL/GUI/lvgl/src/extra/libs/fsdrv/lv_fs_fatfs.c
    LVGL/GUI/lvgl/src/extra/libs/fsdrv/lv_fs_posix.c
    LVGL/GUI/lvgl/src/extra/libs/fsdrv/lv_fs_stdio.c
    LVGL/GUI/lvgl/src/extra/libs/fsdrv/lv_fs_win32.c
    LVGL/GUI/lvgl/src/extra/libs/gif/gifdec.c
    LVGL/GUI/lvgl/src/extra/libs/gif/lv_gif.c
    LVGL/GUI/lvgl/src/extra/libs/png/lodepng.c
    LVGL/GUI/lvgl/src/extra/libs/png/lv_png.c
    LVGL/GUI/lvgl/src/extra/libs/qrcode/lv_qrcode.c
    LVGL/GUI/lvgl/src/extra/libs/qrcode/qrcodegen.c
    LVGL/GUI/lvgl/src/extra/libs/rlottie/lv_rlottie.c
    LVGL/GUI/lvgl/src/extra/libs/sjpg/lv_sjpg.c
    LVGL/GUI/lvgl/src/extra/libs/sjpg/tjpgd.c
    LVGL/GUI/lvgl/src/extra/others/gridnav/lv_gridnav.c
    LVGL/GUI/lvgl/src/extra/others/monkey/lv_monkey.c
    LVGL/GUI/lvgl/src/extra/others/snapshot/lv_snapshot.c
    LVGL/GUI/lvgl/src/extra/themes/basic/lv_theme_basic.c
    LVGL/GUI/lvgl/src/extra/themes/default/lv_theme_default.c
    LVGL/GUI/lvgl/src/extra/themes/mono/lv_theme_mono.c
    LVGL/GUI/lvgl/src/extra/widgets/animimg/lv_animimg.c
    LVGL/GUI/lvgl/src/extra/widgets/calendar/lv_calendar.c
    LVGL/GUI/lvgl/src/extra/widgets/calendar/lv_calendar_header_arrow.c
    LVGL/GUI/lvgl/src/extra/widgets/calendar/lv_calendar_header_dropdown.c
    LVGL/GUI/lvgl/src/extra/widgets/chart/lv_chart.c
    LVGL/GUI/lvgl/src/extra/widgets/colorwheel/lv_colorwheel.c
    LVGL/GUI/lvgl/src/extra/widgets/imgbtn/lv_imgbtn.c
    LVGL/GUI/lvgl/src/extra/widgets/keyboard/lv_keyboard.c
    LVGL/GUI/lvgl/src/extra/widgets/led/lv_led.c
    LVGL/GUI/lvgl/src/extra/widgets/list/lv_list.c
    LVGL/GUI/lvgl/src/extra/widgets/menu/lv_menu.c
    LVGL/GUI/lvgl/src/extra/widgets/meter/lv_meter.c
    LVGL/GUI/lvgl/src/extra/widgets/msgbox/lv_msgbox.c
    LVGL/GUI/lvgl/src/extra/widgets/span/lv_span.c
    LVGL/GUI/lvgl/src/extra/widgets/spinbox/lv_spinbox.c
    LVGL/GUI/lvgl/src/extra/widgets/spinner/lv_spinner.c
    LVGL/GUI/lvgl/src/extra/widgets/tabview/lv_tabview.c
    LVGL/GUI/lvgl/src/extra/widgets/tileview/lv_tileview.c
    LVGL/GUI/lvgl/src/extra/widgets/win/lv_win.c
    LVGL/GUI/lvgl/src/font/lv_font.c
    LVGL/GUI/lvgl/src/font/lv_font_dejavu_16_persian_hebrew.c
    LVGL/GUI/lvgl/src/font/lv_font_fmt_txt.c
    LVGL/GUI/lvgl/src/font/lv_font_loader.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_10.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_12.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_12_subpx.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_14.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_16.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_18.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_20.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_22.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_24.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_26.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_28.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_28_compressed.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_30.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_32.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_34.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_36.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_38.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_40.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_42.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_44.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_46.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_48.c
    LVGL/GUI/lvgl/src/font/lv_font_montserrat_8.c
    LVGL/GUI/lvgl/src/font/lv_font_simsun_16_cjk.c
    LVGL/GUI/lvgl/src/font/lv_font_unscii_16.c
    LVGL/GUI/lvgl/src/draw/stm32_dma2d/lv_gpu_stm32_dma2d.c
    LVGL/GUI/lvgl/src/draw/sdl/lv_draw_sdl.c
    LVGL/GUI/lvgl/src/draw/sdl/lv_draw_sdl_arc.c
    LVGL/GUI/lvgl/src/draw/sdl/lv_draw_sdl_bg.c
    LVGL/GUI/lvgl/src/draw/sdl/lv_draw_sdl_composite.c
    LVGL/GUI/lvgl/src/draw/sdl/lv_draw_sdl_img.c
    LVGL/GUI/lvgl/src/draw/sdl/lv_draw_sdl_label.c
    LVGL/GUI/lvgl/src/draw/sdl/lv_draw_sdl_line.c
    LVGL/GUI/lvgl/src/draw/sdl/lv_draw_sdl_mask.c
    LVGL/GUI/lvgl/src/draw/sdl/lv_draw_sdl_polygon.c
    LVGL/GUI/lvgl/src/draw/sdl/lv_draw_sdl_rect.c
    LVGL/GUI/lvgl/src/draw/sdl/lv_draw_sdl_stack_blur.c
    LVGL/GUI/lvgl/src/draw/sdl/lv_draw_sdl_texture_cache.c
    LVGL/GUI/lvgl/src/draw/sdl/lv_draw_sdl_utils.c
    LVGL/GUI/lvgl/src/hal/lv_hal_disp.c
    LVGL/GUI/lvgl/src/hal/lv_hal_indev.c
    LVGL/GUI/lvgl/src/hal/lv_hal_tick.c
    LVGL/GUI/lvgl/src/misc/lv_anim.c
    LVGL/GUI/lvgl/src/misc/lv_anim_timeline.c
    LVGL/GUI/lvgl/src/misc/lv_area.c
    LVGL/GUI/lvgl/src/misc/lv_async.c
    LVGL/GUI/lvgl/src/misc/lv_bidi.c
    LVGL/GUI/lvgl/src/misc/lv_color.c
    LVGL/GUI/lvgl/src/misc/lv_fs.c
    LVGL/GUI/lvgl/src/misc/lv_gc.c
    LVGL/GUI/lvgl/src/misc/lv_ll.c
    LVGL/GUI/lvgl/src/misc/lv_log.c
    LVGL/GUI/lvgl/src/misc/lv_lru.c
    LVGL/GUI/lvgl/src/misc/lv_math.c
    LVGL/GUI/lvgl/src/misc/lv_mem.c
    LVGL/GUI/lvgl/src/misc/lv_printf.c
    LVGL/GUI/lvgl/src/misc/lv_style.c
    LVGL/GUI/lvgl/src/misc/lv_style_gen.c
    LVGL/GUI/lvgl/src/misc/lv_templ.c
    LVGL/GUI/lvgl/src/misc/lv_timer.c
    LVGL/GUI/lvgl/src/misc/lv_tlsf.c
    LVGL/GUI/lvgl/src/misc/lv_txt.c
    LVGL/GUI/lvgl/src/misc/lv_txt_ap.c
    LVGL/GUI/lvgl/src/misc/lv_utils.c
    LVGL/GUI/lvgl/src/widgets/lv_arc.c
    LVGL/GUI/lvgl/src/widgets/lv_bar.c
    LVGL/GUI/lvgl/src/widgets/lv_btn.c
    LVGL/GUI/lvgl/src/widgets/lv_btnmatrix.c
    LVGL/GUI/lvgl/src/widgets/lv_canvas.c
    LVGL/GUI/lvgl/src/widgets/lv_checkbox.c
    LVGL/GUI/lvgl/src/widgets/lv_dropdown.c
    LVGL/GUI/lvgl/src/widgets/lv_img.c
    LVGL/GUI/lvgl/src/widgets/lv_label.c
    LVGL/GUI/lvgl/src/widgets/lv_line.c
    LVGL/GUI/lvgl/src/widgets/lv_objx_templ.c
    LVGL/GUI/lvgl/src/widgets/lv_roller.c
    LVGL/GUI/lvgl/src/widgets/lv_slider.c
    LVGL/GUI/lvgl/src/widgets/lv_switch.c
    LVGL/GUI/lvgl/src/widgets/lv_table.c
    LVGL/GUI/lvgl/src/widgets/lv_textarea.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    Core/Inc
    ${CMAKE_SOURCE_DIR}/app
    ${CMAKE_SOURCE_DIR}/bsp
    ${CMAKE_SOURCE_DIR}/dev
    ${CMAKE_SOURCE_DIR}/gui
    ${CMAKE_SOURCE_DIR}/LVGL/GUI/lvgl/src
    ${CMAKE_SOURCE_DIR}/LVGL/GUI/lvgl/src/core
    ${CMAKE_SOURCE_DIR}/LVGL/GUI/lvgl/src/draw
    ${CMAKE_SOURCE_DIR}/LVGL/GUI/lvgl/src/font
    ${CMAKE_SOURCE_DIR}/LVGL/GUI/lvgl/src/gpu
    ${CMAKE_SOURCE_DIR}/LVGL/GUI/lvgl/src/hal
    ${CMAKE_SOURCE_DIR}/LVGL/GUI/lvgl/src/misc
    ${CMAKE_SOURCE_DIR}/LVGL/GUI/lvgl/src/extra
    ${CMAKE_SOURCE_DIR}/LVGL/GUI/lvgl/src/widgets
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
    UI_FONT_W25Q128_IMG_PATH="${CMAKE_SOURCE_DIR}/tools/ui_font_w25q128.img"
)

set_source_files_properties(app/app_w25q128_font_image.S PROPERTIES
    OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/tools/ui_font_w25q128.img"
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
