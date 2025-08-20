# 注册一个名为 "gpio_tester" 的组件
idf_component_register(
    # 这个组件本身没有源文件，因为它只包含测试
    # 所以 SRCS 是空的
)

# 为这个组件添加一个单元测试套件
idf_component_add_unit_test_suite(
    SRC_FILES "test/test_gpio.c"
    # 因为这个测试不依赖 gpio_tester 组件内部的任何头文件，
    # 所以我们不需要 INCLUDE_DIRS
)