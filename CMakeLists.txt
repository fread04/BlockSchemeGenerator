cmake_minimum_required(VERSION 3.16)

project(BlockSchemeGenerator VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

# Пути к исходникам и заголовкам
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Добавляем в проект
set(PROJECT_SOURCES
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/mainwindow.cpp
    ${SRC_DIR}/canvas.cpp

    ${INCLUDE_DIR}/mainwindow.h
    ${INCLUDE_DIR}/canvas.h

    ${CMAKE_CURRENT_SOURCE_DIR}/mainwindow.ui
)

# Создание исполняемого файла
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(BlockSchemeGenerator
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(BlockSchemeGenerator SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(BlockSchemeGenerator
            ${PROJECT_SOURCES}
        )
    endif()
endif()

# Указываем, где искать заголовочные файлы
target_include_directories(BlockSchemeGenerator PRIVATE ${INCLUDE_DIR})

# Линкуем Qt Widgets
target_link_libraries(BlockSchemeGenerator PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

# Свойства для macOS и Windows
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.BlockSchemeGenerator)
endif()

set_target_properties(BlockSchemeGenerator PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Установка
include(GNUInstallDirs)
install(TARGETS BlockSchemeGenerator
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Финализация для Qt6
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(BlockSchemeGenerator)
endif()
