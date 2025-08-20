#include "unity.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 这是一个临时的辅助函数，只存在于测试代码中
static void dump_gpios(const char* message) {
    printf("--- %s ---\n", message);
    gpio_dump_io_configuration(stdout, GPIO_SEL_ALL); 
    printf("---------------------\n");
}

TEST_CASE("Dump all GPIO configurations", "[gpio_test]")
{
    printf("Preparing to capture GPIO state...\n");
    vTaskDelay(pdMS_TO_TICKS(100));

    dump_gpios("Current GPIO State");

    // 你可以在这里添加一些GPIO操作来观察变化
    // gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT);
    // dump_gpios("After setting GPIO4 as output");
}