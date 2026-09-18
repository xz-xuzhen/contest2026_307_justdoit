/****************************************************************************
 * vibrate.h — 振动马达驱动（PA20 PWM + LDO3 电源）
 *
 * 硬件连接：
 *   VIB+ = LDO3 输出 VDD33_VOUT (3.3V)
 *   VIB- = PA20 PWM 控制 MOS 管
 *
 * 驱动逻辑：
 *   1. 开启 PMU LDO3 电源
 *   2. PA20 配置为 PWM 输出
 *   3. 占空比控制震动强度
 *
 * Contest 2026 Team 307
 ****************************************************************************/

#ifndef __VIBRATE_H
#define __VIBRATE_H

#include <stdint.h>

/****************************************************************************
 * 公共 API
 ****************************************************************************/

/**
 * 初始化振动马达
 * - 开启 PMU LDO3 电源
 * - 配置 PA20 为 PWM 输出
 * @return 0 成功，-1 失败
 */

int vibrate_init(void);

/**
 * 设置震动强度
 *
 * @param strength 强度 0-100（0=停，100=最大）
 */

void vibrate_set_strength(uint8_t strength);

/**
 * 震动指定时长（阻塞）
 *
 * @param ms       震动时长（毫秒）
 * @param strength 强度 0-100
 */

void vibrate_pulse(uint32_t ms, uint8_t strength);

/**
 * 停止震动
 */

void vibrate_stop(void);

/**
 * 反初始化（关闭 PWM，关闭 LDO3）
 */

void vibrate_deinit(void);

/****************************************************************************
 * 测试函数
 ****************************************************************************/

/**
 * 上电测试：强震300ms → 停200ms → 弱震300ms
 */

void vibrate_test_poweron(void);

#endif /* __VIBRATE_H */
