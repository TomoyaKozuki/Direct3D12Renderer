cmake_minimum_required(VERSION 3.20)

project(Direct3D12_GGX_Renderer)

# C++17を使用
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# サブディレクトリを追加
add_subdirectory(Framework)
add_subdirectory(Sample)

# Visual Studio: スタートアッププロジェクト指定
set_property(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT Direct3D12Renderer
)