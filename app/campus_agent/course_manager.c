/****************************************************************************
 * course_manager.c — 课程表管理
 *
 * 课程数据硬编码，支持按星期/时间查询，提醒状态管理
 *
 * Contest 2026 Team 307
 ****************************************************************************/

#include <stdio.h>
#include <string.h>
#include "course_manager.h"

/****************************************************************************
 * 课程表数据（小林的 2026 秋季课程）
 ****************************************************************************/

static const course_t g_courses[] =
{
  /* 周一 */
  { 1,  "gaodengshuxue",     "jiaoxuelou A302",  1,  8, 0,  9, 35, 15 },
  { 2,  "daxueyingyu",       "waiyulou B105",     1, 10, 0, 11, 35, 15 },
  { 3,  "sixiangdaode",      "jiaoxuelou A201",   1, 14, 0, 15, 35, 15 },
  { 4,  "tiyu",              "tiyuguan",          1, 16, 0, 17, 35, 15 },

  /* 周二 */
  { 5,  "shujujiegou",       "jisuanjilou C201",  2,  8, 0,  9, 35, 15 },
  { 6,  "qianrushixitong",   "shiyanlou B201",    2, 10, 0, 11, 35, 15 },
  { 7,  "daxuewuli",         "lishi A105",        2, 14, 0, 15, 35, 15 },
  { 8,  "yingyuyuedu",       "waiyulou B201",     2, 16, 0, 17, 35, 15 },

  /* 周三 */
  { 9,  "xianxingdaishu",    "jiaoxuelou A108",   3,  8, 0,  9, 35, 15 },
  { 10, "jisuanjiwangluo",   "jisuanjilou C305",  3, 10, 0, 11, 35, 15 },
  { 11, "caozuoxitong",      "jisuanjilou C401",  3, 14, 0, 15, 35, 15 },
  { 12, "ruxuejiaoyu",       "baogaoTing",        3, 16, 0, 17, 35, 15 },

  /* 周四 */
  { 13, "gaodengshuxue",     "jiaoxuelou A302",   4,  8, 0,  9, 35, 15 },
  { 14, "daxueyingyu",       "waiyulou B105",     4, 10, 0, 11, 35, 15 },
  { 15, "bianyuanlixue",     "jisuanjilou C201",  4, 14, 0, 15, 35, 15 },
  { 16, "jisuanjisuzhi",     "jiaoxuelou A201",   4, 16, 0, 17, 35, 15 },

  /* 周五 */
  { 17, "shujujiegou_shiyan","shiyanshi D102",    5,  8, 0, 10, 35, 15 },
  { 18, "qianrushixitong_shiyan","shiyanlou B301", 5, 14, 0, 16, 35, 15 },
  { 19, "banhui",            "jiaoshi",           5, 16, 0, 17, 35, 15 },
};

#define COURSE_COUNT (sizeof(g_courses) / sizeof(g_courses[0]))

/****************************************************************************
 * 提醒状态记录
 ****************************************************************************/

static uint8_t g_reminded[COURSE_COUNT];  /* 0=未提醒 1=已提醒 */

/****************************************************************************
 * 内部工具函数
 ****************************************************************************/

/* 将课程时间转换为分钟数 (0-1439) */

static int time_to_minutes(int hour, int min)
{
  return hour * 60 + min;
}

/* 计算两时间点之间的分钟差 */

static int minutes_diff(int weekday1, int hour1, int min1,
                        int weekday2, int hour2, int min2)
{
  int t1 = (weekday1 - 1) * 1440 + time_to_minutes(hour1, min1);
  int t2 = (weekday2 - 1) * 1440 + time_to_minutes(hour2, min2);
  return t2 - t1;
}

/****************************************************************************
 * 公共 API 实现
 ****************************************************************************/

int course_manager_init(void)
{
  memset(g_reminded, 0, sizeof(g_reminded));
  printf("[course] Loaded %d courses\n", (int)COURSE_COUNT);
  return 0;
}

int course_get_count(void)
{
  return (int)COURSE_COUNT;
}

const course_t *course_get_by_index(int index)
{
  if (index < 0 || index >= (int)COURSE_COUNT)
    return NULL;
  return &g_courses[index];
}

int course_get_by_weekday(int weekday,
                          const course_t **out, int max_out)
{
  int count = 0;
  for (int i = 0; i < (int)COURSE_COUNT && count < max_out; i++)
    {
      if (g_courses[i].weekday == weekday)
        {
          out[count++] = &g_courses[i];
        }
    }

  return count;
}

int course_get_next(int weekday, int hour, int min,
                    course_query_t *result)
{
  if (!result)
    return -1;

  result->course = NULL;
  result->minutes_left = 99999;
  result->is_happening = 0;

  int now_min = time_to_minutes(hour, min);
  int best_idx = -1;

  for (int i = 0; i < (int)COURSE_COUNT; i++)
    {
      const course_t *c = &g_courses[i];
      if (c->weekday != weekday)
        continue;

      int start = time_to_minutes(c->start_hour, c->start_min);
      int end   = time_to_minutes(c->end_hour, c->end_min);

      /* 正在上课 */

      if (now_min >= start && now_min < end)
        {
          result->course = c;
          result->minutes_left = 0;
          result->is_happening = 1;
          return 0;
        }

      /* 还没上课 */

      if (now_min < start)
        {
          int diff = start - now_min;
          if (diff < result->minutes_left)
            {
              result->minutes_left = diff;
              best_idx = i;
            }
        }
    }

  if (best_idx >= 0)
    {
      result->course = &g_courses[best_idx];
      return 0;
    }

  return -1;  /* 今天没课了 */
}

int course_get_today_status(int weekday, int hour, int min,
                            course_query_t *states, int max_states)
{
  int count = 0;
  int now_min = time_to_minutes(hour, min);

  for (int i = 0; i < (int)COURSE_COUNT && count < max_states; i++)
    {
      const course_t *c = &g_courses[i];
      if (c->weekday != weekday)
        continue;

      states[count].course = c;

      int start = time_to_minutes(c->start_hour, c->start_min);
      int end   = time_to_minutes(c->end_hour, c->end_min);

      if (now_min >= end)
        {
          /* 已结束 */

          states[count].minutes_left = -(now_min - end);
          states[count].is_happening = 0;
        }
      else if (now_min >= start)
        {
          /* 正在上课 */

          states[count].minutes_left = 0;
          states[count].is_happening = 1;
        }
      else
        {
          /* 未开始 */

          states[count].minutes_left = start - now_min;
          states[count].is_happening = 0;
        }

      count++;
    }

  return count;
}

int course_check_remind(int weekday, int hour, int min)
{
  int now_min = time_to_minutes(hour, min);

  for (int i = 0; i < (int)COURSE_COUNT; i++)
    {
      const course_t *c = &g_courses[i];
      if (c->weekday != weekday)
        continue;
      if (g_reminded[i])
        continue;

      int start = time_to_minutes(c->start_hour, c->start_min);
      int diff = start - now_min;

      /* 在提醒窗口内 (0 ~ remind_min 分钟) */

      if (diff >= 0 && diff <= c->remind_min)
        {
          printf("[course] Remind: %s in %d min\n",
                 c->name, diff);
          return i;
        }
    }

  return -1;
}

void course_mark_reminded(int course_id)
{
  for (int i = 0; i < (int)COURSE_COUNT; i++)
    {
      if (g_courses[i].id == course_id)
        {
          g_reminded[i] = 1;
          return;
        }
    }
}

void course_reset_reminded(void)
{
  memset(g_reminded, 0, sizeof(g_reminded));
  printf("[course] Reminded flags reset\n");
}

void course_dump(void)
{
  printf("=== Course Schedule (%d courses) ===\n",
         (int)COURSE_COUNT);

  const char *weekday_names[] =
  {
    "", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
  };

  int last_wd = -1;
  for (int i = 0; i < (int)COURSE_COUNT; i++)
    {
      const course_t *c = &g_courses[i];
      if (c->weekday != last_wd)
        {
          printf("\n%s:\n", weekday_names[c->weekday]);
          last_wd = c->weekday;
        }

      printf("  %02d:%02d-%02d:%02d  %-20s  %s  (remind %dmin)\n",
             c->start_hour, c->start_min,
             c->end_hour, c->end_min,
             c->name, c->location, c->remind_min);
    }

  printf("\n");
}
