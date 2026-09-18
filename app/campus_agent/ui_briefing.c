/****************************************************************************
 * ui_briefing.c — 每日简报
 *
 * 显示今天的课程列表（CLI 输出 + LVGL 弹窗）
 *
 * Contest 2026 Team 307
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <nuttx/timers/rtc.h>

#include "course_manager.h"
#include "ui_briefing.h"

/****************************************************************************
 * 读取 RTC 时间
 ****************************************************************************/

static int read_rtc(int *weekday, int *hour, int *min)
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

  return 0;
}

/****************************************************************************
 * CLI 版简报
 ****************************************************************************/

void ui_briefing_show_cli(void)
{
  int wd, h, m;
  if (read_rtc(&wd, &h, &m) < 0)
    {
      printf("[briefing] ERROR: Cannot read RTC\n");
      return;
    }

  const char *weekday_names[] =
    { "", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };

  printf("\n");
  printf("=== Daily Briefing ===\n");
  printf("Date: %s %02d:%02d\n", weekday_names[wd], h, m);
  printf("\n");

  course_query_t states[10];
  int count = course_get_today_status(wd, h, m,
                                      states, 10);

  if (count == 0)
    {
      printf("  No classes today. Enjoy your day!\n\n");
      return;
    }

  printf("  Today's courses (%d):\n\n", count);

  for (int i = 0; i < count; i++)
    {
      const course_t *c = states[i].course;
      const char *status_icon;

      if (states[i].is_happening)
        {
          status_icon = "[*]";  /* 正在上课 */
        }
      else if (states[i].minutes_left < 0)
        {
          status_icon = "[v]";  /* 已结束 */
        }
      else
        {
          status_icon = "[ ]";  /* 未开始 */
        }

      printf("  %s %02d:%02d-%02d:%02d  %s\n",
             status_icon,
             c->start_hour, c->start_min,
             c->end_hour, c->end_min,
             c->name);

      printf("      Location: %s\n", c->location);

      if (states[i].is_happening)
        {
          printf("      Status: IN PROGRESS\n");
        }
      else if (states[i].minutes_left > 0)
        {
          printf("      Status: starts in %d min\n",
                 states[i].minutes_left);
        }
      else
        {
          printf("      Status: DONE\n");
        }

      printf("\n");
    }
}

/****************************************************************************
 * LVGL 版简报（后续集成）
 ****************************************************************************/

#ifdef CONFIG_GRAPHICS_LVGL

lv_obj_t *ui_briefing_create(lv_obj_t *parent)
{
  /* TODO: LVGL 版本的简报页面 */
  /* 暂时返回 NULL，后续集成 */

  ui_briefing_show_cli();
  return NULL;
}

void ui_briefing_refresh(void)
{
  /* TODO: 刷新 LVGL 内容 */
}

#endif /* CONFIG_GRAPHICS_LVGL */
