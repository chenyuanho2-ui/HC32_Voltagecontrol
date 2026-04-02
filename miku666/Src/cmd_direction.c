/**
 * @file cmd_direction.c
 * @brief 旋转方向控制模块
 * * 职责：
 * 1. 管理 PB12 引脚电平：低电平为顺时针 (CW)，高电平为逆时针 (CCW)。
 * 2. 解析 'ch0'：设置为顺时针。
 * 3. 解析 'ch1'：设置为逆时针。
 * 4. 解析 'ch' ：切换当前转向。
 */

#include "cmd_direction.h"
#include "instruction_manager.h"
#include <stdio.h>

static uint8_t g_dir_ccw = 0u;

void Direction_Init(void) {
    g_dir_ccw = 0u;
    Miku_Motor_SetDirectionCcw(g_dir_ccw);
}

uint8_t Direction_Parse(char* cmd) {
    if (custom_stricmp(cmd, "CW") == 0) {
        g_dir_ccw = 0u;
        Miku_Motor_SetDirectionCcw(g_dir_ccw);
        printf(">> Direction: Clockwise (CW)\r\n");
        return 1;
    }
    else if (custom_stricmp(cmd, "CCW") == 0) {
        g_dir_ccw = 1u;
        Miku_Motor_SetDirectionCcw(g_dir_ccw);
        printf(">> Direction: Counter-Clockwise (CCW)\r\n");
        return 1;
    }
    else if (custom_stricmp(cmd, "change") == 0) {
        g_dir_ccw ^= 1u;
        Miku_Motor_SetDirectionCcw(g_dir_ccw);
        printf(">> Direction Toggled: %s\r\n", (g_dir_ccw != 0u) ? "CCW" : "CW");
        return 1;
    }
    return 0;
}
