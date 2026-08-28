# Test Skill

Test skill that shows a popup notification on the screen.

## When to use
When the user says "test popup", "test skill", "show test", or "测试弹窗".

## How to use
1. Use show_popup tool with title "Test Skill" and content "Test Skill Triggered!"
2. Confirm with user that popup was displayed

## Example
User: "test popup"
→ show_popup {"title": "Test Skill", "content": "Test Skill Triggered!"}
→ "Test popup displayed on screen."

User: "测试弹窗"
→ show_popup {"title": "测试技能", "content": "测试技能已触发！"}
→ "测试弹窗已显示在屏幕上。"
