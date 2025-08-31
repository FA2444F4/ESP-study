#ifndef MPU6050_HANDLER_H
#define MPU6050_HANDLER_H

// 定义用于传递MPU6050数据的结构体
typedef struct mpu6050_data{
    float ax;
    float ay;
    float az;
    float gx;
    float gy;
    float gz;
} mpu6050_data_t;

/**
 * @brief 初始化MPU6050传感器和I2C总线
 */
void mpu6050_handler_init(void);

#endif // MPU6050_HANDLER_H