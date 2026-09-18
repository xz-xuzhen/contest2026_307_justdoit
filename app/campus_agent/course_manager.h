/****************************************************************************
 * course_manager.h — 课程表管理
 *
 * 功能：
 *   - 存储课程表数据（硬编码）
 *   - 按星期查询当天课程
 *   - 按时间查询下一节课
 *   - 判断某节课是否需要提醒
 *
 * Contest 2026 Team 307
 ****************************************************************************/

#ifndef __COURSE_MANAGER_H
#define __COURSE_MANAGER_H

/****************************************************************************
 * 课程信息结构体
 ****************************************************************************/

typedef struct {
  int         id;           /* 课程 ID */
  const char *name;         /* 课程名称 */
  const char *location;     /* 教室地点 */
  int         weekday;      /* 1=周一 ... 5=周五 */
  int         start_hour;   /* 开始时 (8-20) */
  int         start_min;    /* 开始分 (0/5/10/...) */
  int         end_hour;     /* 结束时 */
  int         end_min;      /* 结束分 */
  int         remind_min;   /* 提前几分钟提醒 (默认15) */
} course_t;

/****************************************************************************
 * 查询结果结构体
 ****************************************************************************/

typedef struct {
  const course_t *course;       /* 课程指针 (NULL 表示无) */
  int             minutes_left; /* 距开始还有多少分钟 */
  int             is_happening; /* 是否正在上课中 */
} course_query_t;

/****************************************************************************
 * 公共 API
 ****************************************************************************/

/**
 * 初始化课程表
 * 必须在其他函数之前调用
 */

int course_manager_init(void);

/**
 * 获取课程总数
 */

int course_get_count(void);

/**
 * 获取指定索引的课程 (0-based)
 * 返回 NULL 表示索引越界
 */

const course_t *course_get_by_index(int index);

/**
 * 查询指定星期几的所有课程
 *
 * @param weekday   1=周一 ... 5=周五
 * @param out       输出数组
 * @param max_out   数组最大容量
 * @return 实际找到的课程数量
 */

int course_get_by_weekday(int weekday,
                          const course_t **out, int max_out);

/**
 * 查询下一节课
 *
 * @param weekday   当前星期几 (1-5)
 * @param hour      当前时 (0-23)
 * @param min       当前分 (0-59)
 * @param result    输出查询结果
 * @return 0=找到, -1=今天没课了
 */

int course_get_next(int weekday, int hour, int min,
                    course_query_t *result);

/**
 * 查询今天所有课程的状态
 *
 * @param weekday   当前星期几
 * @param hour      当前时
 * @param min       当前分
 * @param states    输出状态数组 (每门课一个)
 * @param max_states 数组最大容量
 * @return 实际课程数量
 *
 * states[i] 含义:
 *   - 已结束: minutes_left < 0, is_happening = 0
 *   - 正在上课: is_happening = 1
 *   - 即将上课: minutes_left > 0, is_happening = 0
 */

int course_get_today_status(int weekday, int hour, int min,
                            course_query_t *states, int max_states);

/**
 * 判断是否需要触发提醒
 *
 * @param weekday   当前星期几
 * @param hour      当前时
 * @param min       当前分
 * @return 需要提醒的课程索引, -1=不需要
 *
 * 条件: 距上课时间 ≤ remind_min 且未提醒过
 */

int course_check_remind(int weekday, int hour, int min);

/**
 * 标记某课程已提醒（避免重复提醒）
 */

void course_mark_reminded(int course_id);

/**
 * 重置提醒标记（每天零点调用）
 */

void course_reset_reminded(void);

/**
 * 打印课程表（调试用）
 */

void course_dump(void);

#endif /* __COURSE_MANAGER_H */
