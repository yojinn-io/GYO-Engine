# The JSON binding (docs/framework/architecture.md, Design Invariants 2 and 4):
# catalog text. Built with the core; it names its JSON library itself, since
# Engine's is private.
add_library(gyo_tool_framework_json STATIC src/CatalogText.cpp)
add_library(GYO::ToolFrameworkJson ALIAS gyo_tool_framework_json)
target_include_directories(gyo_tool_framework_json PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(gyo_tool_framework_json
    PUBLIC GYO::ToolFramework
    PRIVATE nlohmann_json::nlohmann_json
)
gyo_framework_warnings(gyo_tool_framework_json)
