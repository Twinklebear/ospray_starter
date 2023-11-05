// Copyright 2009 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "ExampleSpheres.h"

#include <common/Data.h>

#ifndef OSPRAY_TARGET_SYCL
// 'export'ed functions from the ispc file:
#include "geometry/ExampleSpheres_ispc.h"
#else
namespace ispc {
void Spheres_bounds(const RTCBoundsFunctionArguments *uniform args);
void *Spheres_sampleArea_addr();
void *Spheres_getAreas_addr();
} // namespace ispc
#endif

namespace ospray {
namespace example {

ExampleSpheres::ExampleSpheres(api::ISPCDevice &device)
    : AddStructShared(device.getIspcrtContext(), device, FFG_SPHERE)
{
#ifndef OSPRAY_TARGET_SYCL
  getSh()->super.postIntersect =
      reinterpret_cast<ispc::Geometry_postIntersectFct>(
          ispc::ExampleSpheres_postIntersect_addr());
  getSh()->super.intersect = ispc::ExampleSpheres_intersect_addr();
#endif
  // getSh()->super.getAreas = reinterpret_cast<ispc::Geometry_GetAreasFct>(
      // ispc::ExampleSpheres_getAreas_addr());
  // getSh()->super.sampleArea = reinterpret_cast<ispc::Geometry_SampleAreaFct>(
      // ispc::ExampleSpheres_sampleArea_addr());
}

void ExampleSpheres::commit()
{
  radius = getParam<float>("radius", 0.01f);
  vertexData = getParamDataT<vec3f>("sphere.position", true);
  radiusData = getParamDataT<float>("sphere.radius");
  texcoordData = getParamDataT<vec2f>("sphere.texcoord");

  RTCDevice embreeDevice = getISPCDevice().getEmbreeDevice();
  if (!embreeDevice)
    throw std::runtime_error("invalid Embree device");


  // If the application's data is already interleaved and they just passed us
  // separate views into the interleaved data to make the sphere.position and
  // sphere.radius arrays we can detect this and use the interleaved array
  // directly to avoid copying it. If not, we need to make a new interleaved
  // data array for Embree
  if (radiusData && vertexData
      && reinterpret_cast<uint8_t *>(vertexData->data()) + sizeof(vec3f)
          == reinterpret_cast<uint8_t *>(radiusData->data())
      && vertexData->stride() == radiusData->stride()) {
    auto interleaved = new Data(getISPCDevice(),
        vertexData->data(),
        OSP_VEC4F,
        vertexData->numItems,
        vec3l(sizeof(vec4f), 0, 0));
    sphereData = &interleaved->as<vec4f, 1>();
    interleaved->refDec();
  } else {
    // To maintain OSPRay 2.x compatibility we need to create the interleaved
    // position/radius array that Embree expects from the separate
    // position/radius (or global radius) that the OSPRay geometry takes
    auto interleaved =
        new Data(getISPCDevice(), OSP_VEC4F, vertexData->numItems);
    sphereData = &interleaved->as<vec4f, 1>();
    interleaved->refDec();
    // For now default to always create the interleaved buffer since we
    // don't expose the interleaved data yet
    for (size_t i = 0; i < vertexData->size(); ++i) {
      float ptRadius = radius;
      if (radiusData) {
        ptRadius = (*radiusData)[i];
      }
      (*sphereData)[i] = vec4f((*vertexData)[i], ptRadius);
    }
  }

  // The built in Sphere class uses Embree's sphere geometry,
  // but new geometry will use the user geometry method
#if 0
  createEmbreeGeometry(RTC_GEOMETRY_TYPE_SPHERE_POINT);
  rtcSetSharedGeometryBuffer(embreeGeometry,
                             RTC_BUFFER_TYPE_VERTEX,
                             0,
                             RTC_FORMAT_FLOAT4,
                             sphereData->data(),
                             0,
                             sizeof(vec4f),
                             sphereData->size());
  rtcCommitGeometry(embreeGeometry);
#else
  embreeGeometry = rtcNewGeometry(embreeDevice, RTC_GEOMETRY_TYPE_USER);
  // Test the spheres as the old user geometry
  createEmbreeUserGeometry((RTCBoundsFunction)&ispc::ExampleSpheres_bounds);
#endif


  getSh()->sphere = (vec4f*)sphereData->data();
  if (texcoordData)
    getSh()->texcoord = (vec2f*)texcoordData->data();
  getSh()->super.numPrimitives = numPrimitives();
  postCreationInfo(numPrimitives());
}

size_t ExampleSpheres::numPrimitives() const
{
  return sphereData ? sphereData->size() : 0;
}

} // namespace example
} // namespace ospray
