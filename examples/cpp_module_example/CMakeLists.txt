# Minimal 测试模块 (修复版)
cmake_minimum_required(VERSION 3.10)
project(minimal VERSION 1.1.0)
# 设置模块名称
set(MODULE_NAME minimal)

# 创建共享库 - 使用超级极简版源文件
add_library(${MODULE_NAME} SHARED
        ultra_minimal.cpp
)

# 设置输出目录
set_target_properties(${MODULE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Debug"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Debug"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Debug"
)

# 跨平台编译设置
if(WIN32)
    # Windows 特定设置
    target_compile_definitions(${MODULE_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
    
    if(MSVC)
        # MSVC 特定设置 - 确保使用正确的调用约定
        target_compile_options(${MODULE_NAME} PRIVATE
            /std:c++17
            /EHsc    # 启用C++异常处理
            /MD      # 使用动态运行时库
            /W4      # 警告等级
        )
        
        # 确保与主程序使用相同的运行时库
        set_property(TARGET ${MODULE_NAME} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
        )
    else()
        # MinGW 特定设置，但强烈建议使用MSVC
        message(WARNING "构建${MODULE_NAME}模块时推荐使用MSVC，以避免ABI兼容性问题")
        target_compile_options(${MODULE_NAME} PRIVATE
            -std=c++17
            -fexceptions  # 启用C++异常处理
            -shared       # 确保正确生成共享库
        )
    endif()

elseif(UNIX AND NOT APPLE)
    # Linux 特定设置
    target_compile_options(${MODULE_NAME} PRIVATE
        -fPIC
        -std=c++17
        -fvisibility=hidden
    )
    # 确保符号可见性
    target_compile_definitions(${MODULE_NAME} PRIVATE
        -DLAMINA_EXPORT=__attribute__\(\(visibility\(\"default\"\)\)\)
    )
elseif(APPLE)
    # macOS 特定设置
    target_compile_options(${MODULE_NAME} PRIVATE
        -fPIC
        -std=c++17
        -fvisibility=hidden
    )
    target_compile_definitions(${MODULE_NAME} PRIVATE
        -DLAMINA_EXPORT=__attribute__\(\(visibility\(\"default\"\)\)\)
    )
endif()

# 包含解释器头文件
target_include_directories(${MODULE_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/interpreter
)
