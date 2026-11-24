#include <REG52.H>

/*
============================================================================
 课程设计：AT89C51 六位数码管计算器与电子时钟（V10终稿）
============================================================================
【运行环境】
  芯片 AT89C51；晶振 11.0592 MHz；传统12T单片机。
  使用Keil C51编译、Proteus仿真；仅依赖芯片头文件REG52.H。

【硬件接口】
  P0.0～P0.7：数码管A、B、C、D、E、F、G、DP段码输出。
  P1.0～P1.2：74HC138位选输入A、B、C。
  P1.3：74HC138高有效使能G1；P1.4：低有效使能/G2A。
         74HC138另一个低有效使能/G2B接地。
  P1.6：有源蜂鸣器控制，经基极电阻连接PNP三极管。
  P2.0～P2.3：4×4矩阵键盘行输出。
  P2.4～P2.7：4×4矩阵键盘列输入。
  P3.2：独立模式切换键，低电平按下。
  P3.3：独立时间设置键，低电平按下。

【功能说明】
  1. 计算器：六位整数的加减乘除，除法仅保留整数部分。
  2. 电子时钟：24小时制，六位数码管显示HH.MM.SS。
  3. P3.2切换时钟与计算器；P3.3进入HHMMSS六位校时。
  4. 有源蜂鸣器仅整点响约200ms、计算错误响约150ms。
     普通按键、正常运算、模式切换时不发声。

【定时器分工】
  Timer0：约1ms中断，负责数码管动态扫描、矩阵键盘扫描、
          时钟计时、鸣叫时长倒计时。
  Timer1：本终稿不使用，蜂鸣器采用直流供电使能方式。

【阅读建议】
  按：main -> InterruptTimer0 -> LedScan / KeyScan -> KeyDriver
    -> KeyAction -> CalcApply / ShowClock 的顺序理解。
  本文件仅完善注释，不改动任何C语言可执行语句。
============================================================================
*/

/* [一] 数码管位选引脚。
   74HC138用三位地址选择六个数码管，地址范围000～101。
   共阳极数码管的段码是低电平有效；P0=0xFF可消隐所有段。 */
sbit ADDR0=P1^0;
sbit ADDR1=P1^1;
sbit ADDR2=P1^2;
sbit ADDR3=P1^3;
sbit ENLED=P1^4;
/* [二] 矩阵键盘输入、输出引脚。
   每次把一行置0，其他行置1；读取四列P2.4～P2.7的电平。
   某列变成0，表示当前选中行的该列按键被按下。 */
sbit KEY_IN_1=P2^4;
sbit KEY_IN_2=P2^5;
sbit KEY_IN_3=P2^6;
sbit KEY_IN_4=P2^7;
sbit KEY_OUT_1=P2^3;
sbit KEY_OUT_2=P2^2;
sbit KEY_OUT_3=P2^1;
sbit KEY_OUT_4=P2^0;
/* [三] 独立按键与有源蜂鸣器。
   两个独立按键低电平有效，松开后才能再次触发。
   BUZZ采用PNP高端开关：P1.6=0时PNP导通，P1.6=1时截止。
   按当前电路：PNP发射极接+5V，集电极接蜂鸣器正极，
   蜂鸣器负极接GND，基极经电阻接P1.6。 */
sbit MODE_KEY=P3^2;
sbit SET_KEY=P3^3;
sbit BUZZ=P1^6; /* PNP high-side drive; low on, high off: active buzzer */

/* [四] 数码管段码表及显示缓冲。
   code关键字：把常量表保存在程序存储器（Flash），节约片内RAM。
   LedChar[0]～LedChar[9]分别对应共阳极数码管数字0～9。
   LedBuff[0]逻辑上为个位，LedBuff[5]逻辑上为最高位；
   LedScan负责把这六位映射到实际从左到右的数码管。 */
unsigned char code LedChar[10]={
  0xC0,0xF9,0xA4,0xB0,0x99,0x92,0x82,0xF8,0x80,0x90
};
unsigned char LedBuff[6];
/* [五] 4×4键盘的行列键码表，按教材原顺序排列。
   0x30～0x39是ASCII数字'0'～'9'。
   0x26、0x25、0x28、0x27是四个运算符键对应的内部键码，
   依次表示加、减、乘、除；在时钟模式下前3个另有用途。
   0x1B表示ESC清零/取消，0x0D表示ENTER确认。 */
unsigned char code KeyCodeMap[4][4]={
 {0x31,0x32,0x33,0x26}, {0x34,0x35,0x36,0x25},
 {0x37,0x38,0x39,0x28}, {0x30,0x1B,0x0D,0x27}
};

/* [六] 时钟和模式状态。
   mode=0时显示时钟，mode=1时显示计算器。
   hour、minute、second分别是时、分、秒。
   ms保存10ms节拍数，累加到100后约过1秒。
   clock_changed通知主循环刷新时钟；tick10通知检查独立按键。 */
static unsigned char mode;  /* 0=clock, 1=calculator */
static unsigned char hour,minute,second;
static unsigned char ms; /* 10ms单位，100次为一秒 */
static bit clock_changed;
static bit tick10;
/* 校时键 P3.3 按下进入 HHMMSS 输入；六位输入自动校验保存 */
static bit setting;
/* [七] HHMMSS校时状态。
   setting=1表示正在设置时间；setdigits表示已经输入0～6位。
   setH、setM、setS分别累积小时、分钟、秒，每项输入2位。
   6位输入后检查小时0～23、分钟与秒0～59，合法则保存。 */
static unsigned char setdigits;
static unsigned char setH,setM,setS;
/* 有源蜂鸣器：P1.6低电平持续导通；T0中断倒计时，T1不用 */
/* [八] 蜂鸣器剩余时长，单位约1ms。
   volatile用于主循环与Timer0中断共享数据时防止不当缓存。
   时间到0后中断把P1.6置高，自动关闭有源蜂鸣器。 */
static volatile unsigned char beep_ms;
/* [九] 矩阵键盘消抖信息按“行”分别保存。
   keylast：上次读到的4列状态；keystable：已确认稳定的状态；
   keycount：相同读数持续次数；pendingKey：等待处理的按键键码。 */
static unsigned char keylast[4],keystable[4],keycount[4];
static volatile unsigned char pendingKey; /* KeyScan -> KeyDriver */
/* [十] 计算器的内部状态。
   op：0无运算，1加，2减，3乘，4除。
   stage：0第一个操作数输入期，1第二个操作数输入期，2已显示结果。
   has_digit：本轮input是否已经输入数字。
   negative：结果是否为负；result始终保存非负幅值。
   calc_error：错误状态，需ESC或进入新计算清除。
   result：累计结果；input：当前正输入的操作数。 */
static unsigned char op;    /* 0 none, 1+, 2-, 3*, 4/ */
static unsigned char stage; /* 0 first, 1 second, 2 result */
static bit has_digit,negative,calc_error;
static unsigned long result,input;

void LedScan(void);
void KeyScan(void);
void KeyDriver(void);
void KeyAction(unsigned char keycode);
void ShowNumber(unsigned long num);
/* 复用旧Proteus成功仿真方式：有源蜂鸣器低电平导通，无需输出方波 */
/* [十一] Beep：启动一次蜂鸣提示。
   参数duration是发声时间，单位约ms。
   P1.6输出0 -> PNP导通 -> 有源蜂鸣器得电发声。
   本函数不执行延时：Timer0中断负责倒计时，避免阻塞显示。
   当前程序只有整点进位和CalcError()会调用Beep()。 */
void Beep(unsigned char duration)
{
    beep_ms=duration;
    if(duration)BUZZ=0; /* PNP基极拉低，蜂鸣器持续得电 */
    else BUZZ=1;
}


/* Buffer contains segment codes; lowest digit is LedBuff[0]. */
/* [十二] ShowNumber：将非负整数转换为6位数码管段码。
   每次取num%10得到最低位，再除以10继续取下一位。
   所得段码从LedBuff[0]（个位）往高位存放。
   用do-while保证传入0时也会显示一个“0”。
   不足6位时用0xFF消隐前导零。 */
void ShowNumber(unsigned long num)
{
    unsigned char i;
    i=0;
    do {
        LedBuff[i++]=LedChar[(unsigned char)(num%10)];
        num/=10;
    } while(num && i<6);
    while(i<6) LedBuff[i++]=0xFF;
}

/* [十三] ShowError：所有位置显示字母E。
   0x86为共阳数码管E的段码，用于显示EEEEEE错误提示。 */
void ShowError(void)
{
    unsigned char i;
    for(i=0;i<6;i++) LedBuff[i]=0x86; /* EEEEEE */
}

/* [十四] ShowResult：显示计算结果。
   错误时显示EEEEEE；正常时显示result。
   结果为负时最高位显示负号0xBF，因此负数最多显示5位幅值。 */
void ShowResult(void)
{
    if(calc_error)
    {
        ShowError();
        return;
    }
    ShowNumber(result);
    if(negative) LedBuff[5]=0xBF;
}

/* [十五] ShowClock：将时分秒转换为HH.MM.SS。
   LedBuff[5:4]是小时，[3:2]是分钟，[1:0]是秒。
   与0x7F按位与会把DP段拉低，从而点亮对应的小数点。
   时钟在计算器模式下仍继续计时，只是不立即刷新时钟画面。 */
void ShowClock(void)
{
    LedBuff[5]=LedChar[hour/10];
    LedBuff[4]=LedChar[hour%10]&0x7F;
    LedBuff[3]=LedChar[minute/10];
    LedBuff[2]=LedChar[minute%10]&0x7F;
    LedBuff[1]=LedChar[second/10];
    LedBuff[0]=LedChar[second%10];
}

/* [十六] CalcClear：重置计算器。
   清空结果、输入、运算符、阶段及负数/错误状态，显示0。 */
void CalcClear(void)
{
    result=0; input=0; op=0; stage=0;
    has_digit=0; negative=0; calc_error=0;
    ShowNumber(0);
}

/* 只有非法运算或数值溢出才报警；按键操作不响。 */
/* [十七] CalcError：计算器统一错误处理。
   触发原因包括输入超出六位、计算结果溢出、除数为0等。
   设置错误标志，显示EEEEEE，令有源蜂鸣器报警150ms。 */
void CalcError(void)
{
    calc_error=1;
    ShowResult();
    Beep(150); /* 算术错误150ms；Timer0到时关闭 */
}

/* result stores unsigned magnitude, negative stores its sign. */
/* [十八] CalcApply：执行已选运算符的计算。
   返回1表示成功，返回0表示计算错误。
   加减法根据negative和op选择幅值相加或相减。
   乘法先做大数乘积超限的预判断，再做32位乘法并统一查范围。
   除法先排除除数为0；结果是整数商，不保留小数。
   最高显示范围：非负0～999999，负数绝对值最多99999。 */
bit CalcApply(void)
{
    if(op==1 || op==2)
    {
        if(negative==(op==2)) result+=input;
        else if(result>=input) result-=input;
        else { result=input-result; negative=!negative; }
    }
    else if(op==3)
    {
        /* 结果或输入达到1000时，两个都>=1000必定超过六位；
           否则乘积小于10亿，可安全在unsigned long中计算后判断范围。 */
        if(result>=1000UL && input>=1000UL) {CalcError();return 0;}
        result*=input;
    }
    else if(op==4)
    {
        if(input==0) { CalcError(); return 0; }
        result/=input;
    }
    if(result==0) negative=0;
    if(result>(negative?99999UL:999999UL)) { CalcError();return 0; }
    input=0; has_digit=0;
    return 1;
}

/* 教材按键动作函数：计算器 + 24小时软件时钟 + HHMMSS校时 */
/* [十九] KeyAction：矩阵按键的实际业务处理。
   第一优先级：setting=1，按HHMMSS顺序输入6位校时数字。
   第二优先级：mode=0，时钟模式下处理加小时/加分钟/秒清零。
   第三优先级：mode=1，计算器模式下处理数字、运算符、ENTER、ESC。
   因为键盘复用，同一按键在不同模式下可能执行不同动作。 */
void KeyAction(unsigned char k)
{
    unsigned char newop=0;
    /* 校时规则：前两位组成小时，第三四位组成分钟，最后两位组成秒。
       第六位输入后自动保存；非法时间显示EEEEEE并从头输入。
       按ESC退出校时，但不会修改已经正常运行的原时间。 */
    if(setting)
    {
        if(k==0x1B) /* ESC取消校时 */
        {
            setting=0;ShowClock();
            return;
        }
        if(k>=0x30 && k<=0x39 && setdigits<6)
        {
            unsigned char d=(unsigned char)(k-0x30);
            if(setdigits==0) {
                unsigned char i;
                for(i=0;i<6;i++)LedBuff[i]=0xFF;
            }
            {
                /* 根据当前已输入位数选择setH、setM或setS的地址。
                   通过指针复用两位十进制累计逻辑，节约代码空间。
                   data：8051内部直接寻址RAM存储区。 */
                unsigned char data *ptr;
                if(setdigits<2)ptr=&setH;
                else if(setdigits<4)ptr=&setM;
                else ptr=&setS;
                *ptr=(unsigned char)((*ptr)*10+d);
            }
            /* 从左到右逐位填入 HHMMSS，与原LedBuff位序一致 */
            LedBuff[5-setdigits]=LedChar[d];
            setdigits++;
            
            if(setdigits==6)
            {
                if(setH<24 && setM<60 && setS<60)
                {
                    EA=0;
                    hour=setH;minute=setM;second=setS;ms=0;
                    clock_changed=0;
                    EA=1;
                    setting=0;ShowClock();
                }
                else
                {
                    setdigits=0;setH=0;setM=0;setS=0;
                    ShowError();
                    
                }
            }
        }
        return;
    }
    /* 时钟显示模式：0x26加小时、0x25加分钟、0x28秒清零。
       小时到24回到0，分钟到60回到0；其他键不执行计算操作。 */
    if(mode==0)
    {
        if(k==0x26) { if(++hour>=24) hour=0; }
        else if(k==0x25) { if(++minute>=60) minute=0; }
        else if(k==0x28) second=0;
        ShowClock();
        
        return;
    }

    if(k==0x1B) {CalcClear();return;}
    if(calc_error) return;
    /* 计算器数字输入：新数字追加到input末尾。
       input>99999时再追加一位一定超过六位，因此预先报错。
       若刚显示完结果(stage=2)，新数字会开始一轮新计算。 */
    if(k>=0x30 && k<=0x39)
    {
        if(stage==2) CalcClear();
        if(input>99999UL){CalcError();return;}
        input=input*10+(unsigned char)(k-0x30);
        has_digit=1;
        ShowNumber(input);
        
        return;
    }
    if(k==0x26) newop=1;
    else if(k==0x25) newop=2;
    else if(k==0x28) newop=3;
    else if(k==0x27) newop=4;

    /* 运算符处理：
       第一次输入时把input作为result；
       已有第二操作数时先执行CalcApply，再保存下一运算符。
       连续表达式按实际按键顺序逐次求值，不实现乘除优先级。 */
    if(newop)
    {
        if(stage==0)
        {
            if(!has_digit) return;
            result=input;input=0;has_digit=0;stage=1;
        }
        else if(stage==1 && has_digit)
        {
            if(!CalcApply())return;
        }
        stage=1;
        op=newop;
        ShowResult();
        
    }
    /* ENTER确认：若已有两项运算数，执行对应计算并显示结果；
       若只有第一项数字，则直接把该数字作为结果显示。 */
    else if(k==0x0D)
    {
        if(stage==1 && has_digit)
        {
            if(!CalcApply())return;
            op=0;stage=2;ShowResult();
        }
        else if(stage==0 && has_digit)
        {
            result=input;input=0;has_digit=0;
            stage=2;ShowResult();
        }
        
    }
}

/* 键盘扫描：保留教材 KeyScan/KeyDriver/KeyCodeMap 函数和键位定义。
   每行4ms扫描一次；连续4次相同采样（约16ms）消抖。
   只上报按下边沿，不重复输入。 */
/* [二十] KeyDriver：主循环消费按键事件。
   Timer0扫描产生pendingKey后，这里临时关中断、取走并清零事件。
   读取到有效键码才执行KeyAction，复杂运算不放进中断中。
   pendingKey只保存一个事件，过快同时按键可能丢失后续事件。 */
void KeyDriver(void)
{
    unsigned char k;
    EA=0;
    k=pendingKey;pendingKey=0;
    EA=1;
    if(k)KeyAction(k);
}

/* [二十一] KeyScan：逐行读取4×4矩阵键盘。
   每约1ms扫描1行，每行约4ms被访问一次。
   读取P2高四位判断列电平是否变化；
   相同状态连续采样达到消抖条件（约16ms）才认定新状态。
   down=旧稳定状态 & ~新状态，只识别按下边沿；
   松开按键仅更新稳定状态，不再次执行KeyAction。 */
void KeyScan(void)
{
    unsigned char i,now,down;
    static unsigned char row=0;
    static unsigned char code select_row[4]={0xF7,0xFB,0xFD,0xFE};


    now=(unsigned char)(P2>>4);
    if(now!=keylast[row])
    {
        keylast[row]=now;
        keycount[row]=0;
    }
    else if(keycount[row]<3)
        keycount[row]++;
    else if(now!=keystable[row])
    {
        down=(unsigned char)(keystable[row]&(~now));
        keystable[row]=now;
        if(down && !pendingKey)
        {
            i=0;
            while(!(down&1)) {down>>=1;i++;}
            pendingKey=KeyCodeMap[row][i];
        }
    }
    row=(row+1)&3;
    P2=select_row[row];
}

/* Six-digit common-anode display through 74HC138 and PNP transistors. */
/* [二十二] LedScan：六位数码管动态扫描。
   每次只选择一位，快速循环形成视觉暂留。
   P0先写0xFF关闭段码，避免74HC138切换位选时出现鬼影。
   实物板与Proteus位选连线方向相反：译码器地址0对应最右侧。
   因而此处使用P0=LedBuff[i]：i=0为个位，i=5为最高位。
   修正后时钟从左到右显示HH.MM.SS，例如13.00.18。
   若切回原Proteus位序，可恢复P0=LedBuff[5-i]。 */
void LedScan(void)
{
    static unsigned char i=0;
    P0=0xFF;                 /* 位选切换前先消隐，防止重影 */
    ADDR0=i&1;
    ADDR1=(i>>1)&1;
    ADDR2=(i>>2)&1;
    P0=LedBuff[i];          /* 实物板位序：译码器地址0对应右侧个位 */
    i++;
    if(i==6) i=0;
}

/* [二十三] Timer0中断：整个系统的定时心跳。
   interrupt 1是8051 Timer0中断向量号，方式1为16位定时。
   11.0592MHz/12=921600计数/s。
   重装值0xFC67 -> (65536-0xFC67)=921个计数，理想周期约1ms。
   实际运行仍受中断响应与函数执行耗时影响，软件钟会有误差。

   依次完成：数码管刷新 -> 矩阵键盘读取 -> 10ms节拍产生
   -> 蜂鸣器时长倒计时 -> 秒分时逐级进位。
   只有分钟从59归零并使小时进位时，才触发200ms整点报时。 */
void InterruptTimer0(void) interrupt 1
{
    static unsigned char cnt10=0;
    TH0=0xFC;TL0=0x67;
    LedScan();
    KeyScan();
    /* cnt10累计十个约1ms中断，得到约10ms时基。
       ms以10ms为单位；tick10通知主循环执行独立按键消抖。 */
    if(++cnt10==10)
    {
        cnt10=0;tick10=1;
        ms++;
    }
    /* 蜂鸣器剩余时间每个T0中断减少约1ms。
       计数减到0后P1.6拉高，PNP截止，蜂鸣器关闭。 */
    if(beep_ms && --beep_ms==0) BUZZ=1; /* 低电平有效，计时结束拉高关闭 */
    /* 100个10ms周期约为1秒；秒满60进分钟、分钟满60进小时。
       时钟按24小时制运行，达到24时回到00时。 */
    if(ms>=100)
    {
        ms=0;
        if(++second==60)
        {
            second=0;
            if(++minute==60)
            {
                minute=0;
                if(++hour==24)hour=0;
                Beep(200); /* 整点报时一次 */
            }
        }
        clock_changed=1;
    }
}

/* P3.2模式键 / P3.3校时键：每10ms采样，连续两次一致后认定变化 */
/* [二十四] ButtonTask：P3.2、P3.3独立按键消抖。
   主循环接到tick10时调用，相邻两次采样一致后更新稳定状态。
   down只提取从高到低的按下边沿，按住不重复触发。
   P3.2：切换计算器/时钟，若校时中则先退出校时。
   P3.3：进入校时，清空这次HHMMSS输入进度。 */
void ButtonTask(void)
{
    static unsigned char last,stable; /* 上电首轮采样自动建立释放基线 */
    unsigned char now=(P3>>2)&3,down;
    if(now==last && now!=stable)
    {
        down=stable&(~now);
        stable=now;
        if(down&1)
        {
            if(setting) {setting=0;mode=0;}
            else mode=!mode;
            if(mode)CalcClear();else ShowClock();
        }
        else if(down&2)
        {
            setting=1;mode=0;setH=0;setM=0;setS=0;setdigits=0;
            ShowNumber(0);
        }
    }
    last=now;
}

/* [二十五] main：系统初始化与主循环。
   1. 初始化时钟起始值、显示缓冲、键盘状态。
   2. 配置74HC138、矩阵键盘行、独立按键、蜂鸣器的初始电平。
   3. TMOD=0x01让Timer0使用16位方式1；Timer1不使用。
   4. ET0=1允许T0中断，EA=1开总中断，TR0=1启动计数。
   5. 主循环只处理键盘事件、10ms独立按键与时钟画面刷新。 */
void main(void)
{
    unsigned char i;
    /* 上电默认从12:00:00开始；这里只是软件计时，不是独立RTC。
       断电后不会记忆当前时间，可以通过P3.3重新校时。 */
    hour=12;
    clock_changed=1;
    for(i=0;i<6;i++)LedBuff[i]=0xFF;
    for(i=0;i<4;i++)
    {
        keylast[i]=15;keystable[i]=15;
    }
    /* 74HC138：高有效使能端置1，低有效使能端置0。
       先关闭所有段码，随后由Timer0逐位显示。 */
    ENLED=0;ADDR3=1;P0=0xFF;
    /* 初次选中第0行：P2=1111 0111，只有P2.3为低电平。
       P2高四位为1，保持矩阵列输入处于释放状态。 */
    P2=0xF7; /* KEY_OUT_1=0, 其余行为1，列输入全部为1 */
    /* 两独立按键端口先写1作为输入。
       BUZZ=1使PNP截止，保证上电后蜂鸣器默认静音。 */
    MODE_KEY=1;SET_KEY=1;BUZZ=1;
    TMOD=0x01; /* 只用Timer0，不启用Timer1 */
    TH0=0xFC;TL0=0x67;
    ShowClock();
    ET0=1;EA=1;TR0=1;
    while(1)
    {
        KeyDriver();
        if(tick10)
        {
            tick10=0;
            ButtonTask();
        }
        /* 仅当处于时钟模式且没有正在校时时才重绘时钟。
           即使切换到计算器模式，Timer0仍持续推进内部时钟。 */
        if(clock_changed)
        {
            clock_changed=0;
            if(mode==0 && !setting)ShowClock();
        }
    }
}