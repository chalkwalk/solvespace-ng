add_library(pie STATIC pie.cpp)
target_include_directories(pie PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(pie PUBLIC cxx_std_20)
