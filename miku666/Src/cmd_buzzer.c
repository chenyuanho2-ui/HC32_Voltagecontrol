/**
 * @file cmd_buzzer.c
 * @brief 蜂鸣器控制模块
 * * 职责：
 * 1. 管理 PA3 引脚电平：低电平触发蜂鸣器响，高电平静默。
 * 2. 解析 'buzzer' 或 'b' 指令：切换蜂鸣器开关状态。
 * 3. 解析 'bt[time]' 指令：延时指定秒数后响1秒蜂鸣器。
 * 4. 提供蜂鸣器状态查询和定时控制功能。
 */

#include "cmd_buzzer.h"
#include "instruction_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static uint8_t buzzer_state = 0;         // 0=关闭，1=开启（常开模式）
static uint8_t buzzer_timer_active = 0;  // 1=正在执行定时任务
static uint32_t buzzer_start_time = 0;   // 定时任务开始时间
static uint32_t buzzer_delay_ms = 0;     // 延时时间（毫秒）
static uint8_t buzzer_volume_percent = 15; // 0-100，建议 5-30

void Buzzer_Init(void) {
    Miku_Buzzer_SetVolume(buzzer_volume_percent);
    Miku_Buzzer_SetEnable(0u);
    buzzer_state = 0;
    buzzer_timer_active = 0;
}

uint8_t Buzzer_Parse(char* cmd) {
    // 处理基本开关指令
    if (custom_stricmp(cmd, "buzzer") == 0 || custom_stricmp(cmd, "b") == 0) {
        // 取消任何正在进行的定时任务
        buzzer_timer_active = 0;
        
        // 切换蜂鸣器状态
        buzzer_state = !buzzer_state;
        
        if (buzzer_state) {
            Miku_Buzzer_SetVolume(buzzer_volume_percent);
            Miku_Buzzer_SetEnable(1u);
            printf(">> Buzzer: ON\r\n");
        } else {
            Miku_Buzzer_SetEnable(0u);
            printf(">> Buzzer: OFF\r\n");
        }
        return 1;
    }
    
    // 处理定时蜂鸣指令 bt[time]
    if (strncmp(cmd, "bt", 2) == 0 && strlen(cmd) > 2) {
        char* time_str = cmd + 2;  // 跳过 "bt"
        char* endptr;
        float delay_seconds = strtof(time_str, &endptr);
        
        // 检查转换是否成功
        if (endptr != time_str && *endptr == '\0') {
            buzzer_timer_active = 1;
            buzzer_start_time = Miku_GetTick();
            buzzer_delay_ms = (uint32_t)(delay_seconds * 1000); // 转换为毫秒
            
            printf(">> Buzzer Timer: Will ring after %.1f seconds for 1 second\r\n", delay_seconds);
            return 1;
        }
    }
    
    return 0;
}

void Buzzer_Update(uint32_t now) {
    if (buzzer_timer_active) {
        uint32_t elapsed = now - buzzer_start_time;
        
        // 如果延时期间未到，什么也不做
        if (elapsed < buzzer_delay_ms) {
            return;
        }
        
        // 延时期间已到，开始响1秒
        if (elapsed < buzzer_delay_ms + 1000) {
            Miku_Buzzer_SetVolume(buzzer_volume_percent);
            Miku_Buzzer_SetEnable(1u);
        } else {
            Miku_Buzzer_SetEnable(0u);
            buzzer_timer_active = 0; // 任务完成，取消定时任务
        }
    }
}

void Buzzer_SetState(uint8_t state) {
    // 取消任何正在进行的定时任务
    buzzer_timer_active = 0;
    
    buzzer_state = state;
    Miku_Buzzer_SetVolume(buzzer_volume_percent);
    Miku_Buzzer_SetEnable(state ? 1u : 0u);
}

uint8_t Buzzer_GetState(void) {
    return buzzer_state;
}

uint8_t Buzzer_IsTimerActive(void) {
    return buzzer_timer_active;
}
