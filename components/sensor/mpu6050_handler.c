#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "mpu6050_handler.h"

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

void mpu6050_handler_init(void)
{
    // 1. 初始化 I2C
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    // 2. 唤醒 MPU6050 (通过向电源管理寄存器写入0)
    ESP_ERROR_CHECK(mpu6050_register_write_byte(MPU6050_PWR_MGMT_1_REG, 0));
    ESP_LOGI(TAG, "MPU6050 wakeup successfully");

    uint8_t data[14];

    while (1) {
        // 3. 读取加速度计原始数据
        esp_err_t ret_accel = mpu6050_register_read(MPU6050_ACCEL_XOUT_H_REG, data, 6);
        if (ret_accel == ESP_OK) {
            // 数据是16位有符号整数，高位在前 (Big Endian)
            int16_t accel_x = (data[0] << 8) | data[1];
            int16_t accel_y = (data[2] << 8) | data[3];
            int16_t accel_z = (data[4] << 8) | data[5];
            ESP_LOGI(TAG, "ACCEL_X: %d, ACCEL_Y: %d, ACCEL_Z: %d", accel_x, accel_y, accel_z);
        } else {
            ESP_LOGE(TAG, "Failed to read accelerometer data");
        }

        // 4. 读取陀螺仪原始数据
        esp_err_t ret_gyro = mpu6050_register_read(MPU6050_GYRO_XOUT_H_REG, data, 6);
        if (ret_gyro == ESP_OK) {
            int16_t gyro_x = (data[0] << 8) | data[1];
            int16_t gyro_y = (data[2] << 8) | data[3];
            int16_t gyro_z = (data[4] << 8) | data[5];
            ESP_LOGI(TAG, "GYRO_X: %d, GYRO_Y: %d, GYRO_Z: %d", gyro_x, gyro_y, gyro_z);
        } else {
            ESP_LOGE(TAG, "Failed to read gyroscope data");
        }
        
        ESP_LOGI(TAG, "-------------------------------------------");

        vTaskDelay(1000 / portTICK_PERIOD_MS); // 延时1秒
    }
}