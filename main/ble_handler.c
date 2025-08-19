#include "ble_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

// 引入我们自己的模块
#include "led_control.h"
#include "system_info.h"
#include "cmd_parser.h"

#define TAG "BLE_HANDLER"
#define DEVICE_NAME "我不是徐瑜禧"

// --- 使用 16-bit UUID ---
#define GATTS_SERVICE_UUID   0x00FF // 自定义服务 UUID
#define GATTS_CHAR_UUID_RX   0xFF01 // RX 特征 (用于接收手机写入的数据)
#define GATTS_CHAR_UUID_TX   0xFF02 // TX 特征 (用于向手机发送通知/响应)
#define GATTS_NUM_HANDLES    6      // 预留句柄数：服务(1)+RX特征(2)+TX特征(2)+TX的CCCD(1)

// 广播参数
static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// 广播数据
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// GATT Profile 状态机结构体
struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    uint16_t rx_char_handle; // 接收特征的句柄
    uint16_t tx_char_handle; // 发送特征的句柄
};

// 函数前置声明
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

// 全局 Profile 实例
static struct gatts_profile_inst gl_profile = {
    .gatts_cb = gatts_event_handler,
    .gatts_if = ESP_GATT_IF_NONE,
};

// GAP 事件处理函数
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&adv_params);
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "Advertising started");
            // 开始广播，通知 LED 模块进入“未连接”状态（闪烁）
            led_control_set_ble_status(LED_BLE_DISCONNECTED);
        } else {
            ESP_LOGE(TAG, "Advertising start failed");
        }
        break;
    default:
        break;
    }
}

// --- BLE 响应器，用于将命令解析结果发回 ---
typedef struct {
    esp_gatt_if_t gatts_if;
    uint16_t conn_id;
    uint16_t char_handle;
} ble_responder_context_t;

static void ble_responder(const char *response_str, void *context)
{
    if (context == NULL) return;
    ble_responder_context_t *ble_ctx = (ble_responder_context_t *)context;

    ESP_LOGI(TAG, "Responding via BLE with handle %d: %s", ble_ctx->char_handle, response_str);
    // 使用 Indicate 发送，需要手机端确认，更可靠
    esp_ble_gatts_send_indicate(ble_ctx->gatts_if,
                              ble_ctx->conn_id,
                              ble_ctx->char_handle,
                              strlen(response_str),
                              (uint8_t *)response_str,
                              false); // false for Notification, true for Indication
}

// GATTS 事件处理函数 (核心)
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        gl_profile.gatts_if = gatts_if;
        esp_ble_gap_set_device_name(DEVICE_NAME);
        esp_ble_gap_config_adv_data(&adv_data);
        esp_ble_gatts_create_service(gatts_if, &(esp_gatt_srvc_id_t){.is_primary=true, .id={.inst_id=0, .uuid={.len=ESP_UUID_LEN_16, .uuid={.uuid16=GATTS_SERVICE_UUID}}}}, GATTS_NUM_HANDLES);
        break;
    case ESP_GATTS_CREATE_EVT:
        gl_profile.service_handle = param->create.service_handle;
        esp_ble_gatts_start_service(gl_profile.service_handle);
        // 添加 RX (Write) 特征
        esp_ble_gatts_add_char(gl_profile.service_handle, &(esp_bt_uuid_t){.len=ESP_UUID_LEN_16, .uuid={.uuid16=GATTS_CHAR_UUID_RX}}, ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_WRITE, NULL, NULL);
        // 添加 TX (Notify) 特征
        esp_ble_gatts_add_char(gl_profile.service_handle, &(esp_bt_uuid_t){.len=ESP_UUID_LEN_16, .uuid={.uuid16=GATTS_CHAR_UUID_TX}}, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_NOTIFY, NULL, NULL);
        break;
    case ESP_GATTS_ADD_CHAR_EVT:
        // 根据 UUID 保存不同的句柄
        if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_RX) {
            gl_profile.rx_char_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "RX Char added, handle=%d", gl_profile.rx_char_handle);
        } else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_TX) {
            gl_profile.tx_char_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "TX Char added, handle=%d", gl_profile.tx_char_handle);
            // 为 TX 特征添加 CCCD，这样手机才能订阅通知
            esp_ble_gatts_add_char_descr(gl_profile.service_handle, &(esp_bt_uuid_t){.len=ESP_UUID_LEN_16, .uuid={.uuid16=ESP_GATT_UUID_CHAR_CLIENT_CONFIG}}, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
        }
        break;
    case ESP_GATTS_CONNECT_EVT:
        gl_profile.conn_id = param->connect.conn_id;
        // 通知 LED 模块，蓝牙进入了“已连接”状态
        led_control_set_ble_status(LED_BLE_CONNECTED);
        ESP_LOGI(TAG, "Client connected, conn_id %d", param->connect.conn_id);
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        // 通知 LED 模块，蓝牙恢复“未连接”状态
        led_control_set_ble_status(LED_BLE_DISCONNECTED);
        ESP_LOGI(TAG, "Client disconnected");
        esp_ble_gap_start_advertising(&adv_params);
        break;
    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(TAG, "MTU configured to: %d", param->mtu.mtu);
        break;
    case ESP_GATTS_WRITE_EVT:
        // 检查写入的是否是 RX 特征
        if (param->write.handle == gl_profile.rx_char_handle && param->write.len > 0) {
            ESP_LOGI(TAG, "--- GATT WRITE EVENT ---");
            esp_log_buffer_hex(TAG, param->write.value, param->write.len);

            char cmd_buffer[param->write.len + 1];
            memcpy(cmd_buffer, param->write.value, param->write.len);
            cmd_buffer[param->write.len] = '\0';

            // 创建 BLE 响应器上下文
            ble_responder_context_t ble_ctx = {
                .gatts_if = gatts_if,
                .conn_id = param->write.conn_id,
                .char_handle = gl_profile.tx_char_handle // 使用 TX 句柄进行回复
            };
            
            // 调用通用的命令解析器
            cmd_parser_process_line(cmd_buffer, ble_responder, &ble_ctx);

            // 如果需要，发送写响应
            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
        }
        break;
    default:
        break;
    }
}


void ble_handler_init(void)
{
    // 初始化蓝牙控制器
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    
    // 初始化 Bluedroid 协议栈
    esp_bluedroid_init();
    esp_bluedroid_enable();
    
    // 注册回调函数
    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gap_register_callback(gap_event_handler);
    
    // 注册应用 Profile
    esp_ble_gatts_app_register(0);

    // 设置本地 MTU，请求一个更大的数据包大小
    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret){
        ESP_LOGE(TAG, "Set local MTU failed, error code = %x", local_mtu_ret);
    }
}