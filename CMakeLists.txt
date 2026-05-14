cmake_minimum_required(VERSION 3.21)

project(TherionStudio
    VERSION 0.1.0
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(TherionStudio MACOSX_BUNDLE
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/app/MainWindowDocumentHelpers.cpp
    src/app/MainWindowDocumentHelpers.h
    src/app/MainWindowInspector.cpp
    src/app/MainWindowMapBackground.cpp
    src/app/MainWindowSidebar.cpp
    src/app/MainWindowStructureBrowser.cpp
    src/app/MainWindowStructureRoles.h
    src/app/MainWindowTherionRunner.cpp
    src/app/MapEditorBackgroundLayers.cpp
    src/app/MapEditorInputPolicy.cpp
    src/app/MapEditorInputPolicy.h
    src/app/MapEditorSceneInternals.h
    src/app/MapEditorSceneItems.cpp
    src/app/MapEditorSceneRenderer.cpp
    src/app/MapEditorSceneSupport.h
    src/app/MapEditorTab.cpp
    src/app/MapEditorTabWorkspace.cpp
    src/app/MapEditorTab.h
    src/app/TextEditorTab.cpp
    src/app/TextEditorTab.h
    src/core/DocumentFile.cpp
    src/core/DocumentFile.h
    src/core/MapBackgroundPlacement.cpp
    src/core/MapBackgroundPlacement.h
    src/core/TherionBackgroundMetadata.cpp
    src/core/TherionBackgroundMetadata.h
    src/core/TherionXviParser.cpp
    src/core/TherionXviParser.h
    src/core/TherionDocumentEditor.cpp
    src/core/TherionDocumentEditor.h
    src/core/ProjectStructureIndex.cpp
    src/core/ProjectStructureIndex.h
    src/core/TherionDocumentParser.cpp
    src/core/TherionDocumentParser.h
    src/core/SessionStore.cpp
    src/core/SessionStore.h
    src/editor/TherionSyntaxHighlighter.cpp
    src/editor/TherionSyntaxHighlighter.h
    src/main.cpp)

qt_add_resources(TherionStudio "TherionStudioResources"
    PREFIX "/"
    FILES
        resources/therion_help_metadata.json
        resources/therion_syntax_palette.json)

if(APPLE)
    set_target_properties(TherionStudio PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Therion Studio"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.therionstudio.app"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
endif()

target_link_libraries(TherionStudio PRIVATE Qt6::Widgets)

target_compile_definitions(TherionStudio PRIVATE
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII)

qt_add_executable(TherionDocumentEditorTest
    tests/TherionDocumentEditorTest.cpp
    src/core/TherionDocumentEditor.cpp
    src/core/TherionDocumentEditor.h
    src/core/TherionDocumentParser.cpp
    src/core/TherionDocumentParser.h)

target_link_libraries(TherionDocumentEditorTest PRIVATE Qt6::Widgets)

qt_add_executable(TherionProjectStructureIndexTest
    tests/ProjectStructureIndexTest.cpp
    src/core/DocumentFile.cpp
    src/core/DocumentFile.h
    src/core/ProjectStructureIndex.cpp
    src/core/ProjectStructureIndex.h
    src/core/TherionDocumentParser.cpp
    src/core/TherionDocumentParser.h)

target_link_libraries(TherionProjectStructureIndexTest PRIVATE Qt6::Widgets)

qt_add_executable(MapBackgroundPlacementTest
    tests/MapBackgroundPlacementTest.cpp
    src/core/MapBackgroundPlacement.cpp
    src/core/MapBackgroundPlacement.h)

target_link_libraries(MapBackgroundPlacementTest PRIVATE Qt6::Widgets)

qt_add_executable(TherionBackgroundMetadataTest
    tests/TherionBackgroundMetadataTest.cpp
    src/core/TherionBackgroundMetadata.cpp
    src/core/TherionBackgroundMetadata.h)

target_link_libraries(TherionBackgroundMetadataTest PRIVATE Qt6::Widgets)

qt_add_executable(TherionXviParserTest
    tests/TherionXviParserTest.cpp
    src/core/TherionXviParser.cpp
    src/core/TherionXviParser.h)

target_link_libraries(TherionXviParserTest PRIVATE Qt6::Widgets)

qt_add_executable(MapGeometryFeatureParsingTest
    tests/MapGeometryFeatureParsingTest.cpp
    src/app/MapEditorSceneItems.cpp
    src/app/MapEditorSceneInternals.h
    src/app/MapEditorSceneRenderer.cpp
    src/app/MapEditorSceneSupport.h
    src/core/TherionDocumentParser.cpp
    src/core/TherionDocumentParser.h)

target_link_libraries(MapGeometryFeatureParsingTest PRIVATE Qt6::Widgets)

qt_add_executable(DocumentFileEncodingTest
    tests/DocumentFileEncodingTest.cpp
    src/core/DocumentFile.cpp
    src/core/DocumentFile.h
    src/core/TherionDocumentEditor.cpp
    src/core/TherionDocumentEditor.h
    src/core/TherionDocumentParser.cpp
    src/core/TherionDocumentParser.h)

target_link_libraries(DocumentFileEncodingTest PRIVATE Qt6::Widgets)

qt_add_executable(MapEditorInputPolicyTest
    tests/MapEditorInputPolicyTest.cpp
    src/app/MapEditorInputPolicy.cpp
    src/app/MapEditorInputPolicy.h)

target_link_libraries(MapEditorInputPolicyTest PRIVATE Qt6::Widgets)

qt_add_executable(SessionStoreTest
    tests/SessionStoreTest.cpp
    src/core/SessionStore.cpp
    src/core/SessionStore.h)

target_link_libraries(SessionStoreTest PRIVATE Qt6::Widgets)
