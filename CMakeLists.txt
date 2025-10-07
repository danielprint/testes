cmake_minimum_required(VERSION 3.16)
project(STLViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(USE_ASSIMP "Build with Assimp for model loading" ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets OpenGL Gui)

if(USE_ASSIMP)
    find_package(assimp QUIET)
    if(NOT assimp_FOUND)
        message(WARNING "Assimp not found - falling back to internal STL parser")
        set(USE_ASSIMP OFF)
    endif()
endif()

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/GLViewport.cpp
    src/Mesh.cpp
    src/MeshLoader.cpp
    src/STLParser.cpp
    src/Camera.cpp
    src/GridGizmo.cpp
)

set(HEADERS
    src/MainWindow.h
    src/GLViewport.h
    src/Mesh.h
    src/MeshLoader.h
    src/STLParser.h
    src/Camera.h
    src/GridGizmo.h
    src/MeshStatistics.h
)

add_executable(STLViewer
    ${SOURCES}
    ${HEADERS}
)

target_include_directories(STLViewer PRIVATE src)

target_link_libraries(STLViewer
    Qt6::Widgets
    Qt6::Gui
    Qt6::OpenGL
)

if(USE_ASSIMP AND assimp_FOUND)
    target_compile_definitions(STLViewer PRIVATE USE_ASSIMP)
    target_link_libraries(STLViewer PRIVATE assimp::assimp)
endif()

if (APPLE)
    target_link_libraries(STLViewer PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()

install(TARGETS STLViewer RUNTIME DESTINATION bin)
install(DIRECTORY samples DESTINATION share)
