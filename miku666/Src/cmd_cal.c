/**
 * @file cmd_cal.c
 * @brief 在线校准模式指令模块
 * * 职责：
 * 1. 管理校准状态机（等待输入指令值 -> 等待输入实测值）。
 * 2. 实现 'ci' (开始校准) 和 'co' (完成校准) 的业务逻辑。
 * 3. 配合 calibration.c 记录样本点，从而生成非线性补偿曲线。
 * * 注意：进入校准模式后，manager 会自动跳过常规的渐变和定时更新逻辑。
 */

#include "cmd_cal.h"
#include "instruction_manager.h"
#include "calibration.h"
#include <stdio.h>
#include <stdlib.h>

static enum { WAIT_CMD, WAIT_MEASURE } cal_state;
static float pending_cmd;

void Cal_Start(void) {
    cal_state = WAIT_CMD;
    Calibration_Start(); //
    printf(">> [CAL START] Input Command Value:\r\n");
}

void Cal_Process(char* cmd) {
    float val = atof(cmd);
    if (cal_state == WAIT_CMD) {
        pending_cmd = val;
        Instruction_SetSpeed(val);
        cal_state = WAIT_MEASURE;
        printf(">> Measure and input ACTUAL value:\r\n");
    } else {
        Calibration_AddPoint(pending_cmd, val); //
        cal_state = WAIT_CMD;
        printf(">> Recorded. Input next Command or 'co':\r\n");
    }
}

void Cal_End(void) {
    int count = Calibration_End(); //
    printf(">> [CAL DONE] %d points.\r\n", count);
}
