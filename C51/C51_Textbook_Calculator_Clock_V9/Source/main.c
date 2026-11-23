/*
 * main.c - 全工程唯一的 main() 函数。
 * 以教材第8.5节“简易加法计算器”为主线，加上软件电子时钟。
 * 两种模式：计算器（默认）和电子时钟。
 * 短按ESC(*)清除当前计算；长按ESC(*)约1秒切换模式。
 * 所有外部模块函数在本文件直接声明，不创建自己的 .h。
 */
#include <REG52.H>

#define MODE_CALCULATOR 0
#define MODE_CLOCK      1
#define KEY_MODE        0x80

void Timer_Init(void);
unsigned char Timer_Take10ms(void);
void Seg_Clear(void);
void Matrix_SetMode(unsigned char mode);
void Buzzer_Init(void);
void Buzzer_Beep(unsigned char ticks);
void Buzzer_Task10ms(void);
void Led_Init(void);
void Led_Task10ms(unsigned char mode, unsigned char alarm);
void Key_Init(void);
unsigned char KeyDriver(void);
void Calculator_Init(void);
void KeyAction(unsigned char keycode);  /* 教材中的按键动作函数 */
void Calculator_Show(void);
unsigned char Calculator_HasError(void);
void Clock_Init(void);
void Clock_Task10ms(void);
void Clock_Key(unsigned char keycode);
void Clock_Show(void);

static unsigned char WorkMode = MODE_CALCULATOR;

void main(void)
{
    unsigned char ticks, keycode;

    /* 两组显示器先关闭，再初始化各个模块。
     * 注意：P3.6/P3.7 为两路低电平有效的显示组使能控制线。 */
    P3=0xFE;
    P0=0xFF;
    P2=0xFF;

    Key_Init();
    Buzzer_Init();
    Led_Init();
    Calculator_Init();
    Clock_Init();
    Seg_Clear();
    Matrix_SetMode(WorkMode);
    Timer_Init();

    while(1)
    {
        /* 仅在10ms节拍到达时处理按键和应用逻辑，不阻塞显示扫描。 */
        ticks=Timer_Take10ms();
        while(ticks != 0)
        {
            ticks--;
            keycode=KeyDriver();
            if(keycode == KEY_MODE)
            {
                /* 长按ESC：计算器<->电子时钟 */
                WorkMode = (unsigned char)(WorkMode ^ 1);
                Matrix_SetMode(WorkMode);
                Buzzer_Beep(6);
            }
            else if(keycode != 0)
            {
                Buzzer_Beep(3);
                if(WorkMode == MODE_CALCULATOR)
                    KeyAction(keycode);
                else
                    Clock_Key(keycode);
            }

            /* 即使切换到计算器，电子时钟也继续自动走时。 */
            Clock_Task10ms();
            Buzzer_Task10ms();
            Led_Task10ms(WorkMode, Calculator_HasError());

            if(WorkMode == MODE_CALCULATOR)
                Calculator_Show();
            else
                Clock_Show();
        }
    }
}
