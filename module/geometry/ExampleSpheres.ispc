// Copyright 2009 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// ospray
#include "common/Data.ih"
#include "common/DifferentialGeometry.ih"
#include "common/Intersect.ih"
#include "common/ISPCMessages.h"
#include "common/FilterIntersect.ih"
#include "common/Ray.ih"
#include "geometry/Geometry.ih"
#include "math/sampling.ih"
#include "rkcommon/math/LinearSpace.ih"
#include "rkcommon/math/box.ih"
#include "rkcommon/math/vec.ih"
// c++ shared
#include "ExampleSpheresShared.h"

OSPRAY_BEGIN_ISPC_NAMESPACE

#ifdef OSPRAY_TARGET_SYCL
using namespace ospray;
#endif

void ExampleSpheres_postIntersect(const Geometry *uniform geometry,
    varying DifferentialGeometry &dg,
    const varying Ray &ray,
    uniform int64 flags)
{
  const ExampleSpheres *uniform self = (const ExampleSpheres *uniform)geometry;

  dg.Ng = dg.Ns = ray.Ng;

  float radius = self->global_radius;

  // make epsilon large enough to not get lost when computing
  // |CO| = |center-ray.org| ~ radius for 2ndary rays
  dg.epsilon = radius * ulpEpsilon;
}

// to be able to have embree build a data structure over us we have
// to be able to tell it the bounding box for each primitimve. this
// is the callback embree calls for querying the (uniform) bounding
// box for a single given primitmive 'primID'
export void ExampleSpheres_bounds(
    const RTCBoundsFunctionArguments *uniform args)
{
  uniform ExampleSpheres *uniform self =
      (uniform ExampleSpheres * uniform) args->geometryUserPtr;
  uniform int primID = args->primID;

  uniform vec4f vertex = (*(self->sphere + args->primID));
  const uniform vec3f center = make_vec3f(vertex);
  const uniform float radius = vertex.w;

  box3fa *uniform out = (box3fa * uniform) args->bounds_o;
  *out = make_box3fa(center - radius, center + radius);
}

// this is the function callback the embree calls to compute a single
// ray-primitive intersection. For sake of simplicity we'll use a very,
// very simple recursive test here - this is neither fast nor elegant,
// but does show that you can do mere arbitrary stuff in a ISPC kernel,
// including 'true' recursion.
void ExampleSpheres_intersect_kernel(
    const RTCIntersectFunctionNArguments *uniform args,
    const uniform bool isOcclusionTest)
{
  // make sure to set the mask
  if (!args->valid[programIndex])
    return;

  args->valid[programIndex] = 0;

  uniform ExampleSpheres *uniform self =
      (uniform ExampleSpheres * uniform) args->geometryUserPtr;
  uniform unsigned int primID = args->primID;

  // this assumes that the args->rayhit is actually a pointer to a varying ray!
  varying Ray *uniform ray = (varying Ray * uniform) args->rayhit;

  uniform vec4f vertex = (*(self->sphere + args->primID));
  const uniform vec3f center = make_vec3f(vertex);
  const uniform float radius = vertex.w;

  const Intersections isect = intersectSphere(ray->org, ray->dir, center, radius);

  // call intersection filtering callback and setup hit if accepted
  if (filterIntersectionBoth(args, isect, isOcclusionTest)) {
    args->valid[programIndex] = -1;
  }
}


unmasked void ExampleSpheres_occluded(const struct RTCOccludedFunctionNArguments *uniform args)
{
    ExampleSpheres_intersect_kernel((RTCIntersectFunctionNArguments *)args, true);
}

/*
SampleAreaRes ExampleSpheres_sampleArea(const Geometry *uniform const _self,
                                        const int32 primID,
                                        const uniform affine3f &xfm,
                                        const uniform affine3f &rcp_xfm,
                                        const vec2f &s,
                                        const float)
{
  const ExampleSpheres *const uniform self = (const ExampleSpheres *uniform)_self;

  uniform vec4f vertex = (*(self->sphere + primID));

  const vec3f localNormal = uniformSampleSphere(vertex.w, s);
  const vec3f localPos = make_vec3f(vertex) + localNormal;

  SampleAreaRes res;
  res.pos = xfmPoint(xfm, localPos);
  res.normal = normalize(xfmVector(transposed(rcp_xfm.l), localNormal));

  return res;
}

void ExampleSpheres_getAreas(const Geometry *const uniform _self,
                             const int32 *const uniform primIDs,
                             const uniform int32 numPrims,
                             const uniform affine3f &xfm,
                             float *const uniform areas)
{
  ExampleSpheres *uniform self = (ExampleSpheres * uniform) _self;

  // detect (non-uniform) scaling; get length of transformed unit-vectors
  const uniform vec3f scaling3 =
      make_vec3f(length(xfm.l.vx), length(xfm.l.vy), length(xfm.l.vz));
#if 0
  const uniform float min_scaling = reduce_min(scaling3);
  const uniform float max_scaling = reduce_max(scaling3);
  if ((max_scaling - min_scaling) > 1e-4f * min_scaling) {
    postStatusMsg(ISPC_MSG_SPHERES, OSP_LOG_WARNING);
  }
#endif

  // use average as approximation to closest uniform scaling
  const uniform float scale = (float)four_pi * sqr(reduce_add(scaling3) / 3.f);

  // TODO vectorize this loop, with foreach or ProgramCount & ProgramIndex
  for (uniform int32 i = 0; i < numPrims; i++) {
    const uniform vec4f vertex = get_vec4f(self->sphere, i);
    areas[i] = scale * sqr(vertex.w);
  }
}
*/


export void *uniform ExampleSpheres_intersect_addr()
{
  return ExampleSpheres_intersect_kernel;
}

export void *uniform ExampleSpheres_postIntersect_addr()
{
  return ExampleSpheres_postIntersect;
}

OSPRAY_END_ISPC_NAMESPACE
