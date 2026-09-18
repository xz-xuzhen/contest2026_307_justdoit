/****************************************************************************
 * sedentary_monitor.h — 久坐监测模块
 *
 * 功能：
 *   - 每 5 分钟读取 IMU 加速度
 *   - 计算方差判断是否在运动
 *   - 累计久坐时间，分级告警
 *     30-59 分钟：轻提醒（单次长震）
 *     ≥60 分钟：强提醒（两轮三短震 + 弹窗）
 *
 * Contest 2026 Team 307
 ****************************************************************************/

#ifndef __SEDENTARY_MONITOR_H
#define __SEDENTARY_MONITOR_H

/****************************************************************************
 * 告警级别
 ****************************************************************************/

#define SEDENTARY_ALERT_NONE    0   /* 正常 */
#define SEDENTARY_ALERT_LIGHT   1   /* 轻提醒 (30-59分钟) */
#define SEDENTARY_ALERT_STRONG  2   /* 强提醒 (≥60分钟) */

/****************************************************************************
 * IMU 数据
 ****************************************************************************/

typedef struct {
  int16_t accel_x;      /* 加速度 X (原始值) */
  int16_t accel_y;      /* 加速度 Y */
  int16_t accel_z;      /* 加速度 Z */
  int16_t gyro_x;       /* 陀螺仪 X */
  int16_t gyro_y;       /* 陀螺仪 Y */
  int16_t gyro_z;       /* 陀螺仪 Z */
  int16_t temperature;  /* 温度 */
} imu_data_t;

/****************************************************************************
 * 久坐状态
 ****************************************************************************/

typedef struct {
  int sitting_minutes;      /* 累计久坐分钟数 */
  int alert_level;          /* 当前告警级别 */
  int last_alert_min;       /* 上次告警时的久坐分钟数 */
  float accel_variance;     /* 最近一次加速度方差 */
  int is_moving;            /* 是否在运动 */
} sedentary_state_t;

/****************************************************************************
 * 公共 API
 ****************************************************************************/

/**
 * 初始化久坐监测
 * @return 0 成功, -1 失败
 */

int sedentary_monitor_init(void);

/**
 * 启动久坐监测（创建后台线程）
 * @return 0 成功, -1 失败
 */

int sedentary_monitor_start(void);

/**
 * 停止久坐监测
 */

void sedentary_monitor_stop(void);

/**
 * 查询监测状态
 * @return 1 运行中, 0 已停止
 */

int sedentary_monitor_is_running(void);

/**
 * 获取当前久坐状态
 */

const sedentary_state_t *sedentary_get_state(void);

/**
 * 重置久坐状态（用户活动后调用）
 */

void sedentary_reset(void);

/**
 * 测试 IMU 读取（调试用）
 * 读取一次 IMU 数据并打印
 */

int sedentary_test_imu(void);

#endif /* __SEDENTARY_MONITOR_H */
