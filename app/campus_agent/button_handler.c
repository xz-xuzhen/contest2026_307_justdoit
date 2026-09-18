/****************************************************************************
 * button_handler.c — 按键快捷键处理
 *
 * 监听 KEY1/KEY2 按键，支持短按和长按
 *
 * Contest 2026 Team 307
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <nuttx/input/buttons.h>

#include "button_handler.h"

/****************************************************************************
 * 配置
 ****************************************************************************/

#define POLL_INTERVAL_MS    50      /* 轮询间隔 50ms */
#define LONG_PRESS_MS       2000    /* 长按阈值 2 秒 */
#define DEBOUNCE_MS         100     /* 消抖时间 100ms */

/****************************************************************************
 * 内部状态
 ****************************************************************************/

static volatile int g_running = 0;
static pthread_t g_thread;
static button_cb_t g_callback = NULL;

/****************************************************************************
 * 按键监听线程
 ****************************************************************************/

static void *button_thread(void *arg)
{
  (void)arg;

  int fd = open("/dev/buttons", O_RDONLY);
  if (fd < 0)
    {
      printf("[btn] Cannot open /dev/buttons\n");
      return NULL;
    }

  printf("[btn] Button handler started\n");

  /* 上一次按键状态 */

  int key1_was_pressed = 0;
  int key2_was_pressed = 0;

  /* 按下时间戳（用于判断长按） */

  uint32_t key1_press_time = 0;
  uint32_t key2_press_time = 0;

  /* 消抖计数器 */

  int key1_debounce = 0;
  int key2_debounce = 0;

  uint32_t tick = 0;

  while (g_running)
    {
      /* 读取按键状态（板子只有 KEY2） */

      btn_buttonset_t buttons;
      ssize_t n = read(fd, &buttons, sizeof(buttons));
      if (n != sizeof(buttons))
        {
          usleep(POLL_INTERVAL_MS * 1000);
          continue;
        }

      /* 只有 KEY2 (bit 0)，KEY1 未接按键驱动 */

      int key1_pressed = 0;  /* 无 KEY1 */
      int key2_pressed = (buttons & 1) ? 1 : 0;

      /* ===== KEY1 处理 ===== */

      if (key1_pressed && !key1_was_pressed)
        {
          /* 按下事件 */

          key1_debounce++;
          if (key1_debounce >= (DEBOUNCE_MS / POLL_INTERVAL_MS))
            {
              key1_was_pressed = 1;
              key1_press_time = tick;
              key1_debounce = 0;
            }
        }
      else if (!key1_pressed && key1_was_pressed)
        {
          /* 松开事件 */

          uint32_t press_duration = tick - key1_press_time;
          int is_long = (press_duration >= LONG_PRESS_MS / POLL_INTERVAL_MS)
                        ? 1 : 0;

          if (g_callback)
            {
              g_callback(1, is_long);
            }

          key1_was_pressed = 0;
          key1_debounce = 0;
        }
      else if (!key1_pressed)
        {
          key1_debounce = 0;
        }

      /* ===== KEY2 处理 ===== */

      if (key2_pressed && !key2_was_pressed)
        {
          key2_debounce++;
          if (key2_debounce >= (DEBOUNCE_MS / POLL_INTERVAL_MS))
            {
              key2_was_pressed = 1;
              key2_press_time = tick;
              key2_debounce = 0;
            }
        }
      else if (!key2_pressed && key2_was_pressed)
        {
          uint32_t press_duration = tick - key2_press_time;
          int is_long = (press_duration >= LONG_PRESS_MS / POLL_INTERVAL_MS)
                        ? 1 : 0;

          if (g_callback)
            {
              g_callback(2, is_long);
            }

          key2_was_pressed = 0;
          key2_debounce = 0;
        }
      else if (!key2_pressed)
        {
          key2_debounce = 0;
        }

      tick++;
      usleep(POLL_INTERVAL_MS * 1000);
    }

  close(fd);
  printf("[btn] Button handler stopped\n");
  return NULL;
}

/****************************************************************************
 * 公共 API
 ****************************************************************************/

int button_handler_start(button_cb_t cb)
{
  if (g_running)
    {
      printf("[btn] Already running\n");
      return -1;
    }

  g_callback = cb;
  g_running = 1;

  int ret = pthread_create(&g_thread, NULL, button_thread, NULL);
  if (ret != 0)
    {
      printf("[btn] ERROR: pthread_create failed: %d\n", ret);
      g_running = 0;
      return -1;
    }

  return 0;
}

void button_handler_stop(void)
{
  if (!g_running) return;

  g_running = 0;
  pthread_join(g_thread, NULL);
  g_callback = NULL;
}

int button_handler_is_running(void)
{
  return g_running;
}
