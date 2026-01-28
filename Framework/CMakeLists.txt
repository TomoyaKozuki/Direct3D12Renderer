cmake_minimum_required(VERSION 3.20)
project(Framework)
set(CMAKE_CXX_STANDARD 17)

# =====================================
# Assimp options (submodule use)
# =====================================
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# =====================================
# Prebuilt Assimp detection
# =====================================
set(PREBUILT_ASSIMP_LIB_DEBUG   "${CMAKE_BINARY_DIR}/lib/Debug/assimp-vc143-mtd.lib")
set(PREBUILT_ASSIMP_LIB_RELEASE "${CMAKE_BINARY_DIR}/lib/Release/assimp-vc143-mt.lib")

set(PREBUILT_ASSIMP_DLL_DEBUG   "${CMAKE_BINARY_DIR}/bin/Debug/assimp-vc143-mtd.dll")
set(PREBUILT_ASSIMP_DLL_RELEASE "${CMAKE_BINARY_DIR}/bin/Release/assimp-vc143-mt.dll")

if(EXISTS "${PREBUILT_ASSIMP_LIB_DEBUG}"
   AND EXISTS "${PREBUILT_ASSIMP_LIB_RELEASE}"
   AND EXISTS "${PREBUILT_ASSIMP_DLL_DEBUG}"
   AND EXISTS "${PREBUILT_ASSIMP_DLL_RELEASE}")
    set(USE_PREBUILT_ASSIMP TRUE)
else()
    set(USE_PREBUILT_ASSIMP FALSE)
endif()

message(STATUS "Use prebuilt Assimp = ${USE_PREBUILT_ASSIMP}")

# =====================================
# Add Assimp (prebuilt or build from submodule)
# =====================================
if(USE_PREBUILT_ASSIMP)
    message(STATUS "Using prebuilt Assimp")

    add_library(assimp SHARED IMPORTED)

    set_target_properties(assimp PROPERTIES
        IMPORTED_LOCATION_DEBUG     "${PREBUILT_ASSIMP_DLL_DEBUG}"
        IMPORTED_IMPLIB_DEBUG       "${PREBUILT_ASSIMP_LIB_DEBUG}"
        IMPORTED_LOCATION_RELEASE   "${PREBUILT_ASSIMP_DLL_RELEASE}"
        IMPORTED_IMPLIB_RELEASE     "${PREBUILT_ASSIMP_LIB_RELEASE}"
    )

    target_include_directories(assimp INTERFACE
        "${CMAKE_SOURCE_DIR}/extern/assimp/include"
    )

else()
    message(STATUS "Building Assimp from submodule")
    add_subdirectory(extern/assimp)
endif()

# =====================================
# Add DirectXTK12 submodule
# =====================================
# DirectXTK12 を DLL にしたい場合
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build DirectXTK12 as DLL" FORCE)
add_subdirectory(extern/DirectXTK12)

# =====================================
# Source Files(.cpp)
# =====================================
# ソースファイル
set(FRAMEWORK_SOURCES
    src/App.cpp
    src/ColorTarget.cpp
    src/CommandList.cpp
    src/ConstantBuffer.cpp
    src/DepthTarget.cpp
    src/DescriptorPool.cpp
    src/Fence.cpp
    src/FileUtil.cpp
    src/IndexBuffer.cpp
    src/Logger.cpp
    src/Material.cpp
    src/Mesh.cpp
    src/ResMesh.cpp
    src/Texture.cpp
    src/VertexBuffer.cpp
    #src/ImguiUtil.cpp
    src/WindowEvent.cpp

    extern/nv_helpers_dx12/src/BottomLevelASGenerator.cpp
    extern/nv_helpers_dx12/src/RaytracingPipelineGenerator.cpp
    extern/nv_helpers_dx12/src/RootSignatureGenerator.cpp
    extern/nv_helpers_dx12/src/ShaderBindingTableGenerator.cpp
    extern/nv_helpers_dx12/src/TopLevelASGenerator.cpp

)

# Imgui のすべての cpp を取得
file(GLOB IMGUI_FILES
    extern/imgui/*.cpp

    extern/imgui/imgui.cpp
    extern/imgui/imgui_demo.cpp
    extern/imgui/imgui_draw.cpp
    extern/imgui/imgui_tables.cpp
    extern/imgui/imgui_widgets.cpp

    extern/imgui/backends/imgui_impl_win32.cpp
    extern/imgui/backends/imgui_impl_dx12.cpp
)

# =====================================
# Hearder Files(.h)
# =====================================
# ヘッダファイル
set(FRAMEWORK_HEADERS
    include/App.h
    include/ColorTarget.h
    include/CommandList.h
    include/ComPtr.h
    include/ConstantBuffer.h
    include/DepthTarget.h
    include/DescriptorPool.h
    include/Fence.h
    include/FileUtil.h
    include/IndexBuffer.h
    include/InlineUtil.h
    include/Logger.h
    include/Material.h
    include/Mesh.h
    include/Pool.h
    include/ResMesh.h
    include/Texture.h
    include/VertexBuffer.h
    #include/ImguiUtil.h
    include/EnumUtil.h
    include/WindowEvent.h
    include/d3dx12.h
    include/RenderType.h

    extern/nv_helpers_dx12/include/BottomLevelASGenerator.h
    extern/nv_helpers_dx12/include/RaytracingPipelineGenerator.h
    extern/nv_helpers_dx12/include/RootSignatureGenerator.h
    extern/nv_helpers_dx12/include/ShaderBindingTableGenerator.h
    extern/nv_helpers_dx12/include/TopLevelASGenerator.h
    
    extern/nv_helpers_dx12/include/DXRHelper.h
)



# =====================================
# Framework Library Target
# =====================================
# スタティックライブラリとして作成（ImGuiのソースも含める）
add_library(Framework STATIC 
    ${FRAMEWORK_SOURCES} 
    ${FRAMEWORK_HEADERS}
    ${IMGUI_FILES}
)

# インクルードディレクトリを設定
target_include_directories(Framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/assimp/include
    ${CMAKE_BINARY_DIR}/extern/assimp/include  # 生成されたconfig.hなど
    ${CMAKE_BINARY_DIR}/extern/nv_helpers_dx12/include
)

# Assimpをリンク
target_link_libraries(Framework PUBLIC assimp)

# DirectXTK12をリンク
target_link_libraries(Framework PUBLIC DirectXTK12)

# UNICODE 定義はターゲット作成後に
target_compile_definitions(Framework PRIVATE UNICODE _UNICODE)
# シェーダーコンパイルを実行ファイルのビルド前に実行
#add_dependencies(Framework CompileShaders)
# Windows用の設定
if(WIN32)
    target_compile_definitions(Framework PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    
    # DirectX 12ライブラリをリンク（Windows SDKから提供される）
    target_link_libraries(Framework PUBLIC
        d3d12      # DirectX 12 コアライブラリ
        dxgi       # DirectX Graphics Infrastructure
        dxguid     # DirectX GUIDs
        d3dcompiler # シェーダーコンパイラ
        #dxcapi    # DirectX Compiler API
    )
endif()

# Frameworkライブラリのビルド後、必要なDLLをコピー（Assimpが共有ライブラリの場合）
if(USE_PREBUILT_ASSIMP)
    add_custom_command(TARGET Framework POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PREBUILT_ASSIMP_DLL_DEBUG}"
        "${CMAKE_BINARY_DIR}/bin/Debug/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PREBUILT_ASSIMP_DLL_RELEASE}"
        "${CMAKE_BINARY_DIR}/bin/Release/"
    )
endif()
