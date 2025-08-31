#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>



#include "wifi_handler.h"
#include "system_info.h"

/* --- 模块私有定义 --- */
#define UDP_PORT       50001
#define WIFI_CONNECT_TIMEOUT_MS  20000
#define DISCOVERY_REQUEST_MSG  "DISCOVER_ESP32_REQUEST"  // PC广播的特定数据包内容
#define DISCOVERY_RESPONSE_MSG "DISCOVER_ESP32_RESPONSE" // ESP32响应给PC的内容

static const char *TAG = "WIFI_UDP_EXAMPLE";

/* --- 模块私有全局变量 --- */
static EventGroupHandle_t s_wifi_event_group;// 用于等待Wi-Fi连接成功的事件组
const int WIFI_CONNECTED_BIT = BIT0;
const static int WIFI_FAIL_BIT = BIT1;
static volatile char s_ip_address[16] = "0.0.0.0";
static char s_wifi_ssid[33] = {0};    
static char s_wifi_password[65] = {0};
static TaskHandle_t s_udp_task_handle = NULL;
static QueueHandle_t s_wifi_cmd_queue = NULL;



// 定义一个结构体，用于在队列中传递命令及其响应所需的信息
typedef struct {
    wifi_cmd_type_t type;         // 命令类型
    cmd_responder_t responder;    // 用于响应的回调函数
    void* context;      // 响应器所需的上下文
} wifi_cmd_request_t;

/* --- 模块私有函数声明 --- */
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void udp_discover_task(void *pvParameters);
void wifi_connect(void);
static void wifi_disconnect(void);
static void wifi_control_task(void* arg);
static const char* authmode_to_str(wifi_auth_mode_t authmode);



// 轻量级的连接函数
void wifi_connect(void)
{
    // 将保存的SSID和密码配置到Wi-Fi驱动
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, s_wifi_ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, s_wifi_password, sizeof(wifi_config.sta.password));
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Connecting to SSID: [%s]", s_wifi_ssid);
}

// 轻量级的断开函数
static void wifi_disconnect(void)
{
    if (s_udp_task_handle != NULL) {
        vTaskDelete(s_udp_task_handle);
        s_udp_task_handle = NULL;
    }
    esp_wifi_disconnect();
    esp_wifi_stop();
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    strncpy((char*)s_ip_address, "0.0.0.0", sizeof(s_ip_address));
    ESP_LOGI(TAG, "Wi-Fi has been disconnected and stopped.");
}

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
        if (s_udp_task_handle == NULL) {
            xTaskCreate(udp_discover_task, "udp_discover_task", 3072, NULL, 5, &s_udp_task_handle);
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
    if (s_udp_task_handle != NULL) {
        vTaskDelete(s_udp_task_handle);
        s_udp_task_handle = NULL;
    }
    esp_wifi_disconnect();
    esp_wifi_stop();
    if (responder) responder("OK: Wi-Fi disconnected", context);
}

void wifi_handler_init(void){
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
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        //Wi-Fi Station (客户端模式) 成功获取到了IP地址
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Wi-Fi Connected! Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        snprintf((char*)s_ip_address, sizeof(s_ip_address), IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);//点亮WIFI_CONNECTED_BIT
    }
}


//监听并响应PC的广播
static void udp_discover_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;
    struct sockaddr_in dest_addr;

    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(UDP_PORT);
    
    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        goto UDP_TASK_CLEANUP;
    }
    ESP_LOGI(TAG, "Socket created");

    int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto UDP_TASK_CLEANUP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", UDP_PORT);
    ESP_LOGI(TAG, "Ready to receive broadcast messages...");

    while (1) {
        struct sockaddr_storage source_addr;
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            break;
        } else {
            rx_buffer[len] = 0; 
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
            ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
            ESP_LOGI(TAG, "%s", rx_buffer);

            if (strcmp(rx_buffer, DISCOVERY_REQUEST_MSG) == 0) {
                ESP_LOGI(TAG, "Discovery request received. Sending response...");
                int err = sendto(sock, DISCOVERY_RESPONSE_MSG, strlen(DISCOVERY_RESPONSE_MSG), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                if (err < 0) {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                } else {
                    ESP_LOGI(TAG, "Response sent successfully.");
                }
            }
        }
    }

UDP_TASK_CLEANUP:
    ESP_LOGI(TAG, "UDP Task cleaning up...");
    close(sock);
    // 注意：这里不需要将g_udp_task_handle设为NULL，因为删除任务的代码在button_control_task中
    vTaskDelete(NULL);
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






