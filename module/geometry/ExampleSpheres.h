// Copyright 2009 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "rkcommon/math/box.h"
#include "rkcommon/math/vec.h"

#include "geometry/Geometry.h"
// ispc shared: header containing structures shared with ispc
#include "ExampleSpheresShared.h"

namespace ospray {
namespace example {
using namespace rkcommon;

struct ExampleSpheres
  : public AddStructShared<Geometry, ispc::ExampleSpheres>
{
  ExampleSpheres(api::ISPCDevice &);
  virtual ~ExampleSpheres() override = default;

  virtual void commit() override;

  virtual size_t numPrimitives() const override;

 protected:
  float radius{.01f}; // default radius, if no per-sphere radius
  Ref<const DataT<vec3f>> vertexData;
  Ref<const DataT<float>> radiusData;
  Ref<const DataT<vec2f>> texcoordData;
  Ref<DataT<vec4f>> sphereData;
};

}}
