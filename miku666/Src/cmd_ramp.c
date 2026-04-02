#include "cmd_ramp.h"
#include "instruction_manager.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static struct { 
    uint8_t active; 
    float start; 
    float end; 
    uint32_t start_t; 
    uint32_t dur_ms; 
} ramp;

// 内部辅助：解析时间后缀 't'，默认 10s
static float Parse_Ramp_Time(char* str) {
    char* t_ptr = strchr(str, 't');
    if (t_ptr) return atof(t_ptr + 1);
    return 10.0f;
}

uint8_t Ramp_Parse(char* cmd) {
    char* ptr;
    
    // 1. 处理 A>B 渐变
    if ((ptr = strchr(cmd, '>')) != NULL) {
        ramp.start = atof(cmd);
        ramp.end = atof(ptr + 1);
        float sec = Parse_Ramp_Time(ptr + 1);
        
        ramp.dur_ms = (uint32_t)(sec * 1000.0f);
        ramp.start_t = Miku_GetTick();
        ramp.active = 1;
        Instruction_SetSpeed(ramp.start);
        printf(">> Ramp: %.1f -> %.1f in %.1fs\r\n", ramp.start, ramp.end, sec);
        return 1;
    }
    
    // 2. 处理 + (升至100%) 和 - (降至0%)
    if ((ptr = strchr(cmd, '+')) != NULL || (ptr = strchr(cmd, '-')) != NULL) {
        char op = *ptr;
        ramp.start = (ptr == cmd) ? Instruction_GetSpeed() : atof(cmd);
        ramp.end = (op == '+') ? 100.0f : 0.0f;
        float sec = Parse_Ramp_Time(ptr + 1);
        
        ramp.dur_ms = (uint32_t)(sec * 1000.0f);
        ramp.start_t = Miku_GetTick();
        ramp.active = 1;
        Instruction_SetSpeed(ramp.start);
        printf(">> Ramp %s: %.1f -> %.1f in %.1fs\r\n", 
               (op == '+') ? "UP" : "DOWN", ramp.start, ramp.end, sec);
        return 1;
    }
    return 0;
}

void Ramp_Update(uint32_t now) {
    if (!ramp.active) return;
    
    uint32_t elapsed = now - ramp.start_t;
    if (elapsed >= ramp.dur_ms) {
        Instruction_SetSpeed(ramp.end);
        ramp.active = 0;
        printf(">> Ramp Done.\r\n");
    } else {
        float progress = (float)elapsed / (float)ramp.dur_ms;
        float current = ramp.start + (ramp.end - ramp.start) * progress;
        Instruction_SetSpeed(current);
    }
}

void Ramp_Stop(void) { ramp.active = 0; }
