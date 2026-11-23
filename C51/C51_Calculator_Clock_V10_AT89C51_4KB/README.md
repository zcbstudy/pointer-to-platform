# AT89C51 V10 — 四则运算与电子时钟（Keil Eval 4KB适配版）

基于用户提供的 V10 源码修改，保留六位共阳数码管显示、4×4矩阵键盘、六位数加减乘除（整数除法）、24小时电子时钟、P3.2模式切换、P3.3六位HHMMSS校时、P1.6蜂鸣器。**蜂鸣器仅用于整点报时 200ms 和计算器错误报警 150ms**，其他按键静音。

- 芯片：AT89C51，11.0592MHz（传统12T）
- 显示位序：`LedScan(): P0=LedBuff[5-i]`，适配当前物理从左到右排列
- Timer0：每约1ms扫描显示和键盘，10ms按钮采样和时钟计时
- Timer1：约2kHz方波，驱动PNP高端开关及无源蜂鸣器
- 构建环境：Keil C51 V9.02 Eval，`OPTIMIZE(9,SIZE)`，`RAMSIZE(128)`
- 真实编译结果：0 warnings, 0 errors；code=2057B，data=62.6B；评估版计入 0x063E/0x0800 (78%)
- **HEX最高程序地址 0x0FFF**，不超过AT89C51的4KB Flash，且已经用Keil OH51生成真实Intel HEX。
- 内部为符合4KB做了冗余键盘状态缓存精简和安全的乘法溢出判定重构；可见功能全部保留。

在 Proteus 双击AT89C51，将 Program File 指向 `Build/V10_Final.hex`，Clock Frequency 设为11.0592MHz。其他连线不变。未宣称已经完成Proteus全功能实机仿真。程序地址刚好达到0x0FFF，若继续增加功能需重新验证容量。
