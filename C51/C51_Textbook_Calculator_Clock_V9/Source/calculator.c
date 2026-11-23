/*
 * calculator.c - 以教材8.5节的简易加法计算器为基础扩展四则运算。
 * 原教材函数 ShowNumber、KeyAction、KeyDriver 分工保留：
 * ShowNumber在segment.c；KeyAction在本文件；KeyDriver在key.c。
 * 与教材变量对应：addend为当前输入数；result为累积结果。
 * 为照顾AT89C52和试用版容量，每个输入数限定0~99。
 * 结果可为负数；除法只输出整数商。
 */
#include <REG52.H>
void ShowNumber(signed int num);
void Seg_ShowError(void);
void Buzzer_Beep(unsigned char ticks10ms);

#define KEY_PLUS  0x26  /* A键 */
#define KEY_MINUS 0x25  /* B键 */
#define KEY_MUL   0x28  /* C键 */
#define KEY_DIV   0x27  /* D键 */
#define KEY_ENTER 0x0D  /* #键 */
#define KEY_ESC   0x1B  /* *键 */

static unsigned char addend;
static unsigned char operation;
static unsigned char stage;  /* 0第一个数，1第二个数，2显示结果 */
static unsigned char digit_entered;
static unsigned char error;
static signed int result;

void Calculator_Init(void)
{
    addend=0;
    operation=0;
    stage=0;
    digit_entered=0;
    error=0;
    result=0;
}

static void Calculator_Error(void)
{
    error=1;
    Buzzer_Beep(45);
}

/* 教材原名：按键动作函数，传入KeyCodeMap产生的键码。 */
void KeyAction(unsigned char keycode)
{
    unsigned char digit;
    if(keycode==KEY_ESC)
    {
        Calculator_Init();
        return;
    }
    if(error) return;

    if(keycode>=0x30 && keycode<=0x39)
    {
        digit=(unsigned char)(keycode-0x30);
        if(stage==2)
        {
            Calculator_Init();    /* 结果后再按数字，开始新计算 */
        }
        if(addend>9)          /* 最多输入两位数 */
        {
            Calculator_Error();
            return;
        }
        addend=(unsigned char)(addend*10+digit);
        digit_entered=1;
    }
    else if(keycode==KEY_PLUS || keycode==KEY_MINUS
         || keycode==KEY_MUL || keycode==KEY_DIV)
    {
        if(stage==0)
        {
            result=addend;
            addend=0;
            stage=1;
            operation=keycode;
            digit_entered=0;
        }
        else if(stage==2)
        {
            /* 连续运算时，首个操作数仍限制在-99~99，避免16位乘法溢出。 */
            if(result>99 || result<(-99))
            {
                Calculator_Error();
                return;
            }
            /* 例如12+3=15，再按乘号，使用15作为下次第一个数。 */
            addend=0;
            stage=1;
            operation=keycode;
            digit_entered=0;
        }
        else if(digit_entered==0)
        {
            operation=keycode; /* 未输入第二个数时允许改运算符 */
        }
    }
    else if(keycode==KEY_ENTER && stage==1 && digit_entered)
    {
        if(operation==KEY_PLUS) result+=addend;
        else if(operation==KEY_MINUS) result-=addend;
        else if(operation==KEY_MUL) result*=addend;
        else if(operation==KEY_DIV)
        {
            if(addend==0)
            {
                Calculator_Error();
                return;
            }
            result/=addend;  /* 整数除法，7/2=3 */
        }
        addend=0;
        stage=2;
        digit_entered=0;
    }
}

void Calculator_Show(void)
{
    if(error) Seg_ShowError();
    else if(stage==2) ShowNumber(result);
    else ShowNumber(addend);
}

unsigned char Calculator_HasError(void)
{
    return error;
}
