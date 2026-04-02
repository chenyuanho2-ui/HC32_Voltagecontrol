#include "ddl.h"
#include "hc32f005.h"
#include "gpio.h"
#include "uart.h"
#include "adc.h"
#include "sysctrl.h" 
#include "instruction_manager.h"
#include "interrupts_hc32f005.h"
#include <stdio.h>


// ===================== 核心配置 =====================
// LCD/LED 引脚 (P26)
#define LED_PORT    GpioPort2
#define LED_PIN     GpioPin6

// 串口 引脚 (TX: P02, RX: P03)
#define UART_TX_PORT GpioPort0
#define UART_TX_PIN  GpioPin2
#define UART_RX_PORT GpioPort0
#define UART_RX_PIN  GpioPin3
#define UART_AF      GpioAf1

#define SOFTUART_TX_PORT GpioPort0
#define SOFTUART_TX_PIN  GpioPin2
#define SOFTUART_RX_PORT GpioPort0
#define SOFTUART_RX_PIN  GpioPin3
#define SOFTUART_BAUD    9600u
#define SOFTUART_TICKS_PER_BIT 8u
#define SOFTUART_TICK_HZ (SOFTUART_BAUD * SOFTUART_TICKS_PER_BIT)
#define SOFTUART_RX_START_TICKS (SOFTUART_TICKS_PER_BIT + (SOFTUART_TICKS_PER_BIT / 2u))
#define SOFTUART_RX_BUF_SIZE 128u
#define SOFTUART_TX_BUF_SIZE 256u

// ADC 引脚 (此处使用 ADC通道3 即 P33)
#define ADC_CH       AdcExInputCH3
#define ADC_PORT     GpioPort3
#define ADC_PIN      GpioPin3

static volatile uint8_t g_su_rx_buf[SOFTUART_RX_BUF_SIZE];
static volatile uint16_t g_su_rx_w = 0u;
static volatile uint16_t g_su_rx_r = 0u;

static volatile uint8_t g_su_tx_buf[SOFTUART_TX_BUF_SIZE];
static volatile uint16_t g_su_tx_w = 0u;
static volatile uint16_t g_su_tx_r = 0u;

static volatile uint8_t g_su_tx_active = 0u;
static volatile uint8_t g_su_tx_bitpos = 0u;
static volatile uint8_t g_su_tx_tick = 0u;
static volatile uint16_t g_su_tx_frame = 0u;

static volatile uint8_t g_su_rx_active = 0u;
static volatile uint8_t g_su_rx_bitpos = 0u;
static volatile uint8_t g_su_rx_value = 0u;
static volatile uint8_t g_su_rx_sample_ticks = 0u;

static uint16_t SoftUart_CalcTimReload(uint32_t tick_hz)
{
    uint32_t pclk = Sysctrl_GetPClkFreq();
    if (0u == pclk)
    {
        pclk = SystemCoreClock;
    }
    uint32_t delta = pclk / tick_hz;
    if (delta < 1u)
    {
        delta = 1u;
    }
    if (delta > 65535u)
    {
        delta = 65535u;
    }
    return (uint16_t)(0x10000u - delta);
}

static void SoftUart_TxKickFromIsr(void)
{
    if (g_su_tx_active)
    {
        return;
    }
    if (g_su_tx_r == g_su_tx_w)
    {
        return;
    }

    uint8_t b = g_su_tx_buf[g_su_tx_r];
    g_su_tx_r = (uint16_t)((g_su_tx_r + 1u) & (SOFTUART_TX_BUF_SIZE - 1u));

    g_su_tx_frame = (uint16_t)((1u << 9) | ((uint16_t)b << 1));
    g_su_tx_bitpos = 0u;
    g_su_tx_tick = 0u;
    g_su_tx_active = 1u;
}

static void SoftUart_WriteByte(uint8_t b)
{
    uint16_t next;
    do
    {
        __disable_irq();
        next = (uint16_t)((g_su_tx_w + 1u) & (SOFTUART_TX_BUF_SIZE - 1u));
        if (next != g_su_tx_r)
        {
            g_su_tx_buf[g_su_tx_w] = b;
            g_su_tx_w = next;
            if (!g_su_tx_active)
            {
                SoftUart_TxKickFromIsr();
            }
            __enable_irq();
            break;
        }
        __enable_irq();
    } while (1);
}

static uint8_t SoftUart_ReadByte(uint8_t* out)
{
    uint8_t ok = 0u;
    __disable_irq();
    if (g_su_rx_r != g_su_rx_w)
    {
        *out = g_su_rx_buf[g_su_rx_r];
        g_su_rx_r = (uint16_t)((g_su_rx_r + 1u) & (SOFTUART_RX_BUF_SIZE - 1u));
        ok = 1u;
    }
    __enable_irq();
    return ok;
}

int fputc(int ch, FILE *f)
{
    (void)f;
    if ('\n' == ch)
    {
        SoftUart_WriteByte('\r');
    }
    SoftUart_WriteByte((uint8_t)ch);
    return ch;
}

static void SoftUart_Init(void)
{
    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio, TRUE);
    Sysctrl_SetPeripheralGate(SysctrlPeripheralBt, TRUE);

    stc_gpio_cfg_t tx;
    DDL_ZERO_STRUCT(tx);
    tx.enDir = GpioDirOut;
    tx.enDrv = GpioDrvH;
    tx.enPu = GpioPuDisable;
    tx.enPd = GpioPdDisable;
    tx.enOD = GpioOdDisable;
    Gpio_Init(SOFTUART_TX_PORT, SOFTUART_TX_PIN, &tx);
    Gpio_WriteOutputIO(SOFTUART_TX_PORT, SOFTUART_TX_PIN, TRUE);

    stc_gpio_cfg_t rx;
    DDL_ZERO_STRUCT(rx);
    rx.enDir = GpioDirIn;
    rx.enDrv = GpioDrvH;
    rx.enPu = GpioPuEnable;
    rx.enPd = GpioPdDisable;
    rx.enOD = GpioOdDisable;
    Gpio_Init(SOFTUART_RX_PORT, SOFTUART_RX_PIN, &rx);

    Gpio_EnableIrq(GpioPort0, GpioPin3, GpioIrqFalling);
    Gpio_ClearIrq(GpioPort0, GpioPin3);
    EnableNvic(PORT0_IRQn, IrqLevel1, TRUE);

    uint16_t reload = SoftUart_CalcTimReload(SOFTUART_TICK_HZ);
    M0P_TIM0->CR_f.CTEN = FALSE;
    M0P_TIM0->CR_f.GATEP = 0u;
    M0P_TIM0->CR_f.GATE = 0u;
    M0P_TIM0->CR_f.PRS = 0u;
    M0P_TIM0->CR_f.TOGEN = FALSE;
    M0P_TIM0->CR_f.CT = 0u;
    M0P_TIM0->CR_f.MD = 1u;
    M0P_TIM0->ARR = reload;
    M0P_TIM0->CNT = reload;
    M0P_TIM0->ICLR_f.UIF = FALSE;
    M0P_TIM0->CR_f.UIE = TRUE;
    EnableNvic(TIM0_IRQn, IrqLevel0, TRUE);
    M0P_TIM0->CR_f.CTEN = TRUE;
}

// ===================== 外设初始化 =====================
void Uart_Init_Config(void)
{
    stc_uart_cfg_t  stcCfg;
    stc_uart_baud_cfg_t stcBaud;
    stc_gpio_cfg_t stcTxCfg;
    stc_gpio_cfg_t stcRxCfg;
    
    DDL_ZERO_STRUCT(stcCfg);
    DDL_ZERO_STRUCT(stcBaud);
    DDL_ZERO_STRUCT(stcTxCfg);
    DDL_ZERO_STRUCT(stcRxCfg);

    // 1. 开启UART0外设时钟
    Sysctrl_SetPeripheralGate(SysctrlPeripheralUart0, TRUE);
    Sysctrl_SetPeripheralGate(SysctrlPeripheralBt, TRUE);

    stcTxCfg.enDir = GpioDirOut;
    stcTxCfg.enDrv = GpioDrvH;
    stcTxCfg.enPu = GpioPuDisable;
    stcTxCfg.enPd = GpioPdDisable;
    stcTxCfg.enOD = GpioOdDisable;
    Gpio_Init(UART_TX_PORT, UART_TX_PIN, &stcTxCfg);
    Gpio_SetAfMode(UART_TX_PORT, UART_TX_PIN, UART_AF);

    stcRxCfg.enDir = GpioDirIn;
    stcRxCfg.enDrv = GpioDrvH;
    stcRxCfg.enPu = GpioPuEnable;
    stcRxCfg.enPd = GpioPdDisable;
    stcRxCfg.enOD = GpioOdDisable;
    Gpio_Init(UART_RX_PORT, UART_RX_PIN, &stcRxCfg);

    // 3. UART工作模式配置 (模式1: 8位异步，无校验)
    stcCfg.enRunMode = UartMode1;
    Uart_Init(M0P_UART0, &stcCfg);

    // 4. 设置波特率 (9600)
    stcBaud.enMode = UartMode1;
    stcBaud.bDbaud = TRUE;
    stcBaud.u32Pclk = Sysctrl_GetPClkFreq(); // 获取当前PCLK时钟频率
    stcBaud.u32Baud = SOFTUART_BAUD;
    uint16_t reload = Uart_SetBaudRate(M0P_UART0, &stcBaud);
    (void)reload;

    // 5. 清除标志位并使能收发 (UartRenFunc 宏即可使能模式1下的收发)
    M0P_UART0->ICR_f.RICLR = 0u;
    M0P_UART0->ICR_f.TICLR = 0u;
    M0P_UART0->ICR_f.FECLR = 0u;
    Uart_EnableFunc(M0P_UART0, UartRenFunc);
}

void Adc_Init_Config(void)
{
    stc_adc_cfg_t stcAdcCfg;
    stc_adc_norm_cfg_t stcNormCfg;
    
    DDL_ZERO_STRUCT(stcAdcCfg);
    DDL_ZERO_STRUCT(stcNormCfg);

    // 1. 开启ADC和BGR(内部参考电压)时钟
    Sysctrl_SetPeripheralGate(SysctrlPeripheralAdcBgr, TRUE);
    M0P_BGR->CR |= 0x1u;
    delay10us(2);

    // 2. 将模拟引脚配置为模拟输入模式 (Analog)
    Gpio_SetAnalogMode(ADC_PORT, ADC_PIN);

    // 3. ADC 基础配置
    stcAdcCfg.enAdcOpMode = AdcNormalMode;            // 单通道单次采样模式
    stcAdcCfg.enAdcClkSel = AdcClkSysTDiv2;           // 时钟分频 PCLK/2
    stcAdcCfg.enAdcSampTimeSel = AdcSampTime8Clk;     // 采样周期数设为 8 个时钟
    stcAdcCfg.enAdcRefVolSel = RefVolSelInBgr2p5;     // 参考电压选择内部2.5V
    stcAdcCfg.bAdcInBufEn = FALSE;                    // 不使用输入增益/缓冲
    Adc_Init(&stcAdcCfg);

    // 4. 单次采样通道配置
    stcNormCfg.enAdcNormModeCh = ADC_CH;              // 选中 ADC_CH (通道3)
    stcNormCfg.bAdcResultAccEn = FALSE;               // 关闭结果自动累加
    Adc_ConfigNormMode(&stcAdcCfg, &stcNormCfg);

    // 5. 使能ADC模块
    Adc_Enable();
}

uint16_t Get_Adc_Value(void)
{
    uint16_t adc_val = 0;
    uint32_t timeout = 1000000u;
    
    // 1. 启动ADC转换
    Adc_Start();
    
    // 2. 等待转换完成 (PollBusyState 为 TRUE 表示仍在转换中)
    while ((TRUE == Adc_PollBusyState()) && (timeout-- > 0u))
    {
    }
    if (0u == timeout)
    {
        return 0;
    }
    
    // 3. 读取结果并通过形参返回
    Adc_GetResult(&adc_val);
    
    return adc_val;
}

// ===================== 主函数 =====================
int main(void)
{
    // 1. 系统初始化
    SystemInit();
    
    // 2. 开启GPIO时钟
    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio, TRUE);
    
    // 3. 配置P26(LCD/LED)为输出模式11
    stc_gpio_cfg_t stcGpioCfg;
    DDL_ZERO_STRUCT(stcGpioCfg);
    stcGpioCfg.enDir = GpioDirOut;
    stcGpioCfg.enDrv = GpioDrvH;
    stcGpioCfg.enPu = GpioPuDisable;
    stcGpioCfg.enPd = GpioPdDisable;
    stcGpioCfg.enOD = GpioOdDisable;
    Gpio_Init(LED_PORT, LED_PIN, &stcGpioCfg);
    Gpio_WriteOutputIO(LED_PORT, LED_PIN, TRUE); // 默认高电平(熄灭)
    
    // 4. 初始化 串口
    SoftUart_Init();
    Instruction_Init();
    printf("\r\nstart\r\n");

    while(1)
    {
        Instruction_Loop();

        static uint32_t last_led_ms = 0u;
        uint32_t now = Miku_GetTick();
        if ((now - last_led_ms) >= 500u)
        {
            last_led_ms = now;
            boolean_t cur = Gpio_ReadOutputIO(LED_PORT, LED_PIN);
            Gpio_WriteOutputIO(LED_PORT, LED_PIN, (cur == FALSE) ? TRUE : FALSE);
        }

        static char cmd_buf[64];
        static uint32_t cmd_len = 0u;

        uint8_t c = 0u;
        while (1u == SoftUart_ReadByte(&c))
        {
            if (c == '\b' || c == 0x7Fu)
            {
                if (cmd_len > 0u)
                {
                    cmd_len--;
                }
                SoftUart_WriteByte('\b');
                SoftUart_WriteByte(' ');
                SoftUart_WriteByte('\b');
                continue;
            }

            if (c == '\r' || c == '\n')
            {
                SoftUart_WriteByte('\r');
                SoftUart_WriteByte('\n');

                if (cmd_len > 0u)
                {
                    cmd_buf[cmd_len] = '\0';
                    Instruction_Parse(cmd_buf);
                    cmd_len = 0u;
                }
                continue;
            }

            SoftUart_WriteByte(c);
            if (cmd_len < (sizeof(cmd_buf) - 1u))
            {
                cmd_buf[cmd_len++] = (char)c;
            }
        }
    }
}

void Tim0_IRQHandler(void)
{
    if (TRUE != M0P_TIM0->IFR_f.UIF)
    {
        return;
    }
    M0P_TIM0->ICLR_f.UIF = FALSE;

    if (g_su_tx_active)
    {
        if (0u == g_su_tx_tick)
        {
            uint8_t bit = (uint8_t)((g_su_tx_frame >> g_su_tx_bitpos) & 1u);
            Gpio_WriteOutputIO(SOFTUART_TX_PORT, SOFTUART_TX_PIN, (bit != 0u) ? TRUE : FALSE);
        }

        g_su_tx_tick++;
        if (g_su_tx_tick >= SOFTUART_TICKS_PER_BIT)
        {
            g_su_tx_tick = 0u;
            g_su_tx_bitpos++;
            if (g_su_tx_bitpos >= 10u)
            {
                g_su_tx_active = 0u;
                Gpio_WriteOutputIO(SOFTUART_TX_PORT, SOFTUART_TX_PIN, TRUE);
                SoftUart_TxKickFromIsr();
            }
        }
    }
    else
    {
        SoftUart_TxKickFromIsr();
    }

    if (g_su_rx_active)
    {
        if (g_su_rx_sample_ticks > 0u)
        {
            g_su_rx_sample_ticks--;
        }
        if (0u == g_su_rx_sample_ticks)
        {
            uint8_t bit = (TRUE == Gpio_GetInputIO(SOFTUART_RX_PORT, SOFTUART_RX_PIN)) ? 1u : 0u;
            if (g_su_rx_bitpos < 8u)
            {
                g_su_rx_value |= (uint8_t)(bit << g_su_rx_bitpos);
                g_su_rx_bitpos++;
                g_su_rx_sample_ticks = SOFTUART_TICKS_PER_BIT;
            }
            else
            {
                if (bit != 0u)
                {
                    uint16_t next = (uint16_t)((g_su_rx_w + 1u) & (SOFTUART_RX_BUF_SIZE - 1u));
                    if (next != g_su_rx_r)
                    {
                        g_su_rx_buf[g_su_rx_w] = g_su_rx_value;
                        g_su_rx_w = next;
                    }
                }
                g_su_rx_active = 0u;
                Gpio_EnableIrq(GpioPort0, GpioPin3, GpioIrqFalling);
            }
        }
    }
}

void Port0_IRQHandler(void)
{
    if (TRUE == Gpio_GetIrqStatus(GpioPort0, GpioPin3))
    {
        Gpio_ClearIrq(GpioPort0, GpioPin3);
        if (!g_su_rx_active)
        {
            if (FALSE == Gpio_GetInputIO(SOFTUART_RX_PORT, SOFTUART_RX_PIN))
            {
                g_su_rx_active = 1u;
                g_su_rx_bitpos = 0u;
                g_su_rx_value = 0u;
                g_su_rx_sample_ticks = SOFTUART_RX_START_TICKS;
                Gpio_DisableIrq(GpioPort0, GpioPin3, GpioIrqFalling);
            }
        }
    }
}
