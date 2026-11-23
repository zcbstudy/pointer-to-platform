#include <REG52.H>

/* V10 Lite - AT89C51, 11.0592MHz, 6-digit common anode, 4x4 keypad.
   Calculator: six-digit integers, + - * /, signed results, integer division.
   Clock: HH.MM.SS, always runs; A hour+, B minute+, C second reset.
   Mode key P3.2: momentary switch to ground, short press toggles mode.
   No dot matrix, buzzer, stopwatch, countdown, or custom .h files. */

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

unsigned char code LedChar[10]={
  0xC0,0xF9,0xA4,0xB0,0x99,0x92,0x82,0xF8,0x80,0x90
};
unsigned char LedBuff[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
unsigned char code KeyCodeMap[4][4]={
 {0x31,0x32,0x33,0x26}, {0x34,0x35,0x36,0x25},
 {0x37,0x38,0x39,0x28}, {0x30,0x1B,0x0D,0x27}
};
unsigned char KeySta[4][4]={
 {1,1,1,1},{1,1,1,1},{1,1,1,1},{1,1,1,1}
};

static unsigned char mode=0;  /* 0=clock, 1=calculator */
static unsigned char hour=12,minute=0,second=0;
static unsigned int ms=0;
static bit clock_changed=1;
static bit tick10=0;
static unsigned char op=0;    /* 0 none, 1+, 2-, 3*, 4/ */
static unsigned char stage=0; /* 0 first, 1 second, 2 result */
static bit has_digit=0,negative=0,calc_error=0;
static unsigned long result=0,input=0;

void LedScan(void);
void KeyScan(void);
void KeyDriver(void);
void KeyAction(unsigned char keycode);
void ShowNumber(unsigned long num);

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

void CalcError(void) { calc_error=1; ShowResult(); }

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

/* Textbook KeyAction: digits, operations, Enter, and ESC. */
void KeyAction(unsigned char k)
{
    unsigned char newop=0;
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

/* Debounced keys are written by Timer0 ISR; driver handles press edge. */
void KeyDriver(void)
{
    unsigned char i,j;
    static unsigned char backup[4][4]={
        {1,1,1,1},{1,1,1,1},{1,1,1,1},{1,1,1,1}
    };
    for(i=0;i<4;i++) for(j=0;j<4;j++)
    {
        if(backup[i][j]!=KeySta[i][j])
        {
            if(backup[i][j] && !KeySta[i][j])
                KeyAction(KeyCodeMap[i][j]);
            backup[i][j]=KeySta[i][j];
        }
    }
}

/* One matrix row per millisecond, four consecutive reads for debouncing. */
void KeyScan(void)
{
    unsigned char i;
    static unsigned char keyout=0;
    static unsigned char keybuf[4][4]={
        {0xFF,0xFF,0xFF,0xFF},{0xFF,0xFF,0xFF,0xFF},
        {0xFF,0xFF,0xFF,0xFF},{0xFF,0xFF,0xFF,0xFF}
    };
    keybuf[keyout][0]=(keybuf[keyout][0]<<1)|KEY_IN_1;
    keybuf[keyout][1]=(keybuf[keyout][1]<<1)|KEY_IN_2;
    keybuf[keyout][2]=(keybuf[keyout][2]<<1)|KEY_IN_3;
    keybuf[keyout][3]=(keybuf[keyout][3]<<1)|KEY_IN_4;
    for(i=0;i<4;i++)
    {
        if((keybuf[keyout][i]&15)==0)KeySta[keyout][i]=0;
        else if((keybuf[keyout][i]&15)==15)KeySta[keyout][i]=1;
    }
    keyout=(keyout+1)&3;
    switch(keyout)
    {
      case 0: KEY_OUT_4=1;KEY_OUT_1=0;break;
      case 1: KEY_OUT_1=1;KEY_OUT_2=0;break;
      case 2: KEY_OUT_2=1;KEY_OUT_3=0;break;
      case 3: KEY_OUT_3=1;KEY_OUT_4=0;break;
    }
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
            }
        }
        clock_changed=1;
    }
}

void main(void)
{
    unsigned char stable=1,deb=0;
    ENLED=0;ADDR3=1;P0=0xFF;
    KEY_OUT_1=0;KEY_OUT_2=1;KEY_OUT_3=1;KEY_OUT_4=1;
    P2=P2|0xF0;
    MODE_KEY=1;
    TMOD=0x01;TH0=0xFC;TL0=0x67;
    ShowClock();
    ET0=1;EA=1;TR0=1;
    while(1)
    {
        KeyDriver();
        if(tick10)
        {
            tick10=0;
            if(MODE_KEY==stable) deb=0;
            else if(++deb>=2)
            {
                stable=MODE_KEY; deb=0;
                if(stable==0)
                {
                    mode=!mode;
                    if(mode)CalcClear();
                    else ShowClock();
                }
            }
        }
        if(clock_changed)
        {
            clock_changed=0;
            if(mode==0)ShowClock();
        }
    }
}
