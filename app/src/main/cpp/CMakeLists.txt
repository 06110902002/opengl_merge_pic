cmake_minimum_required(VERSION 3.18.1)

project("texture-stitch")

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)

# 添加源文件
add_library(
        texture-stitch
        SHARED
        texture_stitch.cpp
)

# 查找依赖库
find_library(
        log-lib
        log
)

find_library(
        android-lib
        android
)

# 必须添加jnigraphics库
find_library(
        jnigraphics-lib
        jnigraphics
)

# 链接库
target_link_libraries(
        texture-stitch
        GLESv3
        ${log-lib}
        ${android-lib}
        ${jnigraphics-lib}
)

# 包含头文件目录
target_include_directories(texture-stitch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})