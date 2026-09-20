cmake_minimum_required(VERSION 3.30)
include("${CMAKE_CURRENT_LIST_DIR}/GyoAppRegistry.cmake")

if(NOT DEFINED GYO_REGISTRY_FILE)
    set(GYO_REGISTRY_FILE "${CMAKE_CURRENT_LIST_DIR}/../config/engine/projects.csv")
endif()
gyo_read_app_registry("${GYO_REGISTRY_FILE}" registry)
gyo_export_app_matrix("${registry}" matrix)

if(DEFINED GYO_OUTPUT AND NOT "${GYO_OUTPUT}" STREQUAL "")
    get_filename_component(output_directory "${GYO_OUTPUT}" DIRECTORY)
    if(NOT "${output_directory}" STREQUAL "")
        file(MAKE_DIRECTORY "${output_directory}")
    endif()
    file(WRITE "${GYO_OUTPUT}" "${matrix}\n")
else()
    # message() uses stderr; CI callers need a clean JSON document on stdout.
    execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "${matrix}"
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Could not write the app registry matrix to stdout")
    endif()
endif()
