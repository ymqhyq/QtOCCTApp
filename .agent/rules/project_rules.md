---
trigger: always_on
---

# 项目规则 (Project Rules)

## 目录与文件维护规范 (Directory & File Maintenance Guidelines)

- **3rpart 文件夹**：
  - 此文件夹下的内容由用户手工维护。
  - 如果发现任何问题，请直接报告给用户，**绝对不要自动修改、删除或覆写**该文件夹下的任何内容。
- **编译选项**：
  - 使用vs2026-x64-v142选项编译

## 自动化调试与桌面使用 (Automation & Desktop Usage)

- **自动化运行放隔离桌面**：自动化调试/回归期间启动的程序（应用实例、QtTest 等）
  一律放到独立桌面上运行，避免窗口弹到用户桌面抢占焦点或遮挡并行工作。
  使用 `scripts/run_in_background_desktop.ps1`。
- **需要用户手动交互时放回用户桌面**：当验证步骤需要用户亲自查看界面、点击或输入时，
  必须把程序启动在用户的交互桌面上，并在启动前明确告知。
- **注意**：DWM 仅合成交互桌面，隔离桌面上 `PrintWindow` 截图会是空白；
  需要视觉证据时优先用进程内截图（如 `QWidget::grab()`）。
