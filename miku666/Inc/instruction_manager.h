#ifndef INSTRUCTION_MANAGER_H_
#define INSTRUCTION_MANAGER_H_

#include <stdint.h>

// 定义系统运行模式
typedef enum {
    MODE_NORMAL = 0,
    MODE_CALIBRATION,
	MODE_SELFCHECK
} SystemMode_t;

void Instruction_Init(void);
void Instruction_Parse(char* cmd);
void Instruction_Loop(void);

// 供子模块使用的公共 API
void Instruction_SetSpeed(float speed);
float Instruction_GetSpeed(void);
void Instruction_SetMode(SystemMode_t mode);
SystemMode_t Instruction_GetMode(void);
// 在 instruction_manager.h 中添加
int custom_stricmp(const char *s1, const char *s2);

uint32_t Miku_GetTick(void);
void Miku_DelayMs(uint32_t ms);

void Miku_Motor_SetSpeedPercent(float speed_percent);
void Miku_Motor_SetStop(uint8_t stop);
void Miku_Motor_SetDirectionCcw(uint8_t ccw);

void Miku_Buzzer_SetEnable(uint8_t enable);
void Miku_Buzzer_SetVolume(uint8_t volume_percent);

uint16_t Miku_Adc_ReadRaw(void);
float Miku_Adc_ReadVoltage(void);

#endif
