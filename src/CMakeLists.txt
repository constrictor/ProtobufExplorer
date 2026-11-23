add_subdirectory(core)
add_subdirectory(ui)

qt_add_executable(protobufexplorer main.cpp)
target_compile_definitions(protobufexplorer PRIVATE PROTOEXPLORER_VERSION="${PROJECT_VERSION}")
target_link_libraries(protobufexplorer PRIVATE protoexplorer_ui protoexplorer_warnings)
set_target_properties(protobufexplorer PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
