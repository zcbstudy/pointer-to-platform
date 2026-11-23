/*
 * 文件：buzzer.c（蜂鸣器时长控制）
 * 有声期间由Timer1中断产生方波，本模块只负责计时何时停止。
 * Buzzer_Beep(20)表示20个10ms节拍，即约200ms。
 * 硬件需要无源蜂鸣器（或可被方波驱动的压电元件）。
 */
#include <REG52.H>
/* P3.0 接无源蜂鸣器的控制端。 */
sbit BUZZER_PIN = P3^0;
/* 函数定义在 timer.c。 */
void Timer_ToneStart(void);
void Timer_ToneStop(void);
/* 剩余蜂鸣时长，以10ms为一个计数单位。 */
static unsigned char beep_ticks;
/* 初始化：引脚拉低，剩余时长清零。 */
void Buzzer_Init(void) {BUZZER_PIN=0;beep_ticks=0;}
/* 开始发声。参数不是毫秒，而是10ms节拍数，例如7代表70ms。 */
void Buzzer_Beep(unsigned char ticks10ms)
{
    beep_ticks=ticks10ms;
    Timer_ToneStart();
}
/* 每10ms调用一次。剩余时间归零后通知Timer1停声。 */
void Buzzer_Task10ms(void)
{
    if(beep_ticks)
    {
        beep_ticks--;
        if(beep_ticks==0) Timer_ToneStop();
    }
}
