/*
 * key.c - 4x4矩阵按键扫描和按键消抖。
 * 函数名称尽量沿用教材：KeyCodeMap、KeySta、KeyScan、KeyDriver。
 * 本项目不用教材原开发板的ADDR/ENLED译码接口。
 * 接线：P1.0~P1.3接矩阵键盘4行；P1.4~P1.7接4列。
 * 内部用连续4次相同扫描值确认稳定（相隔4ms，约16ms消抖）。
 * 所有键值对应教材8.5节，ESC即4x4键盘上的*。
 */
#include <REG52.H>
#define KEY_MODE 0x80
#define KEY_ESC  0x1B

/* 教材定义的键码排列；可保留并与书上逐行对照。 */
unsigned char code KeyCodeMap[4][4] = {
    {0x31, 0x32, 0x33, 0x26}, /* 1 2 3 + */
    {0x34, 0x35, 0x36, 0x25}, /* 4 5 6 - */
    {0x37, 0x38, 0x39, 0x28}, /* 7 8 9 *乘 */
    {0x30, 0x1B, 0x0D, 0x27}  /* 0 ESC 回车 /除 */
};

/* KeySta=0表示确认按下，1表示确认松开。 */
volatile unsigned char KeySta[4][4];
static unsigned char keybuf[4][4];
static unsigned char backup[4][4];
static unsigned char keyout;
static unsigned char esc_hold;
static unsigned char esc_long;

void Key_Init(void)
{
    unsigned char i,j;
    keyout=0;
    esc_hold=0;
    esc_long=0;
    P1=0xFE;  /* 行0输出低电平，另外3行及4列高电平 */
    for(i=0;i<4;i++)
    {
        for(j=0;j<4;j++)
        {
            KeySta[i][j]=1;
            keybuf[i][j]=0xFF;
            backup[i][j]=1;
        }
    }
}

/* 教材式：每1ms由定时器中断调用一次，扫描当前一行。 */
void KeyScan(void)
{
    unsigned char i, samples;
    /* P1高4位作为输入；keyout指出当前被拉低的行。 */
    samples=P1;
    for(i=0;i<4;i++)
    {
        keybuf[keyout][i]=(unsigned char)((keybuf[keyout][i]<<1)
                             | ((samples>>(i+4))&1));
        if((keybuf[keyout][i]&0x0F)==0x00)
            KeySta[keyout][i]=0;
        else if((keybuf[keyout][i]&0x0F)==0x0F)
            KeySta[keyout][i]=1;
    }
    keyout=(unsigned char)((keyout+1)&0x03);
    /* 行选1表示不选中；只把下一行对应端口输出0。 */
    P1=(unsigned char)(0xFF ^ (1<<keyout));
}

/* 教材式按键驱动：比较当前KeySta与上次backup，只处理一次按下。
 * 为同时支持清零和模式切换，ESC用短按/长按区别：
 * 短按ESC返回0x1B，长按约1秒返回0x80。
 * 本函数需要每10ms调用一次。
 */
unsigned char KeyDriver(void)
{
    unsigned char i,j;
    unsigned char keycode=0;

    if(KeySta[3][1]==0)       /* ESC是第4行第2列 */
    {
        if(esc_hold<100) esc_hold++;
        if(esc_hold>=100 && esc_long==0)
        {
            esc_long=1;
            keycode=KEY_MODE;
        }
    }
    else
    {
        if(backup[3][1]==0 && esc_long==0)
            keycode=KEY_ESC; /* 短按ESC松开时清零 */
        esc_hold=0;
        esc_long=0;
    }

    /* 其他键在刚刚稳定按下时上报；按住不会重复输入。 */
    for(i=0;i<4;i++)
    {
        for(j=0;j<4;j++)
        {
            if(!(i==3 && j==1) && backup[i][j]!=KeySta[i][j]
               && KeySta[i][j]==0 && keycode==0)
            {
                keycode=KeyCodeMap[i][j];
            }
            backup[i][j]=KeySta[i][j];
        }
    }
    return keycode;
}
