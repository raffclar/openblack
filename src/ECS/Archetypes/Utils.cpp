/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Utils.h"

#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Components/Transform.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;

std::pair<glm::vec2, float> openblack::ecs::archetypes::GetFixedObstacleBoundingCircle(MeshId meshId,
                                                                                       const Transform& transform)
{
	auto resourceId = resources::HashIdentifier(meshId);
	const auto l3dMesh = Locator::resources::value().GetMeshes().Handle(resourceId);
	assert(l3dMesh);
	const auto& bb = l3dMesh->GetBoundingBox();
	const auto bbSize = glm::max(glm::vec2(1.0f, 1.0f), glm::xz(bb.Size() * 0.5f * transform.scale));
	const glm::mat4 modelMatrix = affine::Model(transform);
	const auto point = glm::xz(glm::vec4(bb.Center(), 1.0f) * glm::transpose(modelMatrix));
	const auto radius = (glm::compMax(bbSize) / glm::compMin(bbSize) > 1.4) ? glm::length(bbSize) : glm::compMax(bbSize);

	return std::make_pair(point, radius);
}
