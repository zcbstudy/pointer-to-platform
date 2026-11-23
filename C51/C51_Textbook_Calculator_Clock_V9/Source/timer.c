/*
 * 文件：timer.c（定时器驱动）
 * 参考教材：定时器0方式1，使用 interrupt 1 编写中断服务程序。
 * T0：约500微秒中断一次；20次合成10毫秒，提供给主程序。
 * T1：蜂鸣器发声时才启用，每约250微秒翻转引脚，形成约2kHz方波。
 * 注意：本工程晶振为12MHz、传统12T结构；和教材11.0592MHz初值不同。
 */
#include <REG52.H>
/* P3.0 控制无源蜂鸣器，Timer1中断会反复翻转它。 */
sbit BUZZ_OUT = P3^0;
/* 这个函数定义在 segment.c。 */
void LedScan(void);
void KeyScan(void);
/*
 * Textbook uses TH0=0xFC TL0=0x67 for ~1ms at 11.0592MHz.
 * This project uses 12MHz: 0xFE0C = 500us in 12T Timer0 mode 1.
 * 20 interrupts x 500us = 10ms application tick.
 */
/* pending：还没处理的10ms节拍数；在中断里加1、主程序里取走。 */
static volatile unsigned char pending=0;
/* 初始化Timer0/1的工作方式、初值及中断允许位。 */
void Timer_Init(void)
{
    pending=0;
    /* TMOD=0x11：高4位给T1、低4位给T0；均采用16位方式1。 */
    TMOD=0x11;                 /* T0/T1: mode 1, 16-bit */
    /* 500us：12MHz/12 = 1MHz计数频率；65536-500=65036=0xFE0C。 */
    TH0=0xFE; TL0=0x0C;      /* 500 us @ 12 MHz / 12 */
    /* T1 250us重装值：65536-250=0xFF06。输出每250us翻转一次。 */
    TH1=0xFF; TL1=0x06;      /* buzzer half-period 250 us */
    TF0=0; TF1=0;
    ET0=1; ET1=0; TR0=1; TR1=0; EA=1;
}
/* 从中断共享计数器中一次性读取节拍，关闭总中断避免读写竞争。 */
unsigned char Timer_Take10ms(void)
{
    unsigned char n;
    EA=0; n=pending; pending=0; EA=1;
    return n;
}
/* 发声开始：重新装入T1初值并启用T1中断。 */
void Timer_ToneStart(void)
{
    TR1=0; TH1=0xFF; TL1=0x06;
    TF1=0; ET1=1; TR1=1;
}
/* 发声结束：关闭Timer1，把蜂鸣器引脚拉低。 */
void Timer_ToneStop(void)
{
    TR1=0; ET1=0; BUZZ_OUT=0;
}
/* Book-style interrupt function name kept. */
/* 中断向量1就是T0溢出中断。每次扫描一个数码管位/点阵行。 */
void InterruptTimer0(void) interrupt 1
{
    static unsigned char cnt=0;
    TH0=0xFE; TL0=0x0C;
    /* 快速切换显示，尽量不要在中断中执行长延时。 */
    LedScan();
    if(cnt&1) KeyScan();
    cnt++;
    if(cnt>=20)
    {
        cnt=0;
        if(pending<250) pending++;
    }
}
/* 中断向量3就是T1溢出中断，翻转BUZZ_OUT产生声音。 */
void InterruptTimer1(void) interrupt 3
{
    TH1=0xFF; TL1=0x06;
    BUZZ_OUT=!BUZZ_OUT;
}
