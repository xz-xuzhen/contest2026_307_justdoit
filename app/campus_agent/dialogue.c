/****************************************************************************
 * dialogue.c — CLI 智能对话模块（离线关键词匹配）
 *
 * 通过关键词匹配实现离线智能对话
 * 当有网络时可切换为 LLM 推理（预留接口）
 *
 * Contest 2026 Team 307
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <nuttx/timers/rtc.h>

#include "dialogue.h"
#include "course_manager.h"
#include "reminder_engine.h"
#include "sedentary_monitor.h"

/****************************************************************************
 * 关键词匹配结构
 ****************************************************************************/

typedef struct {
  const char *keywords[8];  /* 关键词列表（最多 8 个） */
  int keyword_count;        /* 关键词数量 */
  const char *(*handler)(void);  /* 处理函数 */
} intent_t;

/****************************************************************************
 * 辅助函数：读取 RTC
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
 * 意图处理函数
 ****************************************************************************/

static char g_response[512];

/* 课程查询 */

static const char *handle_course_query(void)
{
  int wd, h, m;
  if (get_time(&wd, &h, &m) < 0)
    {
      return "无法读取时间";
    }

  const char *wn[] = {"","周一","周二","周三","周四","周五","周六","周日"};

  course_query_t states[10];
  int count = course_get_today_status(wd, h, m, states, 10);

  if (count == 0)
    {
      snprintf(g_response, sizeof(g_response),
               "今天%s没有课，好好休息吧！", wn[wd]);
      return g_response;
    }

  int offset = 0;
  offset += snprintf(g_response + offset, sizeof(g_response) - offset,
                     "今天%s有%d门课：\n", wn[wd], count);

  for (int i = 0; i < count && offset < sizeof(g_response) - 1; i++)
    {
      const course_t *c = states[i].course;
      const char *status;
      if (states[i].is_happening)
        status = "[正在上课]";
      else if (states[i].minutes_left < 0)
        status = "[已结束]";
      else
        status = "[未开始]";

      offset += snprintf(g_response + offset, sizeof(g_response) - offset,
                         "  %s %02d:%02d-%02d:%02d %s %s\n",
                         status,
                         c->start_hour, c->start_min,
                         c->end_hour, c->end_min,
                         c->name, c->location);
    }

  return g_response;
}

/* 下一节课 */

static const char *handle_next_course(void)
{
  int wd, h, m;
  if (get_time(&wd, &h, &m) < 0)
    {
      return "无法读取时间";
    }

  course_query_t q;
  if (course_get_next(wd, h, m, &q) < 0)
    {
      return "今天没有课了";
    }

  const course_t *c = q.course;
  if (q.is_happening)
    {
      snprintf(g_response, sizeof(g_response),
               "正在上：%s (%s)", c->name, c->location);
    }
  else
    {
      snprintf(g_response, sizeof(g_response),
               "下一节：%s %02d:%02d (%s) 还有%d分钟",
               c->name, c->start_hour, c->start_min,
               c->location, q.minutes_left);
    }

  return g_response;
}

/* 开启提醒 */

static const char *handle_start_remind(void)
{
  if (reminder_engine_is_running())
    {
      return "课程提醒已经在运行了";
    }

  reminder_engine_start();
  return "课程提醒已开启，课前15分钟会震动提醒";
}

/* 关闭提醒 */

static const char *handle_stop_remind(void)
{
  if (!reminder_engine_is_running())
    {
      return "课程提醒已经是关闭状态";
    }

  reminder_engine_stop();
  return "课程提醒已关闭";
}

/* 开启久坐监测 */

static const char *handle_start_sedentary(void)
{
  if (sedentary_monitor_is_running())
    {
      return "久坐监测已经在运行了";
    }

  sedentary_monitor_start();
  return "久坐监测已开启，久坐2分钟会震动提醒";
}

/* 关闭久坐监测 */

static const char *handle_stop_sedentary(void)
{
  if (!sedentary_monitor_is_running())
    {
      return "久坐监测已经是关闭状态";
    }

  sedentary_monitor_stop();
  return "久坐监测已关闭";
}

/* 当前时间 */

static const char *handle_time_query(void)
{
  int wd, h, m;
  if (get_time(&wd, &h, &m) < 0)
    {
      return "无法读取时间";
    }

  const char *wn[] = {"","周一","周二","周三","周四","周五","周六","周日"};
  snprintf(g_response, sizeof(g_response),
           "现在是%s %02d:%02d", wn[wd], h, m);
  return g_response;
}

/* 系统状态 */

static const char *handle_status(void)
{
  int wd, h, m;
  get_time(&wd, &h, &m);
  const char *wn[] = {"","周一","周二","周三","周四","周五","周六","周日"};

  snprintf(g_response, sizeof(g_response),
           "=== 系统状态 ===\n"
           "时间：%s %02d:%02d\n"
           "课程提醒：%s\n"
           "久坐监测：%s\n"
           "================",
           wn[wd], h, m,
           reminder_engine_is_running() ? "开启" : "关闭",
           sedentary_monitor_is_running() ? "开启" : "关闭");
  return g_response;
}

/* 帮助 */

static const char *handle_help(void)
{
  return "=== 支持的命令 ===\n"
         "今天有什么课 - 查询今日课程\n"
         "下一节课 - 查询下一节课\n"
         "开启/关闭提醒 - 控制课程提醒\n"
         "开启/关闭久坐 - 控制久坐监测\n"
         "现在几点 - 查询当前时间\n"
         "状态 - 查看系统状态\n"
         "==================";
}

/* 默认回复 */

static const char *handle_default(void)
{
  return "抱歉，我没有理解你的意思。可以试试：\n"
         "- 今天有什么课\n"
         "- 下一节课\n"
         "- 开启提醒\n"
         "- 现在几点\n"
         "- 帮助";
}

/****************************************************************************
 * 意图匹配表
 ****************************************************************************/

static intent_t g_intents[] = {
  /* 课程查询 */

  { {"课", "课程", "今天", "有什么", NULL}, 4,
    handle_course_query },

  /* 下一节课 */

  { {"下一", "下一节", "下节课", NULL}, 3,
    handle_next_course },

  /* 开启提醒 */

  { {"开", "开启", "提醒", "打开", NULL}, 4,
    handle_start_remind },

  /* 关闭提醒 */

  { {"关", "关闭", "提醒", "停止", NULL}, 4,
    handle_stop_remind },

  /* 开启久坐 */

  { {"开", "开启", "久坐", "监测", NULL}, 4,
    handle_start_sedentary },

  /* 关闭久坐 */

  { {"关", "关闭", "久坐", "停止", NULL}, 4,
    handle_stop_sedentary },

  /* 时间查询 */

  { {"时间", "几点", "现在", "什么时候", NULL}, 4,
    handle_time_query },

  /* 系统状态 */

  { {"状态", "系统", "信息", NULL}, 3,
    handle_status },

  /* 帮助 */

  { {"帮助", "help", "命令", "怎么用", NULL}, 4,
    handle_help },
};

#define INTENT_COUNT (sizeof(g_intents) / sizeof(g_intents[0]))

/****************************************************************************
 * 关键词匹配
 ****************************************************************************/

static int match_intent(const char *input, intent_t *intent)
{
  int match_count = 0;

  for (int i = 0; i < intent->keyword_count && intent->keywords[i]; i++)
    {
      if (strstr(input, intent->keywords[i]) != NULL)
        {
          match_count++;
        }
    }

  return match_count;
}

/****************************************************************************
 * 公共 API
 ****************************************************************************/

int dialogue_init(void)
{
  course_manager_init();
  printf("[dialogue] Init OK (offline keyword matching)\n");
  return 0;
}

int dialogue_process(const char *input, char *output, int out_size)
{
  if (!input || !output || out_size <= 0)
    {
      return -1;
    }

  /* 查找最佳匹配的意图 */

  int best_score = 0;
  int best_idx = -1;

  for (int i = 0; i < INTENT_COUNT; i++)
    {
      int score = match_intent(input, &g_intents[i]);
      if (score > best_score)
        {
          best_score = score;
          best_idx = i;
        }
    }

  /* 调用处理函数 */

  const char *response;
  if (best_idx >= 0 && best_score >= 2)
    {
      response = g_intents[best_idx].handler();
    }
  else
    {
      response = handle_default();
    }

  /* 复制结果 */

  strncpy(output, response, out_size - 1);
  output[out_size - 1] = '\0';

  return 0;
}
