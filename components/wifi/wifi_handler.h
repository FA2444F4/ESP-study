#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H
#include "cmd_defs.h"
// #include "mpu6050_handler.h"

// --- 关键修改：使用“前向声明”代替 #include ---
// 这行代码告诉编译器：我向你保证，有一个叫 mpu6050_data 的结构体，
// 它的别名叫 mpu6050_data_t。你不需要知道它里面有什么，
// 只需要知道它是一个存在的类型，可以用来声明指针。
typedef struct mpu6050_data mpu6050_data_t;
// --- 前向声明结束 ---


// --- 模块内部使用的数据结构 ---
typedef enum {
    CMD_TYPE_CONNECT,
    CMD_TYPE_DISCONNECT,
    CMD_TYPE_SCAN,
} wifi_cmd_type_t;// 定义可以发送给 Wi-Fi 控制任务的命令类型

//初始化Wi-Fi处理模块
void wifi_handler_init(void);

//Wi-Fi相关命令的处理器
void wifi_cmd_handler(const char *command, const char *args, cmd_responder_t responder, void *context);

//发送MPU6050数据 (供 mpu6050_handler 调用)
void wifi_handler_send_mpu_data(const mpu6050_data_t* data);

#endif // WIFI_HANDLER_H