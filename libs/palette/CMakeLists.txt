add_library(palette STATIC fuzzy.cpp)
target_include_directories(palette PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(palette PUBLIC cxx_std_20)
