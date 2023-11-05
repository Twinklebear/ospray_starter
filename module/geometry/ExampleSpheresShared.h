// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "geometry/GeometryShared.h"

#ifdef __cplusplus
namespace ispc {
#endif // __cplusplus

  // TODO delete
// input data for a single patch
struct Patch
{
  vec3f v00, v01, v10, v11;
};

struct ExampleSpheres
{
  // inherit from "Geometry" class: since ISPC doesn't support
  // inheritance we simply put the "parent" class as the first
  // member; this way any typecast to the parent class will get the
  // right members (including 'virtual' function pointers, etc)
  Geometry super;

  vec4f* sphere;
  vec2f* texcoord;
  float global_radius;

  // TODO delete
  size_t numPatches;
  Patch *patchArray;

#ifdef __cplusplus
  ExampleSpheres() : numPatches(0), patchArray(nullptr) {}
};
} // namespace ispc
#else
};
#endif // __cplusplus
