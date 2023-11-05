## Copyright 2009 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

# this subdirectory builds the actual ospray plugin into a library
# "libospray_module_<modulename>.so".
#
# This shared library can depend on libospray, but should _not_ assume
# that it is living in the same address space as the application: When
# running in MPI mode ospray will, upon encountering the
# ospLoadModule("<moduleName>"), actually load and use this library on
# every worker node (which will typically not run the application). As
# such, the library should _not_ "commuincate" with the application
# through global variables or globally defined functions, and instead
# communicate solely through the ospNewGeometry(...), ospSet...(),
# ospCommit(...) etc functionality provided by ospray.


# build the actual shared library that defines the geometry.
#
# Note the name is important: In order for ospray to properly find and
# initialize a module referenced by a call to
# "ospLoadModule(<moduleName>) this module _has_ to
#
# a) be called libospray_module_<modulename>.so, and
# b) contain a (extern C linkage) initialization routine named
#    OSPError ospray_module_init_<moduleName>(int16_t versionMajor,
#        int16_t versionMinor, int16_t versionPatch)
#
add_library(ospray_module_example_spheres SHARED)

# include_directories_ispc(${RKCOMMON_INCLUDE_DIRS})
include_directories_ispc(
    ${PROJECT_SOURCE_DIR}/ospray/include
    ${PROJECT_SOURCE_DIR}/ospray
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${RKCOMMON_INCLUDE_DIRS}
    ${EMBREE_INCLUDE_DIRS}
)

# Required definitions for ISPC hybrid code for CPU
add_definitions_ispc(
  -DOSPRAY_BEGIN_ISPC_NAMESPACE=
  -DOSPRAY_END_ISPC_NAMESPACE=
  -DSYCL_EXTERNAL=
)

ispc_target_add_sources(ospray_module_example_spheres
  # the cpp file that contains all the plugin code - parsing
  # parameters in ospCommit(), creating and registering the object,
  # building accel structures, etc
  geometry/ExampleSpheres.cpp

  # the ispc file that contains the plugins for all vector code - ie,
  # for ray-primitive intersection and 'postIntersect' (reporting info
  # on a previously computed ray-prim intersection)
  geometry/ExampleSpheres.ispc

  # and finally, the module init code (not doing much, but must be there)
  moduleInit.cpp
)

target_link_libraries(ospray_module_example_spheres
  PUBLIC
  rkcommon::rkcommon
  ospray_module_cpu)
