#include "system_info.h"
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_system.h"

// --- 模块私有定义 ---
#define STORAGE_NAMESPACE "sys_info" // NVS 的命名空间
#define SN_KEY "device_sn"           // SN号在NVS中存储的键

static const char *TAG = "SYSTEM_INFO";

// 用于在RAM中缓存系统信息的结构体
typedef struct {
    char serial_number[13];//SN // 预留12个字符 + 1个结束符
} system_info_t;

static system_info_t s_system_info;

// --- 公共函数的具体实现 ---
void system_info_init(void)
{
    // 1. 初始化 NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS 分区可能已满或损坏，尝试擦除并重新初始化
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // 2. 打开 NVS 命名空间
    nvs_handle_t my_handle;
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return;
    }

    // 3. 从 NVS 中读取 SN 号
    size_t required_size = sizeof(s_system_info.serial_number);
    err = nvs_get_str(my_handle, SN_KEY, s_system_info.serial_number, &required_size);
    switch (err) {
        case ESP_OK:
            ESP_LOGI(TAG, "SN loaded from NVS: %s", s_system_info.serial_number);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            ESP_LOGW(TAG, "SN not found in NVS, setting to default.");
            // 如果找不到，设置一个默认值
            strcpy(s_system_info.serial_number, "NOT_SET");
            break;
        default:
            ESP_LOGE(TAG, "Error (%s) reading SN from NVS!", esp_err_to_name(err));
    }


    // 4. 关闭 NVS 句柄
    nvs_close(my_handle);
}

//获取sn
const char* system_info_get_sn(void)
{
    // 直接从内存缓存中返回，速度快
    return s_system_info.serial_number;
}

//设置sn
esp_err_t system_info_set_sn(const char* sn)
{
    //sn规范验证
    if (sn == NULL || strlen(sn) >= sizeof(s_system_info.serial_number)) {
        return ESP_ERR_INVALID_ARG;
    }
    //创建句柄
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        return err;
    }

    // 写入新的SN到NVS
    err = nvs_set_str(my_handle, SN_KEY, sn);
    if (err == ESP_OK) {
        // 提交写入操作，使其永久生效
        err = nvs_commit(my_handle);
    }
    
    // 关闭句柄
    nvs_close(my_handle);

    if (err == ESP_OK) {
        // 更新内存中的缓存
        strcpy(s_system_info.serial_number, sn);
        ESP_LOGI(TAG, "SN has been set to: %s", sn);
    } else {
        ESP_LOGE(TAG, "Failed to set SN in NVS (%s)", esp_err_to_name(err));
    }
    
    return err;
}

//命令解析
void system_info_cmd_handler(const char *command, const char *args)
{
    if (strcmp(command, "test_device_get_sn") == 0) {
        printf("SN: %s\r\n", system_info_get_sn());
    } else if (strcmp(command, "test_device_set_sn") == 0) {
        if (args) {
            if(system_info_set_sn(args) == ESP_OK) {
                printf("OK\r\n");
            } else {
                printf("Error: Failed to set SN.\r\n");
            }
        } else {
            printf("Error: Missing value for set_sn.\r\n");
        }
    }
}