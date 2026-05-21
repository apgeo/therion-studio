cmake_minimum_required(VERSION 3.21)

project(TherionStudio
    VERSION 0.1.0
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Svg Concurrent)

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
    src/app/text_editor/ContextHelpController.cpp
    src/app/text_editor/ContextHelpController.h
    src/app/text_editor/TextEditorOptionValidation.cpp
    src/app/text_editor/TextEditorOptionValidation.h
    src/app/text_editor/block_editor/BlockEditorOptionTableDelegate.cpp
    src/app/text_editor/block_editor/BlockEditorOptionTableDelegate.h
src/app/text_editor/block_editor/BlockEditorOptionArgsController.cpp
    src/app/text_editor/block_editor/BlockEditorOptionArgsController.h
    src/app/text_editor/block_editor/BlockEditorDetailsHelpController.cpp
    src/app/text_editor/block_editor/BlockEditorDetailsHelpController.h
    src/app/text_editor/block_editor/BlockEditorLineBuildService.cpp
    src/app/text_editor/block_editor/BlockEditorLineBuildService.h
    src/app/text_editor/block_editor/BlockEditorApplyStateController.cpp
    src/app/text_editor/block_editor/BlockEditorApplyStateController.h
    src/app/text_editor/block_editor/BlockEditorApplyExecutor.cpp
    src/app/text_editor/block_editor/BlockEditorApplyExecutor.h
    src/app/text_editor/block_editor/BlockEditorSelectionDetailsController.cpp
    src/app/text_editor/block_editor/BlockEditorSelectionDetailsController.h
    src/app/text_editor/block_editor/BlockEditorDetailsPaneController.cpp
    src/app/text_editor/block_editor/BlockEditorDetailsPaneController.h
    src/app/text_editor/block_editor/BlockEditorToolboxController.cpp
    src/app/text_editor/block_editor/BlockEditorToolboxController.h
    src/app/text_editor/block_editor/BlockEditorToolboxList.cpp
    src/app/text_editor/block_editor/BlockEditorToolboxList.h
    src/app/text_editor/block_editor/BlockEditorTokenTagEditor.cpp
    src/app/text_editor/block_editor/BlockEditorTokenTagEditor.h
src/app/text_editor/block_editor/BlockEditorToolboxDetailsController.cpp
    src/app/text_editor/block_editor/BlockEditorToolboxDetailsController.h
    src/app/text_editor/block_editor/BlockEditorCanvasSelectionController.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasSelectionController.h
    src/app/text_editor/block_editor/BlockEditorCanvasView.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasView.h
    src/app/text_editor/block_editor/BlockEditorDragMime.h
    src/app/text_editor/block_editor/BlockEditorCanvasItem.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasItem.h
    src/app/text_editor/block_editor/BlockEditorCanvasRebuildController.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasRebuildController.h
    src/app/text_editor/block_editor/BlockEditorCommandOptionParser.cpp
    src/app/text_editor/block_editor/BlockEditorCommandOptionParser.h
    src/app/text_editor/block_editor/BlockEditorConfigureController.cpp
    src/app/text_editor/block_editor/BlockEditorConfigureController.h
    src/app/text_editor/block_editor/BlockEditorCommandOptionsDialog.cpp
    src/app/text_editor/block_editor/BlockEditorCommandOptionsDialog.h
    src/app/text_editor/block_editor/BlockEditorDataBlockDialog.cpp
    src/app/text_editor/block_editor/BlockEditorDataBlockDialog.h
    src/app/text_editor/block_editor/BlockEditorDeleteExecutor.cpp
    src/app/text_editor/block_editor/BlockEditorDeleteExecutor.h
    src/app/text_editor/block_editor/BlockEditorSingleValueCommandDialog.cpp
    src/app/text_editor/block_editor/BlockEditorSingleValueCommandDialog.h
    src/app/text_editor/block_editor/BlockEditorSourceText.cpp
    src/app/text_editor/block_editor/BlockEditorSourceText.h
    src/app/text_editor/block_editor/BlockEditorSourceController.cpp
    src/app/text_editor/block_editor/BlockEditorSourceController.h
    src/app/text_editor/block_editor/BlockEditorCanvasBoundaryController.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasBoundaryController.h
    src/app/text_editor/block_editor/BlockEditorDirectiveRules.cpp
    src/app/text_editor/block_editor/BlockEditorDirectiveRules.h
    src/app/text_editor/block_editor/BlockEditorDocumentOutlineBuilder.cpp
    src/app/text_editor/block_editor/BlockEditorDocumentOutlineBuilder.h
    src/app/text_editor/block_editor/BlockEditorDropTargetResolver.cpp
    src/app/text_editor/block_editor/BlockEditorDropTargetResolver.h
    src/app/text_editor/block_editor/BlockEditorInsertionPlanner.cpp
    src/app/text_editor/block_editor/BlockEditorInsertionPlanner.h
    src/app/text_editor/block_editor/BlockEditorInsertionController.cpp
    src/app/text_editor/block_editor/BlockEditorInsertionController.h
    src/app/text_editor/block_editor/BlockEditorInsertionTemplateBuilder.cpp
    src/app/text_editor/block_editor/BlockEditorInsertionTemplateBuilder.h
    src/app/text_editor/block_editor/BlockEditorMovePlanner.cpp
    src/app/text_editor/block_editor/BlockEditorMovePlanner.h
    src/app/text_editor/block_editor/BlockEditorMoveController.cpp
    src/app/text_editor/block_editor/BlockEditorMoveController.h
    src/app/text_editor/block_editor/BlockEditorMovePreviewController.cpp
    src/app/text_editor/block_editor/BlockEditorMovePreviewController.h
    src/app/text_editor/block_editor/BlockEditorMoveSourceRewriter.cpp
    src/app/text_editor/block_editor/BlockEditorMoveSourceRewriter.h
    src/app/text_editor/TextEditorContextHelpController.cpp
    src/app/text_editor/TextEditorContextHelpController.h
    src/app/text_editor/TextEditorCursorController.cpp
    src/app/text_editor/TextEditorCursorController.h
    src/app/text_editor/TextEditorAppearanceController.cpp
    src/app/text_editor/TextEditorAppearanceController.h
    src/app/text_editor/TextEditorSourceRewriteController.cpp
    src/app/text_editor/TextEditorSourceRewriteController.h
    src/app/text_editor/TextEditorStatusController.cpp
    src/app/text_editor/TextEditorStatusController.h
    src/app/text_editor/TextEditorModeController.cpp
    src/app/text_editor/TextEditorModeController.h
    src/app/text_editor/TextEditorSurfaceStyler.cpp
    src/app/text_editor/TextEditorSurfaceStyler.h
    src/app/text_editor/raw_editor/RawEditorFindController.cpp
    src/app/text_editor/raw_editor/RawEditorFindController.h
    src/app/text_editor/raw_editor/RawEditorCompletionController.cpp
    src/app/text_editor/raw_editor/RawEditorCompletionController.h
    src/app/text_editor/raw_editor/RawEditorTextEdit.cpp
    src/app/text_editor/raw_editor/RawEditorTextEdit.h
    src/app/text_editor/map_editor/MapEditorBackgroundLayers.cpp
    src/app/text_editor/map_editor/MapEditorInputPolicy.cpp
    src/app/text_editor/map_editor/MapEditorInputPolicy.h
    src/app/text_editor/map_editor/MapEditorSceneInternals.h
    src/app/text_editor/map_editor/MapEditorSceneItems.cpp
    src/app/text_editor/map_editor/MapEditorSceneRenderer.cpp
    src/app/text_editor/map_editor/MapEditorSceneSupport.h
    src/app/text_editor/map_editor/MapEditorTab.cpp
    src/app/text_editor/map_editor/MapEditorTabWorkspace.cpp
    src/app/text_editor/map_editor/MapEditorTab.h
    src/app/text_editor/TextEditorTab.cpp
    src/app/text_editor/TextEditorTab.h
    src/core/CommandCatalogService.cpp
    src/core/CommandCatalogService.h
    src/core/TherionCommandSyntax.cpp
    src/core/TherionCommandSyntax.h
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

target_link_libraries(TherionStudio PRIVATE Qt6::Widgets Qt6::Svg Qt6::Concurrent)

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
    src/app/text_editor/map_editor/MapEditorSceneItems.cpp
    src/app/text_editor/map_editor/MapEditorSceneInternals.h
    src/app/text_editor/map_editor/MapEditorSceneRenderer.cpp
    src/app/text_editor/map_editor/MapEditorSceneSupport.h
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
    src/app/text_editor/map_editor/MapEditorInputPolicy.cpp
    src/app/text_editor/map_editor/MapEditorInputPolicy.h)

target_link_libraries(MapEditorInputPolicyTest PRIVATE Qt6::Widgets)

qt_add_executable(SessionStoreTest
    tests/SessionStoreTest.cpp
    src/core/SessionStore.cpp
    src/core/SessionStore.h)

target_link_libraries(SessionStoreTest PRIVATE Qt6::Widgets)

qt_add_executable(TextEditorCaretInteractionTest
    tests/TextEditorCaretInteractionTest.cpp
    src/app/text_editor/ContextHelpController.cpp
    src/app/text_editor/ContextHelpController.h
    src/app/text_editor/TextEditorOptionValidation.cpp
    src/app/text_editor/TextEditorOptionValidation.h
    src/app/text_editor/block_editor/BlockEditorOptionTableDelegate.cpp
    src/app/text_editor/block_editor/BlockEditorOptionTableDelegate.h
src/app/text_editor/block_editor/BlockEditorOptionArgsController.cpp
    src/app/text_editor/block_editor/BlockEditorOptionArgsController.h
    src/app/text_editor/block_editor/BlockEditorDetailsHelpController.cpp
    src/app/text_editor/block_editor/BlockEditorDetailsHelpController.h
    src/app/text_editor/block_editor/BlockEditorLineBuildService.cpp
    src/app/text_editor/block_editor/BlockEditorLineBuildService.h
    src/app/text_editor/block_editor/BlockEditorApplyStateController.cpp
    src/app/text_editor/block_editor/BlockEditorApplyStateController.h
    src/app/text_editor/block_editor/BlockEditorApplyExecutor.cpp
    src/app/text_editor/block_editor/BlockEditorApplyExecutor.h
    src/app/text_editor/block_editor/BlockEditorSelectionDetailsController.cpp
    src/app/text_editor/block_editor/BlockEditorSelectionDetailsController.h
    src/app/text_editor/block_editor/BlockEditorDetailsPaneController.cpp
    src/app/text_editor/block_editor/BlockEditorDetailsPaneController.h
    src/app/text_editor/block_editor/BlockEditorToolboxController.cpp
    src/app/text_editor/block_editor/BlockEditorToolboxController.h
    src/app/text_editor/block_editor/BlockEditorToolboxList.cpp
    src/app/text_editor/block_editor/BlockEditorToolboxList.h
    src/app/text_editor/block_editor/BlockEditorTokenTagEditor.cpp
    src/app/text_editor/block_editor/BlockEditorTokenTagEditor.h
src/app/text_editor/block_editor/BlockEditorToolboxDetailsController.cpp
    src/app/text_editor/block_editor/BlockEditorToolboxDetailsController.h
    src/app/text_editor/block_editor/BlockEditorCanvasSelectionController.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasSelectionController.h
    src/app/text_editor/block_editor/BlockEditorCanvasView.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasView.h
    src/app/text_editor/block_editor/BlockEditorDragMime.h
    src/app/text_editor/block_editor/BlockEditorCanvasItem.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasItem.h
    src/app/text_editor/block_editor/BlockEditorCanvasRebuildController.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasRebuildController.h
    src/app/text_editor/block_editor/BlockEditorCommandOptionParser.cpp
    src/app/text_editor/block_editor/BlockEditorCommandOptionParser.h
    src/app/text_editor/block_editor/BlockEditorConfigureController.cpp
    src/app/text_editor/block_editor/BlockEditorConfigureController.h
    src/app/text_editor/block_editor/BlockEditorCommandOptionsDialog.cpp
    src/app/text_editor/block_editor/BlockEditorCommandOptionsDialog.h
    src/app/text_editor/block_editor/BlockEditorDataBlockDialog.cpp
    src/app/text_editor/block_editor/BlockEditorDataBlockDialog.h
    src/app/text_editor/block_editor/BlockEditorDeleteExecutor.cpp
    src/app/text_editor/block_editor/BlockEditorDeleteExecutor.h
    src/app/text_editor/block_editor/BlockEditorSingleValueCommandDialog.cpp
    src/app/text_editor/block_editor/BlockEditorSingleValueCommandDialog.h
    src/app/text_editor/block_editor/BlockEditorSourceText.cpp
    src/app/text_editor/block_editor/BlockEditorSourceText.h
    src/app/text_editor/block_editor/BlockEditorSourceController.cpp
    src/app/text_editor/block_editor/BlockEditorSourceController.h
    src/app/text_editor/block_editor/BlockEditorCanvasBoundaryController.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasBoundaryController.h
    src/app/text_editor/block_editor/BlockEditorDirectiveRules.cpp
    src/app/text_editor/block_editor/BlockEditorDirectiveRules.h
    src/app/text_editor/block_editor/BlockEditorDocumentOutlineBuilder.cpp
    src/app/text_editor/block_editor/BlockEditorDocumentOutlineBuilder.h
    src/app/text_editor/block_editor/BlockEditorDropTargetResolver.cpp
    src/app/text_editor/block_editor/BlockEditorDropTargetResolver.h
    src/app/text_editor/block_editor/BlockEditorInsertionPlanner.cpp
    src/app/text_editor/block_editor/BlockEditorInsertionPlanner.h
    src/app/text_editor/block_editor/BlockEditorInsertionController.cpp
    src/app/text_editor/block_editor/BlockEditorInsertionController.h
    src/app/text_editor/block_editor/BlockEditorInsertionTemplateBuilder.cpp
    src/app/text_editor/block_editor/BlockEditorInsertionTemplateBuilder.h
    src/app/text_editor/block_editor/BlockEditorMovePlanner.cpp
    src/app/text_editor/block_editor/BlockEditorMovePlanner.h
    src/app/text_editor/block_editor/BlockEditorMoveController.cpp
    src/app/text_editor/block_editor/BlockEditorMoveController.h
    src/app/text_editor/block_editor/BlockEditorMovePreviewController.cpp
    src/app/text_editor/block_editor/BlockEditorMovePreviewController.h
    src/app/text_editor/block_editor/BlockEditorMoveSourceRewriter.cpp
    src/app/text_editor/block_editor/BlockEditorMoveSourceRewriter.h
    src/app/text_editor/TextEditorContextHelpController.cpp
    src/app/text_editor/TextEditorContextHelpController.h
    src/app/text_editor/TextEditorCursorController.cpp
    src/app/text_editor/TextEditorCursorController.h
    src/app/text_editor/TextEditorAppearanceController.cpp
    src/app/text_editor/TextEditorAppearanceController.h
    src/app/text_editor/TextEditorSourceRewriteController.cpp
    src/app/text_editor/TextEditorSourceRewriteController.h
    src/app/text_editor/TextEditorStatusController.cpp
    src/app/text_editor/TextEditorStatusController.h
    src/app/text_editor/TextEditorModeController.cpp
    src/app/text_editor/TextEditorModeController.h
    src/app/text_editor/TextEditorSurfaceStyler.cpp
    src/app/text_editor/TextEditorSurfaceStyler.h
    src/app/text_editor/raw_editor/RawEditorFindController.cpp
    src/app/text_editor/raw_editor/RawEditorFindController.h
    src/app/text_editor/raw_editor/RawEditorCompletionController.cpp
    src/app/text_editor/raw_editor/RawEditorCompletionController.h
    src/app/text_editor/raw_editor/RawEditorTextEdit.cpp
    src/app/text_editor/raw_editor/RawEditorTextEdit.h
    src/app/text_editor/TextEditorTab.cpp
    src/app/text_editor/TextEditorTab.h
    src/core/CommandCatalogService.cpp
    src/core/CommandCatalogService.h
    src/core/TherionCommandSyntax.cpp
    src/core/TherionCommandSyntax.h
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
    src/app/text_editor/ContextHelpController.cpp
    src/app/text_editor/ContextHelpController.h
    src/app/text_editor/TextEditorOptionValidation.cpp
    src/app/text_editor/TextEditorOptionValidation.h
    src/app/text_editor/block_editor/BlockEditorOptionTableDelegate.cpp
    src/app/text_editor/block_editor/BlockEditorOptionTableDelegate.h
src/app/text_editor/block_editor/BlockEditorOptionArgsController.cpp
    src/app/text_editor/block_editor/BlockEditorOptionArgsController.h
    src/app/text_editor/block_editor/BlockEditorDetailsHelpController.cpp
    src/app/text_editor/block_editor/BlockEditorDetailsHelpController.h
    src/app/text_editor/block_editor/BlockEditorLineBuildService.cpp
    src/app/text_editor/block_editor/BlockEditorLineBuildService.h
    src/app/text_editor/block_editor/BlockEditorApplyStateController.cpp
    src/app/text_editor/block_editor/BlockEditorApplyStateController.h
    src/app/text_editor/block_editor/BlockEditorApplyExecutor.cpp
    src/app/text_editor/block_editor/BlockEditorApplyExecutor.h
    src/app/text_editor/block_editor/BlockEditorSelectionDetailsController.cpp
    src/app/text_editor/block_editor/BlockEditorSelectionDetailsController.h
    src/app/text_editor/block_editor/BlockEditorDetailsPaneController.cpp
    src/app/text_editor/block_editor/BlockEditorDetailsPaneController.h
    src/app/text_editor/block_editor/BlockEditorToolboxController.cpp
    src/app/text_editor/block_editor/BlockEditorToolboxController.h
    src/app/text_editor/block_editor/BlockEditorToolboxList.cpp
    src/app/text_editor/block_editor/BlockEditorToolboxList.h
    src/app/text_editor/block_editor/BlockEditorTokenTagEditor.cpp
    src/app/text_editor/block_editor/BlockEditorTokenTagEditor.h
src/app/text_editor/block_editor/BlockEditorToolboxDetailsController.cpp
    src/app/text_editor/block_editor/BlockEditorToolboxDetailsController.h
    src/app/text_editor/block_editor/BlockEditorCanvasSelectionController.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasSelectionController.h
    src/app/text_editor/block_editor/BlockEditorCanvasView.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasView.h
    src/app/text_editor/block_editor/BlockEditorDragMime.h
    src/app/text_editor/block_editor/BlockEditorCanvasItem.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasItem.h
    src/app/text_editor/block_editor/BlockEditorCanvasRebuildController.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasRebuildController.h
    src/app/text_editor/block_editor/BlockEditorCommandOptionParser.cpp
    src/app/text_editor/block_editor/BlockEditorCommandOptionParser.h
    src/app/text_editor/block_editor/BlockEditorConfigureController.cpp
    src/app/text_editor/block_editor/BlockEditorConfigureController.h
    src/app/text_editor/block_editor/BlockEditorCommandOptionsDialog.cpp
    src/app/text_editor/block_editor/BlockEditorCommandOptionsDialog.h
    src/app/text_editor/block_editor/BlockEditorDataBlockDialog.cpp
    src/app/text_editor/block_editor/BlockEditorDataBlockDialog.h
    src/app/text_editor/block_editor/BlockEditorDeleteExecutor.cpp
    src/app/text_editor/block_editor/BlockEditorDeleteExecutor.h
    src/app/text_editor/block_editor/BlockEditorSingleValueCommandDialog.cpp
    src/app/text_editor/block_editor/BlockEditorSingleValueCommandDialog.h
    src/app/text_editor/block_editor/BlockEditorSourceText.cpp
    src/app/text_editor/block_editor/BlockEditorSourceText.h
    src/app/text_editor/block_editor/BlockEditorSourceController.cpp
    src/app/text_editor/block_editor/BlockEditorSourceController.h
    src/app/text_editor/block_editor/BlockEditorCanvasBoundaryController.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasBoundaryController.h
    src/app/text_editor/block_editor/BlockEditorDirectiveRules.cpp
    src/app/text_editor/block_editor/BlockEditorDirectiveRules.h
    src/app/text_editor/block_editor/BlockEditorDocumentOutlineBuilder.cpp
    src/app/text_editor/block_editor/BlockEditorDocumentOutlineBuilder.h
    src/app/text_editor/block_editor/BlockEditorDropTargetResolver.cpp
    src/app/text_editor/block_editor/BlockEditorDropTargetResolver.h
    src/app/text_editor/block_editor/BlockEditorInsertionPlanner.cpp
    src/app/text_editor/block_editor/BlockEditorInsertionPlanner.h
    src/app/text_editor/block_editor/BlockEditorInsertionController.cpp
    src/app/text_editor/block_editor/BlockEditorInsertionController.h
    src/app/text_editor/block_editor/BlockEditorInsertionTemplateBuilder.cpp
    src/app/text_editor/block_editor/BlockEditorInsertionTemplateBuilder.h
    src/app/text_editor/block_editor/BlockEditorMovePlanner.cpp
    src/app/text_editor/block_editor/BlockEditorMovePlanner.h
    src/app/text_editor/block_editor/BlockEditorMoveController.cpp
    src/app/text_editor/block_editor/BlockEditorMoveController.h
    src/app/text_editor/block_editor/BlockEditorMovePreviewController.cpp
    src/app/text_editor/block_editor/BlockEditorMovePreviewController.h
    src/app/text_editor/block_editor/BlockEditorMoveSourceRewriter.cpp
    src/app/text_editor/block_editor/BlockEditorMoveSourceRewriter.h
    src/app/text_editor/TextEditorContextHelpController.cpp
    src/app/text_editor/TextEditorContextHelpController.h
    src/app/text_editor/TextEditorCursorController.cpp
    src/app/text_editor/TextEditorCursorController.h
    src/app/text_editor/TextEditorAppearanceController.cpp
    src/app/text_editor/TextEditorAppearanceController.h
    src/app/text_editor/TextEditorSourceRewriteController.cpp
    src/app/text_editor/TextEditorSourceRewriteController.h
    src/app/text_editor/TextEditorStatusController.cpp
    src/app/text_editor/TextEditorStatusController.h
    src/app/text_editor/TextEditorModeController.cpp
    src/app/text_editor/TextEditorModeController.h
    src/app/text_editor/TextEditorSurfaceStyler.cpp
    src/app/text_editor/TextEditorSurfaceStyler.h
    src/app/text_editor/raw_editor/RawEditorFindController.cpp
    src/app/text_editor/raw_editor/RawEditorFindController.h
    src/app/text_editor/raw_editor/RawEditorCompletionController.cpp
    src/app/text_editor/raw_editor/RawEditorCompletionController.h
    src/app/text_editor/raw_editor/RawEditorTextEdit.cpp
    src/app/text_editor/raw_editor/RawEditorTextEdit.h
    src/app/text_editor/TextEditorTab.cpp
    src/app/text_editor/TextEditorTab.h
    src/core/CommandCatalogService.cpp
    src/core/CommandCatalogService.h
    src/core/TherionCommandSyntax.cpp
    src/core/TherionCommandSyntax.h
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
    src/app/text_editor/ContextHelpController.cpp
    src/app/text_editor/ContextHelpController.h
    src/app/text_editor/TextEditorOptionValidation.cpp
    src/app/text_editor/TextEditorOptionValidation.h
    src/app/text_editor/block_editor/BlockEditorOptionTableDelegate.cpp
    src/app/text_editor/block_editor/BlockEditorOptionTableDelegate.h
src/app/text_editor/block_editor/BlockEditorOptionArgsController.cpp
    src/app/text_editor/block_editor/BlockEditorOptionArgsController.h
    src/app/text_editor/block_editor/BlockEditorDetailsHelpController.cpp
    src/app/text_editor/block_editor/BlockEditorDetailsHelpController.h
    src/app/text_editor/block_editor/BlockEditorLineBuildService.cpp
    src/app/text_editor/block_editor/BlockEditorLineBuildService.h
    src/app/text_editor/block_editor/BlockEditorApplyStateController.cpp
    src/app/text_editor/block_editor/BlockEditorApplyStateController.h
    src/app/text_editor/block_editor/BlockEditorApplyExecutor.cpp
    src/app/text_editor/block_editor/BlockEditorApplyExecutor.h
    src/app/text_editor/block_editor/BlockEditorSelectionDetailsController.cpp
    src/app/text_editor/block_editor/BlockEditorSelectionDetailsController.h
    src/app/text_editor/block_editor/BlockEditorDetailsPaneController.cpp
    src/app/text_editor/block_editor/BlockEditorDetailsPaneController.h
    src/app/text_editor/block_editor/BlockEditorToolboxController.cpp
    src/app/text_editor/block_editor/BlockEditorToolboxController.h
    src/app/text_editor/block_editor/BlockEditorToolboxList.cpp
    src/app/text_editor/block_editor/BlockEditorToolboxList.h
    src/app/text_editor/block_editor/BlockEditorTokenTagEditor.cpp
    src/app/text_editor/block_editor/BlockEditorTokenTagEditor.h
src/app/text_editor/block_editor/BlockEditorToolboxDetailsController.cpp
    src/app/text_editor/block_editor/BlockEditorToolboxDetailsController.h
    src/app/text_editor/block_editor/BlockEditorCanvasSelectionController.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasSelectionController.h
    src/app/text_editor/block_editor/BlockEditorCanvasView.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasView.h
    src/app/text_editor/block_editor/BlockEditorDragMime.h
    src/app/text_editor/block_editor/BlockEditorCanvasItem.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasItem.h
    src/app/text_editor/block_editor/BlockEditorCanvasRebuildController.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasRebuildController.h
    src/app/text_editor/block_editor/BlockEditorCommandOptionParser.cpp
    src/app/text_editor/block_editor/BlockEditorCommandOptionParser.h
    src/app/text_editor/block_editor/BlockEditorConfigureController.cpp
    src/app/text_editor/block_editor/BlockEditorConfigureController.h
    src/app/text_editor/block_editor/BlockEditorCommandOptionsDialog.cpp
    src/app/text_editor/block_editor/BlockEditorCommandOptionsDialog.h
    src/app/text_editor/block_editor/BlockEditorDataBlockDialog.cpp
    src/app/text_editor/block_editor/BlockEditorDataBlockDialog.h
    src/app/text_editor/block_editor/BlockEditorDeleteExecutor.cpp
    src/app/text_editor/block_editor/BlockEditorDeleteExecutor.h
    src/app/text_editor/block_editor/BlockEditorSingleValueCommandDialog.cpp
    src/app/text_editor/block_editor/BlockEditorSingleValueCommandDialog.h
    src/app/text_editor/block_editor/BlockEditorSourceText.cpp
    src/app/text_editor/block_editor/BlockEditorSourceText.h
    src/app/text_editor/block_editor/BlockEditorSourceController.cpp
    src/app/text_editor/block_editor/BlockEditorSourceController.h
    src/app/text_editor/block_editor/BlockEditorCanvasBoundaryController.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasBoundaryController.h
    src/app/text_editor/block_editor/BlockEditorDirectiveRules.cpp
    src/app/text_editor/block_editor/BlockEditorDirectiveRules.h
    src/app/text_editor/block_editor/BlockEditorDocumentOutlineBuilder.cpp
    src/app/text_editor/block_editor/BlockEditorDocumentOutlineBuilder.h
    src/app/text_editor/block_editor/BlockEditorDropTargetResolver.cpp
    src/app/text_editor/block_editor/BlockEditorDropTargetResolver.h
    src/app/text_editor/block_editor/BlockEditorInsertionPlanner.cpp
    src/app/text_editor/block_editor/BlockEditorInsertionPlanner.h
    src/app/text_editor/block_editor/BlockEditorInsertionController.cpp
    src/app/text_editor/block_editor/BlockEditorInsertionController.h
    src/app/text_editor/block_editor/BlockEditorInsertionTemplateBuilder.cpp
    src/app/text_editor/block_editor/BlockEditorInsertionTemplateBuilder.h
    src/app/text_editor/block_editor/BlockEditorMovePlanner.cpp
    src/app/text_editor/block_editor/BlockEditorMovePlanner.h
    src/app/text_editor/block_editor/BlockEditorMoveController.cpp
    src/app/text_editor/block_editor/BlockEditorMoveController.h
    src/app/text_editor/block_editor/BlockEditorMovePreviewController.cpp
    src/app/text_editor/block_editor/BlockEditorMovePreviewController.h
    src/app/text_editor/block_editor/BlockEditorMoveSourceRewriter.cpp
    src/app/text_editor/block_editor/BlockEditorMoveSourceRewriter.h
    src/app/text_editor/TextEditorContextHelpController.cpp
    src/app/text_editor/TextEditorContextHelpController.h
    src/app/text_editor/TextEditorCursorController.cpp
    src/app/text_editor/TextEditorCursorController.h
    src/app/text_editor/TextEditorAppearanceController.cpp
    src/app/text_editor/TextEditorAppearanceController.h
    src/app/text_editor/TextEditorSourceRewriteController.cpp
    src/app/text_editor/TextEditorSourceRewriteController.h
    src/app/text_editor/TextEditorStatusController.cpp
    src/app/text_editor/TextEditorStatusController.h
    src/app/text_editor/TextEditorModeController.cpp
    src/app/text_editor/TextEditorModeController.h
    src/app/text_editor/TextEditorSurfaceStyler.cpp
    src/app/text_editor/TextEditorSurfaceStyler.h
    src/app/text_editor/raw_editor/RawEditorFindController.cpp
    src/app/text_editor/raw_editor/RawEditorFindController.h
    src/app/text_editor/raw_editor/RawEditorCompletionController.cpp
    src/app/text_editor/raw_editor/RawEditorCompletionController.h
    src/app/text_editor/raw_editor/RawEditorTextEdit.cpp
    src/app/text_editor/raw_editor/RawEditorTextEdit.h
    src/app/text_editor/map_editor/MapEditorBackgroundLayers.cpp
    src/app/text_editor/map_editor/MapEditorInputPolicy.cpp
    src/app/text_editor/map_editor/MapEditorInputPolicy.h
    src/app/text_editor/map_editor/MapEditorSceneInternals.h
    src/app/text_editor/map_editor/MapEditorSceneItems.cpp
    src/app/text_editor/map_editor/MapEditorSceneRenderer.cpp
    src/app/text_editor/map_editor/MapEditorSceneSupport.h
    src/app/text_editor/map_editor/MapEditorTab.cpp
    src/app/text_editor/map_editor/MapEditorTabWorkspace.cpp
    src/app/text_editor/map_editor/MapEditorTab.h
    src/app/text_editor/TextEditorTab.cpp
    src/app/text_editor/TextEditorTab.h
    src/core/CommandCatalogService.cpp
    src/core/CommandCatalogService.h
    src/core/TherionCommandSyntax.cpp
    src/core/TherionCommandSyntax.h
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
    src/app/text_editor/ContextHelpController.cpp
    src/app/text_editor/ContextHelpController.h
    src/app/text_editor/TextEditorOptionValidation.cpp
    src/app/text_editor/TextEditorOptionValidation.h
    src/app/text_editor/block_editor/BlockEditorOptionTableDelegate.cpp
    src/app/text_editor/block_editor/BlockEditorOptionTableDelegate.h
src/app/text_editor/block_editor/BlockEditorOptionArgsController.cpp
    src/app/text_editor/block_editor/BlockEditorOptionArgsController.h
    src/app/text_editor/block_editor/BlockEditorDetailsHelpController.cpp
    src/app/text_editor/block_editor/BlockEditorDetailsHelpController.h
    src/app/text_editor/block_editor/BlockEditorLineBuildService.cpp
    src/app/text_editor/block_editor/BlockEditorLineBuildService.h
    src/app/text_editor/block_editor/BlockEditorApplyStateController.cpp
    src/app/text_editor/block_editor/BlockEditorApplyStateController.h
    src/app/text_editor/block_editor/BlockEditorApplyExecutor.cpp
    src/app/text_editor/block_editor/BlockEditorApplyExecutor.h
    src/app/text_editor/block_editor/BlockEditorSelectionDetailsController.cpp
    src/app/text_editor/block_editor/BlockEditorSelectionDetailsController.h
    src/app/text_editor/block_editor/BlockEditorDetailsPaneController.cpp
    src/app/text_editor/block_editor/BlockEditorDetailsPaneController.h
    src/app/text_editor/block_editor/BlockEditorToolboxController.cpp
    src/app/text_editor/block_editor/BlockEditorToolboxController.h
    src/app/text_editor/block_editor/BlockEditorToolboxList.cpp
    src/app/text_editor/block_editor/BlockEditorToolboxList.h
    src/app/text_editor/block_editor/BlockEditorTokenTagEditor.cpp
    src/app/text_editor/block_editor/BlockEditorTokenTagEditor.h
src/app/text_editor/block_editor/BlockEditorToolboxDetailsController.cpp
    src/app/text_editor/block_editor/BlockEditorToolboxDetailsController.h
    src/app/text_editor/block_editor/BlockEditorCanvasSelectionController.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasSelectionController.h
    src/app/text_editor/block_editor/BlockEditorCanvasView.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasView.h
    src/app/text_editor/block_editor/BlockEditorDragMime.h
    src/app/text_editor/block_editor/BlockEditorCanvasItem.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasItem.h
    src/app/text_editor/block_editor/BlockEditorCanvasRebuildController.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasRebuildController.h
    src/app/text_editor/block_editor/BlockEditorCommandOptionParser.cpp
    src/app/text_editor/block_editor/BlockEditorCommandOptionParser.h
    src/app/text_editor/block_editor/BlockEditorConfigureController.cpp
    src/app/text_editor/block_editor/BlockEditorConfigureController.h
    src/app/text_editor/block_editor/BlockEditorCommandOptionsDialog.cpp
    src/app/text_editor/block_editor/BlockEditorCommandOptionsDialog.h
    src/app/text_editor/block_editor/BlockEditorDataBlockDialog.cpp
    src/app/text_editor/block_editor/BlockEditorDataBlockDialog.h
    src/app/text_editor/block_editor/BlockEditorDeleteExecutor.cpp
    src/app/text_editor/block_editor/BlockEditorDeleteExecutor.h
    src/app/text_editor/block_editor/BlockEditorSingleValueCommandDialog.cpp
    src/app/text_editor/block_editor/BlockEditorSingleValueCommandDialog.h
    src/app/text_editor/block_editor/BlockEditorSourceText.cpp
    src/app/text_editor/block_editor/BlockEditorSourceText.h
    src/app/text_editor/block_editor/BlockEditorSourceController.cpp
    src/app/text_editor/block_editor/BlockEditorSourceController.h
    src/app/text_editor/block_editor/BlockEditorCanvasBoundaryController.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasBoundaryController.h
    src/app/text_editor/block_editor/BlockEditorDirectiveRules.cpp
    src/app/text_editor/block_editor/BlockEditorDirectiveRules.h
    src/app/text_editor/block_editor/BlockEditorDocumentOutlineBuilder.cpp
    src/app/text_editor/block_editor/BlockEditorDocumentOutlineBuilder.h
    src/app/text_editor/block_editor/BlockEditorDropTargetResolver.cpp
    src/app/text_editor/block_editor/BlockEditorDropTargetResolver.h
    src/app/text_editor/block_editor/BlockEditorInsertionPlanner.cpp
    src/app/text_editor/block_editor/BlockEditorInsertionPlanner.h
    src/app/text_editor/block_editor/BlockEditorInsertionController.cpp
    src/app/text_editor/block_editor/BlockEditorInsertionController.h
    src/app/text_editor/block_editor/BlockEditorInsertionTemplateBuilder.cpp
    src/app/text_editor/block_editor/BlockEditorInsertionTemplateBuilder.h
    src/app/text_editor/block_editor/BlockEditorMovePlanner.cpp
    src/app/text_editor/block_editor/BlockEditorMovePlanner.h
    src/app/text_editor/block_editor/BlockEditorMoveController.cpp
    src/app/text_editor/block_editor/BlockEditorMoveController.h
    src/app/text_editor/block_editor/BlockEditorMovePreviewController.cpp
    src/app/text_editor/block_editor/BlockEditorMovePreviewController.h
    src/app/text_editor/block_editor/BlockEditorMoveSourceRewriter.cpp
    src/app/text_editor/block_editor/BlockEditorMoveSourceRewriter.h
    src/app/text_editor/TextEditorContextHelpController.cpp
    src/app/text_editor/TextEditorContextHelpController.h
    src/app/text_editor/TextEditorCursorController.cpp
    src/app/text_editor/TextEditorCursorController.h
    src/app/text_editor/TextEditorAppearanceController.cpp
    src/app/text_editor/TextEditorAppearanceController.h
    src/app/text_editor/TextEditorSourceRewriteController.cpp
    src/app/text_editor/TextEditorSourceRewriteController.h
    src/app/text_editor/TextEditorStatusController.cpp
    src/app/text_editor/TextEditorStatusController.h
    src/app/text_editor/TextEditorModeController.cpp
    src/app/text_editor/TextEditorModeController.h
    src/app/text_editor/TextEditorSurfaceStyler.cpp
    src/app/text_editor/TextEditorSurfaceStyler.h
    src/app/text_editor/raw_editor/RawEditorFindController.cpp
    src/app/text_editor/raw_editor/RawEditorFindController.h
    src/app/text_editor/raw_editor/RawEditorCompletionController.cpp
    src/app/text_editor/raw_editor/RawEditorCompletionController.h
    src/app/text_editor/raw_editor/RawEditorTextEdit.cpp
    src/app/text_editor/raw_editor/RawEditorTextEdit.h
    src/app/text_editor/map_editor/MapEditorBackgroundLayers.cpp
    src/app/text_editor/map_editor/MapEditorInputPolicy.cpp
    src/app/text_editor/map_editor/MapEditorInputPolicy.h
    src/app/text_editor/map_editor/MapEditorSceneInternals.h
    src/app/text_editor/map_editor/MapEditorSceneItems.cpp
    src/app/text_editor/map_editor/MapEditorSceneRenderer.cpp
    src/app/text_editor/map_editor/MapEditorSceneSupport.h
    src/app/text_editor/map_editor/MapEditorTab.cpp
    src/app/text_editor/map_editor/MapEditorTabWorkspace.cpp
    src/app/text_editor/map_editor/MapEditorTab.h
    src/app/text_editor/TextEditorTab.cpp
    src/app/text_editor/TextEditorTab.h
    src/core/CommandCatalogService.cpp
    src/core/CommandCatalogService.h
    src/core/TherionCommandSyntax.cpp
    src/core/TherionCommandSyntax.h
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

enable_testing()

add_test(NAME TherionDocumentEditorTest COMMAND TherionDocumentEditorTest)
add_test(NAME TherionProjectStructureIndexTest COMMAND TherionProjectStructureIndexTest)
add_test(NAME MapBackgroundPlacementTest COMMAND MapBackgroundPlacementTest)
add_test(NAME TherionBackgroundMetadataTest COMMAND TherionBackgroundMetadataTest)
add_test(NAME TherionXviParserTest COMMAND TherionXviParserTest)
add_test(NAME MapGeometryFeatureParsingTest COMMAND MapGeometryFeatureParsingTest)
add_test(NAME DocumentFileEncodingTest COMMAND DocumentFileEncodingTest)
add_test(NAME MapEditorInputPolicyTest COMMAND MapEditorInputPolicyTest)
add_test(NAME SessionStoreTest COMMAND SessionStoreTest)
add_test(NAME StructureConstraintsTest COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/check_structure_constraints.py)
add_test(NAME TextEditorCaretInteractionTest COMMAND TextEditorCaretInteractionTest)
add_test(NAME TextEditorCompletionHighlightTest COMMAND TextEditorCompletionHighlightTest)
add_test(NAME MapEditorDetachedPaneTest COMMAND MapEditorDetachedPaneTest)
add_test(NAME MapEditorDragUndoRedoSmokeTest COMMAND MapEditorDragUndoRedoSmokeTest)

set_tests_properties(
    TherionDocumentEditorTest
    TherionProjectStructureIndexTest
    MapBackgroundPlacementTest
    TherionBackgroundMetadataTest
    TherionXviParserTest
    MapGeometryFeatureParsingTest
    DocumentFileEncodingTest
    MapEditorInputPolicyTest
    SessionStoreTest
    MapEditorDetachedPaneTest
    MapEditorDragUndoRedoSmokeTest
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

set_tests_properties(
    TextEditorCaretInteractionTest
    TextEditorCompletionHighlightTest
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        DISABLED TRUE)
