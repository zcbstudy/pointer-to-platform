/*
 * 文件：clock.c（24小时制电子时钟 + 整点蜂鸣提示）
 * 上电后默认为12:00:00，无外部RTC，也没有断电保持功能。
 * 主循环每10ms调用Clock_Task10ms一次，累计100次约为1秒。
 * 操作：A加小时、B加分钟、C秒归零、D开/关整点报时。
 * 例如12:59:59进位为13:00:00时产生一次蜂鸣器提示。
 * 误差取决于晶振和软件执行开销；不能等同于精密时钟。
 */
#include <REG52.H>
void Seg_ShowClock(unsigned char hh, unsigned char mm, unsigned char ss);
void Buzzer_Beep(unsigned char ticks10ms);
/* 24-hour software clock, no external RTC. */
/* 时、分、秒均为无符号8位整数；ten_ms_count累计10ms节拍。
 * chime_enabled=1表示允许整点报时，0表示静音。
 */
static unsigned char hour=12,minute,second,ten_ms_count,chime_enabled=1;
/* 开机时间固定设为12:00:00；该设计无掉电保存能力。 */
void Clock_Init(void)
{
    hour=12; minute=0; second=0; ten_ms_count=0; chime_enabled=1;
}
/* 每调用100次约等于一秒，然后秒→分→时逐级进位。 */
void Clock_Task10ms(void)
{
    ten_ms_count++;
    /* 不足100次就结束本次更新，保证每100个10ms才增加1秒。 */
    if(ten_ms_count<100) return;
    ten_ms_count=0; second++;
    /* 秒在0~59循环；到60时归零，并给分钟加1。 */
    if(second<60) return;
    second=0; minute++;
    /* 分钟到60时归零，小时加1。 */
    if(minute<60) return;
    minute=0; hour++;
    /* 24小时制：23点之后变回0点。 */
    if(hour==24) hour=0;
    if(chime_enabled) Buzzer_Beep(20); /* chime at every full hour */
}
/* 书本键盘A(+)校小时、B(-)校分钟、C(*)归零秒、D(/)切换整点报时。 */
void Clock_Key(unsigned char key)
{
    if(key==0x26) {hour++; if(hour>=24) hour=0;}
    else if(key==0x25) {minute++; if(minute>=60) minute=0;}
    else if(key==0x28) {second=0;ten_ms_count=0;}
    else if(key==0x27) chime_enabled=!chime_enabled;
}
/* 调用数码管模块显示当前时分秒。 */
void Clock_Show(void) {Seg_ShowClock(hour,minute,second);}
