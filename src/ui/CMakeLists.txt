add_library(protoexplorer_ui STATIC
    FindBar.cpp
    FindBar.h
    MainWindow.cpp
    MainWindow.h
    ProtoHighlighter.cpp
    ProtoHighlighter.h
    SchemaTab.cpp
    SchemaTab.h
    SchemaTreeView.cpp
    SchemaTreeView.h
    SourceView.cpp
    SourceView.h
    UiColors.cpp
    UiColors.h
)
target_include_directories(protoexplorer_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(protoexplorer_ui
    PUBLIC protoexplorer_core Qt6::Widgets
    PRIVATE Qt6::Concurrent protoexplorer_warnings)
