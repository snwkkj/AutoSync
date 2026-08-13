cmake_minimum_required(VERSION 3.24)

project(AutoSync
    VERSION 0.1.0
    DESCRIPTION "Automatic audio and video synchronization for Linux"
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

add_library(autosync_core STATIC
    src/core/analysis.c
    src/core/core.c
    src/core/internal.c
    src/core/internal.h
    src/core/muxer.c
)

target_include_directories(autosync_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(autosync_core PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)
target_link_libraries(autosync_core PUBLIC m)

# qt_standard_project_setup enables Qt code generation globally. The core must
# remain a genuinely standalone C library, so disable it explicitly here.
set_target_properties(autosync_core PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
    LINKER_LANGUAGE C
)

qt_add_executable(autosync_gui
    src/gui/delay_spin_box.cpp
    src/gui/delay_spin_box.h
    src/gui/icons.cpp
    src/gui/icons.h
    src/gui/language_combo.cpp
    src/gui/language_combo.h
    src/gui/main.cpp
    src/gui/main_window.cpp
    src/gui/main_window.h
    src/gui/system_monitor.cpp
    src/gui/system_monitor.h
    src/gui/theme.cpp
    src/gui/theme.h
)

target_link_libraries(autosync_gui PRIVATE
    autosync_core
    Qt6::Widgets
)

target_compile_options(autosync_gui PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

set_target_properties(autosync_gui PROPERTIES
    OUTPUT_NAME "autosync"
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS autosync_gui
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

include(CTest)
if(BUILD_TESTING)
    function(autosync_add_core_tool target source)
        add_executable(${target} ${source})
        target_link_libraries(${target} PRIVATE autosync_core)
        set_target_properties(${target} PROPERTIES
            AUTOMOC OFF
            AUTOUIC OFF
            AUTORCC OFF
            LINKER_LANGUAGE C
        )
    endfunction()

    autosync_add_core_tool(autosync_core_test tests/core_test.c)
    add_test(NAME autosync_core_api COMMAND autosync_core_test)

    autosync_add_core_tool(autosync_delay_probe tests/delay_probe.c)
    autosync_add_core_tool(autosync_mux_probe tests/mux_probe.c)
endif()
