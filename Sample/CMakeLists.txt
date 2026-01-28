cmake_minimum_required(VERSION 3.20)
project(Direct3D12Renderer)
set(CMAKE_CXX_STANDARD 17)


# -------------------------------
# 出力ディレクトリの設定 (DLL と exe を同じ bin に)
# -------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# マルチ構成 (Visual Studio 用)
foreach(OUTPUTCONFIG Debug Release RelWithDebInfo MinSizeRel)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib)
endforeach()


# ソースファイル
set(SAMPLE_SOURCES
    src/main.cpp
    src/SampleApp.cpp
    src/ImguiUtil.cpp#追加
)

# ヘッダファイル
set(SAMPLE_HEADERS
    include/SampleApp.h
    include/ImguiUtil.h#追加
)

# シェーダーファイル
set(SAMPLE_SHADERS
    res/GGXPS.hlsl
    res/GGXVS.hlsl
    res/Hit.hlsl
    res/RayGen.hlsl
    res/ShadowRay.hlsl
    res/shaders.hlsl
    res/Common.hlsl
    res/Miss.hlsl
)

# 実行ファイルとして作成（シェーダーは含めない、add_custom_commandでコンパイル）
# wmain()を使用しているためWIN32フラグは不要（コンソールアプリケーション）
add_executable(${PROJECT_NAME} ${SAMPLE_SOURCES} ${SAMPLE_HEADERS})

# インクルードディレクトリを設定
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Frameworkライブラリをリンク（PUBLICインクルードパスも継承される）
# CMakeターゲット名を指定すると、自動的にFramework.libがリンクされる
target_link_libraries(${PROJECT_NAME} PRIVATE
    Framework
)

# Frameworkライブラリへの依存を明示（ビルド順序を保証）
add_dependencies(${PROJECT_NAME} Framework)

# Windows用の設定
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

# =====================================
# HLSL Shader Compilation
# =====================================
# fxc.exe (DirectX Shader Compiler) を探す
find_program(FXC fxc 
    HINTS 
    "$ENV{WindowsSdkDir}bin/$ENV{WindowsSDKVersion}x64"
    "$ENV{WindowsSdkDir}bin/x64"
)

if(NOT FXC)
    message(FATAL_ERROR "fxc.exe not found. Please install Windows SDK.")
endif()

# 出力ディレクトリを実行ファイルと同じ場所に設定
set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR})

# Vertex Shader のコンパイル
add_custom_command(
    OUTPUT ${SHADER_OUTPUT_DIR}/GGXVS.cso
    COMMAND ${FXC} /T vs_5_0 /E main /Fo ${SHADER_OUTPUT_DIR}/GGXVS.cso ${CMAKE_CURRENT_SOURCE_DIR}/res/GGXVS.hlsl
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/res/GGXVS.hlsl
    COMMENT "Compiling Vertex Shader: GGXVS.hlsl"
    VERBATIM
)

# Pixel Shader のコンパイル
add_custom_command(
    OUTPUT ${SHADER_OUTPUT_DIR}/GGXPS.cso
    COMMAND ${FXC} /T ps_5_0 /E main /Fo ${SHADER_OUTPUT_DIR}/GGXPS.cso ${CMAKE_CURRENT_SOURCE_DIR}/res/GGXPS.hlsl
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/res/GGXPS.hlsl
    COMMENT "Compiling Pixel Shader: GGXPS.hlsl"
    VERBATIM
)

# カスタムターゲットを作成してシェーダーをビルド対象に追加
add_custom_target(CompileShaders ALL
    DEPENDS 
        ${SHADER_OUTPUT_DIR}/GGXVS.cso
        ${SHADER_OUTPUT_DIR}/GGXPS.cso
)

# Sampleのビルド前にシェーダーをコンパイル
add_dependencies(${PROJECT_NAME} CompileShaders)

target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)

# シェーダーファイルとリソースをビルド後にコピー
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Copying compiled shaders and resources..."
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SHADER_OUTPUT_DIR}/GGXVS.cso
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/GGXVS.cso
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SHADER_OUTPUT_DIR}/GGXPS.cso
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/GGXPS.cso
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/res/teapot
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/res/teapot
    COMMENT "Copying compiled shaders and resources to output directory"
)

# AssimpのDLLをコピー（ビルドされたDLLの場所から）
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:assimp>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying Assimp DLL to output directory"
)

# DirectXTK12のDLLもコピー
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:DirectXTK12>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying DirectXTK12 DLL to output directory"
)

# DXC (Shader Compiler) 関連 DLL のコピー
if(WIN32)
    set(DXC_DLL_PATH "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64")

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DXC_DLL_PATH}/dxcompiler.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DXC_DLL_PATH}/dxil.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Copying DXC (dxcompiler.dll, dxil.dll) to output directory"
    )
endif()