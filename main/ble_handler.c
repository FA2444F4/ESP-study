/*
第一部分：头文件和宏定义
*/
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
#include "nvs_flash.h"
#include "esp_gatt_common_api.h"

// 引入我们自己的模块
#include "led_control.h"
#include "system_info.h"
#include "ble_handler.h"
#include "cmd_parser.h"

#define GATTS_TAG "BLE_HANDLER"
#define DEVICE_NAME "ESP32C6-ZY"

/*
第二部分：全局变量和静态声明
*/
// --- BLE 响应器上下文context ---
typedef struct{
    esp_gatt_if_t gatts_if;
    uint16_t conn_id;
    uint16_t notify_char_handle;
}ble_responder_context_t;
// 声明GATT Profile的回调函数
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

// === 1. 定义服务和特征的UUID ===
// 为了避免和标准服务冲突，我们使用自定义的128位UUID
// 可以使用在线UUID生成器生成你自己的UUID
// UUID是蓝牙设备用来识别不同服务和特征的“身份证号”。
// 128位的UUID可以保证全球唯一，避免冲突。
// 自定义服务 UUID: 186a72e8-9473-4f0e-a50a-812a02422c54
static const uint8_t GATTS_SERVICE_UUID[16] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    0x54, 0x2c, 0x42, 0x02, 0x2a, 0x81, 0x0a, 0xa5, 0x0e, 0x4f, 0x73, 0x94, 0xe8, 0x72, 0x6a, 0x18,
};

// 自定义特征 UUID (用于写入数据): 208363f3-c5da-48f8-a859-693f1a3e3518
//接收手机写入的数据
static const uint8_t GATTS_CHAR_UUID_WRITE[16] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    0x18, 0x35, 0x3e, 0x1a, 0x3f, 0x69, 0x59, 0xa8, 0xf8, 0x48, 0xda, 0xc5, 0xf3, 0x63, 0x83, 0x20,
};
// 自定义特征 UUID (用于通知数据): 3c2a6730-a94f-458f-a38b-07f0b2a7b818
static const uint8_t GATTS_CHAR_UUID_NOTIFY[16] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    0x18, 0xb8, 0xa7, 0xb2, 0xf0, 0x07, 0x8b, 0xa3, 0x8f, 0x45, 0x4f, 0xa9, 0x30, 0x67, 0x2a, 0x3c,
};
// 用于CCCD的值
static uint8_t char_notification_config_default[2] = {0x00, 0x00};

// 特征的属性：可读、可写
#define GATTS_CHAR_PROPERTY (ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE)
// 通知特征的属性：可读、可通知
#define GATTS_NOTIFY_CHAR_PROPERTY (ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY)
// 特征的默认值,，当手机读取它时，会返回这个值。
static uint8_t char_value_default[] = {0xDE, 0xAD, 0xBE, 0xEF};

// === 2. 配置广播参数 ===
static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,// 最小广播间隔 (0x20 * 0.625ms = 20ms)
    .adv_int_max        = 0x40,// 最大广播间隔 (0x40 * 0.625ms = 40ms)
    .adv_type           = ADV_TYPE_IND,// 广播类型：可连接、不可定向的广播
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,// 使用公共地址
    .channel_map        = ADV_CHNL_ALL,// 在所有3个广播信道(37,38,39)上广播
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,// 过滤器策略：允许任何设备扫描和连接
};

// === 3. 配置广播包（Advertising Data）的内容 ===
//这是设备主动发出去的数据包，通常很小（31字节限制）
// 这里我们在广播包中放入服务的UUID，这样手机App可以根据服务UUID来发现设备
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp       = false,// 标记这是广播包，而不是扫描响应包
    .include_name       = false,// 不包含设备名称，以节省空间
    .include_txpower    = true,// 包含发射功率信息
    .min_interval       = 0x0006, // 连接参数：最小连接间隔, Time = min_interval * 1.25 msec
    .max_interval       = 0x0010, // 连接参数：最大连接间隔, Time = max_interval * 1.25 msec
    .appearance         = 0x00,// 外观类型（通用设备）
    .manufacturer_len   = 0,// 制造商数据长度
    .p_manufacturer_data= NULL,// 制造商数据指针
    .service_data_len   = 0,// 服务数据长度
    .p_service_data     = NULL,// 服务数据指针
    .service_uuid_len   = sizeof(GATTS_SERVICE_UUID),// 包含的服务UUID长度（16字节）
    .p_service_uuid     = (uint8_t*)GATTS_SERVICE_UUID,// 指向服务UUID数组
    .flag               = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// --- 配置扫描响应包（Scan Response Data）的内容 ---
// 当手机主动扫描并请求更多信息时，设备会发送这个包。
static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp       = true, // 标记这是扫描响应包
    .include_name       = true,// 包含设备名称
    .include_txpower    = true,// 包含发射功率
    .appearance         = 0x00,
    .manufacturer_len   = 0,
    .p_manufacturer_data= NULL,
    .service_data_len   = 0,
    .p_service_data     = NULL,
    .service_uuid_len   = 0,// 扫描响应中通常不放UUID，因为广播包里已经有了
    .p_service_uuid     = NULL,
    .flag               = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};


// === 4. 定义GATT Profile实例 ===
// Profile是管理GATT服务的实例。
// 一个Profile可以包含多个服务
#define PROFILE_NUM 1// 我们只定义一个Profile
#define PROFILE_APP_ID 0// 这个Profile的ID为0

// 定义一个结构体来保存Profile的状态信息
struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;// GATT事件回调函数指针
    uint16_t gatts_if;// GATT接口ID，由协议栈分配
    uint16_t app_id;// 应用ID
    uint16_t conn_id;// 连接ID
    uint16_t service_handle;// 服务句柄
    esp_gatt_srvc_id_t service_id;// 服务ID（包含UUID）
    uint16_t write_char_handle;      // 写入特征的句柄
    uint16_t notify_char_handle;     // 通知特征的句柄
    uint16_t notify_descr_handle;    // 通知特征CCCD的句柄
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

// 初始化GATT Profile实例数组
static struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_APP_ID] = {
        .gatts_cb = gatts_profile_event_handler,//将回调函数与这个Profile关联
        .gatts_if = ESP_GATT_IF_NONE,       // 初始时没有合法的接口ID
    },
};
/*
第三部分：GAP事件处理函数
*/
/* GAP事件回调函数 */
// GAP（Generic Access Profile）层负责广播、扫描和连接管理。
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    // 广播数据设置完成事件
    // 广播数据配置完成事件：当调用 esp_ble_gap_config_adv_data 成功后触发
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        // 广播数据已准备好，现在可以启动广播了
        esp_ble_gap_start_advertising(&adv_params);
        break;
    // 扫描响应数据设置完成事件
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
    // 扫描响应数据也准备好了，同样启动广播
        esp_ble_gap_start_advertising(&adv_params);
        break;
    // 广播启动完成事件,当调用 esp_ble_gap_start_advertising 成功后触发
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            // 广播启动失败
            ESP_LOGE(GATTS_TAG, "Advertising start failed");
        } else {
            // 广播启动成功
            ESP_LOGI(GATTS_TAG, "Advertising started successfully");
            //开始广播蓝灯闪烁
            led_control_set_ble_status(LED_BLE_DISCONNECTED);
        }
        break;
    // 广播停止完成事件
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(GATTS_TAG, "Advertising stop failed");
        } else {
            ESP_LOGI(GATTS_TAG, "Advertising stopped successfully");
        }
        break;
    // 连接更新完成事件:当连接建立后，主从设备协商更新连接参数（如间隔）时触发
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
         ESP_LOGI(GATTS_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

// --- BLE 专用的响应器函数 ---
static void ble_responder(const char *response_str, void *context){
    if (context == NULL) return;

    //将context转为ble context
    ble_responder_context_t *ble_ctx=(ble_responder_context_t *)context;
    ESP_LOGI(GATTS_TAG, "Responding via BLE: %s", response_str);
    esp_ble_gatts_send_indicate(
        ble_ctx->gatts_if,
        ble_ctx->conn_id,
        ble_ctx->notify_char_handle,
        strlen(response_str),
        (uint8_t *)response_str,
        false   //false表示发送通知
    );
    
}

/*
第四部分：GATTS事件处理函数 (核心逻辑)
*/
/* GATTS事件回调函数 */
// GATTS（GATT Server）层负责作为服务器，定义和管理服务、特征。
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    // 事件1：应用注册完成
    // 当在 app_main 中调用 esp_ble_gatts_app_register 后触发
    case ESP_GATTS_REG_EVT:
        // 打印注册状态和分配的应用ID
        ESP_LOGI(GATTS_TAG, "REGISTER_APP_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);
        // 配置服务ID结构体
        gl_profile_tab[PROFILE_APP_ID].service_id.is_primary = true;
        gl_profile_tab[PROFILE_APP_ID].service_id.id.inst_id = 0x00;
        gl_profile_tab[PROFILE_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_128;
        // 将之前定义的全局服务UUID复制到Profile的service_id成员中
        memcpy(gl_profile_tab[PROFILE_APP_ID].service_id.id.uuid.uuid.uuid128, GATTS_SERVICE_UUID, sizeof(GATTS_SERVICE_UUID));

        // 设置设备名称（将显示在手机蓝牙列表中）
        esp_ble_gap_set_device_name(DEVICE_NAME);
        // 配置广播数据
        esp_ble_gap_config_adv_data(&adv_data);
        // 配置扫描响应数据
        esp_ble_gap_config_adv_data(&scan_rsp_data);
        // 创建GATT服务
        esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_APP_ID].service_id, 8); /* handle num = 4 (service + char + value + CCCD) */
        // 原来是4，现在需要为新特征(char+value)和CCCD(descr+value)增加4个，总共8个左右比较安全
        break;

    // 事件2：收到手机的读取请求
    case ESP_GATTS_READ_EVT:
        // 打印连接ID、事务ID和被读取的句柄
        ESP_LOGI(GATTS_TAG, "GATT_READ_EVT, conn_id %d, trans_id %d, handle %d", param->read.conn_id, param->read.trans_id, param->read.handle);
        // 可以在这里准备要返回给手机的数据,响应结构体
        esp_gatt_rsp_t rsp={
            .attr_value = {
                .handle = param->read.handle,
                .len = 4,
                .value = {0xde, 0xad, 0xbe, 0xef}
            }
        };
        
        // 发送响应给手机
        esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
        break;

    // 3. 收到写入请求事件 (核心部分)
    case ESP_GATTS_WRITE_EVT:
        ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, value len %d, value:", param->write.len);
        esp_log_buffer_hex(GATTS_TAG, param->write.value, param->write.len);
        // --- 核心修改：检查写入的句柄是否是我们的 write_char_handle ---
        if (param->write.handle == gl_profile_tab[PROFILE_APP_ID].write_char_handle && param->write.len > 0) {
            
            // 将收到的数据转换成 C 字符串
            char cmd_buffer[param->write.len + 1];
            memcpy(cmd_buffer, param->write.value, param->write.len);
            cmd_buffer[param->write.len] = '\0';

            // --- 核心修改：创建上下文时，传入 notify_char_handle ---
            // 1. 创建 BLE 上下文
            ble_responder_context_t ble_ctx = {
                .gatts_if = gatts_if,
                .conn_id = param->write.conn_id,
                .notify_char_handle = gl_profile_tab[PROFILE_APP_ID].notify_char_handle // <--- 把通知句柄打包
            };

            // 2. 调用通用的命令解析器，并传入 BLE 专用的响应器和上下文
            cmd_parser_process_line(cmd_buffer, ble_responder, &ble_ctx);

            // 发送写响应 (Write With Response)
            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
        }
        break;
        

    // 4. 创建服务完成事件
    // 当调用 esp_ble_gatts_create_service 成功后触发
    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(GATTS_TAG, "CREATE_SERVICE_EVT, status %d, service_handle %d", param->create.status, param->create.service_handle);
        // 保存协议栈分配的服务句柄
        gl_profile_tab[PROFILE_APP_ID].service_handle = param->create.service_handle;
        // 启动服务，使其对外部可见
        esp_ble_gatts_start_service(param->create.service_handle);
        
        // 创建一个局部变量来存储特征的UUID
        esp_bt_uuid_t char_uuid;
        char_uuid.len = ESP_UUID_LEN_128;
        memcpy(char_uuid.uuid.uuid128, GATTS_CHAR_UUID_WRITE, sizeof(char_uuid.uuid.uuid128));
        
        // 添加写入特征
        esp_bt_uuid_t write_char_uuid;
        write_char_uuid.len = ESP_UUID_LEN_128;
        memcpy(write_char_uuid.uuid.uuid128, GATTS_CHAR_UUID_WRITE, sizeof(write_char_uuid.uuid.uuid128));
        esp_ble_gatts_add_char(
            // param->create.service_handle
            gl_profile_tab[PROFILE_APP_ID].service_handle
            ,&write_char_uuid
            ,ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE//可读可写
            ,GATTS_CHAR_PROPERTY//设置属性,之前定义的宏
            ,&(esp_attr_value_t){
                .attr_max_len = 100
                ,.attr_len = sizeof(char_value_default)
                ,.attr_value = char_value_default}
                ,NULL
        );
        
        // --- 新增：添加通知特征 ---
        esp_bt_uuid_t notify_char_uuid;
        notify_char_uuid.len = ESP_UUID_LEN_128;
        memcpy(notify_char_uuid.uuid.uuid128, GATTS_CHAR_UUID_NOTIFY, sizeof(notify_char_uuid.uuid.uuid128));
        esp_ble_gatts_add_char(
            // param->create.service_handle
            gl_profile_tab[PROFILE_APP_ID].service_handle
            , &notify_char_uuid
            , ESP_GATT_PERM_READ
            , GATTS_NOTIFY_CHAR_PROPERTY
            , NULL
            , NULL
        );
        

        /* old统一特征
        // 使用新的变量来添加特征
        esp_ble_gatts_add_char(param->create.service_handle, 
                                &char_uuid, // 传递传递特征UUID局部变量的地址
                                ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,// 设置权限：可读可写
                                GATTS_CHAR_PROPERTY,// 设置属性（之前定义的宏）
                                &(esp_attr_value_t){.attr_max_len = 100, .attr_len = sizeof(char_value_default), .attr_value = char_value_default}, 
                                NULL//设置初始值
                                );
        break; 
        */

    // 5. 添加特征完成事件
    // 当调用 esp_ble_gatts_add_char 成功后触发
    case ESP_GATTS_ADD_CHAR_EVT: 
        // --- 修改：区分是哪个特征被添加了 ---
        if (memcmp(param->add_char.char_uuid.uuid.uuid128, GATTS_CHAR_UUID_WRITE, ESP_UUID_LEN_128) == 0) {
            ESP_LOGI(GATTS_TAG, "Write Char Added, handle=%d", param->add_char.attr_handle);
            gl_profile_tab[PROFILE_APP_ID].write_char_handle = param->add_char.attr_handle;
        } else if (memcmp(param->add_char.char_uuid.uuid.uuid128, GATTS_CHAR_UUID_NOTIFY, ESP_UUID_LEN_128) == 0) {
            ESP_LOGI(GATTS_TAG, "Notify Char Added, handle=%d", param->add_char.attr_handle);
            gl_profile_tab[PROFILE_APP_ID].notify_char_handle = param->add_char.attr_handle;
            // 为通知特征添加CCCD描述符
            esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_APP_ID].service_handle,
                                       &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG}},
                                       ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                       &(esp_attr_value_t){.attr_max_len=2, .attr_len=2, .attr_value=char_notification_config_default},
                                       NULL);
        }
        break;
    
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        ESP_LOGI(GATTS_TAG, "ADD_DESCR_EVT, status %d, attr_handle %d, service_handle %d",
                param->add_char_descr.status, param->add_char_descr.attr_handle, param->add_char_descr.service_handle);
        if (param->add_char_descr.descr_uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG) {
            gl_profile_tab[PROFILE_APP_ID].notify_descr_handle = param->add_char_descr.attr_handle;
        }
        break;
    

    // 6. 连接事件
    case ESP_GATTS_CONNECT_EVT:
        // 打印连接ID和手机的MAC地址
        ESP_LOGI(GATTS_TAG, "CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x:",
                 param->connect.conn_id,
                 param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                 param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
        // 保存连接ID
        gl_profile_tab[PROFILE_APP_ID].conn_id = param->connect.conn_id;
        // 连接成功后，停止广播以节省电量并防止其他设备连接
        esp_ble_gap_stop_advertising();
        led_control_set_ble_status(LED_BLE_CONNECTED);
        break;

    // 7. 断开连接事件
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(GATTS_TAG, "DISCONNECT_EVT, reason 0x%x", param->disconnect.reason);
        // 断开后，重新开始广播，以便其他设备可以发现并连接
        esp_ble_gap_start_advertising(&adv_params);
        led_control_set_ble_status(LED_BLE_DISCONNECTED);
        break;
    
    
    case ESP_GATTS_START_EVT:
        ESP_LOGI(GATTS_TAG, "SERVICE_START_EVT, status %d, service_handle %d", param->start.status, param->start.service_handle);
        break;

    // 其他不处理的事件
    case ESP_GATTS_DELETE_EVT:
    case ESP_GATTS_STOP_EVT:
    case ESP_GATTS_CONF_EVT:
    default:
        break;
    }
}

void ble_handler_init(void)
{
    esp_err_t ret;// 用于接收函数返回的错误码
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret=esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }
    ret=esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }
    ret=esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(GATTS_TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }
    ret=esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(GATTS_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }
    ret=esp_ble_gatts_register_callback(gatts_profile_event_handler);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "gatts register error, error code = %x", ret);
        return;
    }
    ret=esp_ble_gap_register_callback(gap_event_handler);
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "gap register error, error code = %x", ret);
        return;
    }
    ret=esp_ble_gatts_app_register(0);
    ret = esp_ble_gatts_app_register(PROFILE_APP_ID);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "gatts app register error, error code = %x", ret);
        return;
    }
    // --- 核心修正：在此处添加设置 MTU 的请求 ---
    // 请求将本地GATT服务的MTU设置为500字节。
    // 连接建立后，ESP32会与手机协商一个双方都支持的最大值。
    // 现代手机通常支持200字节以上。
    ret = esp_ble_gatt_set_local_mtu(500);
    if (ret){
        ESP_LOGE(GATTS_TAG, "Set local MTU failed, error code = %x", ret);
    }

    return;
}

//不做命令处理,直接交给cmd_parser.c

/* void app_main(void)
{
    esp_err_t ret;// 用于接收函数返回的错误码

    // 初始化NVS
    // NVS用于存储Wi-Fi和蓝牙的校准数据，必须在初始化蓝牙前调用。
    ret = nvs_flash_init();
    // 如果NVS分区已满或版本不兼容，则擦除并重新初始化
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);// 检查初始化是否成功

    // --- 初始化蓝牙控制器和协议栈 ---
    // 释放经典蓝牙内存，因为我们只用BLE
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    // 使用默认配置初始化蓝牙控制器（Controller）
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    // 启用蓝牙控制器 (BLE模式)
    // 启用蓝牙控制器，并指定模式为BLE
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    // 初始化蓝牙主机协议栈（Bluedroid）
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(GATTS_TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    // 启用蓝牙主机协议栈
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(GATTS_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    // 注册GATTS回调函数
    ret = esp_ble_gatts_register_callback(gatts_profile_event_handler);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "gatts register error, error code = %x", ret);
        return;
    }

    // 注册GAP回调函数
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "gap register error, error code = %x", ret);
        return;
    }

    // 注册GATTS APP
    // --- 启动GATT服务 ---
    // 向GATT层注册我们的应用程序Profile，这将触发第一个GATT事件：ESP_GATTS_REG_EVT
    ret = esp_ble_gatts_app_register(PROFILE_APP_ID);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "gatts app register error, error code = %x", ret);
        return;
    }
    
    // 设置MTU大小
    // 设置本地支持的最大传输单元（MTU）大小为500字节。
    // MTU决定了单次可以传输的数据包最大长度。
    esp_ble_gatt_set_local_mtu(500);

    return;
}
 */