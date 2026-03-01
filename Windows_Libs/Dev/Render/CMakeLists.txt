cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(4JLibs.Windows.Render LANGUAGES C CXX)

add_library(${PROJECT_NAME} STATIC
    4J_Render.cpp
    RendererCBuff.cpp
    RendererCore.cpp
    RendererMatrix.cpp
    RendererState.cpp
    RendererTexture.cpp
    RendererVertex.cpp
    stdafx.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_precompile_headers(${PROJECT_NAME} PRIVATE
    stdafx.h
)

add_subdirectory(libpng)

target_link_libraries(${PROJECT_NAME} PRIVATE
    4JLibs.Windows.Render.libpng
)

set_target_properties(${PROJECT_NAME} PROPERTIES
	PREFIX ""
	OUTPUT_NAME "4J_Render"
)