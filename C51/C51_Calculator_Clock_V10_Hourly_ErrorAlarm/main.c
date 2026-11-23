#include <REG52.H>

/* V10 HourlyOnly - AT89C51, 11.0592MHz, 6-digit common anode, 4x4 keypad.
   Calculator: six-digit integers, + - * /, signed results, integer division.
   Clock: HH.MM.SS, always runs; A hour+, B minute+, C second reset.
   Mode key P3.2: momentary switch to ground, short press toggles mode.
   Only hourly chime, no other notification sounds. */

sbit ADDR0=P1^0;
sbit ADDR1=P1^1;
sbit ADDR2=P1^2;
sbit ADDR3=P1^3;
sbit ENLED=P1^4;
sbit KEY_IN_1=P2^4;
sbit KEY_IN_2=P2^5;
sbit KEY_IN_3=P2^6;
sbit KEY_IN_4=P2^7;
sbit KEY_OUT_1=P2^3;
sbit KEY_OUT_2=P2^2;
sbit KEY_OUT_3=P2^1;
sbit KEY_OUT_4=P2^0;
sbit MODE_KEY=P3^2;
sbit SET_KEY=P3^3;
sbit BUZZ=P1^6; /* PNP high-side drive; idle high, tone toggles */

unsigned char code LedChar[10]={
  0xC0,0xF9,0xA4,0xB0,0x99,0x92,0x82,0xF8,0x80,0x90
};
unsigned char LedBuff[6];
unsigned char code KeyCodeMap[4][4]={
 {0x31,0x32,0x33,0x26}, {0x34,0x35,0x36,0x25},
 {0x37,0x38,0x39,0x28}, {0x30,0x1B,0x0D,0x27}
};
unsigned char KeySta[4][4];

static unsigned char mode=0;  /* 0=clock, 1=calculator */
static unsigned char hour,minute,second;
static unsigned int ms=0;
static bit clock_changed;
static bit tick10=0;
/* 校时键 P3.3 按下进入 HHMMSS 输入；六位输入自动校验保存 */
static bit setting=0;
static unsigned char setdigits=0;
static unsigned char setH=0,setM=0,setS=0;
/* 蜂鸣器由T1中断产生方波，T0每1ms递减持续时间 */
static volatile unsigned char beep_ms=0;
static unsigned char keylast[4],keystable[4],keycount[4];
static volatile unsigned char pendingKey=0; /* KeyScan -> KeyDriver */
static unsigned char op=0;    /* 0 none, 1+, 2-, 3*, 4/ */
static unsigned char stage=0; /* 0 first, 1 second, 2 result */
static bit has_digit=0,negative=0,calc_error=0;
static unsigned long result=0,input=0;

void LedScan(void);
void KeyScan(void);
void KeyDriver(void);
void KeyAction(unsigned char keycode);
void ShowNumber(unsigned long num);
void Beep(unsigned char duration)
{
    TR1=0; ET1=0; BUZZ=1; /* PNP：高电平截止，先停止 */
    beep_ms=duration;
    if(duration)
    {
        TH1=0xFF;TL1=0x1A;TF1=0; /* 11.0592MHz, 约250us半周期 */
        ET1=1;TR1=1;
    }
}
/* T1 ISR，P1.6翻转产生约2kHz方波，适用于无源蜂鸣器 */
void Timer1_ISR(void) interrupt 3
{
    TH1=0xFF;TL1=0x1A;
    BUZZ=!BUZZ;
}


/* Buffer contains segment codes; lowest digit is LedBuff[0]. */
void ShowNumber(unsigned long num)
{
    unsigned char i;
    for(i=0;i<6;i++) LedBuff[i]=0xFF;
    i=0;
    do {
        LedBuff[i]=LedChar[(unsigned char)(num%10)];
        num=num/10;
        i++;
    } while(num && i<6);
}

void ShowResult(void)
{
    unsigned char i;
    if(calc_error)
    {
        for(i=0;i<6;i++) LedBuff[i]=0x86; /* EEEEEE */
        return;
    }
    ShowNumber(result);
    if(negative) LedBuff[5]=0xBF;
}

void ShowClock(void)
{
    LedBuff[5]=LedChar[hour/10];
    LedBuff[4]=LedChar[hour%10]&0x7F;
    LedBuff[3]=LedChar[minute/10];
    LedBuff[2]=LedChar[minute%10]&0x7F;
    LedBuff[1]=LedChar[second/10];
    LedBuff[0]=LedChar[second%10];
}

void CalcClear(void)
{
    result=0; input=0; op=0; stage=0;
    has_digit=0; negative=0; calc_error=0;
    ShowNumber(0);
}

/* 只有非法运算或数值溢出才报警；按键操作不响。 */
void CalcError(void)
{
    calc_error=1;
    ShowResult();
    Beep(150); /* 150ms，Timer1方波由Timer0控制持续时间 */
}

/* result stores unsigned magnitude, negative stores its sign. */
bit CalcApply(void)
{
    unsigned long limit;
    if(op==1 || op==2)
    {
        if(negative==(op==2)) result+=input;
        else if(result>=input) result-=input;
        else { result=input-result; negative=!negative; }
    }
    else if(op==3)
    {
        limit=negative?99999UL:999999UL;
        if(result!=0 && input>limit/result) { CalcError(); return 0; }
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
void KeyAction(unsigned char k)
{
    unsigned char newop=0;
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
            if(setdigits<2)setH=(unsigned char)(setH*10+d);
            else if(setdigits<4)setM=(unsigned char)(setM*10+d);
            else setS=(unsigned char)(setS*10+d);
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
                    unsigned char i;
                    setdigits=0;setH=0;setM=0;setS=0;
                    for(i=0;i<6;i++)LedBuff[i]=0x86;
                    
                }
            }
        }
        return;
    }
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

/* 键盘扫描：保留教材 KeyScan/KeyDriver/KeySta/KeyCodeMap 名称。
   每行4ms扫描一次；连续4次相同采样（约16ms）消抖。
   只上报按下边沿，不重复输入。 */
void KeyDriver(void)
{
    unsigned char k;
    EA=0;
    k=pendingKey;pendingKey=0;
    EA=1;
    if(k)KeyAction(k);
}

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
        for(i=0;i<4;i++)
        {
            KeySta[row][i]=(now&(1<<i))?1:0;
            if((down&(1<<i)) && pendingKey==0)
                pendingKey=KeyCodeMap[row][i];
        }
    }
    row=(row+1)&3;
    P2=select_row[row];
}

/* Six-digit common-anode display through 74HC138 and PNP transistors. */
void LedScan(void)
{
    static unsigned char i=0;
    P0=0xFF;
    ADDR0=i&1;
    ADDR1=(i>>1)&1;
    ADDR2=(i>>2)&1;
    P0=LedBuff[i];
    i++;
    if(i==6) i=0;
}

void InterruptTimer0(void) interrupt 1
{
    static unsigned char cnt10=0;
    TH0=0xFC;TL0=0x67;
    LedScan();
    KeyScan();
    if(++cnt10==10){cnt10=0;tick10=1;}
    if(beep_ms && --beep_ms==0) {TR1=0;ET1=0;BUZZ=1;}
    if(++ms>=1000)
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

/* 两个独立按键，每10ms采样，稳定20ms触发一次（松开后可重新触发） */
void ButtonTask(void)
{
    static unsigned char last=3,stable=3,deb=0;
    unsigned char now=(P3>>2)&3,down;
    if(now!=last){last=now;deb=0;return;}
    if(deb<2) {deb++;return;}
    if(now==stable)return;
    down=stable&(~now);
    stable=now;
    if(down&1)
    {
        if(setting) {setting=0;mode=0;ShowClock();}
        else {mode=!mode;if(mode)CalcClear();else ShowClock();}
        
    }
    else if(down&2)
    {
        setting=1;mode=0;setH=0;setM=0;setS=0;setdigits=0;
        ShowNumber(0);
    }
}

void main(void)
{
    unsigned char i,j;
    hour=12;
    clock_changed=1;
    for(i=0;i<6;i++)LedBuff[i]=0xFF;
    for(i=0;i<4;i++)
    {
        keylast[i]=15;keystable[i]=15;
        for(j=0;j<4;j++)KeySta[i][j]=1;
    }
    ENLED=0;ADDR3=1;P0=0xFF;
    P2=0xF7; /* KEY_OUT_1=0, 其余行为1，列输入全部为1 */
    MODE_KEY=1;SET_KEY=1;BUZZ=1;
    TMOD=0x11;
    TH0=0xFC;TL0=0x67;TH1=0xFF;TL1=0x1A;
    ET1=0;TR1=0;
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
        if(clock_changed)
        {
            clock_changed=0;
            if(mode==0 && !setting)ShowClock();
        }
    }
}