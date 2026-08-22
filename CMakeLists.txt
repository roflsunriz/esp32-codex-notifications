cmake_minimum_required(VERSION 3.20)
project(esp32_codex_notifications_tests LANGUAGES CXX)

enable_testing()

add_executable(ui-model-test
  src/ui-model.cpp
  test/ui-model-test.cpp
)

target_include_directories(ui-model-test PRIVATE include)
target_compile_features(ui-model-test PRIVATE cxx_std_17)

if(MSVC)
  target_compile_options(ui-model-test PRIVATE /W4 /WX /utf-8)
else()
  target_compile_options(ui-model-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME ui-model COMMAND ui-model-test)
