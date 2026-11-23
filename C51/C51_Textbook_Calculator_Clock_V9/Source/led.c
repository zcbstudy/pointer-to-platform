/*
 * 文件：led.c（5个独立LED状态指示）
 * 接线约定：LED低电平点亮，P3.1～P3.5分别驱动5个LED。
 * 正常模式：显示当前功能的标识灯，叠加一个流水灯效果。
 * 计算器错误：所有灯以约100ms步进交替亮/灭。
 */
#include <REG52.H>
/* 5 个 LED，低电平点亮；其中前2个代表计算器、时钟模式。 */
sbit LED1 = P3^1;
sbit LED2 = P3^2;
sbit LED3 = P3^3;
sbit LED4 = P3^4;
sbit LED5 = P3^5;
/* Discrete P3 LEDs, low level lights LED. */
/* bits的第0~4位代表LED1~LED5。位值1表示要亮。
 * 因硬件低电平点亮，因此写给引脚前用!取反。
 */
static void Led_Write(unsigned char bits)
{
    LED1=!(bits&1);LED2=!(bits&2);LED3=!(bits&4);
    LED4=!(bits&8);LED5=!(bits&16);
}
/* 上电后先关闭5盏灯。 */
void Led_Init(void) {Led_Write(0);}
/* 每10ms被调用一次：count到10约100ms，流水灯移动一格。 */
void Led_Task10ms(unsigned char mode,unsigned char alarm)
{
    static unsigned char count=0,step=0;
    count++;
    if(count>=10)
    {
        count=0;
        step++;
        if(step>=5) step=0;
    }
    if(alarm) Led_Write((step&1)?31:0); /* flashing error */
    else Led_Write((unsigned char)((1U<<step)|(1U<<mode))); /* running light + mode */
}
