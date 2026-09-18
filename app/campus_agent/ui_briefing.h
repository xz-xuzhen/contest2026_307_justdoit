/****************************************************************************
 * ui_briefing.h — 每日简报页面
 *
 * 功能：
 *   - 读取 RTC 时间
 *   - 从 course_manager 获取今天的课程
 *   - 用 LVGL 显示课程列表（带状态标记）
 *
 * Contest 2026 Team 307
 ****************************************************************************/

#ifndef __UI_BRIEFING_H
#define __UI_BRIEFING_H

#include <lvgl.h>

/**
 * 创建每日简报页面
 * @param parent 父对象（屏幕）
 * @return 页面对象
 *
 * 显示今天的课程列表，带状态标记：
 *   ✅ 已结束
 *   🔴 正在上课
 *   ⏳ 未开始
 */

lv_obj_t *ui_briefing_create(lv_obj_t *parent);

/**
 * 刷新简报内容（更新时间和课程状态）
 */

void ui_briefing_refresh(void);

#endif /* __UI_BRIEFING_H */
