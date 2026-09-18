/****************************************************************************
 * dialogue.h — CLI 智能对话模块（离线关键词匹配）
 *
 * 功能：
 *   - 解析用户输入的自然语言
 *   - 匹配关键词，调用对应功能
 *   - 返回格式化回复
 *
 * 支持的意图：
 *   - 课程查询：今天有什么课、下一节课、课程表
 *   - 提醒控制：开启/关闭提醒
 *   - 久坐控制：开启/关闭久坐监测
 *   - 时间查询：现在几点、今天星期几
 *   - 系统状态：状态、帮助
 *
 * Contest 2026 Team 307
 ****************************************************************************/

#ifndef __DIALOGUE_H
#define __DIALOGUE_H

/**
 * 处理用户输入，返回回复文本
 *
 * @param input   用户输入字符串
 * @param output  输出缓冲区
 * @param out_size 缓冲区大小
 * @return 0 成功, -1 错误
 */

int dialogue_process(const char *input, char *output, int out_size);

/**
 * 初始化对话模块
 * @return 0 成功
 */

int dialogue_init(void);

#endif /* __DIALOGUE_H */
