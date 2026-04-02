/*
 * calibration.c
 *
 * 实现分段线性插值算法，用于校正非线性误差
 * 支持在线校准 (Online Calibration)
 */

#include "calibration.h"
#include <string.h> // for memcpy

#define MAX_CAL_POINTS 20

typedef struct {
    float measured; // X轴: 实际转速
    float command;  // Y轴: 指令值
} CalPoint_t;

// 当前生效的校准数据 (RAM中，掉电丢失，除非写入Flash，这里仅演示RAM实现)
static CalPoint_t active_points[MAX_CAL_POINTS];
static int active_count = 0;

// 校准过程中的临时缓冲区
static CalPoint_t temp_points[MAX_CAL_POINTS];
static int temp_count = 0;

// 默认出厂校准数据
static const float default_measured[] = {
    0.0f, 9.8f, 19.9f, 29.9f, 40.1f, 
    50.3f, 60.4f, 70.3f, 80.4f, 90.5f
};
static const float default_commands[] = {
    0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 
    50.0f, 60.0f, 70.0f, 80.0f, 90.0f
};

/**
 * @brief  初始化，加载默认值
 */
void Calibration_Init(void) {
    active_count = sizeof(default_measured) / sizeof(default_measured[0]);
    if (active_count > MAX_CAL_POINTS) active_count = MAX_CAL_POINTS;

    for (int i = 0; i < active_count; i++) {
        active_points[i].measured = default_measured[i];
        active_points[i].command = default_commands[i];
    }
}

/**
 * @brief  输入你想要的目标转速，返回应该设置的指令值
 */
float Calibration_GetCorrectedValue(float target_speed) {
    int i;
    
    // 如果没有校准数据，直接返回原值
    if (active_count < 2) {
        return target_speed;
    }

    // 1. 处理下限
    if (target_speed <= active_points[0].measured) {
        return active_points[0].command;
    }

    // 2. 处理上限 (线性外推)
    if (target_speed >= active_points[active_count - 1].measured) {
        float x1 = active_points[active_count - 2].measured;
        float x2 = active_points[active_count - 1].measured;
        float y1 = active_points[active_count - 2].command;
        float y2 = active_points[active_count - 1].command;
        
        // 防止除以零
        if ((x2 - x1) < 0.001f) return y2;

        float slope = (y2 - y1) / (x2 - x1);
        return y2 + slope * (target_speed - x2);
    }

    // 3. 区间查找与线性插值
    for (i = 0; i < active_count - 1; i++) {
        if (target_speed >= active_points[i].measured && target_speed < active_points[i+1].measured) {
            float x1 = active_points[i].measured;
            float x2 = active_points[i+1].measured;
            float y1 = active_points[i].command;
            float y2 = active_points[i+1].command;

            if ((x2 - x1) < 0.001f) return y1;

            float ratio = (target_speed - x1) / (x2 - x1);
            return y1 + ratio * (y2 - y1);
        }
    }

    return target_speed;
}

// --- 在线校准实现 ---

void Calibration_Start(void) {
    temp_count = 0;
    // 总是添加 0,0 点作为起点，防止用户忘记
    Calibration_AddPoint(0.0f, 0.0f); 
}

int Calibration_AddPoint(float command_val, float measured_val) {
    if (temp_count >= MAX_CAL_POINTS) return 0;
    
    temp_points[temp_count].command = command_val;
    temp_points[temp_count].measured = measured_val;
    temp_count++;
    return 1;
}

// 简单的冒泡排序，按 measured (X轴) 从小到大排序
static void Sort_Points(CalPoint_t* pts, int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - 1 - i; j++) {
            if (pts[j].measured > pts[j+1].measured) {
                CalPoint_t temp = pts[j];
                pts[j] = pts[j+1];
                pts[j+1] = temp;
            }
        }
    }
}

int Calibration_End(void) {
    if (temp_count < 2) return 0; // 点太少，无效

    // 1. 排序
    Sort_Points(temp_points, temp_count);

    // 2. 覆盖当前生效数据
    active_count = temp_count;
    for (int i = 0; i < active_count; i++) {
        active_points[i] = temp_points[i];
    }
    
    return active_count;
}
