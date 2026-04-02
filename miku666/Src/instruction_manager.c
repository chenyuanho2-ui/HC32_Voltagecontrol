/**
 * @file instruction_manager.c
 * @brief 指令系统核心管理器
 * * 职责：
 * 1. 作为指令解析的总入口，协调 Basic, Ramp, Timer, Cal 各模块的解析逻辑。
 * 2. 维护系统全局状态（正常模式/校准模式）并管理当前转速变量。
 * 3. 提供统一的硬件输出接口 Apply_Physical_Output，实现 20ms 定时 DAC 刷新。
 * 4. 提供自定义字符串比较函数 custom_stricmp 以解决编译器库函数兼容性问题。
 */


#include "instruction_manager.h"
#include "cmd_basic.h"
#include "cmd_ramp.h"
#include "cmd_timer.h"
#include "cmd_cal.h"
#include "calibration.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

//新增引用
#include "cmd_help.h"
#include "cmd_direction.h"
#include "cmd_start.h"
#include "cmd_buzzer.h"
#include "cmd_self_test.h"

#include "hc32f005.h"
#include "gpio.h"
#include "sysctrl.h"
#include "adc.h"
#include "interrupts_hc32f005.h"

static float current_speed = 0.0f;
static SystemMode_t global_mode = MODE_NORMAL;

static volatile uint32_t g_miku_ms_tick = 0u;

#define MIKU_MOTOR_PWM_PORT      GpioPort2
#define MIKU_MOTOR_PWM_PIN       GpioPin3
#define MIKU_BUZZER_PWM_PORT     GpioPort2
#define MIKU_BUZZER_PWM_PIN      GpioPin4
#define MIKU_STOP_PORT           GpioPort1
#define MIKU_STOP_PIN            GpioPin5
#define MIKU_DIR_PORT            GpioPort1
#define MIKU_DIR_PIN             GpioPin4

#define MIKU_MOTOR_PWM_FREQ_HZ   10000u
#define MIKU_BUZZER_PWM_FREQ_HZ  4000u

#define MIKU_ADC_CH              AdcExInputCH3
#define MIKU_ADC_PORT            GpioPort3
#define MIKU_ADC_PIN             GpioPin3

typedef struct
{
    uint8_t enabled;
    uint8_t level;
    uint32_t high_ticks;
    uint32_t low_ticks;
    uint32_t remaining;
} miku_pwm_chan_t;

static uint32_t g_pwm_tick_hz = 0u;
static uint32_t g_pwm_last_delta = 1u;
static miku_pwm_chan_t g_pwm_motor;
static miku_pwm_chan_t g_pwm_buzzer;
static uint8_t g_buzzer_volume_percent = 10u;

static uint16_t Miku_CalcTimReload(uint32_t delta_ticks)
{
    if (delta_ticks < 1u)
    {
        delta_ticks = 1u;
    }
    if (delta_ticks > 65535u)
    {
        delta_ticks = 65535u;
    }
    return (uint16_t)(0x10000u - delta_ticks);
}

static void Miku_GpioOutInit(en_gpio_port_t port, en_gpio_pin_t pin, boolean_t init_level)
{
    stc_gpio_cfg_t cfg;
    DDL_ZERO_STRUCT(cfg);
    cfg.enDir = GpioDirOut;
    cfg.enDrv = GpioDrvH;
    cfg.enPu = GpioPuDisable;
    cfg.enPd = GpioPdDisable;
    cfg.enOD = GpioOdDisable;
    Gpio_Init(port, pin, &cfg);
    Gpio_WriteOutputIO(port, pin, init_level);
}

uint32_t Miku_GetTick(void)
{
    return g_miku_ms_tick;
}

void Miku_DelayMs(uint32_t ms)
{
    delay1ms(ms);
}

void Miku_Motor_SetStop(uint8_t stop)
{
    Gpio_WriteOutputIO(MIKU_STOP_PORT, MIKU_STOP_PIN, (stop != 0u) ? TRUE : FALSE);
}

void Miku_Motor_SetDirectionCcw(uint8_t ccw)
{
    Gpio_WriteOutputIO(MIKU_DIR_PORT, MIKU_DIR_PIN, (ccw != 0u) ? TRUE : FALSE);
}

static uint32_t Miku_Min2(uint32_t a, uint32_t b)
{
    return (a < b) ? a : b;
}

static uint32_t Miku_PwmNextDelta(void)
{
    uint32_t next = 0xFFFFFFFFu;
    if (g_pwm_motor.enabled)
    {
        next = Miku_Min2(next, g_pwm_motor.remaining);
    }
    if (g_pwm_buzzer.enabled)
    {
        next = Miku_Min2(next, g_pwm_buzzer.remaining);
    }
    return next;
}

static void Miku_PwmApplySchedule(uint32_t next_delta)
{
    uint16_t reload = Miku_CalcTimReload(next_delta);
    M0P_TIM2->ARR = reload;
    M0P_TIM2->CNT = reload;
    g_pwm_last_delta = next_delta;
}

static void Miku_PwmRecalcAndApply(void)
{
    uint32_t next = Miku_PwmNextDelta();
    if (0xFFFFFFFFu == next)
    {
        M0P_TIM2->CR_f.CTEN = FALSE;
        g_pwm_last_delta = 1u;
        return;
    }
    if (FALSE == M0P_TIM2->CR_f.CTEN)
    {
        M0P_TIM2->CR_f.CTEN = TRUE;
    }
    Miku_PwmApplySchedule(next);
}

static void Miku_PwmChanSetEnable(miku_pwm_chan_t* ch, en_gpio_port_t port, en_gpio_pin_t pin, uint8_t enable)
{
    if (0u == enable)
    {
        ch->enabled = 0u;
        ch->level = 0u;
        ch->remaining = 1u;
        Gpio_WriteOutputIO(port, pin, FALSE);
    }
    else
    {
        ch->enabled = 1u;
        if (0u == ch->high_ticks)
        {
            ch->enabled = 0u;
            ch->level = 0u;
            ch->remaining = 1u;
            Gpio_WriteOutputIO(port, pin, FALSE);
            return;
        }
        if (0u == ch->low_ticks)
        {
            ch->enabled = 0u;
            ch->level = 1u;
            ch->remaining = 1u;
            Gpio_WriteOutputIO(port, pin, TRUE);
            return;
        }
        ch->level = 1u;
        ch->remaining = ch->high_ticks;
        Gpio_WriteOutputIO(port, pin, TRUE);
    }
}

void Miku_Motor_SetSpeedPercent(float speed_percent)
{
    if (speed_percent <= 0.0f)
    {
        g_pwm_motor.high_ticks = 0u;
        g_pwm_motor.low_ticks = 1u;
        Miku_PwmChanSetEnable(&g_pwm_motor, MIKU_MOTOR_PWM_PORT, MIKU_MOTOR_PWM_PIN, 0u);
        Miku_PwmRecalcAndApply();
        return;
    }
    if (speed_percent >= 100.0f)
    {
        g_pwm_motor.high_ticks = 1u;
        g_pwm_motor.low_ticks = 0u;
        Miku_PwmChanSetEnable(&g_pwm_motor, MIKU_MOTOR_PWM_PORT, MIKU_MOTOR_PWM_PIN, 1u);
        Miku_PwmRecalcAndApply();
        return;
    }

    uint32_t period_ticks = (g_pwm_tick_hz / MIKU_MOTOR_PWM_FREQ_HZ);
    if (period_ticks < 2u)
    {
        period_ticks = 2u;
    }
    uint32_t duty = (uint32_t)(speed_percent * 100.0f);
    if (duty > 10000u)
    {
        duty = 10000u;
    }
    uint32_t high = (period_ticks * duty) / 10000u;
    if (high < 1u)
    {
        high = 1u;
    }
    if (high >= period_ticks)
    {
        high = period_ticks - 1u;
    }
    g_pwm_motor.high_ticks = high;
    g_pwm_motor.low_ticks = period_ticks - high;
    Miku_PwmChanSetEnable(&g_pwm_motor, MIKU_MOTOR_PWM_PORT, MIKU_MOTOR_PWM_PIN, 1u);
    Miku_PwmRecalcAndApply();
}

void Miku_Buzzer_SetEnable(uint8_t enable)
{
    if (0u == enable)
    {
        g_pwm_buzzer.high_ticks = 0u;
        g_pwm_buzzer.low_ticks = 1u;
        Miku_PwmChanSetEnable(&g_pwm_buzzer, MIKU_BUZZER_PWM_PORT, MIKU_BUZZER_PWM_PIN, 0u);
        Miku_PwmRecalcAndApply();
        return;
    }

    uint32_t period_ticks = (g_pwm_tick_hz / MIKU_BUZZER_PWM_FREQ_HZ);
    if (period_ticks < 2u)
    {
        period_ticks = 2u;
    }
    uint32_t vol = g_buzzer_volume_percent;
    if (vol > 50u)
    {
        vol = 50u;
    }
    uint32_t high = (period_ticks * vol) / 100u;
    if (high < 1u)
    {
        high = 1u;
    }
    if (high >= period_ticks)
    {
        high = period_ticks - 1u;
    }
    g_pwm_buzzer.high_ticks = high;
    g_pwm_buzzer.low_ticks = period_ticks - high;
    Miku_PwmChanSetEnable(&g_pwm_buzzer, MIKU_BUZZER_PWM_PORT, MIKU_BUZZER_PWM_PIN, 1u);
    Miku_PwmRecalcAndApply();
}

void Miku_Buzzer_SetVolume(uint8_t volume_percent)
{
    if (volume_percent > 100u)
    {
        volume_percent = 100u;
    }
    g_buzzer_volume_percent = volume_percent;
    if (g_pwm_buzzer.enabled)
    {
        Miku_Buzzer_SetEnable(1u);
    }
}

static void Miku_TickInit(void)
{
    g_miku_ms_tick = 0u;

    uint32_t pclk = Sysctrl_GetPClkFreq();
    uint32_t delta = pclk / 1000u;
    if (delta < 1u)
    {
        delta = 1u;
    }
    if (delta > 65535u)
    {
        delta = 65535u;
    }

    uint16_t reload = Miku_CalcTimReload(delta);
    M0P_TIM1->CR_f.CTEN = FALSE;
    M0P_TIM1->CR_f.GATEP = 0u;
    M0P_TIM1->CR_f.GATE = 0u;
    M0P_TIM1->CR_f.PRS = 0u;
    M0P_TIM1->CR_f.TOGEN = FALSE;
    M0P_TIM1->CR_f.CT = 0u;
    M0P_TIM1->CR_f.MD = 1u;
    M0P_TIM1->ARR = reload;
    M0P_TIM1->CNT = reload;
    M0P_TIM1->ICLR_f.UIF = FALSE;
    M0P_TIM1->CR_f.UIE = TRUE;
    EnableNvic(TIM1_IRQn, IrqLevel3, TRUE);
    M0P_TIM1->CR_f.CTEN = TRUE;
}

static void Miku_PwmEngineInit(void)
{
    DDL_ZERO_STRUCT(g_pwm_motor);
    DDL_ZERO_STRUCT(g_pwm_buzzer);

    g_pwm_tick_hz = Sysctrl_GetPClkFreq();
    if (0u == g_pwm_tick_hz)
    {
        g_pwm_tick_hz = 4000000u;
    }

    M0P_TIM2->CR_f.CTEN = FALSE;
    M0P_TIM2->CR_f.GATEP = 0u;
    M0P_TIM2->CR_f.GATE = 0u;
    M0P_TIM2->CR_f.PRS = 0u;
    M0P_TIM2->CR_f.TOGEN = FALSE;
    M0P_TIM2->CR_f.CT = 0u;
    M0P_TIM2->CR_f.MD = 1u;
    M0P_TIM2->ICLR_f.UIF = FALSE;
    M0P_TIM2->CR_f.UIE = TRUE;
    EnableNvic(TIM2_IRQn, IrqLevel3, TRUE);

    g_pwm_last_delta = 1u;
    Miku_PwmApplySchedule(1u);
    M0P_TIM2->CR_f.CTEN = TRUE;
}

static void Miku_AdcInit(void)
{
    Sysctrl_SetPeripheralGate(SysctrlPeripheralAdcBgr, TRUE);
    M0P_BGR->CR |= 0x1u;
    delay10us(2);

    Gpio_SetAnalogMode(MIKU_ADC_PORT, MIKU_ADC_PIN);

    stc_adc_cfg_t stcAdcCfg;
    stc_adc_norm_cfg_t stcNormCfg;
    DDL_ZERO_STRUCT(stcAdcCfg);
    DDL_ZERO_STRUCT(stcNormCfg);

    stcAdcCfg.enAdcOpMode = AdcNormalMode;
    stcAdcCfg.enAdcClkSel = AdcClkSysTDiv2;
    stcAdcCfg.enAdcSampTimeSel = AdcSampTime8Clk;
    stcAdcCfg.enAdcRefVolSel = RefVolSelInBgr2p5;
    stcAdcCfg.bAdcInBufEn = FALSE;
    Adc_Init(&stcAdcCfg);

    stcNormCfg.enAdcNormModeCh = MIKU_ADC_CH;
    stcNormCfg.bAdcResultAccEn = FALSE;
    Adc_ConfigNormMode(&stcAdcCfg, &stcNormCfg);
    Adc_Enable();
}

uint16_t Miku_Adc_ReadRaw(void)
{
    uint16_t adc_val = 0;
    uint32_t timeout = 1000000u;

    Adc_Start();
    while ((TRUE == Adc_PollBusyState()) && (timeout-- > 0u))
    {
    }
    if (0u == timeout)
    {
        return 0;
    }
    Adc_GetResult(&adc_val);
    return adc_val;
}

float Miku_Adc_ReadVoltage(void)
{
    uint16_t raw = Miku_Adc_ReadRaw();
    return ((float)raw) * 2.5f / 4095.0f;
}

void Tim1_IRQHandler(void)
{
    if (TRUE == M0P_TIM1->IFR_f.UIF)
    {
        M0P_TIM1->ICLR_f.UIF = FALSE;
        g_miku_ms_tick++;
    }
}

void Tim2_IRQHandler(void)
{
    if (TRUE != M0P_TIM2->IFR_f.UIF)
    {
        return;
    }
    M0P_TIM2->ICLR_f.UIF = FALSE;

    uint32_t delta = g_pwm_last_delta;
    if (g_pwm_motor.enabled)
    {
        if (g_pwm_motor.remaining > delta)
        {
            g_pwm_motor.remaining -= delta;
        }
        else
        {
            g_pwm_motor.remaining = 0u;
        }
    }
    if (g_pwm_buzzer.enabled)
    {
        if (g_pwm_buzzer.remaining > delta)
        {
            g_pwm_buzzer.remaining -= delta;
        }
        else
        {
            g_pwm_buzzer.remaining = 0u;
        }
    }

    if (g_pwm_motor.enabled && (0u == g_pwm_motor.remaining))
    {
        g_pwm_motor.level ^= 1u;
        Gpio_WriteOutputIO(MIKU_MOTOR_PWM_PORT, MIKU_MOTOR_PWM_PIN, (g_pwm_motor.level != 0u) ? TRUE : FALSE);
        g_pwm_motor.remaining = (g_pwm_motor.level != 0u) ? g_pwm_motor.high_ticks : g_pwm_motor.low_ticks;
    }
    if (g_pwm_buzzer.enabled && (0u == g_pwm_buzzer.remaining))
    {
        g_pwm_buzzer.level ^= 1u;
        Gpio_WriteOutputIO(MIKU_BUZZER_PWM_PORT, MIKU_BUZZER_PWM_PIN, (g_pwm_buzzer.level != 0u) ? TRUE : FALSE);
        g_pwm_buzzer.remaining = (g_pwm_buzzer.level != 0u) ? g_pwm_buzzer.high_ticks : g_pwm_buzzer.low_ticks;
    }

    Miku_PwmRecalcAndApply();
}

static void Apply_Physical_Output(float speed)
{
    float val = (global_mode == MODE_CALIBRATION) ? speed : Calibration_GetCorrectedValue(speed);
    if (val < 0.0f)
    {
        val = 0.0f;
    }
    if (val > 100.0f)
    {
        val = 100.0f;
    }
    Miku_Motor_SetSpeedPercent(val);
}

///////////新增初始化
void Instruction_Init(void) {
    current_speed = 0.0f;
    global_mode = MODE_NORMAL;

    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio, TRUE);
    Sysctrl_SetPeripheralGate(SysctrlPeripheralBt, TRUE);
    Miku_GpioOutInit(MIKU_STOP_PORT, MIKU_STOP_PIN, TRUE);
    Miku_GpioOutInit(MIKU_DIR_PORT, MIKU_DIR_PIN, FALSE);
    Miku_GpioOutInit(MIKU_MOTOR_PWM_PORT, MIKU_MOTOR_PWM_PIN, FALSE);
    Miku_GpioOutInit(MIKU_BUZZER_PWM_PORT, MIKU_BUZZER_PWM_PIN, FALSE);

    Miku_TickInit();
    Miku_PwmEngineInit();
    Miku_AdcInit();

    Calibration_Init(); // 初始化校准
	Direction_Init(); // 初始化方向引脚
    Start_Init();     // 初始化启停引脚
	Buzzer_Init();      // 初始化蜂鸣器

    Miku_Buzzer_SetEnable(1u);
    Miku_DelayMs(500u);
    Miku_Buzzer_SetEnable(0u);
	
    Apply_Physical_Output(0);
}

void Instruction_Parse(char* cmd) {
    char mapped_cmd[32]; 

    // 1. 去除原指令中的回车换行符
    char* p = strchr(cmd, '\r');
    if(p) *p = '\0';
    p = strchr(cmd, '\n');
    if(p) *p = '\0';

    // 2. 默认将原指令安全地拷贝进缓存
    strncpy(mapped_cmd, cmd, sizeof(mapped_cmd) - 1);
    mapped_cmd[sizeof(mapped_cmd) - 1] = '\0';

    // 3. 构建快捷键映射表 (预留8个快捷键位置)
    typedef struct {
        const char* key;     // 快捷键字符
        const char* target;  // 实际替换的指令
    } Shortcut_t;
    
    // 你可以在这里极其方便地增加或修改快捷键
    Shortcut_t shortcuts[8] = {
        {"q", "on"},     // 快捷键1：发送 q 执行 on
        {"w", "off"},    // 快捷键2：发送 w 执行 off
        {"e", "bt15"},   // 快捷键3：发送 e 执行 bt15
        {"",  ""},       // 快捷键4 (预留，格式如 {"r", "50>100t5"})
        {"",  ""},       // 快捷键5 (预留)
        {"",  ""},       // 快捷键6 (预留)
        {"",  ""},       // 快捷键7 (预留)
        {"",  ""}        // 快捷键8 (预留)
    };

    // 遍历映射表进行拦截与替换 (使用 custom_stricmp 支持大小写混用，发 Q 和 q 都可以)
    for (int i = 0; i < 8; i++) {
        // 如果按键被定义了，且匹配当前接收到的指令
        if (shortcuts[i].key[0] != '\0' && custom_stricmp(mapped_cmd, shortcuts[i].key) == 0) {
            strcpy(mapped_cmd, shortcuts[i].target); // 将内容替换为目标指令
            break; // 匹配成功，直接跳出循环
        }
    }

    // 打印实际执行的指令，方便调试
    printf("CMD: %s\r\n", mapped_cmd);

    // 4. 处理校准模式切换
    if (custom_stricmp(mapped_cmd, "ci") == 0) {
        global_mode = MODE_CALIBRATION;
        Cal_Start();
        return;
    }
    if (custom_stricmp(mapped_cmd, "co") == 0) {
        Cal_End();
        global_mode = MODE_NORMAL;
        return;
    }

    // 5. 根据模式分发解析任务 (后续全部使用 mapped_cmd 进行解析)
    if (Instruction_GetMode() == MODE_CALIBRATION) {
        if (SelfTest_Parse(mapped_cmd)) return; 
        Cal_Process(mapped_cmd);
    } else {
        if (Help_Parse(mapped_cmd))       return; 
        if (Buzzer_Parse(mapped_cmd))     return; 
        if (Start_Parse(mapped_cmd))      return; 
        if (Direction_Parse(mapped_cmd))  return; 
        if (Ramp_Parse(mapped_cmd))       return;
        if (Timer_Parse(mapped_cmd))      return;
        if (Basic_Parse(mapped_cmd))      return;
    }
}

void Instruction_Loop(void) {
    uint32_t now = Miku_GetTick();
    if (global_mode == MODE_NORMAL) {
        Ramp_Update(now);
        Timer_Update(now);
		Buzzer_Update(now); // 新增蜂鸣器定时更新
    }

    // 每 20ms 刷新一次 DAC
    static uint32_t last_tick = 0;
    if (now - last_tick >= 20) {
        Apply_Physical_Output(current_speed);
        last_tick = now;
    }
}

// Getters & Setters
void Instruction_SetSpeed(float speed) { current_speed = speed; }
float Instruction_GetSpeed(void) { return current_speed; }
void Instruction_SetMode(SystemMode_t mode) { global_mode = mode; }
SystemMode_t Instruction_GetMode(void) { return global_mode; }

// 自定义实现不区分大小写的字符串比较
int custom_stricmp(const char *s1, const char *s2) {
    while (*s1 && (tolower((unsigned char)*s1) == tolower((unsigned char)*s2))) {
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}
