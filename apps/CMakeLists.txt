foreach(GYO_CURRENT_APP IN LISTS GYO_ACTIVE_APPS)
    add_subdirectory("${GYO_CURRENT_APP}")
endforeach()
