cmake_minimum_required(VERSION 3.20)
project(TaskbarEnd LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_compile_definitions(UNICODE _UNICODE WINVER=0x0A00 _WIN32_WINNT=0x0A00)

if(MSVC)
  add_compile_options(/utf-8 /EHsc)
endif()

add_library(TaskbarEndCommon STATIC
  src/common/log.cpp
)
target_include_directories(TaskbarEndCommon PUBLIC src/common)
target_link_libraries(TaskbarEndCommon PUBLIC shlwapi)

add_library(TaskbarEndHook SHARED
  src/hook/dll_main.cpp
  src/hook/shared_state.cpp
  src/hook/worker.cpp
  src/hook/input_hooks.cpp
  src/hook/target.cpp
  src/hook/end_button.cpp
  src/hook/terminate.cpp
  src/hook/safety.cpp
)
target_include_directories(TaskbarEndHook PRIVATE src/hook src/common)
target_link_libraries(TaskbarEndHook PRIVATE TaskbarEndCommon oleacc ole32 uiautomationcore)
target_compile_definitions(TaskbarEndHook PRIVATE TASKBAREND_HOOK_EXPORTS)

add_executable(TaskbarEndInjector
  src/injector/main.cpp
)
target_include_directories(TaskbarEndInjector PRIVATE src/common)
target_link_libraries(TaskbarEndInjector PRIVATE TaskbarEndCommon)
