// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// ospray
#include "common/Data.ih"
#include "common/FilterIntersect.ih"
#include "common/ISPCMessages.h"
#include "common/Intersect.ih"
#include "common/Ray.ih"
#include "common/World.ih"
#include "geometry/Geometry.ih"
#include "math/sampling.ih"
#include "rkcommon/math/box.ih"
#include "rkcommon/math/vec.ih"

struct ExampleSpheres {
    Geometry super;
    Data1D vertex;
    Data1D radius;
    Data1D texcoord;
    float global_radius;
};

static void ExampleSpheres_postIntersect(const Geometry *uniform geometry,
                                         varying DifferentialGeometry &dg,
                                         const varying Ray &ray,
                                         uniform int64 flags)
{
    ExampleSpheres *uniform self = (ExampleSpheres * uniform) geometry;

    dg.Ng = dg.Ns = ray.Ng;

    float radius = self->global_radius;
    if (valid(self->radius))
        radius = get_float(self->radius, ray.primID);

    // make epsilon large enough to not get lost when computing
    // |CO| = |center-ray.org| ~ radius for 2ndary rays
    dg.epsilon = radius * ulpEpsilon;

    if (and(flags & DG_TEXCOORD, valid(self->texcoord)))
        dg.st = get_vec2f(self->texcoord, ray.primID);
}

unmasked void ExampleSpheres_bounds(const RTCBoundsFunctionArguments *uniform args)
{
    ExampleSpheres *uniform self = (ExampleSpheres * uniform) args->geometryUserPtr;
    uniform int primID = args->primID;
    uniform float radius =
        valid(self->radius) ? get_float(self->radius, primID) : self->global_radius;
    uniform vec3f center = get_vec3f(self->vertex, primID);
    box3fa *uniform out = (box3fa * uniform) args->bounds_o;
    *out = make_box3fa(center - radius, center + radius);
}

void ExampleSpheres_intersect_kernel(const RTCIntersectFunctionNArguments *uniform args,
                                     const uniform bool isOcclusionTest)
{
    // make sure to set the mask
    if (!args->valid[programIndex])
        return;

    ExampleSpheres *uniform self = (ExampleSpheres * uniform) args->geometryUserPtr;
    uniform int primID = args->primID;

    // this assumes that the args->rayhit is actually a pointer to a varying ray!
    varying Ray *uniform ray = (varying Ray * uniform) args->rayhit;

    uniform float radius =
        valid(self->radius) ? get_float(self->radius, primID) : self->global_radius;
    uniform vec3f center = get_vec3f(self->vertex, primID);

    const Intersections isect = intersectSphere(ray->org, ray->dir, center, radius);

    // call intersection filtering callback and setup hit if accepted
    filterIntersectionBoth(args, isect, isOcclusionTest);
}

unmasked void ExampleSpheres_intersect(
    const struct RTCIntersectFunctionNArguments *uniform args)
{
    ExampleSpheres_intersect_kernel(args, false);
}

unmasked void ExampleSpheres_occluded(const struct RTCOccludedFunctionNArguments *uniform args)
{
    ExampleSpheres_intersect_kernel((RTCIntersectFunctionNArguments *)args, true);
}

SampleAreaRes ExampleSpheres_sampleArea(const Geometry *uniform const _self,
                                        const int32 primID,
                                        const uniform affine3f &xfm,
                                        const uniform affine3f &rcp_xfm,
                                        const vec2f &s,
                                        const float)
{
    const ExampleSpheres *const uniform self = (const ExampleSpheres *uniform)_self;

    float radius = self->global_radius;
    if (valid(self->radius))
        radius = get_float(self->radius, primID);

    const vec3f localNormal = uniformSampleSphere(radius, s);
    const vec3f localPos = get_vec3f(self->vertex, primID) + localNormal;

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
    const uniform float min_scaling = reduce_min(scaling3);
    const uniform float max_scaling = reduce_max(scaling3);
    if ((max_scaling - min_scaling) > 1e-4f * min_scaling)
        postStatusMsg(ISPC_MSG_SPHERES, OSP_LOG_WARNING);

    // use average as approximation to closest uniform scaling
    const uniform float scale = four_pi * sqr(reduce_add(scaling3) / 3.f);

    if (!valid(self->radius)) {
        const uniform float sphereArea = scale * sqr(self->global_radius);
        foreach (i = 0 ... numPrims)
            areas[i] = sphereArea;
    } else {
        // TODO vectorize this loop, with foreach or ProgramCount & ProgramIndex
        for (uniform int32 i = 0; i < numPrims; i++)
            areas[i] = scale * sqr(get_float(self->radius, i));
    }
}

void ExampleSpheres_Constructor(ExampleSpheres *uniform self)
{
    Geometry_Constructor(&self->super, ExampleSpheres_postIntersect);
    self->super.getAreas = ExampleSpheres_getAreas;
    self->super.sampleArea = ExampleSpheres_sampleArea;
    Data1D_Constructor(&self->vertex);
    Data1D_Constructor(&self->radius);
    Data1D_Constructor(&self->texcoord);
    self->global_radius = 0.01;
}

export void *uniform ExampleSpheres_create()
{
    ExampleSpheres *uniform self = uniform new ExampleSpheres;
    ExampleSpheres_Constructor(self);
    return self;
}

export void ExampleSpheres_set(void *uniform _self,
                               void *uniform _embreeGeometry,
                               const Data1D *uniform vertex,
                               const Data1D *uniform radius,
                               const Data1D *uniform texcoord,
                               uniform float global_radius)
{
    ExampleSpheres *uniform self = (ExampleSpheres * uniform) _self;

    self->vertex = *vertex;
    self->radius = *radius;
    self->texcoord = *texcoord;
    self->global_radius = global_radius;
    self->super.numPrimitives = vertex->numItems;

    Geometry_setEmbreeUserGeometry(&self->super,
                                   (RTCGeometry)_embreeGeometry,
                                   ExampleSpheres_bounds,
                                   ExampleSpheres_intersect,
                                   ExampleSpheres_occluded);
}
