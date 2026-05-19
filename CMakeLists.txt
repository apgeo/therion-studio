cmake_minimum_required(VERSION 3.21)

project(TherionStudio
    VERSION 0.1.0
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Svg)

qt_standard_project_setup()

qt_add_executable(TherionStudio MACOSX_BUNDLE
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/app/MainWindowDocumentHelpers.cpp
    src/app/MainWindowDocumentHelpers.h
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

set(THERION_STUDIO_RESOURCE_FILES
    resources/therion_command_catalog.json
    resources/therion_command_catalog.overrides.json
    resources/therion_syntax_palette.json
    resources/icons/lucide/LICENSE
    resources/icons/lucide/check.svg
    resources/icons/lucide/code.svg
    resources/icons/lucide/cog.svg
    resources/icons/lucide/external-link.svg
    resources/icons/lucide/eye.svg
    resources/icons/lucide/eye-off.svg
    resources/icons/lucide/folder-open.svg
    resources/icons/lucide/hand.svg
    resources/icons/lucide/image.svg
    resources/icons/lucide/map-pin.svg
    resources/icons/lucide/locate-fixed.svg
    resources/icons/lucide/map.svg
    resources/icons/lucide/mouse-pointer-2.svg
    resources/icons/lucide/network.svg
    resources/icons/lucide/pen-tool.svg
    resources/icons/lucide/pencil-line.svg
    resources/icons/lucide/pentagon.svg
    resources/icons/lucide/puzzle.svg
    resources/icons/lucide/redo-2.svg
    resources/icons/lucide/save.svg
    resources/icons/lucide/scan.svg
    resources/icons/lucide/screen-share.svg
    resources/icons/lucide/screen-share-off.svg
    resources/icons/lucide/spline.svg
    resources/icons/lucide/square-plus.svg
    resources/icons/lucide/toy-brick.svg
    resources/icons/lucide/trash-2.svg
    resources/icons/lucide/undo-2.svg
    resources/icons/lucide/wand-sparkles.svg
    resources/icons/lucide/zoom-in.svg
    resources/icons/lucide/zoom-out.svg)

qt_add_resources(TherionStudio "TherionStudioResources"
    PREFIX "/"
    FILES
        ${THERION_STUDIO_RESOURCE_FILES})

if(APPLE)
    set_target_properties(TherionStudio PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Therion Studio"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.therionstudio.app"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
endif()

target_link_libraries(TherionStudio PRIVATE Qt6::Widgets Qt6::Svg)

target_compile_definitions(TherionStudio PRIVATE
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII)

install(TARGETS TherionStudio
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .)

if(WIN32)
    qt_generate_deploy_app_script(
        TARGET TherionStudio
        OUTPUT_SCRIPT THERION_STUDIO_QT_DEPLOY_SCRIPT
        NO_UNSUPPORTED_PLATFORM_ERROR)
    install(SCRIPT ${THERION_STUDIO_QT_DEPLOY_SCRIPT})

    set(CPACK_GENERATOR "NSIS")
    set(CPACK_PACKAGE_NAME "Therion Studio")
    set(CPACK_PACKAGE_VENDOR "Therion Studio")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Therion Studio")
    set(CPACK_PACKAGE_EXECUTABLES "TherionStudio" "Therion Studio")
    set(CPACK_CREATE_DESKTOP_LINKS "TherionStudio")
    set(CPACK_NSIS_DISPLAY_NAME "Therion Studio")
    set(CPACK_NSIS_PACKAGE_NAME "Therion Studio")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    include(CPack)
endif()

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

qt_add_executable(TextEditorCaretInteractionTest
    tests/TextEditorCaretInteractionTest.cpp
    src/app/TextEditorTab.cpp
    src/app/TextEditorTab.h
    src/core/DocumentFile.cpp
    src/core/DocumentFile.h
    src/core/TherionDocumentEditor.cpp
    src/core/TherionDocumentEditor.h
    src/core/TherionDocumentParser.cpp
    src/core/TherionDocumentParser.h
    src/editor/TherionSyntaxHighlighter.cpp
    src/editor/TherionSyntaxHighlighter.h)

target_link_libraries(TextEditorCaretInteractionTest PRIVATE Qt6::Widgets)

qt_add_resources(TextEditorCaretInteractionTest "TextEditorCaretInteractionResources"
    PREFIX "/"
    FILES
        resources/therion_command_catalog.json
        resources/therion_command_catalog.overrides.json
        resources/therion_syntax_palette.json)

qt_add_executable(TextEditorCompletionHighlightTest
    tests/TextEditorCompletionHighlightTest.cpp
    src/app/TextEditorTab.cpp
    src/app/TextEditorTab.h
    src/core/DocumentFile.cpp
    src/core/DocumentFile.h
    src/core/TherionDocumentEditor.cpp
    src/core/TherionDocumentEditor.h
    src/core/TherionDocumentParser.cpp
    src/core/TherionDocumentParser.h
    src/editor/TherionSyntaxHighlighter.cpp
    src/editor/TherionSyntaxHighlighter.h)

target_link_libraries(TextEditorCompletionHighlightTest PRIVATE Qt6::Widgets)

qt_add_resources(TextEditorCompletionHighlightTest "TextEditorCompletionHighlightResources"
    PREFIX "/"
    FILES
        resources/therion_command_catalog.json
        resources/therion_command_catalog.overrides.json
        resources/therion_syntax_palette.json)

qt_add_executable(MapEditorDetachedPaneTest
    tests/MapEditorDetachedPaneTest.cpp
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
    src/core/ProjectStructureIndex.cpp
    src/core/ProjectStructureIndex.h
    src/core/SessionStore.cpp
    src/core/SessionStore.h
    src/core/TherionBackgroundMetadata.cpp
    src/core/TherionBackgroundMetadata.h
    src/core/TherionDocumentEditor.cpp
    src/core/TherionDocumentEditor.h
    src/core/TherionDocumentParser.cpp
    src/core/TherionDocumentParser.h
    src/core/TherionXviParser.cpp
    src/core/TherionXviParser.h
    src/editor/TherionSyntaxHighlighter.cpp
    src/editor/TherionSyntaxHighlighter.h)

target_link_libraries(MapEditorDetachedPaneTest PRIVATE Qt6::Widgets Qt6::Svg)

qt_add_resources(MapEditorDetachedPaneTest "MapEditorDetachedPaneResources"
    PREFIX "/"
    FILES
        ${THERION_STUDIO_RESOURCE_FILES})

qt_add_executable(MapEditorDragUndoRedoSmokeTest
    tests/MapEditorDragUndoRedoSmokeTest.cpp
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
    src/core/ProjectStructureIndex.cpp
    src/core/ProjectStructureIndex.h
    src/core/SessionStore.cpp
    src/core/SessionStore.h
    src/core/TherionBackgroundMetadata.cpp
    src/core/TherionBackgroundMetadata.h
    src/core/TherionDocumentEditor.cpp
    src/core/TherionDocumentEditor.h
    src/core/TherionDocumentParser.cpp
    src/core/TherionDocumentParser.h
    src/core/TherionXviParser.cpp
    src/core/TherionXviParser.h
    src/editor/TherionSyntaxHighlighter.cpp
    src/editor/TherionSyntaxHighlighter.h)

target_link_libraries(MapEditorDragUndoRedoSmokeTest PRIVATE Qt6::Widgets Qt6::Svg)

qt_add_resources(MapEditorDragUndoRedoSmokeTest "MapEditorDragUndoRedoSmokeResources"
    PREFIX "/"
    FILES
        ${THERION_STUDIO_RESOURCE_FILES})
