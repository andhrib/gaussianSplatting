cmake_minimum_required(VERSION 3.20)
project(GaussianSplatting)

# ── Safety: refuse in-source builds ─────────────────────────
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not allowed.\n"
        "Run CMake from a separate build directory, e.g.:\n"
        "  cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug\n"
        "  cmake --build build-debug"
    )
endif()

set(D3D12_LIB "d3d12.lib")
find_package(directx-headers CONFIG REQUIRED GLOBAL)
find_package(directx12-agility CONFIG REQUIRED GLOBAL)
find_package(directxtex CONFIG REQUIRED GLOBAL)
find_package(assimp CONFIG REQUIRED GLOBAL)

add_library(DirectXTex ALIAS Microsoft::DirectXTex)
add_library(assimp ALIAS assimp::assimp)

add_library(DirectX12 INTERFACE)
add_library(DirectX12::DirectX12 ALIAS DirectX12)

target_link_libraries(DirectX12
    INTERFACE
        Microsoft::DirectX-Headers
        Microsoft::DirectX-Guids
        Microsoft::DirectX12-Agility
        d3d12.lib
        dxgi.lib
        dxguid.lib
)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# ── Dependencies ─────────────────────────────────────────────
set(GAINPUT_SAMPLES OFF CACHE BOOL "" FORCE)
set(GAINPUT_TESTS   OFF CACHE BOOL "" FORCE)
add_subdirectory(extern/gainput)
add_subdirectory(DX12Lib)
add_subdirectory(GameFramework)

# ── Source files ─────────────────────────────────────────────
set(SOURCE_FILES
    src/main.cpp
    src/Camera.cpp
    src/SplatLoader.cpp
    src/Benchmark.cpp
    src/Screenshot.cpp
)

set(HEADER_FILES
    src/Globals.h
    src/Camera.h
    src/SplatLoader.h
    src/Benchmark.h
    src/Screenshot.h
)

set(SHADER_FILES
    shaders/VertexShader.hlsl
    shaders/PixelShader.hlsl
    shaders/BitonicSort.hlsl
)

# ── Visual Studio source groups ──────────────────────────────
# These control the folder tabs / filters shown in the VS Solution Explorer.
source_group("Source Files"      FILES ${SOURCE_FILES})
source_group("Header Files"      FILES ${HEADER_FILES})
source_group("Resources\\Shaders" FILES ${SHADER_FILES})

# ── Shader compilation properties ────────────────────────────
set_source_files_properties(${SHADER_FILES} PROPERTIES
    VS_SHADER_MODEL 6.0
    VS_SHADER_DISABLE_OPTIMIZATIONS $<$<CONFIG:Debug>:ON>
    VS_SHADER_ENABLE_DEBUG $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:ON>
    VS_SHADER_FLAGS $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:-Qembed_debug>
)

set_source_files_properties(shaders/VertexShader.hlsl PROPERTIES VS_SHADER_TYPE Vertex)
set_source_files_properties(shaders/PixelShader.hlsl  PROPERTIES VS_SHADER_TYPE Pixel)
set_source_files_properties(shaders/BitonicSort.hlsl  PROPERTIES VS_SHADER_TYPE Compute)

# ── Executable ───────────────────────────────────────────────
add_executable(GaussianSplatting WIN32
    ${SOURCE_FILES}
    ${HEADER_FILES}
    ${SHADER_FILES}
)

# ── Agility SDK DLLs ─────────────────────────────────────────
if(TARGET Microsoft::DirectX12-Agility)
    add_custom_command(TARGET GaussianSplatting POST_BUILD
        COMMENT "Copy DirectX12 Agility SDK DLLs"
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:GaussianSplatting>/D3D12
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PROPERTY:Microsoft::DirectX12-Core,IMPORTED_LOCATION_RELEASE>  $<TARGET_FILE_DIR:GaussianSplatting>/D3D12/
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PROPERTY:Microsoft::DirectX12-Layers,IMPORTED_LOCATION_DEBUG> $<TARGET_FILE_DIR:GaussianSplatting>/D3D12/
        COMMAND_EXPAND_LISTS
    )
endif()

# ── Compile definitions ───────────────────────────────────────
target_compile_definitions(GaussianSplatting PRIVATE
    PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}"
)

# ── Include paths ─────────────────────────────────────────────
target_include_directories(GaussianSplatting PRIVATE
    src               # lets files include "Globals.h" etc. without path prefixes
    include           # any other project-wide headers
)

# ── Link libraries ────────────────────────────────────────────
target_link_libraries(GaussianSplatting PRIVATE
    DX12Lib
    GameFramework
    d3d12
    dxgi
    dxguid
    d3dcompiler
)

# ── Assets ────────────────────────────────────────────────────
if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
    add_custom_command(TARGET GaussianSplatting POST_BUILD
        COMMENT "Copying assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:GaussianSplatting>"
    )
endif()

# ── Visual Studio: set startup project ───────────────────────
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT GaussianSplatting)