#ifndef LVGL_HANDLER_H
#define LVGL_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 LVGL 库、显示驱动并创建主任务
 */
void lvgl_handler_init(void);

/**
 * @brief 创建一个简单的 UI 示例
 */
void lvgl_handler_create_ui(void);


#ifdef __cplusplus
}
#endif

#endif // LVGL_HANDLER_H