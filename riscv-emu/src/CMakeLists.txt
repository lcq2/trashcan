set(SRC_FILES
        rv_cpu.cpp
        rv_memory.cpp
        rv_machine.cpp
    main.cpp
        rv_device.cpp devices/rv_uart.cpp devices/rv_clint.cpp devices/rv_plic.cpp)

add_executable(${PROJECT_NAME} ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} pthread)
target_compile_options(${PROJECT_NAME} PRIVATE -fno-rtti)