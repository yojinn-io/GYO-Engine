if(GYO_BUILD_RUNTIME)
    add_subdirectory(runtime)
endif()

if(GYO_BUILD_SANDBOX)
    add_subdirectory(sandbox)
endif()

if(GYO_BUILD_OBJECT_FPS)
    add_subdirectory(object_fps)
endif()
