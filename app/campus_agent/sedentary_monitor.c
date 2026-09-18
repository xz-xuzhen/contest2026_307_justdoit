/****************************************************************************
 * sedentary_monitor.c — 久坐监测模块
 *
 * 读取 IMU 加速度，计算方差判断运动状态，累计久坐时间并分级告警
 *
 * Contest 2026 Team 307
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <pthread.h>

#include <nuttx/sensors/lsm6dsl.h>
#include "sedentary_monitor.h"

/****************************************************************************
 * 配置参数
 ****************************************************************************/

#define CHECK_INTERVAL_SEC    30    /* 检查间隔：30 秒 */
#define SAMPLE_COUNT          3     /* 每次采样次数 */
#define SAMPLE_INTERVAL_MS    200   /* 采样间隔 200ms */
#define MOTION_THRESHOLD      500.0 /* 加速度方差阈值：低于此值判定为静止 */
#define ALERT_LIGHT_MIN       2     /* 轻提醒：2 分钟（演示用） */
#define ALERT_STRONG_MIN      4     /* 强提醒：4 分钟（演示用） */

/****************************************************************************
 * 内部状态
 ****************************************************************************/

static volatile int g_running = 0;
static pthread_t g_thread;
static sedentary_state_t g_state;

/****************************************************************************
 * 读取 IMU 数据
 ****************************************************************************/

static int read_imu(imu_data_t *data)
{
  int fd = open("/dev/lsm6dsl0", O_RDONLY);
  if (fd < 0)
    {
      return -1;
    }

  int ret = ioctl(fd, SNIOC_START, 0);
  if (ret < 0)
    {
      close(fd);
      return -1;
    }

  struct lsm6dsl_sensor_data_s raw;
  ret = ioctl(fd, SNIOC_LSM6DSLSENSORREAD,
              (unsigned long)&raw);
  close(fd);

  if (ret < 0)
    {
      return -1;
    }

  data->accel_x = raw.x_data;
  data->accel_y = raw.y_data;
  data->accel_z = raw.z_data;
  data->gyro_x  = raw.g_x_data;
  data->gyro_y  = raw.g_y_data;
  data->gyro_z  = raw.g_z_data;
  data->temperature = raw.temperature;

  return 0;
}

/****************************************************************************
 * 计算方差
 ****************************************************************************/

static float calc_variance(float *values, int count)
{
  if (count <= 1)
    return 0.0;

  float sum = 0.0;
  for (int i = 0; i < count; i++)
    {
      sum += values[i];
    }

  float mean = sum / count;
  float var_sum = 0.0;

  for (int i = 0; i < count; i++)
    {
      float diff = values[i] - mean;
      var_sum += diff * diff;
    }

  return var_sum / (count - 1);
}

/****************************************************************************
 * 采样并判断运动状态
 ****************************************************************************/

static int sample_motion(float *out_variance)
{
  float accel_magnitudes[SAMPLE_COUNT];

  for (int i = 0; i < SAMPLE_COUNT; i++)
    {
      imu_data_t data;
      if (read_imu(&data) < 0)
        {
          /* 读取失败，假设静止 */

          accel_magnitudes[i] = 0.0;
        }
      else
        {
          /* 计算加速度向量模长 */

          float ax = (float)data.accel_x;
          float ay = (float)data.accel_y;
          float az = (float)data.accel_z;
          accel_magnitudes[i] = sqrtf(ax * ax + ay * ay + az * az);
        }

      usleep(SAMPLE_INTERVAL_MS * 1000);
    }

  *out_variance = calc_variance(accel_magnitudes, SAMPLE_COUNT);
  return (*out_variance >= MOTION_THRESHOLD) ? 1 : 0;
}

/****************************************************************************
 * 震动控制（使用 vibrate 模块）
 ****************************************************************************/

#include "vibrate.h"

/* 轻提醒：单次长震 1500ms，确保马达转起来 */

static void vibrate_light(void)
{
  printf("[sedentary] Vibrate: light (1500ms)\n");
  vibrate_pulse(1500, 100);
}

/* 强提醒：三次长震，每次 1000ms */

static void vibrate_strong(void)
{
  printf("[sedentary] Vibrate: strong (3x1000ms)\n");
  for (int i = 0; i < 3; i++)
    {
      vibrate_pulse(1000, 100);
      if (i < 2) usleep(500000);
    }
}

/****************************************************************************
 * 弹窗提醒（当前用 printf，后续接入 LVGL）
 ****************************************************************************/

static void popup_sedentary(int minutes)
{
  printf("\n");
  printf("========================================\n");
  printf("  SEDENTARY ALERT\n");
  printf("  You have been sitting for %d minutes\n", minutes);
  printf("  Time to stand up and move!\n");
  printf("========================================\n\n");
}

/****************************************************************************
 * 监测线程
 ****************************************************************************/

static void *sedentary_thread(void *arg)
{
  (void)arg;

  printf("[sedentary] Monitor started (check every %ds)\n",
         CHECK_INTERVAL_SEC);

  memset(&g_state, 0, sizeof(g_state));

  int check_count = 0;

  while (g_running)
    {
      /* 采样判断运动状态 */

      float variance;
      int is_moving = sample_motion(&variance);

      g_state.accel_variance = variance;
      g_state.is_moving = is_moving;

      check_count++;

      printf("[sedentary] #%d: var=%.1f %s (sitting=%dmin)\n",
             check_count, variance,
             is_moving ? "MOVING" : "STILL",
             g_state.sitting_minutes);

      if (is_moving)
        {
          /* 检测到运动，重置久坐时间 */

          if (g_state.sitting_minutes > 0)
            {
              printf("[sedentary] Motion detected, reset timer\n");
            }

          g_state.sitting_minutes = 0;
          g_state.alert_level = SEDENTARY_ALERT_NONE;
          g_state.last_alert_min = 0;
        }
      else
        {
          /* 静止，累计久坐时间（每次检查 +0.5 分钟） */

          g_state.sitting_minutes += 1;

          /* 判断告警级别 */

          if (g_state.sitting_minutes >= ALERT_STRONG_MIN)
            {
              g_state.alert_level = SEDENTARY_ALERT_STRONG;

              /* 每 15 分钟触发一次强提醒 */

              if (g_state.sitting_minutes - g_state.last_alert_min >= 15)
                {
                  vibrate_strong();
                  popup_sedentary(g_state.sitting_minutes);
                  g_state.last_alert_min = g_state.sitting_minutes;
                }
            }
          else if (g_state.sitting_minutes >= ALERT_LIGHT_MIN)
            {
              g_state.alert_level = SEDENTARY_ALERT_LIGHT;

              /* 只触发一次轻提醒 */

              if (g_state.last_alert_min < ALERT_LIGHT_MIN)
                {
                  vibrate_light();
                  popup_sedentary(g_state.sitting_minutes);
                  g_state.last_alert_min = g_state.sitting_minutes;
                }
            }
        }

      /* 等待下次检查 */

      for (int i = 0; i < CHECK_INTERVAL_SEC && g_running; i++)
        {
          sleep(1);
        }
    }

  printf("[sedentary] Monitor stopped\n");
  return NULL;
}

/****************************************************************************
 * 公共 API
 ****************************************************************************/

int sedentary_monitor_init(void)
{
  memset(&g_state, 0, sizeof(g_state));

  /* 测试 IMU 是否可用 */

  imu_data_t data;
  if (read_imu(&data) < 0)
    {
      printf("[sedentary] WARNING: IMU not available\n");
      return -1;
    }

  printf("[sedentary] IMU OK (accel: %d %d %d)\n",
         data.accel_x, data.accel_y, data.accel_z);
  return 0;
}

int sedentary_monitor_start(void)
{
  if (g_running)
    {
      printf("[sedentary] Already running\n");
      return -1;
    }

  if (sedentary_monitor_init() < 0)
    {
      return -1;
    }

  g_running = 1;
  int ret = pthread_create(&g_thread, NULL, sedentary_thread, NULL);
  if (ret != 0)
    {
      printf("[sedentary] ERROR: pthread_create failed: %d\n", ret);
      g_running = 0;
      return -1;
    }

  return 0;
}

void sedentary_monitor_stop(void)
{
  if (!g_running)
    return;

  g_running = 0;
  pthread_join(g_thread, NULL);
}

int sedentary_monitor_is_running(void)
{
  return g_running;
}

const sedentary_state_t *sedentary_get_state(void)
{
  return &g_state;
}

void sedentary_reset(void)
{
  g_state.sitting_minutes = 0;
  g_state.alert_level = SEDENTARY_ALERT_NONE;
  g_state.last_alert_min = 0;
  printf("[sedentary] State reset\n");
}

int sedentary_test_imu(void)
{
  imu_data_t data;

  printf("[sedentary] Reading IMU...\n");

  if (read_imu(&data) < 0)
    {
      printf("[sedentary] ERROR: Cannot read IMU\n");
      return -1;
    }

  printf("[sedentary] Accel: X=%d Y=%d Z=%d\n",
         data.accel_x, data.accel_y, data.accel_z);
  printf("[sedentary] Gyro:  X=%d Y=%d Z=%d\n",
         data.gyro_x, data.gyro_y, data.gyro_z);
  printf("[sedentary] Temp:  %d\n", data.temperature);

  /* 多次采样计算方差 */

  printf("[sedentary] Sampling %d times...\n", SAMPLE_COUNT);
  float variance;
  int is_moving = sample_motion(&variance);

  printf("[sedentary] Variance: %.1f (threshold: %.1f)\n",
         variance, MOTION_THRESHOLD);
  printf("[sedentary] Status: %s\n",
         is_moving ? "MOVING" : "STILL");

  return 0;
}
