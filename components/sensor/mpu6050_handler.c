#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "mpu6050_handler.h"
#include "wifi_handler.h"

static const char *TAG = "MPU6050";

// I2C 配置
#define I2C_MASTER_SCL_IO           7      // 根据您的开发板，SCL 连接到 GPIO7
#define I2C_MASTER_SDA_IO           6      // 根据您的开发板，SDA 连接到 GPIO6
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000 // I2C 时钟频率
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0

// MPU6050 寄存器地址和地址
#define MPU6050_SENSOR_ADDR         0x68    // AD0 引脚接地或悬空时的地址
#define MPU6050_PWR_MGMT_1_REG      0x6B    // 电源管理寄存器
#define MPU6050_ACCEL_XOUT_H_REG    0x3B    // 加速度计数据起始地址
#define MPU6050_GYRO_XOUT_H_REG     0x43    // 陀螺仪数据起始地址

/**
 * @brief 初始化I2C总线
 */
static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        return err;
    }
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

/**
 * @brief 从 MPU6050 的指定寄存器读取数据
 * @param reg_addr 寄存器地址
 * @param data_rd 读取数据的缓冲区
 * @param size 读取的数据长度
 */
static esp_err_t mpu6050_register_read(uint8_t reg_addr, uint8_t *data_rd, size_t size)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, MPU6050_SENSOR_ADDR, &reg_addr, 1, data_rd, size, 1000 / portTICK_PERIOD_MS);
}

/**
 * @brief 向 MPU6050 的指定寄存器写入一个字节
 * @param reg_addr 寄存器地址
 * @param data 要写入的数据
 */
static esp_err_t mpu6050_register_write_byte(uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_write_to_device(I2C_MASTER_NUM, MPU6050_SENSOR_ADDR, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);
}


static void mpu6050_task(void *pvParameters)
{
    // 唤醒MPU6050
    uint8_t write_buf[2] = {MPU6050_PWR_MGMT_1_REG, 0x00};
    i2c_master_write_to_device(I2C_MASTER_NUM, MPU6050_SENSOR_ADDR, write_buf, sizeof(write_buf), pdMS_TO_TICKS(1000));
    
    uint8_t data_rd[14];
    mpu6050_data_t sensor_data;

    while (1) {
        // 1. 读取传感器原始数据
        uint8_t reg_addr = MPU6050_ACCEL_XOUT_H_REG;
        esp_err_t ret = i2c_master_write_read_device(I2C_MASTER_NUM, MPU6050_SENSOR_ADDR, &reg_addr, 1, data_rd, 14, pdMS_TO_TICKS(100));

        if (ret == ESP_OK) {
            // 2. 将原始数据转换为物理单位 (m/s^2 和 deg/s)
            int16_t accel_x_raw = (data_rd[0] << 8) | data_rd[1];
            int16_t accel_y_raw = (data_rd[2] << 8) | data_rd[3];
            int16_t accel_z_raw = (data_rd[4] << 8) | data_rd[5];
            
            int16_t gyro_x_raw = (data_rd[8] << 8) | data_rd[9];
            int16_t gyro_y_raw = (data_rd[10] << 8) | data_rd[11];
            int16_t gyro_z_raw = (data_rd[12] << 8) | data_rd[13];

            sensor_data.ax = (accel_x_raw / 16384.0f) * 9.80665f;
            sensor_data.ay = (accel_y_raw / 16384.0f) * 9.80665f;
            sensor_data.az = (accel_z_raw / 16384.0f) * 9.80665f;
            sensor_data.gx = gyro_x_raw / 131.0f;
            sensor_data.gy = gyro_y_raw / 131.0f;
            sensor_data.gz = gyro_z_raw / 131.0f;

            // 3. 将处理好的数据递交给Wi-Fi模块
            //    我们不关心Wi-Fi是否连接，只管调用函数，让Wi-Fi模块自己去判断
            wifi_handler_send_mpu_data(&sensor_data);

        }

        // 每100毫秒采集一次
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


void mpu6050_handler_init(void)
{
    ESP_ERROR_CHECK(i2c_master_init());
    xTaskCreate(mpu6050_task, "mpu6050_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "MPU6050 handler initialized.");
}




