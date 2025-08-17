#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H
#include "esp_err.h"
#include "cmd_defs.h" // 包含通用定义


//初始化系统信息模块,初始化NVS并从Flash中加载配置信息
void system_info_init(void);

//设置SN
esp_err_t system_info_set_sn(const char* sn);

//读取SN
const char* system_info_get_sn(void);

//命令处理test_device_get_sn,test_device_set_sn=...
void system_info_cmd_handler(const char *command, const char *args,cmd_responder_t responder, void *context);

#endif // SYSTEM_INFO_H