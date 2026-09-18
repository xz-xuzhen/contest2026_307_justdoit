/****************************************************************************
 * reminder_engine.c — 课程提醒引擎
 *
 * 后台线程每 60 秒检查课程表，到时间触发震动 + 弹窗
 *
 * Contest 2026 Team 307
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <nuttx/timers/rtc.h>
#include <pthread.h>

#include "course_manager.h"
#include "reminder_engine.h"

/****************************************************************************
 * 外部依赖：震动和弹窗（后续集成 LVGL 时实现）
 ****************************************************************************/

/****************************************************************************
 * 震动控制（使用 vibrate 模块）
 ****************************************************************************/

#include "vibrate.h"

/* 三短震（课程提醒）：500ms x 3，强度 100 */

static void do_vibrate_course(void)
{
  printf("[remind] Vibrate: triple (500ms x 3)\n");
  for (int i = 0; i < 3; i++)
    {
      vibrate_pulse(500, 100);
      if (i < 2) usleep(300000);
    }
}

/* 弹窗：课程提醒（当前用 printf 代替，后续接入 LVGL） */

static void do_popup_course(const course_t *c)
{
  printf("\n");
  printf("========================================\n");
  printf("  COURSE REMINDER\n");
  printf("  %s\n", c->name);
  printf("  Location: %s\n", c->location);
  printf("  Time: %02d:%02d - %02d:%02d\n",
         c->start_hour, c->start_min,
         c->end_hour, c->end_min);
  printf("========================================\n\n");
}

/****************************************************************************
 * 读取 RTC 时间
 ****************************************************************************/

static int read_rtc(int *weekday, int *hour, int *min, int *sec)
{
  int fd = open("/dev/rtc0", O_RDONLY);
  if (fd < 0)
    return -1;

  struct rtc_time rt;
  int ret = ioctl(fd, RTC_RD_TIME, (unsigned long)&rt);
  close(fd);

  if (ret < 0)
    return -1;

  if (weekday) *weekday = rt.tm_wday == 0 ? 7 : rt.tm_wday;
  if (hour)    *hour    = rt.tm_hour;
  if (min)     *min     = rt.tm_min;
  if (sec)     *sec     = rt.tm_sec;

  return 0;
}

/****************************************************************************
 * 提醒线程
 ****************************************************************************/

static volatile int g_running = 0;
static pthread_t g_thread;

static void *reminder_thread(void *arg)
{
  (void)arg;

  printf("[remind] Engine started (check every 60s)\n");

  int last_min = -1;  /* 避免同一分钟重复检查 */

  while (g_running)
    {
      int wd, h, m, s;
      if (read_rtc(&wd, &h, &m, &s) < 0)
        {
          sleep(10);
          continue;
        }

      /* 每分钟只检查一次 */

      if (m == last_min)
        {
          sleep(5);
          continue;
        }

      last_min = m;

      /* 检查是否有课程需要提醒 */

      int idx = course_check_remind(wd, h, m);
      if (idx >= 0)
        {
          const course_t *c = course_get_by_index(idx);
          if (c)
            {
              printf("[remind] Alert: %s in %d min!\n",
                     c->name, c->remind_min);

              /* 震动 */

              do_vibrate_course();

              /* 弹窗 */

              do_popup_course(c);

              /* 标记已提醒 */

              course_mark_reminded(c->id);
            }
        }

      /* 零点重置提醒标记 */

      if (h == 0 && m == 0)
        {
          course_reset_reminded();
        }

      sleep(30);
    }

  printf("[remind] Engine stopped\n");
  return NULL;
}

/****************************************************************************
 * 公共 API
 ****************************************************************************/

int reminder_engine_start(void)
{
  if (g_running)
    {
      printf("[remind] Already running\n");
      return -1;
    }

  g_running = 1;
  int ret = pthread_create(&g_thread, NULL, reminder_thread, NULL);
  if (ret != 0)
    {
      printf("[remind] ERROR: pthread_create failed: %d\n", ret);
      g_running = 0;
      return -1;
    }

  return 0;
}

void reminder_engine_stop(void)
{
  if (!g_running)
    return;

  g_running = 0;
  pthread_join(g_thread, NULL);
}

int reminder_engine_is_running(void)
{
  return g_running;
}

int reminder_engine_test(void)
{
  printf("[remind] Test: vibrate + popup\n");

  do_vibrate_course();

  course_t test_course =
    {
      .id = 0,
      .name = "TEST COURSE",
      .location = "TEST LOCATION",
      .weekday = 2,
      .start_hour = 14,
      .start_min = 0,
      .end_hour = 15,
      .end_min = 35,
      .remind_min = 15,
    };

  do_popup_course(&test_course);

  return 0;
}
