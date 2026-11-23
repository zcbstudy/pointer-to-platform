# C51 V10 — P3.0 有源蜂鸣器低电平驱动修正版

**关键引脚更正：** 对照用户之前成功发声的 Proteus 截图，PNP 基极电阻 R3 实际连接 AT89C51 的 P3.0（物理 10 脚），因此本版本使用 `sbit BUZZ=P3^0`，不再使用 P1.6。P3.0 不与 V10 的 P3.2 模式键、P3.3 校时键冲突。

- P3.0=0：PNP 导通，有源蜂鸣器得电
- P3.0=1：PNP 截止，蜂鸣器关闭
- Timer0 负责持续时间倒计时；Timer1 不使用
- 整点报时 200ms；计算错误 150ms；按键不响
- 保留 V10 原有六位计算器、四则运算、24小时软件时钟、HHMMSS校时、六位数码管位序修正

Keil C51 V9.02 Eval 编译：0 errors / 0 warnings；code=2023 bytes，data=62.6 bytes；Eval 用量 0x061C/0x0800；HEX最高地址 0x0FE0，适配 AT89C51 4KB。

实际蜂鸣器声音仍需在 Proteus 工程中验证。