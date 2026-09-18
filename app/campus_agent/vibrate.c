/****************************************************************************
 * vibrate.c — 振动马达驱动（PA20 GPIO + LDO3 电源）
 *
 * 硬件连接：
 *   VIB+ = LDO3 输出 VDD33_VOUT (3.3V)
 *   VIB- = PA20 控制 MOS 管
 *
 * 当前实现：GPIO 高低电平控制（验证硬件）
 * 后续优化：改为 PWM 占空比控制
 *
 * Contest 2026 Team 307
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include "vibrate.h"

/****************************************************************************
 * 外部函数声明
 ****************************************************************************/

/* BSP GPIO 直接控制函数 */

extern void BSP_GPIO_Set(int pin, int val, int is_porta);

/* SiFli PMU LDO 控制（直接用寄存器值，避免头文件依赖）
 * PMUC_PERI_LDO_EN_VDD33_LDO3_Pos = 16
 */

#define PMU_LDO3_3V3   16

/* 外部函数：开启/关闭外设 LDO */

extern int HAL_PMU_ConfigPeriLdo(int ldo, int en, int wait);

/****************************************************************************
 * 硬件配置
 ****************************************************************************/

#define MOTOR_PIN     20    /* PA20 */
#define MOTOR_PORTA   1     /* Port A */

/****************************************************************************
 * 振动时序参数（毫秒）
 ****************************************************************************/

#define SHORT_MS          150
#define LONG_MS           800
#define TRIPLE_GAP_MS     150
#define STRONG_ROUND_GAP  800

/****************************************************************************
 * 公共 API：初始化
 ****************************************************************************/

int vibrate_init(void)
{
  printf("[vibrate] Init start...\n");

  /* Step 1: 开启 PMU LDO3 电源 (3.3V) */

  printf("[vibrate] Enabling PMU LDO3 (reg=%d)...\n", PMU_LDO3_3V3);
  HAL_PMU_ConfigPeriLdo(PMU_LDO3_3V3, 1, 1);
  printf("[vibrate] LDO3 enabled\n");

  /* Step 2: PA20 初始低电平（马达停） */

  BSP_GPIO_Set(MOTOR_PIN, 0, MOTOR_PORTA);

  printf("[vibrate] Init OK: PA20 GPIO\n");
  return 0;
}

/****************************************************************************
 * 公共 API：设置震动强度（当前为开/关，后续改 PWM）
 ****************************************************************************/

void vibrate_set_strength(uint8_t strength)
{
  if (strength > 0)
    {
      BSP_GPIO_Set(MOTOR_PIN, 1, MOTOR_PORTA);
    }
  else
    {
      BSP_GPIO_Set(MOTOR_PIN, 0, MOTOR_PORTA);
    }
}

/****************************************************************************
 * 公共 API：震动指定时长
 ****************************************************************************/

void vibrate_pulse(uint32_t ms, uint8_t strength)
{
  vibrate_set_strength(strength);
  usleep(ms * 1000);
  vibrate_set_strength(0);
}

/****************************************************************************
 * 公共 API：停止震动
 ****************************************************************************/

void vibrate_stop(void)
{
  BSP_GPIO_Set(MOTOR_PIN, 0, MOTOR_PORTA);
}

/****************************************************************************
 * 公共 API：反初始化
 ****************************************************************************/

void vibrate_deinit(void)
{
  BSP_GPIO_Set(MOTOR_PIN, 0, MOTOR_PORTA);
  printf("[vibrate] Deinitialized\n");
}

/****************************************************************************
 * 测试函数：上电测试
 ****************************************************************************/

void vibrate_test_poweron(void)
{
  printf("[vibrate] Power-on test:\n");

  /* 先测试 GPIO 直接拉高 1 秒 */

  printf("  [0] GPIO HIGH 1s (direct test)...\n");
  BSP_GPIO_Set(MOTOR_PIN, 1, MOTOR_PORTA);
  printf("      PA20 = HIGH, waiting 1s...\n");
  usleep(1000000);
  BSP_GPIO_Set(MOTOR_PIN, 0, MOTOR_PORTA);
  printf("      PA20 = LOW\n");

  usleep(500000);

  /* 强震 300ms */

  printf("  [1] Strong 300ms...\n");
  vibrate_pulse(300, 100);

  usleep(200000);

  /* 弱震 300ms */

  printf("  [2] Weak 300ms...\n");
  vibrate_pulse(300, 30);

  printf("  Done.\n");
}
