/*
 * SSTV receiver UI - initial panel skeleton
 */

#include "dialog_sstv.h"
#include "styles.h"
#include "dialog.h"
#include "buttons.h"
#include "main_screen.h"
#include "keyboard.h"
#include "sstv/decoder.h"
#include "sstv/encoder.h"
#include "radio.h"
#include "events.h"

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <png.h>

#include "audio.h"

#define SSTV_CANVAS_W 440
#define SSTV_CANVAS_H 330
#define SSTV_TEST_WAV "/mnt/sstv.wav"
#define SSTV_DEBUG_WAV "/mnt/sstv-debug.wav"
#define SSTV_SAVE_DIR "/mnt/sstv"
#define SSTV_IMAGE_H 330
#define SSTV_IMAGE_W 440
#define SSTV_IMAGE_X 10
#define SSTV_IMAGE_Y 10
#define SSTV_BROWSER_MAX_FILES 128
#define SSTV_BROWSER_W 710
#define SSTV_BROWSER_H 320
/* Image position is calculated from the mode detected by VIS. */

static void construct_cb(lv_obj_t *parent);
static void destruct_cb(void);
static void test_cb(button_data_t *data);
static void test_tx_cb(button_data_t *data);
static void key_cb(lv_event_t *e);
static void start_cb(button_data_t *data);
static void cont_start_cb(button_data_t *data);
static void cont_mode_cb(button_data_t *data);
static void browser_cb(button_data_t *data);
static void delete_picture_cb(button_data_t *data);
static void page_1_cb(button_data_t *data);
static void page_2_cb(button_data_t *data);
static void audio_cb(unsigned int n, float *samples);
static void close_cb(button_data_t *data);
static void ui_timer_cb(lv_timer_t *timer);
static void *test_thread_cb(void *arg);
static void *live_decode_thread_cb(void *arg);
static void *cont_decode_thread_cb(void *arg);
static void *tx_thread_cb(void *arg);
static bool save_debug_wav(const char *path, const float *samples, long count);
static bool save_sstv_png(const uint8_t *rgb, int width, int height, char *saved_name, size_t saved_name_size);
static void set_status(const char *text);
static void reset_ui_to_standby(void);
static void render_scaled_image(void);
static button_data_t btn_start;
static button_data_t btn_cont_start;
static button_data_t btn_cont_mode;
static button_data_t btn_browser;
static button_data_t btn_delete_picture;
static buttons_page_t btn_page_1;

static void set_receiver_ui_stopped(void);
static void set_receiver_ui_listening(void);
static void set_receiver_ui_decoding(const char *mode_name);

static lv_obj_t *sstv_frame = NULL;
static lv_obj_t *sstv_canvas = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *progress_bar = NULL;
static volatile bool saved_overlay_pending = false;
static char saved_overlay_name[96];
static lv_color_t sstv_buffer[SSTV_CANVAS_W * SSTV_CANVAS_H];
static uint8_t test_rgb[SSTV_MAX_W * SSTV_MAX_H * 3];
static sstv_info_t test_info;
static lv_timer_t *ui_timer = NULL;
static pthread_t test_thread;
static bool test_thread_valid = false;
static volatile bool test_running = false;
static volatile bool test_ready = false;
static volatile int test_rows = 0;
static int shown_rows = 0;

/* SSTV test transmitter.  The worker owns all blocking audio/radio work;
 * LVGL is touched only by ui_timer_cb(). */
static pthread_t tx_thread;
static bool tx_thread_valid = false;
static volatile bool tx_running = false;
static volatile bool tx_abort = false;
static volatile bool tx_started = false;
static volatile int tx_progress = 0;
static struct timespec tx_key_time;
static volatile bool tx_key_time_valid = false;

/* Nominal on-air SSTV payload durations.  Progress is deliberately based on
 * elapsed wall-clock time, not on how quickly audio_play() accepts samples. */
#define SSTV_TX_PREROLL_MS 500u
static unsigned int sstv_tx_duration_ms(sstv_mode_t mode)
{
    switch (mode) {
        case SSTV_MODE_ROBOT36:    return 36910u;  /* VIS 910 ms + 240 x 150.000 ms */
        case SSTV_MODE_SCOTTIE_S1: return 110031u; /* VIS 910 + 9 ms + 256 x 426.220 ms */
        default:                   return 0u;
    }
}

static uint64_t monotonic_ms_since(const struct timespec *start)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int64_t sec = (int64_t)now.tv_sec - (int64_t)start->tv_sec;
    int64_t nsec = (int64_t)now.tv_nsec - (int64_t)start->tv_nsec;
    return (uint64_t)(sec * 1000 + nsec / 1000000);
}
static volatile int tx_result = 0; /* 0 idle/running, 1 done, <0 error/abort */

/* Saved-SSTV browser.  It is deliberately independent from the live SSTV
 * canvas/state machine so reception and decoding continue while it is open. */
static lv_obj_t *sstv_browser_table = NULL;
static lv_obj_t *sstv_browser_canvas = NULL;
static lv_color_t *sstv_browser_buffer = NULL;
static char *sstv_browser_files[SSTV_BROWSER_MAX_FILES];
static unsigned int sstv_browser_file_count = 0;
static bool sstv_browser_open = false;
static bool sstv_browser_preview = false;
static uint8_t *sstv_browser_image = NULL;
static unsigned int sstv_browser_image_width = 0;
static unsigned int sstv_browser_image_height = 0;

static void sstv_browser_free_files(void);
static void sstv_browser_refresh(void);
static void sstv_browser_update_list(void);
static void sstv_browser_build(void);
static void sstv_browser_close(void);
static void sstv_browser_return_to_list(void);
static void sstv_browser_delete_selected(void);
static bool sstv_browser_open_png(const char *filename);
static void sstv_browser_free_image(void);
static void sstv_browser_render_image(void);
static void sstv_browser_pressed_cb(lv_event_t *e);
static void sstv_browser_preview_pressed_cb(lv_event_t *e);
static void sstv_browser_update_buttons(void);

/* Live RF capture.  Keep a short rolling prebuffer so the VIS leader is
 * preserved when we decide that an SSTV transmission has started. */
#define SSTV_PREBUF_SAMPLES (SSTV_SAMPLE_RATE * 2)
#define SSTV_VIS_WINDOW_SAMPLES (SSTV_SAMPLE_RATE)
#define SSTV_VIS_CHECK_STEP (SSTV_SAMPLE_RATE / 20) /* 50 ms */

static pthread_mutex_t live_mutex = PTHREAD_MUTEX_INITIALIZER;
static float live_ring[SSTV_PREBUF_SAMPLES];
static float live_vis_window[SSTV_VIS_WINDOW_SAMPLES];
static size_t live_ring_pos = 0;
static size_t live_ring_count = 0;
static unsigned int live_since_check = 0;
static uint64_t live_wait_samples = 0;
static uint64_t live_wait_log_mark = 0;
static volatile bool live_enabled = false;
static volatile bool live_capturing = false;
static volatile bool live_decode_pending = false;
static volatile bool live_decoding = false;
static volatile bool live_canvas_clear_pending = false;
static float *live_samples = NULL;
static long live_written = 0;
static long live_target = 0;
static sstv_info_t live_info;

/* Manual continuous RX. This state machine is deliberately separate from
 * Auto RX so the validated VIS-driven path remains unchanged. */
#define SSTV_CONT_MAX_SECONDS 300
static volatile bool cont_capturing = false;
static volatile bool cont_decode_pending = false;
static volatile bool cont_decoding = false;
static float *cont_samples = NULL;
static long cont_written = 0;
static long cont_capacity = 0;
static sstv_mode_t cont_mode = SSTV_MODE_SCOTTIE_S1;
static sstv_mode_t cont_decode_mode = SSTV_MODE_SCOTTIE_S1;

static const sstv_mode_t cont_modes[] = {
    SSTV_MODE_ROBOT8_BW, SSTV_MODE_ROBOT36, SSTV_MODE_ROBOT72,
    SSTV_MODE_MARTIN_M1, SSTV_MODE_MARTIN_M2,
    SSTV_MODE_SCOTTIE_S1, SSTV_MODE_SCOTTIE_S2
};

static const char *cont_mode_name(sstv_mode_t mode)
{
    switch (mode) {
        case SSTV_MODE_ROBOT8_BW: return "Robot 8";
        case SSTV_MODE_ROBOT36: return "Robot 36";
        case SSTV_MODE_ROBOT72: return "Robot 72";
        case SSTV_MODE_MARTIN_M1: return "Martin M1";
        case SSTV_MODE_MARTIN_M2: return "Martin M2";
        case SSTV_MODE_SCOTTIE_S1: return "Scottie S1";
        case SSTV_MODE_SCOTTIE_S2: return "Scottie S2";
        default: return "Unknown";
    }
}

static void update_rx_controls(void)
{
    bool auto_active = live_enabled || live_capturing || live_decode_pending || live_decoding;
    bool cont_active = cont_capturing || cont_decode_pending || cont_decoding;

    buttons_disabled(&btn_start, cont_active);
    buttons_disabled(&btn_cont_start, auto_active || cont_decode_pending || cont_decoding);
    buttons_disabled(&btn_cont_mode, auto_active || cont_active);

    /* The mode selector is only present while both receivers are idle. */
    btn_page_1.items[3] = (!auto_active && !cont_active) ? &btn_cont_mode : NULL;
    if (buttons_get_cur_page() == &btn_page_1)
        buttons_load_page(&btn_page_1);
}

static void set_status(const char *text)
{
    if (status_label) lv_label_set_text(status_label, text ? text : "");
}

static void set_receiver_ui_stopped(void)
{
    set_status("Stand by.\n\n- Press Start Auto RX to start Autodecoder.\n\n- Press Start Manual RX to start Manual decoder, press RX Mode button to cycle all available modes for the Manual decoder.");
    if (progress_bar) {
        lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
        lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
    }
    btn_start.label = "Start\nAuto RX";
    buttons_disabled(&btn_start, false);
    if (btn_start.disp_btn) buttons_refresh(&btn_start);
    update_rx_controls();
}

static void set_receiver_ui_listening(void)
{
    set_status("SSTV Autodecoder is running.\n\nAwaiting signal...");
    if (progress_bar) {
        lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
        lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
    }
    btn_start.label = "Stop\nAuto RX";
    buttons_disabled(&btn_start, false);
    if (btn_start.disp_btn) buttons_refresh(&btn_start);
    update_rx_controls();
}

static void set_receiver_ui_decoding(const char *mode_name)
{
    char status[96];
    snprintf(status, sizeof(status), "Decoding %s", mode_name ? mode_name : "SSTV");
    set_status(status);
    if (progress_bar) lv_obj_clear_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
    btn_start.label = "Stop\nAuto RX";
    buttons_disabled(&btn_start, false);
    if (btn_start.disp_btn) buttons_refresh(&btn_start);
    update_rx_controls();
}

static void reset_ui_to_standby(void)
{
    set_receiver_ui_stopped();
}

static bool save_sstv_png(const uint8_t *rgb, int width, int height, char *saved_name, size_t saved_name_size)
{
    if (!rgb || width <= 0 || height <= 0) return false;
    if (mkdir(SSTV_SAVE_DIR, 0775) != 0 && errno != EEXIST) {
        fprintf(stderr, "SSTV SAVE: mkdir failed: %s\n", strerror(errno)); return false;
    }
    time_t now=time(NULL); struct tm tm_now;
    if (!localtime_r(&now,&tm_now)) return false;
    char path[160];
    snprintf(path,sizeof(path),SSTV_SAVE_DIR "/sstv-%04d%02d%02d-%02d%02d%02d.png",
             tm_now.tm_year+1900,tm_now.tm_mon+1,tm_now.tm_mday,
             tm_now.tm_hour,tm_now.tm_min,tm_now.tm_sec);
    if (saved_name && saved_name_size > 0) {
        const char *base = strrchr(path, '/');
        snprintf(saved_name, saved_name_size, "%s", base ? base + 1 : path);
    }
    FILE *fp=fopen(path,"wb"); if(!fp){fprintf(stderr,"SSTV SAVE: fopen %s failed: %s\n",path,strerror(errno));return false;}
    png_structp png=png_create_write_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
    png_infop info=png ? png_create_info_struct(png) : NULL;
    if(!png || !info){if(png)png_destroy_write_struct(&png,NULL);fclose(fp);return false;}
    if(setjmp(png_jmpbuf(png))){png_destroy_write_struct(&png,&info);fclose(fp);return false;}
    png_init_io(png,fp);
    png_set_IHDR(png,info,width,height,8,PNG_COLOR_TYPE_RGB,PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_DEFAULT,PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png,info);
    for(int y=0;y<height;++y) png_write_row(png,(png_bytep)(rgb+(size_t)y*width*3));
    png_write_end(png,NULL); png_destroy_write_struct(&png,&info); fclose(fp);
    fprintf(stderr,"SSTV SAVE: image saved to %s (%dx%d)\n",path,width,height);
    return true;
}

static void render_scaled_image(void)
{
    if (!sstv_canvas || test_rows <= 0 || test_info.width <= 0 || test_info.height <= 0) return;
    lv_canvas_fill_bg(sstv_canvas, lv_color_white(), LV_OPA_COVER);
    int out_h=SSTV_IMAGE_H;
    int out_w=(test_info.width*out_h)/test_info.height;
    if(out_w>SSTV_IMAGE_W){out_w=SSTV_IMAGE_W;out_h=(test_info.height*out_w)/test_info.width;}
    int x0=(SSTV_IMAGE_W-out_w)/2;
    int y0=(SSTV_IMAGE_H-out_h)/2;
    for(int oy=0;oy<out_h;++oy){
        int sy=(int)((int64_t)oy*test_info.height/out_h);
        for(int ox=0;ox<out_w;++ox){
            int sx=(int)((int64_t)ox*test_info.width/out_w);
            size_t o=((size_t)sy*test_info.width+sx)*3;
            lv_canvas_set_px(sstv_canvas,x0+ox,y0+oy,lv_color_make(test_rgb[o],test_rgb[o+1],test_rgb[o+2]));
        }
    }
    lv_obj_invalidate(sstv_canvas);
}


static float live_goertzel(const float *x, int n, float hz)
{
    int k = (int)(0.5f + (float)n * hz / (float)SSTV_SAMPLE_RATE);
    float w = 2.0f * 3.14159265358979323846f * (float)k / (float)n;
    float coeff = 2.0f * cosf(w), s0 = 0.0f, s1 = 0.0f, s2 = 0.0f;
    for (int i = 0; i < n; ++i) { s0 = x[i] + coeff*s1 - s2; s2=s1; s1=s0; }
    return s1*s1 + s2*s2 - coeff*s1*s2;
}

static int live_tone(const float *x, int n, float sec, float dur)
{
    int a=(int)(sec*SSTV_SAMPLE_RATE), cnt=(int)(dur*SSTV_SAMPLE_RATE);
    if (a < 0 || a+cnt > n || cnt < 32) return 0;
    static const int hz[] = {1100,1200,1300,1900};
    float best=-1.0f; int tone=0;
    for (unsigned i=0;i<sizeof(hz)/sizeof(hz[0]);++i) {
        float q=live_goertzel(x+a,cnt,(float)hz[i]);
        if (q>best) { best=q; tone=hz[i]; }
    }
    return tone;
}

static bool live_find_vis(const float *x, int n, sstv_info_t *out)
{
    const float limit=(float)n/SSTV_SAMPLE_RATE-0.90f;
    for (float t=0.0f; t<limit; t+=0.005f) {
        if (live_tone(x,n,t+0.10f,0.020f)!=1900 ||
            live_tone(x,n,t+0.295f,0.010f)!=1200 ||
            live_tone(x,n,t+0.42f,0.020f)!=1900) continue;
        int vis=0;
        for (int i=0;i<7;++i) {
            int f=live_tone(x,n,t+0.640f+i*0.030f+0.005f,0.020f);
            if (f==1100) vis|=1<<i;
            else if (f!=1300) { vis=-1; break; }
        }
        if (vis < 0) continue;
        memset(out,0,sizeof(*out)); out->vis=vis;
        switch(vis) {
            case 8:  out->mode=SSTV_MODE_ROBOT36; out->name="Robot 36"; out->width=320; out->height=240; break;
            case 12: out->mode=SSTV_MODE_ROBOT72; out->name="Robot 72"; out->width=320; out->height=240; break;
            case 40: out->mode=SSTV_MODE_MARTIN_M2; out->name="Martin M2"; out->width=320; out->height=256; break;
            case 44: out->mode=SSTV_MODE_MARTIN_M1; out->name="Martin M1"; out->width=320; out->height=256; break;
            case 56: out->mode=SSTV_MODE_SCOTTIE_S2; out->name="Scottie S2"; out->width=320; out->height=256; break;
            case 60: out->mode=SSTV_MODE_SCOTTIE_S1; out->name="Scottie S1"; out->width=320; out->height=256; break;
            case 76: out->mode=SSTV_MODE_SCOTTIE_DX; out->name="Scottie DX"; out->width=320; out->height=256; break;
            default:
                fprintf(stderr,"SSTV LIVE: VIS=%d detected but unsupported\n",vis);
                continue;
        }
        fprintf(stderr,"SSTV LIVE: VIS header found in RF audio\n");
        for (int i=0;i<7;++i)
            fprintf(stderr,"SSTV LIVE: VIS bit %d = %d\n",i,(vis>>i)&1);
        fprintf(stderr,"SSTV LIVE: VIS=%d -> %s (%dx%d)\n",vis,out->name,out->width,out->height);
        return true;
    }
    return false;
}

static float live_mode_seconds(sstv_mode_t m)
{
    switch(m) {
        case SSTV_MODE_ROBOT36: return 36.5f;
        case SSTV_MODE_ROBOT72: return 72.5f;
        case SSTV_MODE_MARTIN_M1: return 115.0f;
        case SSTV_MODE_MARTIN_M2: return 59.0f;
        case SSTV_MODE_SCOTTIE_S1: return 111.0f;
        case SSTV_MODE_SCOTTIE_S2: return 72.0f;
        case SSTV_MODE_SCOTTIE_DX: return 270.0f;
        default: return 120.0f;
    }
}

static button_data_t btn_sstv_page_1 = {
    .type = BTN_TEXT, .label = "(SSTV 1:2)", .press = page_2_cb
};
static button_data_t btn_sstv_page_2 = {
    .type = BTN_TEXT, .label = "(SSTV 2:2)", .press = page_1_cb
};
static button_data_t btn_start = { .type = BTN_TEXT, .label = "Start\nAuto RX", .press = start_cb };
static button_data_t btn_cont_start = { .type = BTN_TEXT, .label = "Start\nManual RX", .press = cont_start_cb };
static button_data_t btn_cont_mode = { .type = BTN_TEXT, .label = "RX Mode\nScottie S1", .press = cont_mode_cb };
static button_data_t btn_browser = { .type = BTN_TEXT, .label = "SSTV\nBrowser", .press = browser_cb };
static button_data_t btn_delete_picture = { .type = BTN_TEXT, .label = "Delete\npicture", .press = delete_picture_cb };
static button_data_t btn_close = { .type = BTN_TEXT, .label = "Close", .press = close_cb };
static button_data_t btn_test_tx = { .type = BTN_TEXT, .label = "Test TX", .press = test_tx_cb };
static button_data_t btn_test = { .type = BTN_TEXT, .label = "Test RX", .press = test_cb };

static buttons_page_t btn_page_1 = {
    .items = { &btn_sstv_page_1, &btn_start, &btn_cont_start, &btn_cont_mode, &btn_close }
};
static buttons_page_t btn_page_2 = {
    .items = { &btn_sstv_page_2, &btn_browser, &btn_delete_picture, &btn_test_tx, &btn_test }
};

static dialog_t dialog = {
    .run = false,
    .construct_cb = construct_cb,
    .destruct_cb = destruct_cb,
    .audio_cb = audio_cb,
    .key_cb = key_cb,
    .btn_page = &btn_page_1
};

dialog_t *dialog_sstv = &dialog;

static void construct_cb(lv_obj_t *parent)
{
    dialog.obj = dialog_init(parent);
    lv_obj_set_size(dialog.obj, 795, 350);
    lv_obj_center(dialog.obj);

    /* Same BIG geometry/background treatment as WEFAX. */
    lv_obj_set_style_bg_img_src(dialog.obj, NULL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dialog.obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(dialog.obj, 0, LV_PART_MAIN);

    sstv_canvas = lv_canvas_create(dialog.obj);
    lv_canvas_set_buffer(
        sstv_canvas,
        sstv_buffer,
        SSTV_CANVAS_W,
        SSTV_CANVAS_H,
        LV_IMG_CF_TRUE_COLOR
    );
    lv_obj_set_size(sstv_canvas, SSTV_CANVAS_W, SSTV_CANVAS_H);
    lv_canvas_fill_bg(sstv_canvas, lv_color_hex(0x182028), LV_OPA_COVER);

    /* Pixel-identical frame setup copied from WEFAX BIG. */
    sstv_frame = lv_obj_create(dialog.obj);
    lv_obj_remove_style_all(sstv_frame);
    lv_obj_add_style(sstv_frame, &panel_style, 0);
    lv_obj_set_style_bg_img_src(sstv_frame, NULL, LV_PART_MAIN);
    lv_obj_set_size(sstv_frame, 795, 350);
    lv_obj_center(sstv_frame);
    lv_obj_set_style_bg_color(sstv_frame, lv_color_hex(0x182028), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sstv_frame, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(sstv_frame, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(sstv_frame, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_opa(sstv_frame, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(sstv_frame, 8, LV_PART_MAIN);
    lv_obj_clear_flag(sstv_frame, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(sstv_frame, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_background(sstv_frame);

    /* White image sheet: maximum 4:3 SSTV area, 10 px from left/top/bottom. */
    lv_obj_align(sstv_canvas, LV_ALIGN_TOP_LEFT, SSTV_IMAGE_X, SSTV_IMAGE_Y);
    lv_obj_set_size(sstv_canvas, SSTV_IMAGE_W, SSTV_IMAGE_H);
    lv_canvas_fill_bg(sstv_canvas, lv_color_white(), LV_OPA_COVER);

    status_label = lv_label_create(dialog.obj);
    lv_label_set_text(status_label, "Stand by.\nPress Start Auto RX to start Autodecoder.\nPress Start Manual RX to start Manual decoder, press Mode button to cycle all available modes for the Manual decoder.");
    lv_obj_set_style_text_color(status_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(status_label, &sony_28, 0);
    lv_obj_set_width(status_label, 300);
    lv_obj_set_pos(status_label, 475, 105);
    lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_LEFT, 0);

    progress_bar = lv_bar_create(dialog.obj);
    lv_obj_set_size(progress_bar, 250, 16);
    lv_obj_set_pos(progress_bar, 500, 155);
    lv_bar_set_range(progress_bar, 0, 1000);
    lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
    lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
    set_receiver_ui_stopped();

    /* RX-only beta: keep the two internal test functions visible but disabled. */
    buttons_disabled(&btn_test_tx, true);
    buttons_disabled(&btn_test, true);

    shown_rows = 0;
    test_ready = false;
    test_rows = 0;
    live_canvas_clear_pending = false;
    ui_timer = lv_timer_create(ui_timer_cb, 30, NULL);
}

static void destruct_cb(void)
{
    /* Closing the SSTV dialog is a hard receiver reset.  Stop both RX paths
     * before destroying the LVGL objects so audio_cb() cannot append to a
     * buffer while it is being released. */
    pthread_mutex_lock(&live_mutex);
    live_enabled = false;
    live_capturing = false;
    live_decode_pending = false;
    cont_capturing = false;
    cont_decode_pending = false;
    pthread_mutex_unlock(&live_mutex);

    /* Never allow the dialog to disappear while its TX worker can still own
     * the modem.  Request abort and wait for radio_set_modem(false). */
    if (tx_thread_valid) {
        tx_abort = true;
        pthread_join(tx_thread, NULL);
        tx_thread_valid = false;
    }
    tx_running = false;
    tx_started = false;

    if (ui_timer) {
        lv_timer_del(ui_timer);
        ui_timer = NULL;
    }

    /* A decoder thread may already be using live_samples/cont_samples and
     * test_rgb.  Let it leave cleanly before freeing/resetting that state. */
    if (test_thread_valid) {
        pthread_join(test_thread, NULL);
        test_thread_valid = false;
    }

    /* A decoder that was already running can re-arm Auto RX when it exits,
     * so force every receiver flag back to idle after the join as well. */
    pthread_mutex_lock(&live_mutex);
    live_enabled = false;
    live_capturing = false;
    live_decode_pending = false;
    live_decoding = false;
    live_canvas_clear_pending = false;
    live_ring_pos = 0;
    live_ring_count = 0;
    live_since_check = 0;
    live_wait_samples = 0;
    live_wait_log_mark = 0;

    cont_capturing = false;
    cont_decode_pending = false;
    cont_decoding = false;
    pthread_mutex_unlock(&live_mutex);

    free(live_samples);
    live_samples = NULL;
    live_written = 0;
    live_target = 0;

    free(cont_samples);
    cont_samples = NULL;
    cont_written = 0;
    cont_capacity = 0;

    test_running = false;
    test_ready = false;
    test_rows = 0;
    shown_rows = 0;
    saved_overlay_pending = false;
    saved_overlay_name[0] = '\0';

    /* Button_data objects are static and survive dialog destruction.  Restore
     * their default labels/mode now so the next open always starts clean. */
    cont_mode = SSTV_MODE_SCOTTIE_S1;
    cont_decode_mode = SSTV_MODE_SCOTTIE_S1;
    btn_start.label = "Start\nAuto RX";
    btn_cont_start.label = "Start\nManual RX";
    btn_cont_mode.label = "RX Mode\nScottie S1";
    btn_page_1.items[3] = &btn_cont_mode;

    sstv_browser_close();

    fprintf(stderr, "SSTV: dialog closed; Auto RX and Manual RX reset to defaults\n");

    sstv_canvas = NULL;
    sstv_frame = NULL;
    status_label = NULL;
    progress_bar = NULL;
}

static void *test_thread_cb(void *arg)
{
    const char *path = arg ? (const char *)arg : SSTV_TEST_WAV;
    fprintf(stderr, "SSTV %s: starting automatic VIS detection\n", arg ? "LIVE" : "TEST");
    int rows = sstv_decode_wav(path, test_rgb, &test_info);
    test_rows = rows > 0 ? rows : 0;
    test_ready = true;
    test_running = false;
    if (arg) {
        pthread_mutex_lock(&live_mutex);
        live_ring_pos=live_ring_count=0; live_since_check=0; live_wait_samples=live_wait_log_mark=0;
        live_enabled=true;
        pthread_mutex_unlock(&live_mutex);
        fprintf(stderr,"SSTV LIVE: decoder finished; listening again for next VIS header\n");
    }
    return NULL;
}


static bool save_debug_wav(const char *path, const float *samples, long count)
{
    if (!path || !samples || count <= 0) return false;
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "SSTV LIVE: ERROR opening debug WAV %s\n", path);
        return false;
    }

    uint32_t data_bytes = (uint32_t)count * 2u;
    uint32_t riff_size = 36u + data_bytes;
    uint32_t sample_rate = SSTV_SAMPLE_RATE;
    uint32_t byte_rate = sample_rate * 2u;
    uint16_t audio_format = 1, channels = 1, block_align = 2, bits = 16;

    fwrite("RIFF", 1, 4, fp); fwrite(&riff_size, 4, 1, fp); fwrite("WAVE", 1, 4, fp);
    fwrite("fmt ", 1, 4, fp);
    uint32_t fmt_size = 16; fwrite(&fmt_size, 4, 1, fp);
    fwrite(&audio_format, 2, 1, fp); fwrite(&channels, 2, 1, fp);
    fwrite(&sample_rate, 4, 1, fp); fwrite(&byte_rate, 4, 1, fp);
    fwrite(&block_align, 2, 1, fp); fwrite(&bits, 2, 1, fp);
    fwrite("data", 1, 4, fp); fwrite(&data_bytes, 4, 1, fp);

    int16_t pcm[1024];
    long pos = 0;
    while (pos < count) {
        long n = count - pos;
        if (n > (long)(sizeof(pcm) / sizeof(pcm[0]))) n = (long)(sizeof(pcm) / sizeof(pcm[0]));
        for (long i = 0; i < n; ++i) {
            float v = samples[pos + i];
            if (v > 1.0f) v = 1.0f;
            if (v < -1.0f) v = -1.0f;
            pcm[i] = (int16_t)lrintf(v * 32767.0f);
        }
        if (fwrite(pcm, sizeof(int16_t), (size_t)n, fp) != (size_t)n) {
            fprintf(stderr, "SSTV LIVE: ERROR writing debug WAV %s\n", path);
            fclose(fp);
            return false;
        }
        pos += n;
    }
    fclose(fp);
    fprintf(stderr, "SSTV LIVE: debug WAV saved to %s (%ld samples @ %d Hz)\n",
            path, count, SSTV_SAMPLE_RATE);
    return true;
}

static void *live_decode_thread_cb(void *arg)
{
    (void)arg;
    fprintf(stderr, "SSTV LIVE: decoding captured %s directly from RAM (%ld samples @ %d Hz)\n",
            live_info.name, live_written, SSTV_SAMPLE_RATE);
    save_debug_wav(SSTV_DEBUG_WAV, live_samples, live_written);
    int rows = sstv_decode_samples(live_samples, live_written, live_info.mode, test_rgb, &test_info);
    test_rows = rows > 0 ? rows : 0;
    if (rows > 0) {
        char saved_name[96] = {0};
        if (save_sstv_png(test_rgb, test_info.width, test_info.height,
                          saved_name, sizeof(saved_name))) {
            snprintf(saved_overlay_name, sizeof(saved_overlay_name), "%s", saved_name);
            saved_overlay_pending = true;
        }
    }
    test_ready = true;
    free(live_samples); live_samples = NULL; live_written = live_target = 0;
    test_running = false;
    live_decoding = false;
    pthread_mutex_lock(&live_mutex);
    live_ring_pos=live_ring_count=0; live_since_check=0; live_wait_samples=live_wait_log_mark=0;
    live_enabled=true;
    pthread_mutex_unlock(&live_mutex);
    fprintf(stderr,"SSTV LIVE: RAM decoder finished, rows=%d; listening again for next VIS header\n", rows);
    return NULL;
}

static void *cont_decode_thread_cb(void *arg)
{
    (void)arg;
    const char *name = cont_mode_name(cont_decode_mode);
    fprintf(stderr, "SSTV CONT: decoding manual %s capture from RAM (%ld samples @ %d Hz)\n",
            name, cont_written, SSTV_SAMPLE_RATE);
    int rows = sstv_decode_cont_samples(cont_samples, cont_written, cont_decode_mode,
                                        test_rgb, &test_info);
    test_rows = rows > 0 ? rows : 0;
    if (rows > 0) {
        char saved_name[96] = {0};
        if (save_sstv_png(test_rgb, test_info.width, test_info.height,
                          saved_name, sizeof(saved_name))) {
            snprintf(saved_overlay_name, sizeof(saved_overlay_name), "%s", saved_name);
            saved_overlay_pending = true;
        }
    }
    test_ready = true;
    free(cont_samples); cont_samples = NULL;
    cont_written = cont_capacity = 0;
    test_running = false;
    cont_decoding = false;
    fprintf(stderr, "SSTV CONT: decoder finished, rows=%d; receiver returned to stand-by\n", rows);
    return NULL;
}

static void ui_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    /* audio_cb() runs outside the LVGL/UI context.  It only requests the
     * clear; the actual canvas operation is performed here by the UI timer. */
    if (live_canvas_clear_pending) {
        live_canvas_clear_pending = false;
        if (sstv_canvas) {
            lv_canvas_fill_bg(sstv_canvas, lv_color_white(), LV_OPA_COVER);
            lv_obj_invalidate(sstv_canvas);
        }
    }

    if (saved_overlay_pending) {
        char message[160];
        saved_overlay_pending = false;
        snprintf(message, sizeof(message), "SSTV picture %s saved", saved_overlay_name);
        channel_overlay_show(message);
    }

    if (tx_running) {
        if (progress_bar) {
            lv_obj_clear_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
            int v = 0;
            if (tx_key_time_valid) {
                uint64_t keyed_ms = monotonic_ms_since(&tx_key_time);
                if (keyed_ms > SSTV_TX_PREROLL_MS) {
                    uint64_t payload_ms = keyed_ms - SSTV_TX_PREROLL_MS;
                    unsigned int duration_ms = sstv_tx_duration_ms(SSTV_MODE_SCOTTIE_S1);
                    if (duration_ms > 0) {
                        if (payload_ms >= duration_ms) v = 1000;
                        else v = (int)((payload_ms * 1000u) / duration_ms);
                    }
                }
            }
            tx_progress = v;
            lv_bar_set_value(progress_bar,v,LV_ANIM_OFF);
        }
        if (tx_started) set_status("Transmitting Scottie S1...");
    } else if (tx_thread_valid) {
        pthread_join(tx_thread, NULL);
        tx_thread_valid = false;
        if (progress_bar) {
            lv_bar_set_value(progress_bar, tx_result == 1 ? 1000 : 0, LV_ANIM_OFF);
            lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
        }
        if (tx_result == 1) {
            set_status("Scottie S1 test TX complete.");
            channel_overlay_show("SSTV Scottie S1 test TX complete");
        } else if (tx_result == -1) {
            set_status("Scottie S1 test TX aborted.");
        } else {
            char msg[96];
            snprintf(msg,sizeof(msg),"Scottie S1 test TX failed (%d).",tx_result);
            set_status(msg);
        }
        tx_started=false; tx_progress=0;
    }

    if (test_thread_valid && !test_running) {
        pthread_join(test_thread, NULL);
        test_thread_valid = false;
    }

    if (cont_decode_pending && !test_running && !test_thread_valid) {
        cont_decode_pending = false;
        cont_decoding = true;
        fprintf(stderr, "SSTV CONT: STOP - starting manual RAM decoder\n");
        char status[96];
        snprintf(status, sizeof(status), "Decoding Cont %s", cont_mode_name(cont_decode_mode));
        set_status(status);
        if (progress_bar) lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
        shown_rows=0; test_rows=0; test_ready=false; test_running=true;
        update_rx_controls();
        int rc=pthread_create(&test_thread,NULL,cont_decode_thread_cb,NULL);
        if (rc==0) test_thread_valid=true;
        else { fprintf(stderr,"SSTV CONT: decoder pthread_create failed: %d\n",rc); test_running=false; cont_decoding=false; }
    }

    if (live_decode_pending && !test_running && !test_thread_valid) {
        live_decode_pending = false;
        live_decoding = true;
        fprintf(stderr, "SSTV LIVE: capture complete; starting direct RAM image decoder\n");
        set_receiver_ui_decoding(live_info.name);
        if (progress_bar) lv_bar_set_value(progress_bar,1000,LV_ANIM_OFF);
        shown_rows=0; test_rows=0; test_ready=false; test_running=true;
        int rc=pthread_create(&test_thread,NULL,live_decode_thread_cb,NULL);
        if (rc==0) test_thread_valid=true;
        else { fprintf(stderr,"SSTV LIVE: decoder pthread_create failed: %d\n",rc); test_running=false; live_decoding=false; }
    }

    /* Live progress follows the expected capture duration. */
    if (live_capturing && live_target > 0) {
        set_receiver_ui_decoding(live_info.name);
        if (progress_bar) {
            int v=(int)((live_written*1000L)/live_target); if(v<0)v=0; if(v>1000)v=1000;
            lv_bar_set_value(progress_bar,v,LV_ANIM_OFF);
        }
    }

    if (!sstv_canvas || !test_ready || shown_rows >= test_rows) return;
    render_scaled_image();
    shown_rows=test_rows;
    if (progress_bar) lv_bar_set_value(progress_bar,1000,LV_ANIM_OFF);
    if (live_enabled)
        set_receiver_ui_listening();
    else
        reset_ui_to_standby();
}


static void key_cb(lv_event_t *e)
{
    uint32_t key = *((uint32_t *)lv_event_get_param(e));
    switch (key) {
        case KEY_VOL_LEFT_EDIT:
        case KEY_VOL_LEFT_SELECT:
            radio_change_vol(-1);
            break;
        case KEY_VOL_RIGHT_EDIT:
        case KEY_VOL_RIGHT_SELECT:
            radio_change_vol(1);
            break;
        default:
            break;
    }
}

static void *tx_thread_cb(void *arg)
{
    (void)arg;
    int16_t *samples = NULL;
    size_t count = 0;

    int rc = sstv_encode_scottie_s1_png_pcm("/mnt/sstv-tx.png", AUDIO_PLAY_RATE,
                                         &samples, &count);
    if (rc < 0) {
        fprintf(stderr,"SSTV TX: PCM generation failed rc=%d\n",rc);
        tx_result = rc;
        tx_running = false;
        return NULL;
    }
    if (tx_abort) {
        free(samples);
        tx_result = -1;
        tx_running = false;
        return NULL;
    }

    fprintf(stderr,"SSTV TX: keying modem for Scottie S1 test (%zu samples)\n",count);
    radio_set_modem(true);
    clock_gettime(CLOCK_MONOTONIC, &tx_key_time);
    tx_key_time_valid = true;
    tx_started = true;

    /* Give the TX/codec path time to settle before the SSTV VIS leader starts.
     * Keep this out of the encoder so generated WAV files remain standard. */
    enum { TX_PREROLL_CHUNK = 2048 };
    const size_t preroll_count = ((size_t)AUDIO_PLAY_RATE * SSTV_TX_PREROLL_MS) / 1000u;
    static const int16_t silence[TX_PREROLL_CHUNK] = {0};
    size_t preroll_sent = 0;
    while (preroll_sent < preroll_count && !tx_abort) {
        size_t part = preroll_count - preroll_sent;
        if (part > TX_PREROLL_CHUNK) part = TX_PREROLL_CHUNK;
        if (audio_play(silence, part) < 0) {
            rc = -30;
            break;
        }
        preroll_sent += part;
    }

    fprintf(stderr,"SSTV TX: preroll %zu/%zu silent samples\n",
            preroll_sent, preroll_count);

    size_t sent = 0;
    while (rc >= 0 && sent < count && !tx_abort) {
        size_t part = count - sent;
        if (part > 2048u) part = 2048u;
        if (audio_play(samples + sent, part) < 0) {
            rc = -30;
            break;
        }
        sent += part;
    }

    /* Drain everything already accepted by PulseAudio before unkeying.  On an
     * abort this is at most the small amount already queued, not the image. */
    audio_play_wait();
    radio_set_modem(false);
    tx_started = false;
    tx_key_time_valid = false;
    free(samples);

    if (tx_abort) {
        fprintf(stderr,"SSTV TX: aborted at %zu/%zu samples\n",sent,count);
        tx_result = -1;
    } else if (rc < 0) {
        fprintf(stderr,"SSTV TX: audio_play failed rc=%d\n",rc);
        tx_result = rc;
    } else {
        fprintf(stderr,"SSTV TX: Scottie S1 test complete\n");
        tx_progress = 1000;
        tx_result = 1;
    }
    tx_running = false;
    return NULL;
}

static void test_tx_cb(button_data_t *data)
{
    (void)data;

    /* Pressing Test TX again is an emergency/normal abort. */
    if (tx_running) {
        tx_abort = true;
        set_status("Stopping Scottie S1 test TX...");
        return;
    }

    if (test_running || cont_capturing || cont_decode_pending || cont_decoding ||
        live_capturing || live_decode_pending || live_decoding)
        return;

    if (tx_thread_valid) {
        pthread_join(tx_thread,NULL);
        tx_thread_valid=false;
    }

    /* Auto RX may be listening without capturing.  Stop it before keying so
     * our own transmission cannot trigger the receiver state machine. */
    pthread_mutex_lock(&live_mutex);
    live_enabled=false;
    live_capturing=false;
    free(live_samples); live_samples=NULL; live_written=live_target=0;
    pthread_mutex_unlock(&live_mutex);
    set_receiver_ui_stopped();

    tx_abort=false;
    tx_started=false;
    tx_key_time_valid=false;
    tx_progress=0;
    tx_result=0;
    tx_running=true;
    set_status("Preparing Scottie S1 test TX...");
    if (progress_bar) {
        lv_bar_set_value(progress_bar,0,LV_ANIM_OFF);
        lv_obj_clear_flag(progress_bar,LV_OBJ_FLAG_HIDDEN);
    }

    int rc=pthread_create(&tx_thread,NULL,tx_thread_cb,NULL);
    if (rc==0) {
        tx_thread_valid=true;
    } else {
        fprintf(stderr,"SSTV TX: pthread_create failed: %d\n",rc);
        tx_running=false;
        tx_result=-31;
        set_status("Scottie S1 test TX: thread creation failed.");
        if (progress_bar) lv_obj_add_flag(progress_bar,LV_OBJ_FLAG_HIDDEN);
    }
}

static void test_cb(button_data_t *data)
{
    (void)data;

    if (!sstv_canvas || test_running || cont_capturing || cont_decode_pending || cont_decoding)
        return;

    if (live_enabled || live_capturing) {
        pthread_mutex_lock(&live_mutex);
        live_enabled=false; live_capturing=false;
        free(live_samples); live_samples=NULL; live_written=live_target=0;
        pthread_mutex_unlock(&live_mutex);
        fprintf(stderr,"SSTV LIVE: paused by WAV Test\n");
    }

    if (test_thread_valid) {
        pthread_join(test_thread, NULL);
        test_thread_valid = false;
    }

    lv_canvas_fill_bg(sstv_canvas, lv_color_white(), LV_OPA_COVER);
    set_status("Decoding Test WAV");
    if (progress_bar) {
        lv_bar_set_value(progress_bar,0,LV_ANIM_OFF);
        lv_obj_clear_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
    }
    shown_rows = 0;
    test_rows = 0;
    test_ready = false;
    test_running = true;

    int rc = pthread_create(&test_thread, NULL, test_thread_cb, NULL);
    if (rc != 0) {
        fprintf(stderr, "SSTV TEST: pthread_create failed: %d\n", rc);
        test_running = false;
        return;
    }
    test_thread_valid = true;
}

static void page_1_cb(button_data_t *data)
{
    (void)data; buttons_load_page(&btn_page_1);
}

static void page_2_cb(button_data_t *data)
{
    (void)data;
    buttons_load_page(&btn_page_2);
    sstv_browser_update_buttons();
}

static int sstv_browser_name_compare(const void *a, const void *b)
{
    const char *const *sa = a;
    const char *const *sb = b;
    return strcmp(*sb, *sa);   /* newest timestamped filename first */
}

static void sstv_browser_free_files(void)
{
    for (unsigned int i = 0; i < sstv_browser_file_count; ++i) {
        free(sstv_browser_files[i]);
        sstv_browser_files[i] = NULL;
    }
    sstv_browser_file_count = 0;
}

static void sstv_browser_refresh(void)
{
    sstv_browser_free_files();

    DIR *dir = opendir(SSTV_SAVE_DIR);
    if (!dir)
        return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL &&
           sstv_browser_file_count < SSTV_BROWSER_MAX_FILES) {
        const char *name = entry->d_name;
        size_t len = strlen(name);

        if (len < 5 || strcmp(name + len - 4, ".png") != 0)
            continue;
        if (strncmp(name, "sstv-", 5) != 0)
            continue;

        char *copy = strdup(name);
        if (copy)
            sstv_browser_files[sstv_browser_file_count++] = copy;
    }
    closedir(dir);

    qsort(sstv_browser_files, sstv_browser_file_count,
          sizeof(sstv_browser_files[0]), sstv_browser_name_compare);
}

static void sstv_browser_update_list(void)
{
    if (!sstv_browser_table)
        return;

    if (sstv_browser_file_count == 0) {
        lv_table_set_row_cnt(sstv_browser_table, 1);
        lv_table_set_cell_value(sstv_browser_table, 0, 0, "No saved pictures");
    } else {
        lv_table_set_row_cnt(sstv_browser_table, sstv_browser_file_count);
        for (unsigned int i = 0; i < sstv_browser_file_count; ++i)
            lv_table_set_cell_value(sstv_browser_table, i, 0, sstv_browser_files[i]);
    }
    lv_obj_invalidate(sstv_browser_table);
}

static void sstv_browser_update_buttons(void)
{
    if (sstv_browser_preview) {
        btn_delete_picture.label = "Done";
        buttons_disabled(&btn_delete_picture, false);
    } else {
        btn_delete_picture.label = "Delete\npicture";
        buttons_disabled(&btn_delete_picture,
                         !sstv_browser_open || sstv_browser_file_count == 0);
    }

    if (sstv_browser_open)
        btn_browser.label = "Close\nBrowser";
    else
        btn_browser.label = "SSTV\nBrowser";

    if (btn_browser.disp_btn)
        buttons_refresh(&btn_browser);
    if (btn_delete_picture.disp_btn)
        buttons_refresh(&btn_delete_picture);
}

static void sstv_browser_free_image(void)
{
    free(sstv_browser_image);
    sstv_browser_image = NULL;
    sstv_browser_image_width = 0;
    sstv_browser_image_height = 0;
    sstv_browser_preview = false;
}

static void sstv_browser_render_image(void)
{
    if (!sstv_browser_buffer || !sstv_browser_image ||
        !sstv_browser_image_width || !sstv_browser_image_height)
        return;

    const unsigned int width = sstv_browser_image_width;
    const unsigned int height = sstv_browser_image_height;
    unsigned int out_w = SSTV_BROWSER_W;
    unsigned int out_h = (unsigned int)(((uint64_t)height * SSTV_BROWSER_W) / width);

    if (out_h > SSTV_BROWSER_H) {
        out_h = SSTV_BROWSER_H;
        out_w = (unsigned int)(((uint64_t)width * SSTV_BROWSER_H) / height);
    }
    if (out_w < 1) out_w = 1;
    if (out_h < 1) out_h = 1;

    const unsigned int x0 = (SSTV_BROWSER_W - out_w) / 2;
    const unsigned int y0 = (SSTV_BROWSER_H - out_h) / 2;
    const lv_color_t black = lv_color_make(0, 0, 0);

    for (unsigned int i = 0; i < SSTV_BROWSER_W * SSTV_BROWSER_H; ++i)
        sstv_browser_buffer[i] = black;

    for (unsigned int oy = 0; oy < out_h; ++oy) {
        unsigned int sy = (unsigned int)(((uint64_t)oy * height) / out_h);
        if (sy >= height) sy = height - 1;

        for (unsigned int ox = 0; ox < out_w; ++ox) {
            unsigned int sx = (unsigned int)(((uint64_t)ox * width) / out_w);
            if (sx >= width) sx = width - 1;

            const uint8_t *p = &sstv_browser_image[
                ((size_t)sy * width + sx) * 3u
            ];
            sstv_browser_buffer[(y0 + oy) * SSTV_BROWSER_W + x0 + ox] =
                lv_color_make(p[0], p[1], p[2]);
        }
    }

    if (sstv_browser_canvas)
        lv_obj_invalidate(sstv_browser_canvas);
}

static bool sstv_browser_open_png(const char *filename)
{
    if (!filename || !*filename)
        return false;

    char path[256];
    snprintf(path, sizeof(path), "%s/%s", SSTV_SAVE_DIR, filename);

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "SSTV BROWSER: unable to open %s: %s\n",
                path, strerror(errno));
        return false;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                                 NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(fp);
        return false;
    }
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        return false;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        fprintf(stderr, "SSTV BROWSER: libpng read error for %s\n", path);
        return false;
    }

    png_init_io(png_ptr, fp);
    png_read_info(png_ptr, info_ptr);

    png_uint_32 width = png_get_image_width(png_ptr, info_ptr);
    png_uint_32 height = png_get_image_height(png_ptr, info_ptr);
    int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    int color_type = png_get_color_type(png_ptr, info_ptr);

    if (!width || !height || width > 10000 || height > 10000) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return false;
    }

    if (bit_depth == 16) png_set_strip_16(png_ptr);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);

    color_type = png_get_color_type(png_ptr, info_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);

    png_read_update_info(png_ptr, info_ptr);
    png_size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    int channels = png_get_channels(png_ptr, info_ptr);
    if (channels < 3 || !rowbytes) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return false;
    }

    uint8_t *image = malloc((size_t)width * (size_t)height * 3u);
    uint8_t *row = malloc(rowbytes);
    if (!image || !row) {
        free(image);
        free(row);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return false;
    }

    for (png_uint_32 y = 0; y < height; ++y) {
        png_read_row(png_ptr, row, NULL);
        for (png_uint_32 x = 0; x < width; ++x) {
            const uint8_t *sp = row + (size_t)x * (size_t)channels;
            uint8_t *dp = image + ((size_t)y * width + x) * 3u;
            dp[0] = sp[0];
            dp[1] = sp[1];
            dp[2] = sp[2];
        }
    }

    png_read_end(png_ptr, NULL);
    free(row);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);

    sstv_browser_free_image();
    sstv_browser_image = image;
    sstv_browser_image_width = (unsigned int)width;
    sstv_browser_image_height = (unsigned int)height;

    if (!sstv_browser_buffer) {
        sstv_browser_buffer = malloc(
            (size_t)SSTV_BROWSER_W * SSTV_BROWSER_H * sizeof(lv_color_t)
        );
        if (!sstv_browser_buffer) {
            sstv_browser_free_image();
            return false;
        }
    }

    if (!sstv_browser_canvas) {
        sstv_browser_canvas = lv_canvas_create(dialog.obj);
        lv_canvas_set_buffer(sstv_browser_canvas, sstv_browser_buffer,
                             SSTV_BROWSER_W, SSTV_BROWSER_H,
                             LV_IMG_CF_TRUE_COLOR);
        lv_obj_set_size(sstv_browser_canvas, SSTV_BROWSER_W, SSTV_BROWSER_H);
        lv_obj_align(sstv_browser_canvas, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_event_cb(sstv_browser_canvas,
                            sstv_browser_preview_pressed_cb,
                            LV_EVENT_PRESSED, NULL);
    }

    sstv_browser_render_image();

    if (sstv_browser_table) {
        lv_group_remove_obj(sstv_browser_table);
        lv_obj_add_flag(sstv_browser_table, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_clear_flag(sstv_browser_canvas, LV_OBJ_FLAG_HIDDEN);
    lv_group_add_obj(keyboard_group, sstv_browser_canvas);
    lv_group_focus_obj(sstv_browser_canvas);
    lv_group_set_editing(keyboard_group, true);

    sstv_browser_preview = true;
    sstv_browser_update_buttons();

    fprintf(stderr, "SSTV BROWSER: opened %s (%u x %u)\n",
            path, sstv_browser_image_width, sstv_browser_image_height);
    return true;
}

static void sstv_browser_pressed_cb(lv_event_t *e)
{
    (void)e;
    if (!sstv_browser_table || sstv_browser_file_count == 0)
        return;

    int16_t row = LV_TABLE_CELL_NONE;
    int16_t col = LV_TABLE_CELL_NONE;
    lv_table_get_selected_cell(sstv_browser_table, &row, &col);

    if (row == LV_TABLE_CELL_NONE || row < 0 ||
        (unsigned int)row >= sstv_browser_file_count)
        return;

    sstv_browser_open_png(sstv_browser_files[row]);
}

static void sstv_browser_return_to_list(void)
{
    if (!sstv_browser_open)
        return;

    if (sstv_browser_canvas) {
        lv_group_remove_obj(sstv_browser_canvas);
        lv_obj_add_flag(sstv_browser_canvas, LV_OBJ_FLAG_HIDDEN);
    }

    sstv_browser_free_image();
    sstv_browser_refresh();

    if (sstv_browser_table) {
        lv_obj_clear_flag(sstv_browser_table, LV_OBJ_FLAG_HIDDEN);
        sstv_browser_update_list();
        lv_group_add_obj(keyboard_group, sstv_browser_table);
        lv_group_focus_obj(sstv_browser_table);
        lv_group_set_editing(keyboard_group, true);
        lv_obj_invalidate(sstv_browser_table);
    }
    sstv_browser_update_buttons();
}

static void sstv_browser_preview_pressed_cb(lv_event_t *e)
{
    (void)e;
    /* MFK press on the preview returns to the file list. */
    sstv_browser_return_to_list();
}

static void sstv_browser_delete_selected(void)
{
    if (!sstv_browser_open || sstv_browser_preview ||
        !sstv_browser_table || sstv_browser_file_count == 0)
        return;

    int16_t row = LV_TABLE_CELL_NONE;
    int16_t col = LV_TABLE_CELL_NONE;
    lv_table_get_selected_cell(sstv_browser_table, &row, &col);

    if (row == LV_TABLE_CELL_NONE || row < 0 ||
        (unsigned int)row >= sstv_browser_file_count)
        return;

    char path[256];
    snprintf(path, sizeof(path), "%s/%s",
             SSTV_SAVE_DIR, sstv_browser_files[row]);

    if (unlink(path) != 0) {
        fprintf(stderr, "SSTV BROWSER: unable to delete %s: %s\n",
                path, strerror(errno));
        return;
    }

    fprintf(stderr, "SSTV BROWSER: deleted %s\n", path);

    /* Re-scan every time: deleting the last picture immediately changes the
     * table to "No saved pictures", exactly like the WEFAX browser. */
    sstv_browser_refresh();
    sstv_browser_update_list();
    sstv_browser_update_buttons();

    lv_group_focus_obj(sstv_browser_table);
    lv_group_set_editing(keyboard_group, true);
}

static void sstv_browser_build(void)
{
    if (sstv_browser_open)
        return;

    sstv_browser_refresh();

    sstv_browser_table = lv_table_create(dialog.obj);
    lv_obj_set_style_border_width(sstv_browser_table, 0, LV_PART_MAIN);

    /* Pixel-for-pixel WEFAX BIG browser geometry. */
    lv_obj_set_size(sstv_browser_table, 700, 270);
    lv_obj_align(sstv_browser_table, LV_ALIGN_TOP_MID, 0, 40);
    lv_table_set_col_cnt(sstv_browser_table, 1);
    lv_table_set_col_width(sstv_browser_table, 0, 690);

    lv_obj_set_style_bg_color(sstv_browser_table, lv_color_hex(0x27313a),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sstv_browser_table, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(sstv_browser_table, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(sstv_browser_table, LV_OPA_TRANSP, LV_PART_ITEMS);
    lv_obj_set_style_text_color(sstv_browser_table, lv_color_white(),
                                LV_PART_ITEMS);
    lv_obj_set_style_text_font(sstv_browser_table, &sony_24, LV_PART_ITEMS);
    lv_obj_set_style_pad_top(sstv_browser_table, 5, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(sstv_browser_table, 5, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(sstv_browser_table, 12, LV_PART_ITEMS);
    lv_obj_set_style_text_color(sstv_browser_table, lv_color_hex(0x808080),
                                LV_PART_ITEMS | LV_STATE_EDITED);
    lv_obj_set_style_bg_opa(sstv_browser_table, LV_OPA_TRANSP,
                            LV_PART_ITEMS | LV_STATE_EDITED);

    sstv_browser_update_list();
    lv_obj_add_event_cb(sstv_browser_table, sstv_browser_pressed_cb,
                        LV_EVENT_PRESSED, NULL);
    lv_group_add_obj(keyboard_group, sstv_browser_table);
    lv_group_focus_obj(sstv_browser_table);
    lv_group_set_editing(keyboard_group, true);

    sstv_browser_open = true;
    sstv_browser_preview = false;
    sstv_browser_update_buttons();
}

static void sstv_browser_close(void)
{
    if (sstv_browser_table) {
        lv_group_remove_obj(sstv_browser_table);
        lv_obj_del(sstv_browser_table);
        sstv_browser_table = NULL;
    }

    if (sstv_browser_canvas) {
        lv_group_remove_obj(sstv_browser_canvas);
        lv_obj_del(sstv_browser_canvas);
        sstv_browser_canvas = NULL;
    }

    free(sstv_browser_buffer);
    sstv_browser_buffer = NULL;
    sstv_browser_free_image();
    sstv_browser_free_files();

    sstv_browser_open = false;
    sstv_browser_preview = false;

    if (dialog.obj)
        lv_obj_invalidate(dialog.obj);

    sstv_browser_update_buttons();
}

static void browser_cb(button_data_t *data)
{
    (void)data;

    if (sstv_browser_open)
        sstv_browser_close();
    else
        sstv_browser_build();
}

static void delete_picture_cb(button_data_t *data)
{
    (void)data;

    if (sstv_browser_preview)
        sstv_browser_return_to_list();
    else
        sstv_browser_delete_selected();
}

static void cont_mode_cb(button_data_t *data)
{
    (void)data;
    if (live_enabled || live_capturing || cont_capturing || cont_decode_pending || test_running)
        return;
    unsigned count = sizeof(cont_modes)/sizeof(cont_modes[0]);
    unsigned idx = 0;
    while (idx < count && cont_modes[idx] != cont_mode) ++idx;
    cont_mode = cont_modes[(idx + 1) % count];
    static char label[48];
    snprintf(label, sizeof(label), "RX Mode\n%s", cont_mode_name(cont_mode));
    btn_cont_mode.label = label;
    if (btn_cont_mode.disp_btn) buttons_refresh(&btn_cont_mode);
}

static void cont_start_cb(button_data_t *data)
{
    (void)data;
    pthread_mutex_lock(&live_mutex);

    if (cont_capturing) {
        cont_capturing = false;
        if (cont_written > 0) {
            cont_decode_mode = cont_mode;
            cont_decode_pending = true;
            fprintf(stderr, "SSTV CONT: STOP requested after %.1f s (%ld samples); decode pending as %s\n",
                    (double)cont_written/SSTV_SAMPLE_RATE, cont_written, cont_mode_name(cont_decode_mode));
        } else {
            free(cont_samples); cont_samples=NULL; cont_capacity=cont_written=0;
        }
        pthread_mutex_unlock(&live_mutex);
        btn_cont_start.label = "Start\nManual RX";
        if (btn_cont_start.disp_btn) buttons_refresh(&btn_cont_start);
        update_rx_controls();
        return;
    }

    if (live_enabled || live_capturing || live_decode_pending || test_running) {
        pthread_mutex_unlock(&live_mutex);
        return;
    }

    cont_capacity = (long)SSTV_CONT_MAX_SECONDS * SSTV_SAMPLE_RATE;
    cont_samples = malloc((size_t)cont_capacity * sizeof(float));
    if (!cont_samples) {
        cont_capacity=0;
        pthread_mutex_unlock(&live_mutex);
        fprintf(stderr, "SSTV CONT: ERROR allocating capture buffer\n");
        return;
    }
    cont_written=0;
    cont_capturing=true;
    pthread_mutex_unlock(&live_mutex);

    if (sstv_canvas) lv_canvas_fill_bg(sstv_canvas, lv_color_white(), LV_OPA_COVER);
    char status[192];
    snprintf(status, sizeof(status),
             "SSTV Manual decoder is running\n\nDecoding %s signal\n\nPress Stop Manual RX to render received picture",
             cont_mode_name(cont_mode));
    set_status(status);
    if (progress_bar) lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
    btn_cont_start.label = "Stop\nManual RX";
    if (btn_cont_start.disp_btn) buttons_refresh(&btn_cont_start);
    update_rx_controls();
    fprintf(stderr,"SSTV CONT: START - manual %s capture at %d Hz, max %d s\n",
            cont_mode_name(cont_mode),SSTV_SAMPLE_RATE,SSTV_CONT_MAX_SECONDS);
}

static void start_cb(button_data_t *data)
{
    (void)data;
    pthread_mutex_lock(&live_mutex);

    if (cont_capturing || cont_decode_pending || cont_decoding) {
        pthread_mutex_unlock(&live_mutex);
        return;
    }

    if (live_enabled || live_capturing) {
        live_enabled=false; live_capturing=false; live_decode_pending=false;
        live_ring_pos=live_ring_count=0; live_since_check=0;
        live_wait_samples=live_wait_log_mark=0;
        free(live_samples); live_samples=NULL; live_written=live_target=0;
        pthread_mutex_unlock(&live_mutex);
        reset_ui_to_standby();
        fprintf(stderr,"SSTV LIVE: STOP - receiver returned to stand-by\n");
        return;
    }

    if (!test_running) {
        live_enabled=true; live_capturing=false; live_decode_pending=false;
        live_ring_pos=live_ring_count=0; live_since_check=0;
        live_wait_samples=live_wait_log_mark=0;
        free(live_samples); live_samples=NULL; live_written=live_target=0;
        fprintf(stderr,"SSTV LIVE: START - listening to RX audio at %d Hz\n", SSTV_SAMPLE_RATE);
        fprintf(stderr,"SSTV LIVE: waiting for 1900/1200/1900 VIS leader...\n");
    }
    pthread_mutex_unlock(&live_mutex);
    set_receiver_ui_listening();
}

static void audio_cb(unsigned int n, float *samples)
{
    if ((!live_enabled && !cont_capturing) || !samples || n==0 || test_running) return;
    pthread_mutex_lock(&live_mutex);

    if (cont_capturing) {
        long room=cont_capacity-cont_written;
        long copy=(long)n < room ? (long)n : room;
        if (copy>0) {
            memcpy(cont_samples+cont_written,samples,(size_t)copy*sizeof(float));
            cont_written += copy;
        }
        if (cont_written >= cont_capacity) {
            cont_capturing=false;
            cont_decode_mode=cont_mode;
            cont_decode_pending=true;
            fprintf(stderr,"SSTV CONT: maximum %d s capture reached; stopping and decoding %s\n",
                    SSTV_CONT_MAX_SECONDS,cont_mode_name(cont_decode_mode));
        }
        pthread_mutex_unlock(&live_mutex);
        return;
    }

    if (!live_enabled) { pthread_mutex_unlock(&live_mutex); return; }

    if (live_capturing) {
        long room=live_target-live_written;
        long copy=(long)n < room ? (long)n : room;
        if (copy>0) { memcpy(live_samples+live_written,samples,(size_t)copy*sizeof(float)); live_written+=copy; }
        static int last_sec=-1;
        int sec=(int)(live_written/SSTV_SAMPLE_RATE);
        if (sec/10 != last_sec/10) {
            last_sec=sec;
            fprintf(stderr,"SSTV LIVE: receiving %s into RAM... %d / %.0f s\n",live_info.name,sec,(double)live_target/SSTV_SAMPLE_RATE);
        }
        if (live_written >= live_target) {
            live_capturing=false; live_decode_pending=true; live_enabled=false;
            fprintf(stderr,"SSTV LIVE: expected %s duration reached; RAM capture complete (%ld samples)\n",
                    live_info.name,live_written);
        }
        pthread_mutex_unlock(&live_mutex); return;
    }

    for (unsigned int i=0;i<n;++i) {
        live_ring[live_ring_pos]=samples[i];
        live_ring_pos=(live_ring_pos+1)%SSTV_PREBUF_SAMPLES;
        if (live_ring_count<SSTV_PREBUF_SAMPLES) ++live_ring_count;
    }
    live_since_check += n;
    live_wait_samples += n;
    if (live_wait_samples - live_wait_log_mark >= (uint64_t)SSTV_SAMPLE_RATE * 60u) {
        live_wait_log_mark = live_wait_samples;
        fprintf(stderr,"SSTV LIVE: still listening - %llu s of RX audio checked, no valid VIS yet\n",
                (unsigned long long)(live_wait_samples/SSTV_SAMPLE_RATE));
    }
    if (live_ring_count >= SSTV_VIS_WINDOW_SAMPLES && live_since_check >= SSTV_VIS_CHECK_STEP) {
        live_since_check=0;
        size_t start=(live_ring_pos + SSTV_PREBUF_SAMPLES - SSTV_VIS_WINDOW_SAMPLES)%SSTV_PREBUF_SAMPLES;
        for (size_t i=0;i<SSTV_VIS_WINDOW_SAMPLES;++i) live_vis_window[i]=live_ring[(start+i)%SSTV_PREBUF_SAMPLES];
        if (live_find_vis(live_vis_window,SSTV_VIS_WINDOW_SAMPLES,&live_info)) {
            live_target=(long)((live_mode_seconds(live_info.mode)+2.0f)*SSTV_SAMPLE_RATE);
            live_samples=malloc((size_t)live_target*sizeof(float));
            if (!live_samples) {
                fprintf(stderr,"SSTV LIVE: ERROR allocating %.1f MB capture buffer\n",
                        (double)live_target*sizeof(float)/(1024.0*1024.0));
                live_enabled=false;
            } else {
                size_t keep=live_ring_count;
                if ((long)keep>live_target) keep=(size_t)live_target;
                size_t oldest=(live_ring_pos + SSTV_PREBUF_SAMPLES - live_ring_count)%SSTV_PREBUF_SAMPLES;
                for (size_t i=0;i<keep;++i) live_samples[i]=live_ring[(oldest+i)%SSTV_PREBUF_SAMPLES];
                live_written=(long)keep;
                live_capturing=true;
                /* A valid VIS starts a new picture.  Do not touch LVGL from
                 * the audio callback; ask the UI timer to clear the canvas. */
                live_canvas_clear_pending = true;
                fprintf(stderr,"SSTV LIVE: capturing %s directly to RAM; target %.1f s, buffer %.1f MB, prebuffer=%ld samples\n",
                        live_info.name,(double)live_target/SSTV_SAMPLE_RATE,
                        (double)live_target*sizeof(float)/(1024.0*1024.0),live_written);
            }
        }
    }
    pthread_mutex_unlock(&live_mutex);
}

static void close_cb(button_data_t *data)
{
    (void)data;
    dialog_destruct();
}
