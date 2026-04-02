/**
 * @file cmd_help.c
 * @brief 指令帮助查询模块
 * * 职责：
 * 1. 当用户输入 "instruction" 时，打印系统支持的所有控制语法说明。
 * 2. 集中管理指令集的文档，方便用户查阅。
 */

#include "cmd_help.h"
#include "instruction_manager.h"
#include <stdio.h>

uint8_t Help_Parse(char* cmd) {
    // 检查是否输入了 "instruction"
    if (custom_stricmp(cmd, "instruction") == 0 || custom_stricmp(cmd, "ins") == 0) {
        printf("\r\n================================================\r\n");
        printf("           PUMP CONTROL INSTRUCTION MENU          \r\n");
        printf("================================================\r\n");
        printf("1. Direct Set:\r\n");
        printf("   - [Value]      : Set speed 0-100%% immediately (PWM on P23 @10kHz).\r\n");
        printf("   - s            : Emergency Stop (Speed to 0).\r\n");
        printf("   - p            : Pause/Resume current process.\r\n\r\n");
        
        printf("2. Ramp :\r\n");
        printf("   - A>BtS        : Ramp from A%% to B%% in S seconds.\r\n");
        printf("   - A>B          : Ramp from A%% to B%% (Default 10s).\r\n");
        printf("   - A+tS / A-tS  : Speed up to 100%% or down to 0%%.\r\n\r\n");
        
        printf("3. Timer:\r\n");
        printf("   - AsS          : Run at A%% for S seconds, then stop.\r\n");
        printf("   - sS           : Keep current speed for S seconds.\r\n\r\n");
        
        printf("4. Calibration:\r\n");
        printf("   - ci           : Enter Calibration Mode.\r\n");
        printf("   - co           : Exit and save Calibration.\r\n");
        printf("================================================\r\n\r\n");
		
		
		printf("5. direction:\r\n");
        printf("   - change           : change direction.\r\n");
        printf("   - CW           : change direction to CW.\r\n");
		printf("   - CCW           : change direction to CCW.\r\n");
        printf("================================================\r\n\r\n");
		
		printf("6. Start/Stop Control:\r\n");
        printf("   - on           : start (P15 low).\r\n");
        printf("   - off          : stop  (P15 high).\r\n");
        printf("================================================\r\n\r\n");
		
		printf("7. Buzzer Control:\r\n");
		printf("   - buzzer/b     : Toggle buzzer ON/OFF (PWM on P24 @4kHz).\r\n");
		printf("   - bt[time]     : Ring buzzer after [time] seconds for 1 second.\r\n");
		printf("  Example: bt15 - ring after 15 seconds\r\n");
		printf("================================================\r\n\r\n");
		
		printf("8. selftest:\r\n");
		printf("   - selfcheck/st  : self-check routine.\r\n");
		printf("================================================\r\n\r\n");
        return 1;
    }
    return 0;
}
