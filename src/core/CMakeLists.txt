add_library(protoexplorer_core STATIC
    ProtoParser.cpp
    ProtoParser.h
    Schema.h
    SchemaGraph.cpp
    SchemaGraph.h
    SchemaLoader.cpp
    SchemaLoader.h
    SchemaModel.cpp
    SchemaModel.h
    SchemaTree.cpp
    SchemaTree.h
    SourceText.cpp
    SourceText.h
    TypeResolver.cpp
    TypeResolver.h
)
target_include_directories(protoexplorer_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(protoexplorer_core PRIVATE QT_NO_KEYWORDS)
target_link_libraries(protoexplorer_core
    PUBLIC Qt6::Core
    PRIVATE protobuf::libprotobuf absl::log_globals protoexplorer_warnings)

# The schemas protoc lets every file import without having them.
set(bundled any api compiler/plugin cpp_features descriptor duration empty field_mask
    source_context struct timestamp type wrappers)
list(TRANSFORM bundled PREPEND ${protobuf_SOURCE_DIR}/src/google/protobuf/)
list(TRANSFORM bundled APPEND .proto)
qt_add_resources(protoexplorer_core bundled_protos
    PREFIX /protobuf
    BASE ${protobuf_SOURCE_DIR}/src
    FILES ${bundled})
