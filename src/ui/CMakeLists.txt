add_library(protoexplorer_ui STATIC
    FindBar.cpp
    FindBar.h
    JsonEdit.cpp
    JsonEdit.h
    JsonHighlighter.cpp
    JsonHighlighter.h
    JsonRows.cpp
    JsonRows.h
    JsonView.cpp
    JsonView.h
    MainWindow.cpp
    MainWindow.h
    MessagePicker.cpp
    MessagePicker.h
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
    WhereLabel.cpp
    WhereLabel.h
)
target_include_directories(protoexplorer_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(protoexplorer_ui
    PUBLIC protoexplorer_core Qt6::Widgets
    PRIVATE Qt6::Concurrent protoexplorer_warnings)
