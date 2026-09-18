/****************************************************************************
 * main.c — Campus Agent 主入口
 *
 * UI 模式：nsh> campus_agent （LVGL 触摸界面 + 后台任务）
 * CLI 模式：nsh> campus_agent <command>
 *
 * Contest 2026 Team 307
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <nuttx/timers/rtc.h>

#include <lvgl/lvgl.h>

#include "course_manager.h"
#include "reminder_engine.h"
#include "sedentary_monitor.h"
#include "ui_briefing.h"
#include "vibrate.h"
#include "button_handler.h"
#include "dialogue.h"

/****************************************************************************
 * 屏幕参数
 ****************************************************************************/

#define SW  390
#define SH  450

/****************************************************************************
 * 配色方案（柔和深色）
 ****************************************************************************/

#define CLR_BG_1        lv_color_hex(0x101018)   /* 最深背景 */
#define CLR_BG_2        lv_color_hex(0x181820)   /* 卡片背景 */
#define CLR_BG_3        lv_color_hex(0x202028)   /* 列表项背景 */
#define CLR_BG_TOP      lv_color_hex(0x14141C)   /* 顶栏背景 */
#define CLR_BG_BOTTOM   lv_color_hex(0x14141C)   /* 底栏背景 */
#define CLR_TEXT_W       lv_color_hex(0xE8E8EC)   /* 主文字 */
#define CLR_TEXT_G       lv_color_hex(0x888898)   /* 次文字 */
#define CLR_TEXT_T       lv_color_hex(0x585868)   /* 辅助文字 */
#define CLR_DOT_GREEN    lv_color_hex(0x3AE088)   /* 待上 */
#define CLR_DOT_RED      lv_color_hex(0xE04848)   /* 进行中 */
#define CLR_DOT_GRAY     lv_color_hex(0x484858)   /* 已结束 */
#define CLR_BTN_BLUE     lv_color_hex(0x3868D0)   /* 提醒按钮 */
#define CLR_BTN_ORANGE   lv_color_hex(0xD08838)   /* 久坐按钮 */
#define CLR_BTN_GREEN    lv_color_hex(0x38B068)   /* 刷新按钮 */
#define CLR_BTN_PRESS    lv_color_hex(0x282838)   /* 按钮按下 */
#define CLR_DIVIDER      lv_color_hex(0x282838)   /* 分割线 */

/****************************************************************************
 * 字体（使用内置 Montserrat，不依赖 FreeType）
 ****************************************************************************/

#define F_TIME   &lv_font_montserrat_28   /* 时钟大字 */
#define F_DATE   &lv_font_montserrat_16   /* 星期 */
#define F_NAME   &lv_font_montserrat_16   /* 课程名 */
#define F_INFO   &lv_font_montserrat_14   /* 课程信息 */
#define F_BTN    &lv_font_montserrat_14   /* 按钮文字 */
#define F_STATUS &lv_font_montserrat_14   /* 状态文字 */

/****************************************************************************
 * 全局 UI 对象
 ****************************************************************************/

static lv_obj_t *g_scr           = NULL;
static lv_obj_t *g_time_label    = NULL;
static lv_obj_t *g_date_label    = NULL;
static lv_obj_t *g_course_list   = NULL;
static lv_obj_t *g_btn_remind    = NULL;
static lv_obj_t *g_btn_sed       = NULL;
static lv_obj_t *g_btn_refresh   = NULL;
static lv_obj_t *g_status_lbl    = NULL;
static lv_obj_t *g_sed_lbl       = NULL;

/****************************************************************************
 * RTC
 ****************************************************************************/

static int read_rtc(int *wd, int *h, int *m, int *s)
{
  int fd = open("/dev/rtc0", O_RDONLY);
  if (fd < 0) return -1;
  struct rtc_time rt;
  int ret = ioctl(fd, RTC_RD_TIME, (unsigned long)&rt);
  close(fd);
  if (ret < 0) return -1;
  if (wd) *wd = rt.tm_wday == 0 ? 7 : rt.tm_wday;
  if (h)  *h  = rt.tm_hour;
  if (m)  *m  = rt.tm_min;
  if (s)  *s  = rt.tm_sec;
  return 0;
}

/****************************************************************************
 * 刷新课程列表
 ****************************************************************************/

static void refresh_course_list(void)
{
  if (!g_course_list) return;

  /* 清除旧内容 */

  lv_obj_clean(g_course_list);

  int wd, h, m;
  if (read_rtc(&wd, &h, &m, NULL) < 0) return;

  course_query_t states[10];
  int count = course_get_today_status(wd, h, m, states, 10);

  if (count == 0)
    {
      lv_obj_t *lbl = lv_label_create(g_course_list);
      lv_label_set_text(lbl, "No classes today");
      lv_obj_set_style_text_font(lbl, F_NAME, 0);
      lv_obj_set_style_text_color(lbl, CLR_DOT_GREEN, 0);
      lv_obj_center(lbl);
      return;
    }

  for (int i = 0; i < count; i++)
    {
      const course_t *c = states[i].course;

      /* ===== 列表项容器 ===== */

      lv_obj_t *item = lv_obj_create(g_course_list);
      lv_obj_set_size(item, SW - 24, 60);
      lv_obj_set_style_bg_color(item, CLR_BG_3, 0);
      lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
      lv_obj_set_style_radius(item, 10, 0);
      lv_obj_set_style_border_width(item, 0, 0);
      lv_obj_set_style_pad_all(item, 0, 0);
      lv_obj_remove_flag(item, LV_OBJ_FLAG_SCROLLABLE);

      /* ===== 左侧：圆形状态灯 ===== */

      lv_obj_t *dot = lv_obj_create(item);
      lv_obj_set_size(dot, 10, 10);
      lv_obj_set_style_radius(dot, 5, 0);
      lv_obj_set_style_border_width(dot, 0, 0);
      lv_obj_align(dot, LV_ALIGN_LEFT_MID, 14, 0);

      if (states[i].is_happening)
        {
          lv_obj_set_style_bg_color(dot, CLR_DOT_RED, 0);
        }
      else if (states[i].minutes_left < 0)
        {
          lv_obj_set_style_bg_color(dot, CLR_DOT_GRAY, 0);
        }
      else
        {
          lv_obj_set_style_bg_color(dot, CLR_DOT_GREEN, 0);
        }

      lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);

      /* ===== 右侧：文本区域 ===== */

      /* 课程名（加粗效果用稍大字号） */

      lv_obj_t *name = lv_label_create(item);
      lv_label_set_text(name, c->name);
      lv_obj_set_style_text_font(name, F_NAME, 0);
      lv_obj_set_style_text_color(name, CLR_TEXT_W, 0);
      lv_obj_align(name, LV_ALIGN_LEFT_MID, 32, -10);

      /* 时间 + 教室（小字） */

      char info[64];
      snprintf(info, sizeof(info), "%02d:%02d - %02d:%02d   %s",
               c->start_hour, c->start_min,
               c->end_hour, c->end_min,
               c->location);

      lv_obj_t *info_lbl = lv_label_create(item);
      lv_label_set_text(info_lbl, info);
      lv_obj_set_style_text_font(info_lbl, F_INFO, 0);
      lv_obj_set_style_text_color(info_lbl, CLR_TEXT_G, 0);
      lv_obj_align(info_lbl, LV_ALIGN_LEFT_MID, 32, 10);

      /* ===== 分割线（最后一项不加） ===== */

      if (i < count - 1)
        {
          lv_obj_t *sep = lv_obj_create(g_course_list);
          lv_obj_set_size(sep, SW - 48, 1);
          lv_obj_set_style_bg_color(sep, CLR_DIVIDER, 0);
          lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
          lv_obj_set_style_border_width(sep, 0, 0);
        }
    }
}

/****************************************************************************
 * 时钟更新
 ****************************************************************************/

static void clock_timer_cb(lv_timer_t *timer)
{
  int wd, h, m, s;
  if (read_rtc(&wd, &h, &m, &s) < 0) return;

  const char *wn[] = {"","Mon","Tue","Wed","Thu","Fri","Sat","Sun"};

  char t[16];
  snprintf(t, sizeof(t), "%02d:%02d:%02d", h, m, s);
  lv_label_set_text(g_time_label, t);

  char d[16];
  snprintf(d, sizeof(d), "%s", wn[wd]);
  lv_label_set_text(g_date_label, d);
}

/****************************************************************************
 * 状态更新
 ****************************************************************************/

static void status_timer_cb(lv_timer_t *timer)
{
  /* 只更新状态文字，不刷新整个列表（太重） */

  if (g_status_lbl)
    {
      char buf[64];
      snprintf(buf, sizeof(buf), "Remind:%s  Sed:%s",
               reminder_engine_is_running() ? "ON" : "off",
               sedentary_monitor_is_running() ? "ON" : "off");
      lv_label_set_text(g_status_lbl, buf);
    }

  if (g_sed_lbl)
    {
      const sedentary_state_t *st = sedentary_get_state();
      char buf[32];
      snprintf(buf, sizeof(buf), "Sit:%dmin", st->sitting_minutes);
      lv_label_set_text(g_sed_lbl, buf);
    }
}

/****************************************************************************
 * 按钮回调（异步，不阻塞 LVGL）
 ****************************************************************************/

static void async_start_remind(void *arg)
{
  (void)arg;
  reminder_engine_start();
}

static void async_stop_remind(void *arg)
{
  (void)arg;
  reminder_engine_stop();
}

static void async_start_sed(void *arg)
{
  (void)arg;
  sedentary_monitor_start();
}

static void async_stop_sed(void *arg)
{
  (void)arg;
  sedentary_monitor_stop();
}

/* 防抖：记录上次点击时间 */

static uint32_t g_last_remind_click = 0;
static uint32_t g_last_sed_click = 0;
#define DEBOUNCE_MS  500

static void btn_remind_cb(lv_event_t *e)
{
  uint32_t now = lv_tick_get();
  if (now - g_last_remind_click < DEBOUNCE_MS) return;
  g_last_remind_click = now;

  if (reminder_engine_is_running())
    {
      reminder_engine_stop();
      lv_obj_set_style_bg_color(g_btn_remind, CLR_BTN_BLUE, 0);
      lv_obj_t *lbl = lv_obj_get_child(g_btn_remind, 0);
      lv_label_set_text(lbl, "Remind");
    }
  else
    {
      reminder_engine_start();
      lv_obj_set_style_bg_color(g_btn_remind, CLR_DOT_RED, 0);
      lv_obj_t *lbl = lv_obj_get_child(g_btn_remind, 0);
      lv_label_set_text(lbl, "Remind ON");
    }
}

static void btn_sed_cb(lv_event_t *e)
{
  uint32_t now = lv_tick_get();
  if (now - g_last_sed_click < DEBOUNCE_MS) return;
  g_last_sed_click = now;

  if (sedentary_monitor_is_running())
    {
      sedentary_monitor_stop();
      lv_obj_set_style_bg_color(g_btn_sed, CLR_BTN_ORANGE, 0);
      lv_obj_t *lbl = lv_obj_get_child(g_btn_sed, 0);
      lv_label_set_text(lbl, "Sedentary");
    }
  else
    {
      sedentary_monitor_start();
      lv_obj_set_style_bg_color(g_btn_sed, CLR_DOT_RED, 0);
      lv_obj_t *lbl = lv_obj_get_child(g_btn_sed, 0);
      lv_label_set_text(lbl, "Sed ON");
    }
}

static void btn_refresh_cb(lv_event_t *e)
{
  refresh_course_list();
}

/****************************************************************************
 * 物理按键回调
 ****************************************************************************/

/* 异步 UI 更新（避免 LVGL 线程冲突） */

static int g_pending_action = 0;  /* 0=无, 1=切换提醒, 2=切换久坐 */

static void async_ui_update(void *arg)
{
  int action = (int)arg;

  if (action == 1)  /* 切换提醒 */
    {
      if (reminder_engine_is_running())
        {
          reminder_engine_stop();
          if (g_btn_remind)
            {
              lv_obj_set_style_bg_color(g_btn_remind, CLR_BTN_BLUE, 0);
              lv_obj_t *lbl = lv_obj_get_child(g_btn_remind, 0);
              lv_label_set_text(lbl, "Remind");
            }
        }
      else
        {
          reminder_engine_start();
          if (g_btn_remind)
            {
              lv_obj_set_style_bg_color(g_btn_remind, CLR_DOT_RED, 0);
              lv_obj_t *lbl = lv_obj_get_child(g_btn_remind, 0);
              lv_label_set_text(lbl, "Remind ON");
            }
        }
    }
  else if (action == 2)  /* 切换久坐 */
    {
      if (sedentary_monitor_is_running())
        {
          sedentary_monitor_stop();
          if (g_btn_sed)
            {
              lv_obj_set_style_bg_color(g_btn_sed, CLR_BTN_ORANGE, 0);
              lv_obj_t *lbl = lv_obj_get_child(g_btn_sed, 0);
              lv_label_set_text(lbl, "Sedentary");
            }
        }
      else
        {
          sedentary_monitor_start();
          if (g_btn_sed)
            {
              lv_obj_set_style_bg_color(g_btn_sed, CLR_DOT_RED, 0);
              lv_obj_t *lbl = lv_obj_get_child(g_btn_sed, 0);
              lv_label_set_text(lbl, "Sed ON");
            }
        }
    }
}

static void button_event_cb(int key, int is_long_press)
{
  if (is_long_press)
    {
      printf("[btn] KEY2 long: toggle sedentary\n");
      lv_async_call(async_ui_update, (void *)2);
    }
  else
    {
      printf("[btn] KEY2 short: toggle remind\n");
      lv_async_call(async_ui_update, (void *)1);
    }
}

/****************************************************************************
 * 创建按钮（带按下反馈）
 ****************************************************************************/

static lv_obj_t *create_action_btn(lv_obj_t *parent, const char *text,
                                    lv_color_t color, lv_event_cb_t cb)
{
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_size(btn, 110, 38);
  lv_obj_set_style_bg_color(btn, color, 0);
  lv_obj_set_style_radius(btn, 10, 0);
  lv_obj_set_style_border_width(btn, 0, 0);

  /* 按下效果（简化，不用 transform） */

  lv_obj_set_style_bg_color(btn, CLR_BTN_PRESS, LV_STATE_PRESSED);

  /* 不加阴影（渲染慢） */

  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, F_BTN, 0);
  lv_obj_set_style_text_color(lbl, CLR_TEXT_W, 0);
  lv_obj_center(lbl);

  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

  return btn;
}

/****************************************************************************
 * 创建主界面
 ****************************************************************************/

static void create_main_ui(void)
{
  g_scr = lv_screen_active();
  lv_obj_set_style_bg_color(g_scr, CLR_BG_1, 0);
  lv_obj_set_style_bg_opa(g_scr, LV_OPA_COVER, 0);

  /* ===== 顶部状态栏 ===== */

  lv_obj_t *top = lv_obj_create(g_scr);
  lv_obj_set_size(top, SW, 72);
  lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(top, CLR_BG_TOP, 0);
  lv_obj_set_style_bg_opa(top, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(top, 0, 0);
  lv_obj_set_style_radius(top, 0, 0);
  lv_obj_remove_flag(top, LV_OBJ_FLAG_SCROLLABLE);

  /* 时间（大号粗体） */

  g_time_label = lv_label_create(top);
  lv_label_set_text(g_time_label, "00:00:00");
  lv_obj_set_style_text_font(g_time_label, F_TIME, 0);
  lv_obj_set_style_text_color(g_time_label, CLR_TEXT_W, 0);
  lv_obj_set_style_text_letter_space(g_time_label, 2, 0);
  lv_obj_align(g_time_label, LV_ALIGN_LEFT_MID, 16, -6);

  /* 星期（小字，时间右下方） */

  g_date_label = lv_label_create(top);
  lv_label_set_text(g_date_label, "---");
  lv_obj_set_style_text_font(g_date_label, F_DATE, 0);
  lv_obj_set_style_text_color(g_date_label, CLR_DOT_GREEN, 0);
  lv_obj_align_to(g_date_label, g_time_label, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 2);

  /* 状态信息（右上角） */

  g_status_lbl = lv_label_create(top);
  lv_label_set_text(g_status_lbl, "R:off S:off");
  lv_obj_set_style_text_font(g_status_lbl, F_STATUS, 0);
  lv_obj_set_style_text_color(g_status_lbl, CLR_TEXT_T, 0);
  lv_obj_align(g_status_lbl, LV_ALIGN_TOP_RIGHT, -12, 10);

  g_sed_lbl = lv_label_create(top);
  lv_label_set_text(g_sed_lbl, "Sit:0");
  lv_obj_set_style_text_font(g_sed_lbl, F_STATUS, 0);
  lv_obj_set_style_text_color(g_sed_lbl, CLR_TEXT_T, 0);
  lv_obj_align(g_sed_lbl, LV_ALIGN_TOP_RIGHT, -12, 26);

  /* ===== 课程列表区域 ===== */

  g_course_list = lv_obj_create(g_scr);
  lv_obj_set_size(g_course_list, SW - 8, SH - 72 - 58);
  lv_obj_align(g_course_list, LV_ALIGN_TOP_MID, 0, 74);
  lv_obj_set_style_bg_opa(g_course_list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_course_list, 0, 0);
  lv_obj_set_style_pad_all(g_course_list, 4, 0);
  lv_obj_set_style_pad_row(g_course_list, 4, 0);
  lv_obj_set_flex_flow(g_course_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(g_course_list, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(g_course_list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(g_course_list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(g_course_list, LV_SCROLLBAR_MODE_AUTO);

  /* ===== 底部操作栏 ===== */

  lv_obj_t *bottom = lv_obj_create(g_scr);
  lv_obj_set_size(bottom, SW, 56);
  lv_obj_align(bottom, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(bottom, CLR_BG_BOTTOM, 0);
  lv_obj_set_style_bg_opa(bottom, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(bottom, 0, 0);
  lv_obj_set_style_radius(bottom, 0, 0);
  lv_obj_remove_flag(bottom, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(bottom, 0, 0);

  /* 按钮容器 */

  lv_obj_t *btn_row = lv_obj_create(bottom);
  lv_obj_set_size(btn_row, SW - 16, 42);
  lv_obj_align(btn_row, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(btn_row, 0, 0);
  lv_obj_set_style_pad_all(btn_row, 0, 0);
  lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  g_btn_remind  = create_action_btn(btn_row, "Remind",
                                     CLR_BTN_BLUE, btn_remind_cb);
  g_btn_sed     = create_action_btn(btn_row, "Sedentary",
                                     CLR_BTN_ORANGE, btn_sed_cb);
  g_btn_refresh = create_action_btn(btn_row, "Refresh",
                                     CLR_BTN_GREEN, btn_refresh_cb);

  /* 初始刷新 */

  refresh_course_list();
}

/****************************************************************************
 * UI 模式
 ****************************************************************************/

static int run_ui_mode(void)
{
  printf("=== Campus Agent UI ===\n");

  if (lv_is_initialized())
    {
      printf("ERROR: LVGL already initialized.\n");
      return -1;
    }

  lv_init();
  lv_nuttx_dsc_t dsc;
  lv_nuttx_result_t result;
  lv_nuttx_dsc_init(&dsc);
  dsc.fb_path    = "/dev/lcd0";
  dsc.input_path = "/dev/input0";
  lv_nuttx_init(&dsc, &result);
  if (!result.disp) { printf("ERROR: display\n"); return -1; }

  course_manager_init();
  vibrate_init();  /* 开启 LDO3 电源 */
  button_handler_start(button_event_cb);  /* 启动按键监听 */
  create_main_ui();

  lv_timer_create(clock_timer_cb, 1000, NULL);
  lv_timer_create(status_timer_cb, 30000, NULL);

  printf("UI ready.\n");

  while (1)
    {
      lv_timer_handler();
      usleep(5000);  /* 5ms sleep，约 200fps */
    }

  return 0;
}

/****************************************************************************
 * CLI 命令
 ****************************************************************************/

static int get_time(int *wd, int *h, int *m)
{
  return read_rtc(wd, h, m, NULL);
}

static void cmd_dump(void)
{
  course_manager_init();
  course_dump();
}

static void cmd_today(void)
{
  course_manager_init();
  int wd, h, m;
  if (get_time(&wd, &h, &m) < 0) return;
  const char *wn[] = {"","Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
  printf("\n=== Today: %s %02d:%02d ===\n", wn[wd], h, m);
  course_query_t st[10];
  int n = course_get_today_status(wd, h, m, st, 10);
  if (!n) { printf("No classes.\n\n"); return; }
  for (int i = 0; i < n; i++)
    {
      const course_t *c = st[i].course;
      printf("  [%s] %02d:%02d-%02d:%02d  %s  %s\n",
             st[i].is_happening ? "NOW" :
             st[i].minutes_left < 0 ? "DONE" : "NEXT",
             c->start_hour, c->start_min,
             c->end_hour, c->end_min, c->name, c->location);
    }
  printf("\n");
}

static void cmd_next(void)
{
  course_manager_init();
  int wd, h, m;
  if (get_time(&wd, &h, &m) < 0) return;
  course_query_t q;
  if (course_get_next(wd, h, m, &q) < 0)
    { printf("No more classes.\n"); return; }
  const course_t *c = q.course;
  if (q.is_happening)
    printf("Now: %s (%s) IN PROGRESS\n", c->name, c->location);
  else
    printf("Next: %s (%s) at %02d:%02d (%d min)\n",
           c->name, c->location, c->start_hour, c->start_min,
           q.minutes_left);
}

static void cmd_set_time(int argc, char *argv[])
{
  if (argc < 4) { printf("Usage: set_time <wd> <h> <m>\n"); return; }
  int wd = atoi(argv[2]), h = atoi(argv[3]), m = argc > 4 ? atoi(argv[4]) : 0;
  int fd = open("/dev/rtc0", O_RDONLY);
  if (fd < 0) return;
  struct rtc_time rt;
  ioctl(fd, RTC_RD_TIME, (unsigned long)&rt);
  rt.tm_wday = wd == 7 ? 0 : wd;
  rt.tm_hour = h; rt.tm_min = m; rt.tm_sec = 0;
  ioctl(fd, RTC_SET_TIME, (unsigned long)&rt);
  close(fd);
  const char *wn[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  printf("RTC: %s %02d:%02d\n", wn[rt.tm_wday], h, m);
}

static void cmd_help(void)
{
  printf("\n=== Campus Agent (Team 307) ===\n");
  printf("campus_agent          UI mode\n");
  printf("campus_agent dump     Course schedule\n");
  printf("campus_agent today    Today's courses\n");
  printf("campus_agent next     Next course\n");
  printf("campus_agent briefing Daily briefing\n");
  printf("campus_agent set_time <wd> <h> <m>\n");
  printf("campus_agent remind   Start reminder\n");
  printf("campus_agent stop     Stop reminder\n");
  printf("campus_agent sedentary Start sedentary\n");
  printf("campus_agent sed_stop Stop sedentary\n");
  printf("campus_agent test_vibrate\n");
  printf("campus_agent test_imu\n");
  printf("campus_agent ble_init [name]\n");
  printf("campus_agent ble_start\n");
  printf("campus_agent ble_stop\n");
  printf("campus_agent ble_status\n");
  printf("campus_agent ble_send <text>\n");
  printf("campus_agent ask <问题>      智能对话\n");
  printf("\n");
  printf("对话示例：\n");
  printf("  ask 今天有什么课\n");
  printf("  ask 下一节课\n");
  printf("  ask 开启提醒\n");
  printf("  ask 现在几点\n");
  printf("  ask 状态\n");
  printf("\n");
}

/****************************************************************************
 * 主函数
 ****************************************************************************/

int main(int argc, char *argv[])
{
  if (argc < 2) return run_ui_mode();

  if (strcmp(argv[1], "dump") == 0) cmd_dump();
  else if (strcmp(argv[1], "today") == 0) cmd_today();
  else if (strcmp(argv[1], "next") == 0) cmd_next();
  else if (strcmp(argv[1], "briefing") == 0)
    { course_manager_init(); ui_briefing_show_cli(); }
  else if (strcmp(argv[1], "set_time") == 0) cmd_set_time(argc, argv);
  else if (strcmp(argv[1], "remind") == 0)
    { reminder_engine_start(); printf("Started.\n"); }
  else if (strcmp(argv[1], "stop") == 0) reminder_engine_stop();
  else if (strcmp(argv[1], "sedentary") == 0)
    { sedentary_monitor_start(); printf("Started.\n"); }
  else if (strcmp(argv[1], "sed_stop") == 0) sedentary_monitor_stop();
  else if (strcmp(argv[1], "test_vibrate") == 0)
    {
      if (vibrate_init() == 0)
        {
          vibrate_test_poweron();
        }
    }
  else if (strcmp(argv[1], "test_imu") == 0) sedentary_test_imu();
  else if (strcmp(argv[1], "ask") == 0)
    {
      if (argc < 3)
        {
          printf("Usage: ask <问题>\n");
        }
      else
        {
          /* 拼接所有参数 */

          char input[128] = {0};
          int offset = 0;
          for (int i = 2; i < argc && offset < sizeof(input) - 1; i++)
            {
              if (i > 2) input[offset++] = ' ';
              int len = strlen(argv[i]);
              if (offset + len >= sizeof(input)) len = sizeof(input) - offset - 1;
              memcpy(input + offset, argv[i], len);
              offset += len;
            }

          /* 处理对话 */

          char response[512];
          dialogue_process(input, response, sizeof(response));
          printf("\n%s\n\n", response);
        }
    }
  else if (strcmp(argv[1], "help") == 0) cmd_help();
  else { printf("Unknown: %s\n", argv[1]); cmd_help(); }

  return 0;
}
