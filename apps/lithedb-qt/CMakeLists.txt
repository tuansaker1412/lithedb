cmake_minimum_required(VERSION 3.21)

project(LitheDBQt VERSION 1.0.2 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

qt_standard_project_setup()

set(LITHEDB_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(LITHEDB_PACKAGING_DIR "${CMAKE_CURRENT_SOURCE_DIR}/packaging")
set(LITHEDB_LINUX_PACKAGING_DIR "${LITHEDB_PACKAGING_DIR}/linux")

set(
    LITHEDB_BRIDGE_BINARY
    "${LITHEDB_REPO_ROOT}/target/release/lithedb-bridge${CMAKE_EXECUTABLE_SUFFIX}"
    CACHE FILEPATH
    "Path to the packaged lithedb-bridge binary."
)

if(WIN32)
    set(LITHEDB_PACKAGE_PLATFORM "windows")
elseif(APPLE)
    set(LITHEDB_PACKAGE_PLATFORM "macos")
else()
    set(LITHEDB_PACKAGE_PLATFORM "linux")
endif()

set(
    LITHEDB_PACKAGE_BASENAME
    "LitheDB-${PROJECT_VERSION}-${LITHEDB_PACKAGE_PLATFORM}-${CMAKE_SYSTEM_PROCESSOR}"
    CACHE STRING
    "Base name for generated package artifacts."
)

qt_add_executable(lithedb-qt
    WIN32
    MACOSX_BUNDLE
    src/bridge_utils.cpp
    src/bridge_utils.h
    src/components/query/query_editor_tab_widget.cpp
    src/components/query/query_editor_tab_widget.h
    src/components/query/query_result_widget.cpp
    src/components/query/query_result_widget.h
    src/components/sidebar/connection_sidebar_widget.cpp
    src/components/sidebar/connection_sidebar_widget.h
    src/components/table/table_data_widget.cpp
    src/components/table/table_data_widget.h
    src/components/table/table_page_widget.cpp
    src/components/table/table_page_widget.h
    src/components/table/table_structure_widget.cpp
    src/components/table/table_structure_widget.h
    src/connection_dialog.cpp
    src/connection_dialog.h
    src/dialogs/about_dialog.cpp
    src/dialogs/about_dialog.h
    src/dialogs/preferences_dialog.cpp
    src/dialogs/preferences_dialog.h
    src/dialogs/shortcuts_dialog.cpp
    src/dialogs/shortcuts_dialog.h
    src/main.cpp
    src/mainwindow/mainwindow_chrome.cpp
    src/mainwindow/mainwindow_connections.cpp
    src/mainwindow/mainwindow_query.cpp
    src/mainwindow/mainwindow_shared.cpp
    src/mainwindow/mainwindow_shared.h
    src/mainwindow/mainwindow_table.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/models/connection_tree_roles.h
    src/models/query_tab_state.h
    src/row_editor_dialog.cpp
    src/row_editor_dialog.h
    src/resources.qrc
    src/table_model_utils.cpp
    src/table_model_utils.h
    src/theme.cpp
    src/theme.h
    src/ui_helpers.cpp
    src/ui_helpers.h
)

target_link_libraries(lithedb-qt PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
target_compile_definitions(lithedb-qt PRIVATE LITHEDB_QT_VERSION="${PROJECT_VERSION}")

set(LITHEDB_TARGET_OUTPUT_NAME "LitheDB")

set_target_properties(
    lithedb-qt
    PROPERTIES
        OUTPUT_NAME "${LITHEDB_TARGET_OUTPUT_NAME}"
        MACOSX_BUNDLE_BUNDLE_NAME "LitheDB"
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.tuansaker1412.LitheDB"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

install(TARGETS lithedb-qt
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(WIN32)
    install(PROGRAMS "${LITHEDB_BRIDGE_BINARY}" DESTINATION . RENAME "lithedb-bridge.exe")
elseif(APPLE)
    install(PROGRAMS "${LITHEDB_BRIDGE_BINARY}" DESTINATION "LitheDB.app/Contents/Helpers")
else()
    install(PROGRAMS "${LITHEDB_BRIDGE_BINARY}" DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
endif()

if(UNIX AND NOT APPLE)
    install(FILES
        "${LITHEDB_LINUX_PACKAGING_DIR}/io.github.tuansaker1412.LitheDB.desktop"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
    )
    install(FILES
        "${LITHEDB_LINUX_PACKAGING_DIR}/io.github.tuansaker1412.LitheDB.metainfo.xml"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo
    )
    install(FILES
        "${LITHEDB_REPO_ROOT}/packaging/flatpak/io.github.tuansaker1412.LitheDB.svg"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
    )
endif()

install(FILES
    "${LITHEDB_REPO_ROOT}/README.md"
    "${LITHEDB_REPO_ROOT}/LICENSE"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/lithedb
)

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt)
    if(NOT WINDEPLOYQT_EXECUTABLE)
        message(WARNING "windeployqt was not found during configure; packaging will require it to be in PATH at install time.")
        set(WINDEPLOYQT_EXECUTABLE "windeployqt")
    endif()

    install(CODE
        "set(app_path \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/LitheDB.exe\")\n"
        "if(NOT EXISTS \"\${app_path}\")\n"
        "  set(app_path \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/LitheDB.exe\")\n"
        "endif()\n"
        "execute_process(\n"
        "  COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --release \"\${app_path}\"\n"
        "  COMMAND_ERROR_IS_FATAL ANY\n"
        ")\n"
    )
endif()

set(CPACK_PACKAGE_NAME "LitheDB")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "LitheDB")
set(CPACK_PACKAGE_VENDOR "LitheDB Contributors")
set(CPACK_PACKAGE_CONTACT "LitheDB Contributors <contact@lithedb.dev>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross-platform Qt database client with table editing and SQL query tools")
set(CPACK_PACKAGE_DESCRIPTION "Cross-platform desktop database client for PostgreSQL, MySQL, and SQLite, with a Qt frontend, shared Rust core, table editing, and SQL query tools.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/tuansaker1412/lithedb")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${LITHEDB_REPO_ROOT}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${LITHEDB_REPO_ROOT}/README.md")
set(CPACK_PACKAGE_FILE_NAME "${LITHEDB_PACKAGE_BASENAME}")
set(CPACK_STRIP_FILES TRUE)

set(CPACK_MONOLITHIC_INSTALL TRUE)
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY TRUE)

if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "LitheDB")
    set(CPACK_NSIS_PACKAGE_NAME "LitheDB")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "LitheDB")
else()
    set(CPACK_GENERATOR "DEB;RPM")
    set(CPACK_DEBIAN_PACKAGE_NAME "lithedb")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "LitheDB Contributors")
    set(CPACK_DEBIAN_PACKAGE_SECTION "database")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Cross-platform desktop database client for PostgreSQL, MySQL, and SQLite, with a Qt frontend, shared Rust core, table editing, and SQL query tools.")
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "xdg-desktop-portal")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_RPM_PACKAGE_NAME "lithedb")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Databases")
    set(CPACK_RPM_PACKAGE_SUMMARY "Cross-platform Qt database client with table editing and SQL query tools")
    set(CPACK_RPM_PACKAGE_AUTOREQ ON)
endif()

include(CPack)
