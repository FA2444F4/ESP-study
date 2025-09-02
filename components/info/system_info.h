#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H
#include "esp_err.h"
#include "cmd_defs.h" // 包含通用定义


//初始化系统信息模块,初始化NVS并从Flash中加载配置信息
void system_info_init(void);

//设置
esp_err_t system_info_set_sn(const char* sn);
esp_err_t system_info_set_wifi_name(const char* wifi_name);
esp_err_t system_info_set_wifi_password(const char* wifi_password);
esp_err_t system_info_set_gyro_offsets(float offset_x, float offset_y, float offset_z);

//读取
const char* system_info_get_sn(void);
const char* system_info_get_wifi_name(void);
const char* system_info_get_wifi_password(void);
void system_info_get_gyro_offsets(float* offset_x, float* offset_y, float* offset_z);


//命令处理test_device_get_sn,test_device_set_sn=...
void system_info_cmd_handler(const char *command, const char *args,cmd_responder_t responder, void *context);

#endif // SYSTEM_INFO_H