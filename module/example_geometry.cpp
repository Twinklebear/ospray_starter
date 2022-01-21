// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// ospray
#include "example_geometry.h"
#include "common/Data.h"
#include "common/World.h"
// ispc-generated files
#include "example_geometry_ispc.h"

namespace ospray {
namespace example {

    ExampleSpheres::ExampleSpheres()
    {
        ispcEquivalent = ispc::ExampleSpheres_create();
    }

    std::string ExampleSpheres::toString() const
    {
        return "ospray::ExampleSpheres";
    }

    void ExampleSpheres::commit()
    {
        if (!embreeDevice) {
            throw std::runtime_error("invalid Embree device");
        }
        if (!embreeGeometry) {
            embreeGeometry = rtcNewGeometry(embreeDevice, RTC_GEOMETRY_TYPE_USER);
        }
        radius = getParam<float>("radius", 0.01f);
        vertexData = getParamDataT<vec3f>("sphere.position", true);
        radiusData = getParamDataT<float>("sphere.radius");
        texcoordData = getParamDataT<vec2f>("sphere.texcoord");

        ispc::ExampleSpheres_set(getIE(),
                                 embreeGeometry,
                                 ispc(vertexData),
                                 ispc(radiusData),
                                 ispc(texcoordData),
                                 radius);

        postCreationInfo();
    }

    size_t ExampleSpheres::numPrimitives() const
    {
        return vertexData ? vertexData->size() : 0;
    }

}
}
