/*
 * 文件：segment.c（8位数码管 + 点阵显示分时扫描）
 * 教材中的ShowNumber()与LedScan()函数名称继续使用。
 * 教材同名变量：LedChar[16] 数码管字形表；LedBuff[8] 数码管缓冲区。
 * 教材原例是6位，本项目扩展为8位，使用低电平点亮的共阳极段码。
 * 本工程的接线假设：P0输出段/列数据，P2输出位/行选择。
 * 注意：软件控制两个银行使能，不代表能省略实际的晶体管隔离电路。
 */
#include <REG52.H>
/* 两种显示模块分时复用总线。两组使能低电平有效。 */
sbit SEG_ENABLE = P3^6;
sbit MATRIX_ENABLE = P3^7;
/* 这个函数定义在 matrix.c。 */
unsigned char Matrix_GetRow(unsigned char row);
/* Textbook font: common-anode, 0 lights a segment; bits are dp,g,f,e,d,c,b,a. */
/* code表示常量放入程序存储器Flash，不占用普通RAM。
 * LedChar[0]=0xC0表示显示数字0；bit0~bit7顺序为a b c d e f g dp。
 * 共阳极数码管段码：位0点亮，位1熄灭。
 */
unsigned char code LedChar[16]={
    0xC0,0xF9,0xA4,0xB0,0x99,0x92,0x82,0xF8,
    0x80,0x90,0x88,0x83,0xC6,0xA1,0x86,0x8E
};
/* Textbook calls this the display buffer; here expanded to 8 digits. */
/* LedBuff：显示缓冲区，8个元素分别对应8个数码管位。
 * 中断读取它；主程序修改它，所以使用volatile。0xFF表示全灭。
 */
volatile unsigned char LedBuff[8]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

/* 将数码管8个位置全部设置为不发光。 */
void Seg_Clear(void)
{
    unsigned char i;
    for(i=0;i<8;i++) LedBuff[i]=0xFF;  /* 0xFF = all segments off */
}
/* 错误时在中间3个位置显示Err，其余位置为空白。 */
void Seg_ShowError(void)
{
    Seg_Clear();
    LedBuff[2]=0x86; /* E */
    LedBuff[3]=0xAF; /* r */
    LedBuff[4]=0xAF; /* r */
}
/* 将一个有符号整数转成十进制数字，由右向左显示。
 * %10取得个位，/10去掉个位；循环即可逐位分离。
 */
void ShowNumber(signed int value)
{
    unsigned char i=7;
    unsigned int v;
    Seg_Clear();
    if(value<0) v=(unsigned int)(-value);
    else v=(unsigned int)value;
    do
    {
        LedBuff[i]=LedChar[v%10];
        v=v/10;
        if(i==0) break;
        i--;
    } while(v!=0);
    if(value<0) LedBuff[i]=0xBF; /* '-' */
}
/* 电子时钟：左两位空白，中间6位显示HH.MM.SS，dp充当分隔点。 */
void Seg_ShowClock(unsigned char hh,unsigned char mm,unsigned char ss)
{
    LedBuff[0]=0xFF; LedBuff[1]=0xFF;
    LedBuff[2]=LedChar[hh/10];
    LedBuff[3]=LedChar[hh%10]&0x7F; /* decimal point separator */
    LedBuff[4]=LedChar[mm/10];
    LedBuff[5]=LedChar[mm%10]&0x7F;
    LedBuff[6]=LedChar[ss/10];
    LedBuff[7]=LedChar[ss%10];
}
/*
 * 16 steps: 8 digit positions + 8 matrix rows.
 * Both banks must be blanked before changing P0/P2, to prevent ghosting.
 * Hardware MUST isolate the two display banks. P3.6/7 are control only.
 */
/* 每500us从定时器中断调用一次。共16个阶段：
 * 0~7：逐位扫描数码管；8~15：逐行扫描8×8点阵。
 * 两个显示模块分时共享P0和P2。一定先关闭使能再改数据，防止重影。
 */
void LedScan(void)
{
    static unsigned char i=0;
    unsigned char row=i&7;
    /* 使能线是低有效：1代表暂时关闭显示。 */
    SEG_ENABLE=1; MATRIX_ENABLE=1; /* both disabled */
    P2=0xFF; P0=0xFF;              /* eliminate ghosting */
    if(i<8)
    {
        P0=LedBuff[row];
        /* 移位1U<<row得到“只有第row位为1”的掩码，取反后该位为0。 */
        P2=(unsigned char)~(1U<<row);
        SEG_ENABLE=0;
    }
    else
    {
        /* 内部点阵字模用1表示亮；实际总线低电平亮，所以逐位取反。 */
        P0=(unsigned char)~Matrix_GetRow(row); /* matrix data 1 = lit */
        P2=(unsigned char)~(1U<<row);
        MATRIX_ENABLE=0;
    }
    i++;
    if(i>=16) i=0;
}
