# Day 3 测试指南

## 概述

本文档说明如何测试触摸屏和中文字体功能。

## 编译和烧录

### 1. 清理并重新编译

```bash
cd /home/xz/openvela
# 清理旧的编译结果
rm -rf cmake_out/lckfb_huangshan_pi

# 重新配置（使用更新后的 defconfig）
./build.sh vendor/sifli/boards/sf32lb52/lckfb_huangshan_pi/configs/nsh/defconfig

# 编译
./build.sh
```

### 2. 烧录固件

按照正常的烧录流程将固件烧录到黄山派。

## 测试步骤

### 测试 1：触摸屏测试

1. 启动后，在 NSH 终端输入：
   ```
   nsh> touch_test
   ```

2. 触摸屏幕任意位置，应该看到类似输出：
   ```
   === Touch Screen Test ===
   Open /dev/input0...
   Touch device opened successfully.
   Touch the screen to see coordinates (Ctrl+C to exit):

   [1] Touch DOWN: x=120, y=200, pressure=0
   [2] Touch UP: x=120, y=200, pressure=0
   ```

3. 如果没有输出，可能的原因：
   - 触摸驱动未正确初始化
   - I2C 通信问题
   - GPIO 中断配置问题

**调试方法**：
- 检查 `/dev/input0` 是否存在：`ls -la /dev/input*`
- 检查 I2C 设备：`ls -la /dev/i2c*`
- 查看系统日志中的触摸相关错误信息

### 测试 2：中文字体测试

1. 启动后，在 NSH 终端输入：
   ```
   nsh> font_test
   ```

2. 应该看到：
   - 屏幕显示中文测试文字（不是方框）
   - 包含"课程提醒：高等数学"、"地点：教学楼 A302"等

3. 如果显示方框，可能的原因：
   - 字体文件不存在
   - 字体文件路径错误
   - FreeType 初始化失败

**调试方法**：
- 检查字体文件是否存在：`ls -la /etc/data/font/`
- 查看系统日志中的 FreeType 相关信息
- 确认 `CONFIG_LV_USE_FREETYPE=y` 已启用

### 测试 3：Day 2 UI 完整测试

1. 启动后，在 NSH 终端输入：
   ```
   nsh> day2_ui
   ```

2. 应该看到：
   - 主界面显示时钟
   - 三个测试按钮（弹窗测试、课程提醒：高数、课程提醒：嵌入式）
   - 按钮文字是中文（不是方框）
   - 点击按钮应该能触发弹窗或提醒页面

3. 触摸功能测试：
   - 点击"弹窗测试"按钮，应该弹出提示窗口
   - 点击"课程提醒：高数"，应该显示课程提醒页面
   - 点击"课程提醒：嵌入式"，应该显示另一个课程提醒

## 常见问题

### Q1: 触摸屏无响应

**可能原因**：
1. 触摸驱动初始化时序问题
2. I2C 地址不匹配
3. GPIO 中断未正确配置

**解决方案**：
1. 等待几秒后再测试（触摸驱动异步初始化）
2. 检查 FT6146 驱动日志
3. 确认 `CONFIG_TOUCH_IRQ_PIN=41` 正确

### Q2: 中文显示方框

**可能原因**：
1. 字体文件不存在
2. FreeType 未初始化
3. 字体路径错误

**解决方案**：
1. 确认 `/etc/data/font/MiSans-Regular.ttf` 存在
2. 检查 `CONFIG_LV_USE_FREETYPE=y` 是否启用
3. 查看系统日志中的 FreeType 初始化信息

### Q3: 编译错误

**可能原因**：
1. Kconfig 配置未更新
2. 依赖库缺失

**解决方案**：
1. 重新运行 `make menuconfig` 检查配置
2. 确保所有依赖库已安装

## 文件说明

- `touch_test.c` - 触摸屏最小测试程序
- `font_test.c` - 中文字体测试程序
- `ui_common.c` - UI 公共函数（包含 FreeType 字体初始化）
- `day2_ui_main.c` - Day 2 UI 主程序

## 配置说明

在 `defconfig` 中添加的配置：

```
# Day 2 UI Demo
CONFIG_LVX_CONTEST2026_307_DAY2_UI=y
CONFIG_LVX_CONTEST2026_307_DAY2_UI_PROGNAME="day2_ui"

# Touch Test Utility
CONFIG_LVX_CONTEST2026_307_TOUCH_TEST=y
CONFIG_LVX_CONTEST2026_307_TOUCH_TEST_PROGNAME="touch_test"

# Chinese Font Test Utility
CONFIG_LVX_CONTEST2026_307_FONT_TEST=y
CONFIG_LVX_CONTEST2026_307_FONT_TEST_PROGNAME="font_test"
```

## 下一步

测试完成后，可以开始 Day 3 的 ai_agent 框架集成任务。
