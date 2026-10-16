add_library(keymap STATIC keymap.cpp)
target_include_directories(keymap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(keymap PUBLIC cxx_std_20)
