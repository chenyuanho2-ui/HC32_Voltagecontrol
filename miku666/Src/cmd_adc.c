/**
 * @file cmd_adc.c
 * @brief ADC 电压测量与 5Hz 定时打印模块
 * * 职责：
 * 1. 解析 "adc" 指令，切换测量任务的开启与关闭。
 * 2. 使用 PA0 (ADC_IN0) 进行模拟量采集。
 * 3. 在开启状态下，以 5Hz (每 200ms) 的频率向串口打印实时电压。
 */

#include "cmd_adc.h"
#include "instruction_manager.h"
#include <stdio.h>

static uint8_t adc_enabled = 0;
static uint32_t last_print_tick = 0;

uint8_t ADC_Parse(char* cmd) {
    if (custom_stricmp(cmd, "adc") == 0) {
        adc_enabled = !adc_enabled; // 切换开关状态
        if (adc_enabled) {
            printf(">> ADC Measurement: ON (5Hz)\r\n");
            last_print_tick = Miku_GetTick();
        } else {
            printf(">> ADC Measurement: OFF\r\n");
        }
        return 1;
    }
    return 0;
}

void ADC_Update(uint32_t now) {
    if (!adc_enabled) return;

    // 5Hz 频率判断 (200ms)
    if (now - last_print_tick >= 200) {
        last_print_tick = now;

        uint16_t raw_val = Miku_Adc_ReadRaw();
        float voltage = Miku_Adc_ReadVoltage();
        printf(">> ADC Raw: %u, Voltage: %.2f V\r\n", (unsigned)raw_val, voltage);
    }
}
