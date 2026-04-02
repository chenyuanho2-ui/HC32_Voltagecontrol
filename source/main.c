#include "ddl.h"
#include "hc32f005.h"
#include "gpio.h"
#include "uart.h"
#include "adc.h"
#include "sysctrl.h" 
#include "instruction_manager.h"
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

#define SOFTUART_RX_PORT GpioPort0
#define SOFTUART_RX_PIN  GpioPin3
#define SOFTUART_BAUD    9600u

// ADC 引脚 (此处使用 ADC通道3 即 P33)
#define ADC_CH       AdcExInputCH3
#define ADC_PORT     GpioPort3
#define ADC_PIN      GpioPin3

// ===================== 串口重定向 =====================
// 重定义 fputc 函数，使得 printf 可以通过串口输出
static void Tim0_StartForUartBaud(uint16_t reload)
{
    M0P_TIM0->CR_f.CTEN = FALSE;
    M0P_TIM0->CR_f.GATEP = 0u;
    M0P_TIM0->CR_f.GATE = 0u;
    M0P_TIM0->CR_f.PRS = 0u;
    M0P_TIM0->CR_f.TOGEN = TRUE;
    M0P_TIM0->CR_f.CT = 0u;
    M0P_TIM0->CR_f.MD = 1u;
    M0P_TIM0->ARR = reload;
    M0P_TIM0->CNT = reload;
    M0P_TIM0->CR_f.CTEN = TRUE;
}

static void Uart0_SendByteTimeout(uint8_t data)
{
    Uart_ClrStatus(M0P_UART0, UartTC);
    M0P_UART0->SBUF_f.SBUF = data;
    for (uint32_t i = 0; i < 0x20000u; i++)
    {
        if (TRUE == Uart_GetStatus(M0P_UART0, UartTC))
        {
            Uart_ClrStatus(M0P_UART0, UartTC);
            break;
        }
    }
}

static void SoftUart_DelayCycles(uint32_t cycles)
{
    if (0u == cycles)
    {
        return;
    }

    SysTick->LOAD = 0xFFFFFF;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_CLKSOURCE_Msk;

    uint32_t start = SysTick->VAL & 0xFFFFFFu;
    while ((((start - (SysTick->VAL & 0xFFFFFFu)) & 0xFFFFFFu)) < cycles)
    {
    }

    SysTick->CTRL = (SysTick->CTRL & (~SysTick_CTRL_ENABLE_Msk));
}

static uint8_t SoftUart_TryReadByte(uint8_t* out)
{
    if (TRUE == Gpio_GetInputIO(SOFTUART_RX_PORT, SOFTUART_RX_PIN))
    {
        return 0u;
    }

    uint32_t bit_cycles = (SystemCoreClock / SOFTUART_BAUD);
    if (bit_cycles < 16u)
    {
        bit_cycles = 16u;
    }

    SoftUart_DelayCycles(bit_cycles / 2u);
    if (TRUE == Gpio_GetInputIO(SOFTUART_RX_PORT, SOFTUART_RX_PIN))
    {
        return 0u;
    }

    SoftUart_DelayCycles(bit_cycles);

    uint8_t value = 0u;
    for (uint8_t i = 0u; i < 8u; i++)
    {
        if (TRUE == Gpio_GetInputIO(SOFTUART_RX_PORT, SOFTUART_RX_PIN))
        {
            value |= (uint8_t)(1u << i);
        }
        SoftUart_DelayCycles(bit_cycles);
    }

    uint8_t stop = (TRUE == Gpio_GetInputIO(SOFTUART_RX_PORT, SOFTUART_RX_PIN)) ? 1u : 0u;
    SoftUart_DelayCycles(bit_cycles);
    if (0u == stop)
    {
        return 0u;
    }

    *out = value;
    return 1u;
}

int fputc(int ch, FILE *f)
{
    (void)f;
    Uart0_SendByteTimeout((uint8_t)ch);
    return ch;
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
    stcBaud.bDbaud = FALSE;
    stcBaud.u32Pclk = Sysctrl_GetPClkFreq(); // 获取当前PCLK时钟频率
    stcBaud.u32Baud = SOFTUART_BAUD;
    uint16_t reload = Uart_SetBaudRate(M0P_UART0, &stcBaud);
    Tim0_StartForUartBaud(reload);

    // 5. 清除标志位并使能收发 (UartRenFunc 宏即可使能模式1下的收发)
    Uart_ClrStatus(M0P_UART0, UartRC);
    Uart_ClrStatus(M0P_UART0, UartTC);
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
    Uart_Init_Config();
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
        while (1u == SoftUart_TryReadByte(&c))
        {
            if (c == '\b' || c == 0x7Fu)
            {
                if (cmd_len > 0u)
                {
                    cmd_len--;
                }
                Uart0_SendByteTimeout('\b');
                Uart0_SendByteTimeout(' ');
                Uart0_SendByteTimeout('\b');
                continue;
            }

            if (c == '\r' || c == '\n')
            {
                Uart0_SendByteTimeout('\r');
                Uart0_SendByteTimeout('\n');

                if (cmd_len > 0u)
                {
                    cmd_buf[cmd_len] = '\0';
                    Instruction_Parse(cmd_buf);
                    cmd_len = 0u;
                }
                continue;
            }

            Uart0_SendByteTimeout(c);
            if (cmd_len < (sizeof(cmd_buf) - 1u))
            {
                cmd_buf[cmd_len++] = (char)c;
            }
        }
    }
}
