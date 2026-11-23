/*
 * matrix.c - 8x8点阵固定图案。
 * 计算器时显示“+”，电子时钟显示钟面Logo。
 * 一行一个字节，bit7是最左边的LED；1表示需要点亮。
 */
#include <REG52.H>
static unsigned char code Logo[2][8] = {
    {0x00,0x10,0x10,0x7C,0x10,0x10,0x00,0x00}, /* + */
    {0x3C,0x42,0x99,0x91,0x89,0x42,0x3C,0x00}  /* 时钟 */
};
static volatile unsigned char MatrixBuff[8];

void Matrix_SetMode(unsigned char mode)
{
    unsigned char i;
    if(mode>=2) mode=0;
    for(i=0;i<8;i++) MatrixBuff[i]=Logo[mode][i];
}

unsigned char Matrix_GetRow(unsigned char row)
{
    return MatrixBuff[row&7];
}
