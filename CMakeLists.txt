cmake_dependent_option(
    OSPRAY_MODULE_STARTER
    "Build the OSPRay Starter Example"
    OFF
    OSPRAY_MODULE_CPU
    OFF
)

if (NOT OSPRAY_MODULE_STARTER)
    return()
endif()

include(ExternalProject)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
include(cmake/glm.cmake)

add_subdirectory(imgui)
add_subdirectory(util)
add_subdirectory(module)

add_executable(osp_starter
    main.cpp
    imgui_impl_opengl3.cpp
    imgui_impl_sdl.cpp)

set_target_properties(osp_starter PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON)

target_link_libraries(osp_starter PUBLIC
    util)

target_compile_definitions(osp_starter PUBLIC
    -DNOMINMAX
    -DSDL_MAIN_HANDLED
    -DOSPRAY_CPP_RKCOMMON_TYPES
    -DOSPRAY_GLM_DEFINITIONS)

