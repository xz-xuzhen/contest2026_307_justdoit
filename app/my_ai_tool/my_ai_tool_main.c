/****************************************************************************
 * Contest 2026 team 307 - My AI Tool
 *
 * 功能：
 *   - hello: 打招呼
 *   - info: 显示系统信息
 *   - popup <title> <content>: 显示弹窗（用于 AI Agent 触发）
 *   - vibrate <mode>: 震动（course/light/strong）
 *   - course_status: 显示课程状态
 *   - sedentary_status: 显示久坐状态
 *   - imu_read: 读取 IMU 数据
 *
 * 用法：nsh> my_ai_tool <command>
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
#include <nuttx/sensors/lsm6dsl.h>

#include <lvgl/lvgl.h>

/****************************************************************************
 * 弹窗数据结构
 ****************************************************************************/

typedef struct {
  char title[128];
  char content[256];
} popup_data_t;

/****************************************************************************
 * 震动控制（使用 GPIO PA20 + LDO3）
 ****************************************************************************/

extern void BSP_GPIO_Set(int pin, int val, int is_porta);

#define MOTOR_PIN     20
#define MOTOR_PORTA   1

static void vibrate_on(void)
{
  BSP_GPIO_Set(MOTOR_PIN, 1, MOTOR_PORTA);
}

static void vibrate_off(void)
{
  BSP_GPIO_Set(MOTOR_PIN, 0, MOTOR_PORTA);
}

/* 震动模式：课程提醒（三短震） */

static void vibrate_course(void)
{
  printf("[vibrate] Course reminder: 3 short pulses\n");
  for (int i = 0; i < 3; i++)
    {
      vibrate_on();
      usleep(500000);  /* 500ms */
      vibrate_off();
      if (i < 2) usleep(300000);  /* 300ms间隔 */
    }
}

/* 震动模式：轻提醒（单次长震） */

static void vibrate_light(void)
{
  printf("[vibrate] Light alert: 1 long pulse\n");
  vibrate_on();
  usleep(1500000);  /* 1500ms */
  vibrate_off();
}

/* 震动模式：强提醒（三次长震） */

static void vibrate_strong(void)
{
  printf("[vibrate] Strong alert: 3 long pulses\n");
  for (int i = 0; i < 3; i++)
    {
      vibrate_on();
      usleep(1000000);  /* 1000ms */
      vibrate_off();
      if (i < 2) usleep(500000);  /* 500ms间隔 */
    }
}

/****************************************************************************
 * LVGL 事件回调
 ****************************************************************************/

static void popup_btn_close_cb(lv_event_t *e)
{
  lv_obj_t *overlay = (lv_obj_t *)lv_event_get_user_data(e);
  if (overlay) lv_obj_del(overlay);
}

static void popup_overlay_close_cb(lv_event_t *e)
{
  lv_obj_t *overlay = (lv_obj_t *)lv_event_get_target(e);
  if (overlay) lv_obj_del(overlay);
}

/****************************************************************************
 * LVGL 异步回调（在 LVGL 线程中执行）
 ****************************************************************************/

static void popup_async_cb(void *data)
{
  popup_data_t *popup_data = (popup_data_t *)data;
  if (!popup_data) return;

  lv_obj_t *scr = lv_screen_active();
  if (!scr) {
    free(popup_data);
    return;
  }

  /* 创建全屏遮罩 */

  lv_obj_t *overlay = lv_obj_create(scr);
  lv_obj_set_size(overlay, 390, 450);
  lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_80, 0);
  lv_obj_set_style_border_width(overlay, 0, 0);
  lv_obj_set_style_radius(overlay, 0, 0);
  lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(overlay, LV_ALIGN_TOP_LEFT, 0, 0);

  /* Flex 布局 */

  lv_obj_set_flex_flow(overlay, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(overlay, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(overlay, 16, 0);

  /* 标题 */

  lv_obj_t *title = lv_label_create(overlay);
  lv_label_set_text(title, popup_data->title);
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

  /* 内容 */

  lv_obj_t *content = lv_label_create(overlay);
  lv_label_set_text(content, popup_data->content);
  lv_obj_set_style_text_color(content, lv_color_hex(0xAAAAAA), 0);

  /* 关闭按钮 */

  lv_obj_t *btn = lv_btn_create(overlay);
  lv_obj_set_size(btn, 200, 44);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x00E676), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(btn, 8, 0);

  lv_obj_t *btn_label = lv_label_create(btn);
  lv_label_set_text(btn_label, "OK");
  lv_obj_set_style_text_color(btn_label, lv_color_hex(0x000000), 0);
  lv_obj_center(btn_label);

  /* 存储 overlay 指针用于关闭回调 */

  lv_obj_set_user_data(btn, overlay);

  /* 注册关闭回调 */

  lv_obj_add_event_cb(btn, popup_btn_close_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(overlay, popup_overlay_close_cb, LV_EVENT_CLICKED, NULL);

  printf("[popup] Displayed: %s\n", popup_data->title);
  free(popup_data);
}

/****************************************************************************
 * 显示弹窗
 ****************************************************************************/

static int show_popup(const char *title, const char *content)
{
  popup_data_t *popup_data = malloc(sizeof(popup_data_t));
  if (!popup_data) {
    printf("[popup] Error: Memory allocation failed\n");
    return -1;
  }

  strncpy(popup_data->title, title, sizeof(popup_data->title) - 1);
  popup_data->title[sizeof(popup_data->title) - 1] = '\0';
  strncpy(popup_data->content, content, sizeof(popup_data->content) - 1);
  popup_data->content[sizeof(popup_data->content) - 1] = '\0';

  lv_result_t res = lv_async_call(popup_async_cb, popup_data);
  if (res != LV_RESULT_OK) {
    free(popup_data);
    printf("[popup] Error: lv_async_call failed\n");
    return -1;
  }

  printf("[popup] Scheduled: %s\n", title);
  return 0;
}

/****************************************************************************
 * RTC 时间读取
 ****************************************************************************/

static int get_time(int *wd, int *h, int *m)
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
  return 0;
}

/****************************************************************************
 * 课程状态查询
 ****************************************************************************/

/* 课程表数据（简化版） */

typedef struct {
  const char *name;
  const char *location;
  int weekday;
  int start_h, start_m;
  int end_h, end_m;
} course_info_t;

static const course_info_t g_courses[] = {
  {"gaodengshuxue",   "A302", 1,  8, 0,  9, 35},
  {"daxueyingyu",     "B105", 1, 10, 0, 11, 35},
  {"sixiangdaode",    "A201", 1, 14, 0, 15, 35},
  {"tiyu",            "tiguguan", 1, 16, 0, 17, 35},
  {"shujujiegou",     "C201", 2,  8, 0,  9, 35},
  {"qianrushixitong", "B201", 2, 10, 0, 11, 35},
  {"daxuewuli",       "A105", 2, 14, 0, 15, 35},
  {"yingyuyuedu",     "B201", 2, 16, 0, 17, 35},
  {"xianxingdaishu",  "A108", 3,  8, 0,  9, 35},
  {"jisuanjiwangluo", "C305", 3, 10, 0, 11, 35},
  {"caozuoxitong",    "C401", 3, 14, 0, 15, 35},
  {"ruxuejiaoyu",     "baogaoTing", 3, 16, 0, 17, 35},
  {"gaodengshuxue",   "A302", 4,  8, 0,  9, 35},
  {"daxueyingyu",     "B105", 4, 10, 0, 11, 35},
  {"bianyuanlixue",   "C201", 4, 14, 0, 15, 35},
  {"jisuanjisuzhi",   "A201", 4, 16, 0, 17, 35},
  {"shujujiegou_shiyan","D102", 5,  8, 0, 10, 35},
  {"qianrushixitong_shiyan","B301", 5, 14, 0, 16, 35},
  {"banhui",          "jiaoshi", 5, 16, 0, 17, 35},
};

#define COURSE_COUNT (sizeof(g_courses) / sizeof(g_courses[0]))

static void cmd_course_status(void)
{
  int wd, h, m;
  if (get_time(&wd, &h, &m) < 0) {
    printf("ERROR: Cannot read RTC\n");
    return;
  }

  const char *wn[] = {"","Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
  int now_min = h * 60 + m;
  int found = 0;

  printf("=== Today: %s %02d:%02d ===\n", wn[wd], h, m);

  for (int i = 0; i < COURSE_COUNT; i++) {
    if (g_courses[i].weekday != wd) continue;
    found++;

    int start_min = g_courses[i].start_h * 60 + g_courses[i].start_m;
    int end_min = g_courses[i].end_h * 60 + g_courses[i].end_m;

    const char *status;
    if (now_min >= end_min)
      status = "[DONE]";
    else if (now_min >= start_min)
      status = "[NOW]";
    else
      status = "[NEXT]";

    printf("  %s %02d:%02d-%02d:%02d %s %s\n",
           status,
           g_courses[i].start_h, g_courses[i].start_m,
           g_courses[i].end_h, g_courses[i].end_m,
           g_courses[i].name, g_courses[i].location);
  }

  if (!found) printf("  No classes today.\n");
}

/****************************************************************************
 * IMU 数据读取
 ****************************************************************************/

static void cmd_imu_read(void)
{
  int fd = open("/dev/lsm6dsl0", O_RDONLY);
  if (fd < 0) {
    printf("ERROR: Cannot open IMU\n");
    return;
  }

  struct lsm6dsl_sensor_data_s data;
  int ret = ioctl(fd, SNIOC_LSM6DSLSENSORREAD, (unsigned long)&data);
  close(fd);

  if (ret < 0) {
    printf("ERROR: IMU read failed\n");
    return;
  }

  printf("IMU Data:\n");
  printf("  Accel: X=%d Y=%d Z=%d\n", data.x_data, data.y_data, data.z_data);
  printf("  Gyro:  X=%d Y=%d Z=%d\n", data.g_x_data, data.g_y_data, data.g_z_data);
  printf("  Temp:  %d\n", data.temperature);

  /* 判断运动状态 */

  float ax = (float)data.x_data;
  float ay = (float)data.y_data;
  float az = (float)data.z_data;
  float magnitude = sqrtf(ax*ax + ay*ay + az*az);

  printf("  Magnitude: %.1f\n", magnitude);
  printf("  Status: %s\n", (magnitude > 1100 || magnitude < 900) ? "MOVING" : "STILL");
}

/****************************************************************************
 * 主函数
 ****************************************************************************/

int main(int argc, char *argv[])
{
  if (argc < 2) {
    printf("=== My AI Tool (team 307) ===\n");
    printf("Usage: my_ai_tool <command>\n\n");
    printf("Commands:\n");
    printf("  hello                    - Say hello\n");
    printf("  info                     - Show system info\n");
    printf("  popup <title> <content>  - Show popup\n");
    printf("  vibrate <mode>           - Vibrate (course/light/strong)\n");
    printf("  course_status            - Show course status\n");
    printf("  sedentary_status         - Show sedentary status\n");
    printf("  imu_read                 - Read IMU data\n");
    return 0;
  }

  if (strcmp(argv[1], "hello") == 0) {
    printf("Hello from team 307!\n");
  } else if (strcmp(argv[1], "info") == 0) {
    printf("Board: lckfb_huangshan_pi (SF32LB52)\n");
    printf("Team: 307 justdoit\n");
  } else if (strcmp(argv[1], "popup") == 0) {
    if (argc < 4) {
      printf("Usage: my_ai_tool popup <title> <content>\n");
      return -1;
    }
    return show_popup(argv[2], argv[3]);
  } else if (strcmp(argv[1], "vibrate") == 0) {
    if (argc < 3) {
      printf("Usage: my_ai_tool vibrate <course|light|strong>\n");
      return -1;
    }
    if (strcmp(argv[2], "course") == 0) vibrate_course();
    else if (strcmp(argv[2], "light") == 0) vibrate_light();
    else if (strcmp(argv[2], "strong") == 0) vibrate_strong();
    else printf("Unknown mode: %s\n", argv[2]);
  } else if (strcmp(argv[1], "course_status") == 0) {
    cmd_course_status();
  } else if (strcmp(argv[1], "sedentary_status") == 0) {
    printf("Sedentary status: not implemented yet\n");
  } else if (strcmp(argv[1], "imu_read") == 0) {
    cmd_imu_read();
  } else {
    printf("Unknown command: %s\n", argv[1]);
  }

  return 0;
}
