add_library(gyo_model STATIC src/ModelAsset.cpp src/Animation.cpp)
add_library(GYO::Model ALIAS gyo_model)
target_include_directories(gyo_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gyo_model PUBLIC GYO::Engine)
target_compile_features(gyo_model PUBLIC cxx_std_20)

if(TARGET gyo_ufbx)
    add_subdirectory(backend/ufbx)
endif()


