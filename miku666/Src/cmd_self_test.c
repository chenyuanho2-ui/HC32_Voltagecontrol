/* cmd_self_test.c */
#include "cmd_self_test.h"
#include "instruction_manager.h"
#include "cmd_ramp.h"
#include "cmd_timer.h"
#include "cmd_buzzer.h"
#include <stdio.h>
#include <string.h>

// 辅助：获取稳定电压均值
static float Get_Average_Voltage(void) {
    float sum = 0;
    for (int i = 0; i < 20; i++) { // 采样 20 次
        sum += Miku_Adc_ReadVoltage();
        Miku_DelayMs(10);
    }
    return sum / 20.0f;
}

// 辅助：执行功能步骤并验证
static uint8_t Verify_Step(const char* name, char* cmd, float target_speed, float k, float b) {
    printf("Testing %s... ", name);
    Instruction_SetMode(MODE_NORMAL); // 必须在 Normal 模式才能跑 Loop
    
    // 执行解析
    if (strchr(cmd, '>') || strchr(cmd, '+') || strchr(cmd, '-')) Ramp_Parse(cmd);
    else if (strchr(cmd, 's')) Timer_Parse(cmd);
    
    // 模拟运行 1.2 秒（给 1s 的任务留出余量）
    uint32_t start = Miku_GetTick();
    while (Miku_GetTick() - start < 1200) {
        Instruction_Loop(); 
        Miku_DelayMs(5);
    }
    
    float v_real = Get_Average_Voltage();
    float v_exp = k * target_speed + b; // 根据线性公式计算理论电压
    
    if (v_real > (v_exp - 0.2f) && v_real < (v_exp + 0.2f)) {
        printf("[OK] (%.2fV)\r\n", v_real);
        return 1;
    } else {
        printf("[FAIL] (Real:%.2fV, Exp:%.2fV)\r\n", v_real, v_exp);
        return 0;
    }
}

// 修复链接错误：确保函数名完全一致且不带 static
int SelfTest_Parse(char* cmd) {
    if (custom_stricmp(cmd, "st") == 0 || custom_stricmp(cmd, "selfcheck") == 0) {
        SelfTest_Run();
        return 1;
    }
    return 0;
}

void SelfTest_Run(void) {
    float results[3]; // 20%, 60%, 100%
    float k, b;
    
    Instruction_SetMode(MODE_SELFCHECK);
    printf("\r\n=== START AUTO SELF-CHECK ===\r\n");

    // 1. 硬件点位采样
    float points[] = {20.0f, 60.0f, 100.0f};
    for (int i = 0; i < 3; i++) {
        Instruction_SetSpeed(points[i]);
        uint32_t start = Miku_GetTick();
        while (Miku_GetTick() - start < 800) {
            Instruction_Loop();
            Miku_DelayMs(5);
        }
        results[i] = Get_Average_Voltage();
        printf("Point %.0f%%: %.2fV\r\n", points[i], results[i]);
    }

    // 2. 计算线性公式: V = k*Speed + b
    k = (results[2] - results[0]) / 80.0f; 
    b = results[2] - k * 100.0f;
    printf("Linear Formula: V = %.4f * Speed + %.4f\r\n", k, b);

    // 3. 动态功能测试
    Verify_Step("Ramp 20>60t1",  "20>60t1",  60.0f,  k, b);
    Verify_Step("Ramp 60>100t1", "60>100t1", 100.0f, k, b);
    Verify_Step("Ramp Down 1s",  "100-t1",   0.0f,   k, b);
    Verify_Step("Timer 100s1",   "100s1",    0.0f,   k, b);

    // 4. 恢复环境
    Instruction_SetSpeed(0);
    Instruction_SetMode(MODE_NORMAL);
    printf("=== SELF-CHECK COMPLETE ===\r\n\r\n");
}
