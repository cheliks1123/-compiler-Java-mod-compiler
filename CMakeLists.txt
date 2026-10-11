cmake_minimum_required(VERSION 3.20)
project(ModBuilder CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  # статический рантайм: exe запускается без установки VC++ Redistributable
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.91.5 GIT_SHALLOW TRUE)
FetchContent_Declare(json  GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.3 GIT_SHALLOW TRUE)
FetchContent_Declare(miniz GIT_REPOSITORY https://github.com/richgel999/miniz.git GIT_TAG 3.0.2 GIT_SHALLOW TRUE)

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(INSTALL_PROJECT OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(imgui json miniz)

add_library(core STATIC src/logparse.cpp src/zip_util.cpp)
target_include_directories(core PUBLIC src)
target_link_libraries(core PUBLIC miniz)
if(MSVC)
  target_compile_options(core PRIVATE /utf-8)
endif()

if(WIN32)
  add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp)
  target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)

  add_executable(ModBuilder WIN32 src/main.cpp src/github.cpp src/config.cpp)
  target_compile_definitions(ModBuilder PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
  target_link_libraries(ModBuilder PRIVATE core imgui_lib nlohmann_json::nlohmann_json
    d3d11 dxgi d3dcompiler winhttp crypt32 shell32 dwmapi)
  if(MSVC)
    target_compile_options(ModBuilder PRIVATE /utf-8)
  endif()
else()
  enable_testing()
  add_executable(logparse_test tests/logparse_test.cpp)
  target_link_libraries(logparse_test PRIVATE core)
  add_test(NAME logparse_test COMMAND logparse_test)
endif()
