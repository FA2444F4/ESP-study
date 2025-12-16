#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "cJSON.h"
#include <math.h> 


#include "wifi_handler.h"
#include "mpu6050_handler.h"
#include "system_info.h"

/* --- 模块私有定义 --- */
#define UDP_LISTEN_PORT       50001
#define WIFI_CONNECT_TIMEOUT_MS  20000
#define DISCOVERY_REQUEST_MSG  "DISCOVER_ESP32_REQUEST"  // PC广播的特定数据包内容
#define DISCOVERY_RESPONSE_MSG "DISCOVER_ESP32_RESPONSE" // ESP32响应给PC的内容

static const char *TAG = "WIFI_HANDLER";




/* --- 模块私有全局变量 --- */
static EventGroupHandle_t s_wifi_event_group;// 用于等待Wi-Fi连接成功的事件组
const int WIFI_CONNECTED_BIT = BIT0;
const static int WIFI_FAIL_BIT = BIT1;
static volatile char s_ip_address[16] = "0.0.0.0";
static char s_wifi_ssid[33] = {0};    
static char s_wifi_password[65] = {0};
static QueueHandle_t s_wifi_cmd_queue = NULL;
static TaskHandle_t s_udp_discovery_task_handle = NULL;
static int s_udp_socket = -1; // 全局socket句柄
static struct sockaddr_in s_target_addr; // 存储目标PC的地址
static bool s_target_addr_valid = false; // 标记PC地址是否有效
static SemaphoreHandle_t s_target_addr_mutex; // 保护目标地址的互斥锁


// 定义一个结构体，用于在队列中传递命令及其响应所需的信息
typedef struct {
    wifi_cmd_type_t type;         // 命令类型
    cmd_responder_t responder;    // 用于响应的回调函数
    void* context;      // 响应器所需的上下文
} wifi_cmd_request_t;

/* --- 模块私有函数声明 --- */
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void wifi_control_task(void* arg);
static const char* authmode_to_str(wifi_auth_mode_t authmode);
static void udp_discovery_task(void *pvParameters);




// --- 新增一个内部使用的、实际执行连接的函数 ---
static void do_wifi_connect(cmd_responder_t responder, void* context)
{
    char response_buffer[128];

    // 检查是否已连接
    if (xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT) {
        if (responder) responder("OK: Already connected", context);
        return;
    }
    
    // 配置SSID和密码
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, s_wifi_ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, s_wifi_password, sizeof(wifi_config.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // 清理状态位并启动连接
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect()); // 显式调用连接

    // 等待结果
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));

    if (bits & WIFI_CONNECTED_BIT) {
        snprintf(response_buffer, sizeof(response_buffer), "OK: Wi-Fi connected, IP: %s", (char*)s_ip_address);
        if (s_udp_discovery_task_handle == NULL) {
            xTaskCreate(udp_discovery_task, "udp_discovery_task", 4096, NULL, 5, &s_udp_discovery_task_handle);
        }
    } else {
        snprintf(response_buffer, sizeof(response_buffer), "FAIL: Wi-Fi connection failed or timed out.");
        esp_wifi_stop();
    }
    if (responder) responder(response_buffer, context);
}


// --- 新增一个内部使用的、实际执行断开的函数 ---
static void do_wifi_disconnect(cmd_responder_t responder, void* context)
{
     if (!(xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT)) {
        if (responder) responder("OK: Already disconnected", context);
        return;
    }
    if (s_udp_discovery_task_handle != NULL) {
        vTaskDelete(s_udp_discovery_task_handle);
        s_udp_discovery_task_handle = NULL;
    }
    // 确保socket被关闭
    if (s_udp_socket >= 0) {
        close(s_udp_socket);
        s_udp_socket = -1;
    }
    // 使目标地址无效
    xSemaphoreTake(s_target_addr_mutex, portMAX_DELAY);
    s_target_addr_valid = false;
    xSemaphoreGive(s_target_addr_mutex);
    esp_wifi_disconnect();
    esp_wifi_stop();
    if (responder) responder("OK: Wi-Fi disconnected", context);
}

// 这是新的UDP发现任务
static void udp_discovery_task(void *pvParameters)
{
    char rx_buffer[128];
    struct sockaddr_in listen_addr;
    
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(UDP_LISTEN_PORT);
    
    // 如果全局socket未创建，则创建它
    if (s_udp_socket < 0) {
        s_udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (s_udp_socket < 0) {
            ESP_LOGE(TAG, "UDP: Unable to create socket");
            goto DISCOVERY_TASK_CLEANUP;
        }
        
        if (bind(s_udp_socket, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) {
            ESP_LOGE(TAG, "UDP: Socket unable to bind");
            close(s_udp_socket);
            s_udp_socket = -1;
            goto DISCOVERY_TASK_CLEANUP;
        }
    }
    ESP_LOGI(TAG, "UDP discovery listener started on port %d", UDP_LISTEN_PORT);

    while (1) {
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(s_udp_socket, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

        if (len > 0) {
            rx_buffer[len] = 0;
            cJSON *root = cJSON_Parse(rx_buffer);
            if (root) {//建立连接指令{"command": "discover", "report_port": 50001}
                cJSON *cmd = cJSON_GetObjectItem(root, "command");
                cJSON *port = cJSON_GetObjectItem(root, "report_port");
                
                if (cJSON_IsString(cmd) && (strcmp(cmd->valuestring, "discover") == 0) && cJSON_IsNumber(port)) {
                    // --- 线程安全地更新目标地址 ---
                    xSemaphoreTake(s_target_addr_mutex, portMAX_DELAY);
                    memcpy(&s_target_addr, &source_addr, sizeof(source_addr));
                    s_target_addr.sin_port = htons(port->valueint);
                    s_target_addr_valid = true;
                    xSemaphoreGive(s_target_addr_mutex);
                    // --- 更新结束 ---
                    
                    char addr_str[32];
                    inet_ntoa_r(s_target_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
                    ESP_LOGI(TAG, "Discovery packet received. Target PC updated to: %s:%d", addr_str, port->valueint);
                }
                cJSON_Delete(root);
            }
        }
    }

    DISCOVERY_TASK_CLEANUP:
        s_udp_discovery_task_handle = NULL;
        vTaskDelete(NULL);
}

void wifi_handler_send_mpu_data(const mpu6050_data_t* data)
{
    // 1. 检查Wi-Fi是否连接
    if (!(xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT)) {
        return;
    }
    // 2. 准备局部变量
    struct sockaddr_in target_addr_copy;
    bool is_valid = false;

    // 3. 线程安全地读取目标地址
    if (xSemaphoreTake(s_target_addr_mutex, 0) == pdTRUE) {
        is_valid = s_target_addr_valid;
        if (is_valid) {
            memcpy(&target_addr_copy, &s_target_addr, sizeof(s_target_addr));
        }
        xSemaphoreGive(s_target_addr_mutex);
    } else {
        // 如果锁被占用（例如udp_listen_task正在更新地址），这次就跳过发送
        return;
    }
    // 4. 检查目标地址和socket的有效性
    if (!is_valid) {
        return;
    }
    if (s_udp_socket < 0) {
        return;
    }
    // 5. 创建JSON对象并发送数据
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return;
    }
    // 使用 cJSON_AddNumberToObject 来添加浮点数
    // 注意：ESP-IDF的cJSON版本可能需要 (double) 类型转换
    /* float rounded_ax = roundf(data->ax * 100.0f) / 100.0f;
    float rounded_ay = roundf(data->ay * 100.0f) / 100.0f;
    float rounded_az = roundf(data->az * 100.0f) / 100.0f;
    float rounded_gx = roundf(data->gx * 100.0f) / 100.0f;
    float rounded_gy = roundf(data->gy * 100.0f) / 100.0f;
    float rounded_gz = roundf(data->gz * 100.0f) / 100.0f;
    cJSON_AddNumberToObject(root, "ax", (double)rounded_ax);
    cJSON_AddNumberToObject(root, "ay", (double)rounded_ay);
    cJSON_AddNumberToObject(root, "az", (double)rounded_az);
    cJSON_AddNumberToObject(root, "gx", (double)rounded_gx);
    cJSON_AddNumberToObject(root, "gy", (double)rounded_gy);
    cJSON_AddNumberToObject(root, "gz", (double)rounded_gz); */
    // 准备一个临时缓冲区来存放格式化后的数字字符串
    char num_buffer[32];

    // 1. 格式化加速度计数据
    snprintf(num_buffer, sizeof(num_buffer), "%.2f", data->ax);
    cJSON_AddItemToObject(root, "ax", cJSON_CreateRaw(num_buffer));

    snprintf(num_buffer, sizeof(num_buffer), "%.2f", data->ay);
    cJSON_AddItemToObject(root, "ay", cJSON_CreateRaw(num_buffer));

    snprintf(num_buffer, sizeof(num_buffer), "%.2f", data->az);
    cJSON_AddItemToObject(root, "az", cJSON_CreateRaw(num_buffer));


    // 2. 格式化陀螺仪数据
    snprintf(num_buffer, sizeof(num_buffer), "%.2f", data->gx);
    cJSON_AddItemToObject(root, "gx", cJSON_CreateRaw(num_buffer));

    snprintf(num_buffer, sizeof(num_buffer), "%.2f", data->gy);
    cJSON_AddItemToObject(root, "gy", cJSON_CreateRaw(num_buffer));

    snprintf(num_buffer, sizeof(num_buffer), "%.2f", data->gz);
    cJSON_AddItemToObject(root, "gz", cJSON_CreateRaw(num_buffer));

    

    char *json_string = cJSON_PrintUnformatted(root);
    if (json_string) {
        // 准备一个缓冲区来保存目标IP地址的字符串形式
        char target_ip_str[16];
        inet_ntoa_r(target_addr_copy.sin_addr, target_ip_str, sizeof(target_ip_str));
        int sent_len = sendto(s_udp_socket, json_string, strlen(json_string), 0, (struct sockaddr *)&target_addr_copy, sizeof(target_addr_copy));
        free(json_string);
    } else {
        ESP_LOGE(TAG, "Send failed: cJSON_PrintUnformatted returned NULL.");
    }
    
    cJSON_Delete(root);
}


void wifi_handler_init(void){
    // 创建互斥锁
    s_target_addr_mutex = xSemaphoreCreateMutex();
    // 2. 创建事件组
    s_wifi_event_group = xEventGroupCreate();

    // 3. 初始化网络底层和事件循环
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 4. 初始化Wi-Fi并注册事件处理器
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // 5. 从system_info读取Wi-Fi凭证并保存到全局变量
    strncpy(s_wifi_ssid, system_info_get_wifi_name(), sizeof(s_wifi_ssid) - 1);
    strncpy(s_wifi_password, system_info_get_wifi_password(), sizeof(s_wifi_password) - 1);
    
    // 创建命令队列
    s_wifi_cmd_queue = xQueueCreate(5, sizeof(wifi_cmd_request_t));
    if (s_wifi_cmd_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create command queue");
    }

    // 创建Wi-Fi控制任务，并分配充足的栈空间
    xTaskCreate(wifi_control_task, "wifi_control_task", 8192, NULL, 10, NULL);

    ESP_LOGI(TAG, "Wi-Fi handler initialized.");
}


// wifi模块命令解析
void wifi_cmd_handler(const char *command, const char *args, cmd_responder_t responder, void *context)
{
    char response_buffer[128]; 

    // 命令: test_device_get_wifi_ip (瞬时命令，直接处理)
    if (strcmp(command, "test_device_get_wifi_ip") == 0) {
        snprintf(response_buffer, sizeof(response_buffer), "OK: Wi-Fi IP is %s", (char*)s_ip_address);
        responder(response_buffer, context);
    } 
    // 命令: test_device_get_wifi_list 
    if (strcmp(command, "test_device_get_wifi_list") == 0) {
        wifi_cmd_request_t request = {
            .type = CMD_TYPE_SCAN,
            .responder = responder,
            .context = context
        };
        if (xQueueSend(s_wifi_cmd_queue, &request, pdMS_TO_TICKS(100)) == pdPASS) {
            responder("OK: Scan command received, starting scan...", context);
        } else {
            responder("FAIL: Command queue is full. Try again later.", context);
        }
        return;
    }
    // 命令: test_device_set_wifi_connect (耗时命令，发送到队列)
    if (strcmp(command, "test_device_set_wifi_connect") == 0) {
        if (args == NULL) {
            responder("FAIL: Missing argument (on/off)", context);
            return;
        }

        wifi_cmd_request_t request = {
            .responder = responder,
            .context = context
        };
        bool cmd_sent = false;

        // 参数: on
        if (strcmp(args, "on") == 0) {
            request.type=CMD_TYPE_CONNECT;
            if (xQueueSend(s_wifi_cmd_queue, &request, pdMS_TO_TICKS(100)) == pdPASS) {
                cmd_sent = true;
            }
        }
        // 参数: off
        else if (strcmp(args, "off") == 0) {
            request.type = CMD_TYPE_DISCONNECT;
            if (xQueueSend(s_wifi_cmd_queue, &request, pdMS_TO_TICKS(100)) == pdPASS) {
                cmd_sent = true;
            }
        }
        else {
            responder("FAIL: Invalid argument for wifi_connect", context);
            return;
        }

        // 立即响应，告知用户命令已接收
        if (cmd_sent) {
            responder("OK: Command received, processing...", context);
        } else {
            responder("FAIL: Command queue is full. Try again later.", context);
        }

    }
}


// --- 修改后的 wifi_control_task ---
static void wifi_control_task(void* arg)
{
    wifi_cmd_request_t request;
    char* scan_response_buffer = (char*)malloc(2048);
    // ... (malloc 错误检查) ...

    while (1) {
        if (xQueueReceive(s_wifi_cmd_queue, &request, portMAX_DELAY)) {
            switch (request.type) {
                case CMD_TYPE_CONNECT:
                    ESP_LOGI(TAG, "Processing CONNECT command...");
                    do_wifi_connect(request.responder, request.context);
                    break;

                case CMD_TYPE_DISCONNECT:
                    ESP_LOGI(TAG, "Processing DISCONNECT command...");
                    do_wifi_disconnect(request.responder, request.context);
                    break;

                case CMD_TYPE_SCAN:
                    ESP_LOGI(TAG, "Processing SCAN command...");
                    
                    bool was_connected = (xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT);
                    
                    // 扫描前，必须确保Wi-Fi已停止
                    if (was_connected) {
                         // 注意：这里我们只停止，而不调用完整的disconnect流程
                        esp_wifi_stop(); 
                        vTaskDelay(pdMS_TO_TICKS(500)); 
                    }
                    
                    esp_wifi_start();
                    
                    wifi_scan_config_t scan_config = { .show_hidden = false };
                    
                    if (esp_wifi_scan_start(&scan_config, true) == ESP_OK) {
                        // ... (构建扫描结果字符串的代码) ...
                    } else {
                        snprintf(scan_response_buffer, 2048, "FAIL: Wi-Fi scan failed to start.");
                    }
                    
                    request.responder(scan_response_buffer, request.context);

                    // 扫描后恢复之前的状态
                    if (was_connected) {
                        ESP_LOGI(TAG, "Scan finished. Reconnecting...");
                        // 这里我们只调用连接，而不响应给用户
                        do_wifi_connect(NULL, NULL); 
                    } else {
                        esp_wifi_stop();
                    }
                    break;
            }
        }
    }
    free(scan_response_buffer);
}

//Wi-Fi事件处理函数
static void event_handler(void* arg
    ,esp_event_base_t event_base
    ,int32_t event_id
    ,void* event_data){
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        //Wi-Fi Station (客户端模式) 启动成功
        // esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        //Wi-Fi Station (客户端模式) 断开连接
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGE(TAG, "Wi-Fi Disconnected. Reason: %d", event->reason);
        strncpy((char*)s_ip_address, "0.0.0.0", sizeof(s_ip_address));
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);//熄灭WIFI_CONNECTED_BIT
        // Wi-Fi断开，必须停止发现任务并使目标地址无效
        if (s_udp_discovery_task_handle != NULL) {
            vTaskDelete(s_udp_discovery_task_handle);
            s_udp_discovery_task_handle = NULL;
        }
        
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        //Wi-Fi Station (客户端模式) 成功获取到了IP地址
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Wi-Fi Connected! Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        snprintf((char*)s_ip_address, sizeof(s_ip_address), IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);//点亮WIFI_CONNECTED_BIT
    }
}



// --- 新增：辅助函数，将加密模式转换为字符串 ---
static const char* authmode_to_str(wifi_auth_mode_t authmode)
{
    switch (authmode) {
        case WIFI_AUTH_OPEN:         return "OPEN";
        case WIFI_AUTH_WEP:          return "WEP";
        case WIFI_AUTH_WPA_PSK:      return "WPA_PSK";
        case WIFI_AUTH_WPA2_PSK:     return "WPA2_PSK";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2_PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2_ENTERPRISE";
        case WIFI_AUTH_WPA3_PSK:     return "WPA3_PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3_PSK";
        default:                     return "UNKNOWN";
    }
}






