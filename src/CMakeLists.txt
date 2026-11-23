add_subdirectory(core)
add_subdirectory(ui)

qt_add_executable(protobufexplorer main.cpp)
target_compile_definitions(protobufexplorer PRIVATE PROTOEXPLORER_VERSION="${PROJECT_VERSION}")
target_link_libraries(protobufexplorer PRIVATE protoexplorer_ui protoexplorer_warnings)
set_target_properties(protobufexplorer PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

# What the Debian package and the AppImage are made of.
include(GNUInstallDirs)
set(packaging ${PROJECT_SOURCE_DIR}/packaging/linux)
set(doc ${CMAKE_INSTALL_DATADIR}/doc/protobufexplorer)
install(TARGETS protobufexplorer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${packaging}/protobufexplorer.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${packaging}/protobufexplorer.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES ${PROJECT_SOURCE_DIR}/LICENSE ${packaging}/copyright DESTINATION ${doc})
install(FILES ${protobuf_SOURCE_DIR}/LICENSE DESTINATION ${doc} RENAME LICENSE.protobuf)
install(FILES ${absl_SOURCE_DIR}/LICENSE DESTINATION ${doc} RENAME LICENSE.abseil)
install(FILES ${protobuf_SOURCE_DIR}/third_party/utf8_range/LICENSE
    DESTINATION ${doc} RENAME LICENSE.utf8_range)
