#include "cmd_timer.h"
#include "instruction_manager.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

static struct { 
    uint8_t active; 
    uint32_t start_t; 
    uint32_t delay_ms; 
} timer;

uint8_t Timer_Parse(char* cmd) {
    char* ptr = strchr(cmd, 's');
    // 确保不是单独的 's' (那是急停)，且包含 's' 字符
    if (ptr != NULL && strlen(cmd) > 1) {
        // 1. 如果 s 前面有数字，则设置速度
		if (!isdigit((unsigned char)*(ptr + 1)) && *(ptr + 1) != '.') {
            return 0; // 识别为非定时指令，交给 SelfTest 处理
        }       
		if (ptr != cmd) {
            float speed = atof(cmd);
            Instruction_SetSpeed(speed);
        }
        // 2. 解析 s 后面的时间 (秒)
        float seconds = atof(ptr + 1);
        if (seconds <= 0) seconds = 1.0f; // 防止非法输入

        timer.delay_ms = (uint32_t)(seconds * 1000.0f);
        timer.start_t = Miku_GetTick();
        timer.active = 1;
        
        printf(">> Timer Set: Run %.1f, Stop in %.1fs\r\n", Instruction_GetSpeed(), seconds);
        return 1;
    }
    return 0;
}

void Timer_Update(uint32_t now) {
    if (timer.active) {
        if (now - timer.start_t >= timer.delay_ms) {
            Instruction_SetSpeed(0.0f);
            timer.active = 0;
            printf(">> Timer: Time's up. Stopped.\r\n");
        }
    }
}

void Timer_Stop(void) { timer.active = 0; }
