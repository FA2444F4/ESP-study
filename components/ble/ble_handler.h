#include "esp_err.h"
#ifndef BLE_HANDLER_H
#define BLE_HANDLER_H

void ble_handler_init(void);
esp_err_t ble_send_data_to_phone(const char* data);
#endif // BLE_HANDLER_H
