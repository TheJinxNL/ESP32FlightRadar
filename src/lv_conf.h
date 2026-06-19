/**
 * lv_conf.h — LVGL 8.4 configuration for ESP32-C3 + GC9A01 240x240
 *
 * Place this file in firmware/src/ so the build flag -I src lets
 * LVGL find it via LV_CONF_INCLUDE_SIMPLE.
 */

/* clang-format off */
#if 1  /* Set to "1" to enable */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/
/* Color depth: 1 (1 byte/pixel), 8 (RGB332), 16 (RGB565), 32 (ARGB8888) */
#define LV_COLOR_DEPTH 16

/* Swap the 2 bytes of RGB565 color (useful on SPI displays) */
#define LV_COLOR_16_SWAP 0

/* Enable more complex drawing routines to manage screens transparency.
 * Can be used if the screen has round or irregular shape.
 * Useful if OPA_COVER fills are never used or custom indev or mouse cursor is used */
#define LV_COLOR_SCREEN_TRANSP 0

/* Adjust color mix functions rounding. GPUs might calculate color mix (blending) differently.
 * 0: round down, 64: round up from x.75, 128: round up from half, 192: round up from x.25, 254: round up */
#define LV_COLOR_MIX_ROUND_OFS 0

/* Images pixels with this color will not be drawn if they are chroma keyed) */
#define LV_COLOR_CHROMA_KEY lv_color_hex(0x00ff00)  /*pure green*/

/*=========================
   MEMORY SETTINGS
 *=========================*/
/* Size of the memory available for `lv_mem_alloc()` in bytes (>= 2kB)
 * Set 0 to use stdlib's `malloc()` and `free()` */
#define LV_MEM_CUSTOM 1
#if LV_MEM_CUSTOM == 0
    #define LV_MEM_SIZE (48U * 1024U)   /*[bytes]*/
    #define LV_MEM_ADR 0                /*0: unused*/
    #define LV_MEM_AUTO_DEFRAG 1
#else
    #define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
    #define LV_MEM_CUSTOM_ALLOC   malloc
    #define LV_MEM_CUSTOM_FREE    free
    #define LV_MEM_CUSTOM_REALLOC realloc
#endif

/* Use stdlib's memset and memcpy instead of LVGL's own */
#define LV_MEMCPY_MEMSET_STD 1

/*====================
   HAL SETTINGS
 *====================*/
/* Default display refresh period in ms (can be changed at runtime) */
#define LV_DISP_DEF_REFR_PERIOD 16   /*[ms] ~60 fps*/

/* Input device read period in ms (can be changed at runtime) */
#define LV_INDEV_DEF_READ_PERIOD 30  /*[ms]*/

/* Use a custom tick source that tells elapsed time in ms.
 * It removes the need to manually update the tick with `lv_tick_inc()` */
#define LV_TICK_CUSTOM 0

/* Default DPI (can be changed at runtime) */
#define LV_DPI_DEF 130

/*=======================
 * FEATURE CONFIGURATION
 *=======================*/

/*-------------
 * Drawing
 *-----------*/
/* Enable complex draw engine.
 * Required to draw shadow, gradient, rounded corners, circles, arc, skew lines, image transformations, or any masks */
#define LV_DRAW_COMPLEX 1
#if LV_DRAW_COMPLEX != 0
    /* Allow buffering some shadow calculation.
     * LV_SHADOW_CACHE_SIZE is the max shadow size to buffer, where shadow size is
     * `shadow_width + radius`. Caching has LV_SHADOW_CACHE_SIZE^2 RAM cost */
    #define LV_SHADOW_CACHE_SIZE 0
    /* Set number of maximally cached circle data. The memory usage of one entry is 4 * radius bytes */
    #define LV_CIRCLE_CACHE_SIZE 4
#endif

/*-------------
 * GPU
 *-----------*/
#define LV_USE_GPU_STM32_DMA2D 0
#define LV_USE_GPU_NXP_PXP     0
#define LV_USE_GPU_NXP_VG_LITE 0
#define LV_USE_GPU_SWM341_DMA2D 0
#define LV_USE_GPU_SDL         0

/*-------------
 * Logging
 *-----------*/
#define LV_USE_LOG 1
#if LV_USE_LOG
    /* Logging level: LV_LOG_LEVEL_TRACE/INFO/WARN/ERROR/USER/NONE */
    #define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
    /* Print log to serial with `printf` */
    #define LV_LOG_PRINTF 1
    /* Enable/disable LV_LOG_TRACE for modules */
    #define LV_LOG_TRACE_MEM        0
    #define LV_LOG_TRACE_TIMER      0
    #define LV_LOG_TRACE_INDEV      0
    #define LV_LOG_TRACE_DISP_REFR  0
    #define LV_LOG_TRACE_EVENT      0
    #define LV_LOG_TRACE_OBJ_CREATE 0
    #define LV_LOG_TRACE_LAYOUT     0
    #define LV_LOG_TRACE_ANIM       0
#endif

/*-------------
 * Asserts
 *-----------*/
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0
#define LV_ASSERT_HANDLER_INCLUDE <stdint.h>
#define LV_ASSERT_HANDLER while(1);

/*-------------
 * Others
 *-----------*/
/* Add a custom handler when assert happens e.g. to restart the MCU */
#define LV_USE_USER_DATA 1

/*=====================
 *  COMPILER SETTINGS
 *====================*/
#define LV_BIG_ENDIAN_SYSTEM 0
#define LV_ATTRIBUTE_MEM_ALIGN_SIZE 4
#define LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_LARGE_CONST
#define LV_ATTRIBUTE_LARGE_RAM_ARRAY
#define LV_ATTRIBUTE_FAST_MEM
#define LV_ATTRIBUTE_DMA
#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning  /*The default value just prevents GCC warning*/
#define LV_USE_LARGE_COORD 0

/*==================
 *   FONT USAGE
 *===================*/
/* Montserrat fonts with bpp = 4 (anti-aliased) */
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0

/* Demonstrate special features */
#define LV_FONT_MONTSERRAT_12_SUBPX      0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_SIMSUN_16_CJK            0

/* Pixel perfect monospace font */
#define LV_FONT_UNSCII_8  0
#define LV_FONT_UNSCII_16 0

/* Optionally declare custom fonts here. If the custom font uses `LV_FONT_CUSTOM_DECLARE` it will be added here */
#define LV_FONT_CUSTOM_DECLARE

/* Always set a default font */
#define LV_FONT_DEFAULT &lv_font_montserrat_16

/* Enable it if you have fonts with a lot of characters.
 * The limit depends on the font size, font face and bpp.
 * Compiler error will be triggered if exceeded. */
#define LV_FONT_FMT_TXT_LARGE 0

/* Enables/disables support for compressed fonts. */
#define LV_USE_FONT_COMPRESSED 0

/* Enable subpixel rendering */
#define LV_USE_FONT_SUBPX 0
#if LV_USE_FONT_SUBPX
    /* Set the pixel order of the display. Physical order of R,G,B channels.
     * Doesn't matter with "normal" fonts. */
    #define LV_FONT_SUBPX_BGR 0  /*0: RGB; 1:BGR order*/
#endif

/*=================
 *  TEXT SETTINGS
 *=================*/
/**
 * Select a character encoding for strings.
 * Your IDE or editor should have the same character encoding.
 * - LV_TXT_ENC_UTF8
 * - LV_TXT_ENC_ASCII
 */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/* Can break (wrap) texts on these chars */
#define LV_TXT_BREAK_CHARS " ,.;:-_"

/* If a word is at least this long, will break wherever "prettiest"
 * To disable, set to a value <= 0 */
#define LV_TXT_LINE_BREAK_LONG_LEN 0

/* Minimum number of characters in a long word to put on a line before a break.
 * Depends on LV_TXT_LINE_BREAK_LONG_LEN. */
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN 3

/* Minimum number of characters in a long word to put on a line after a break.
 * Depends on LV_TXT_LINE_BREAK_LONG_LEN. */
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3

/* The control character to use for signaling text recoloring. */
#define LV_TXT_COLOR_CMD "#"

/* Support bidirectional texts. Allows mixing Left-to-Right and Right-to-Left texts.
 * The direction will be processed according to the Unicode Bidirectional Algorithm:
 * https://www.unicode.org/reports/tr9/ */
#define LV_USE_BIDI 0

/* Enable Arabic/Persian processing
 * In these languages characters should be replaced with an
 * other form based on their position in the text */
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*==================
 *  WIDGET USAGE
 *================*/
#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  0
#define LV_USE_CANVAS     1
#define LV_USE_CHECKBOX   0
#define LV_USE_DROPDOWN   0
#define LV_USE_IMG        1
#define LV_USE_LABEL      1
#if LV_USE_LABEL != 0
    #define LV_LABEL_TEXT_SELECTION 0
    #define LV_LABEL_LONG_TXT_HINT  0
#endif
#define LV_USE_LINE       1
#define LV_USE_ROLLER     0
#define LV_USE_SLIDER     0
#define LV_USE_SWITCH     0
#define LV_USE_TEXTAREA   0
#define LV_USE_TABLE      0

/*==================
 * EXTRA COMPONENTS
 *==================*/

/*-----------
 * Widgets
 *----------*/
#define LV_USE_ANIMIMG    0
#define LV_USE_CALENDAR   0
#define LV_USE_CHART      0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN     0
#define LV_USE_KEYBOARD   0
#define LV_USE_LED        1
#define LV_USE_LIST       0
#define LV_USE_MENU       0
#define LV_USE_METER      0
#define LV_USE_MSGBOX     0
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    1
#define LV_USE_TABVIEW    0
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0
#define LV_USE_SPAN       0

/*-----------
 * Themes
 *----------*/
/* A simple, impressive and very complete theme */
#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
    /* 0: Light mode; 1: Dark mode */
    #define LV_THEME_DEFAULT_DARK 1
    /* 1: Enable grow on press */
    #define LV_THEME_DEFAULT_GROW 1
    /* Default transition time in [ms] */
    #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif

/* A very simple theme that is a good starting point for a custom theme */
#define LV_USE_THEME_BASIC 1

/* A theme designed for monochrome displays */
#define LV_USE_THEME_MONO 0

/*-----------
 * Layouts
 *----------*/
#define LV_USE_FLEX 1
#define LV_USE_GRID 0

/*---------------------
 * 3rd party libraries
 *--------------------*/
/* File system interfaces */
#define LV_USE_FS_STDIO     0
#define LV_USE_FS_POSIX     0
#define LV_USE_FS_WIN32     0
#define LV_USE_FS_FATFS     0

/* PNG decoder library */
#define LV_USE_PNG 0

/* BMP decoder library */
#define LV_USE_BMP 0

/* JPG + split JPG decoder library */
#define LV_USE_SJPG 0

/* GIF decoder library */
#define LV_USE_GIF 0

/* QR code library */
#define LV_USE_QRCODE 0

/* FreeType library */
#define LV_USE_FREETYPE 0

/* Rlottie library */
#define LV_USE_RLOTTIE 0

/* FFmpeg library */
#define LV_USE_FFMPEG  0

/*-----------
 * Others
 *----------*/
/* 1: Enable API to take snapshot for object */
#define LV_USE_SNAPSHOT 0

/* 1: Enable Monkey test */
#define LV_USE_MONKEY 0

/* 1: Enable grid navigation */
#define LV_USE_GRIDNAV 0

/* 1: Enable lv_obj fragment */
#define LV_USE_FRAGMENT 0

/* 1: Support using images as font in label or span widgets */
#define LV_USE_IMGFONT 0

/* 1: Enable a published subscriber based messaging system */
#define LV_USE_MSG 0

/* 1: Enable Pinyin input method */
#define LV_USE_IME_PINYIN 0

/*==================
 * EXAMPLES
 *==================*/
/* Enable the examples to be built with the library */
#define LV_BUILD_EXAMPLES 0

/*===================
 * DEMO USAGE
 ====================*/
#define LV_USE_DEMO_WIDGETS        0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0
#define LV_USE_DEMO_BENCHMARK      0
#define LV_USE_DEMO_STRESS         0
#define LV_USE_DEMO_MUSIC          0

#endif /*LV_CONF_H*/
#endif /*End of "Content enable"*/
