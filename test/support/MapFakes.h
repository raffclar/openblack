/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "3D/AxisAlignedBoundingBox.h"
#include "ECS/MapCollide.h"
#include "ECS/Systems/MapShapeProviderInterface.h"
#include "ECS/Systems/MeshBoxProviderInterface.h"
#include "ECS/Systems/TownCellObjectsInterface.h"
#include "FakeCallOr.h"

// Fakes of the map services for the tests, made like the villager ones (support/VillagerFakes.h): one std::function
// per method, a function left empty calls the fallback service given to the constructor, or throws
// std::bad_function_call without one.

namespace openblack::test
{
class FakeTownCellObjects final: public ecs::systems::TownCellObjectsInterface
{
public:
	using Interface = ecs::systems::TownCellObjectsInterface;

	FakeTownCellObjects() = default;
	explicit FakeTownCellObjects(std::shared_ptr<Interface> fallback)
	    : _fallback(std::move(fallback))
	{
	}

	std::function<std::vector<entt::entity>(glm::ivec2 cell)> objectsInCell;
	std::function<float(entt::entity object)> get2DRadius;

	[[nodiscard]] std::vector<entt::entity> ObjectsInCell(glm::ivec2 cell) const override
	{
		return detail::CallOr(objectsInCell, _fallback, &Interface::ObjectsInCell, cell);
	}
	[[nodiscard]] float Get2DRadius(entt::entity object) const override
	{
		return detail::CallOr(get2DRadius, _fallback, &Interface::Get2DRadius, object);
	}

private:
	std::shared_ptr<Interface> _fallback;
};

class FakeMapShapeProvider final: public ecs::systems::MapShapeProviderInterface
{
public:
	using Interface = ecs::systems::MapShapeProviderInterface;

	FakeMapShapeProvider() = default;
	explicit FakeMapShapeProvider(std::shared_ptr<Interface> fallback)
	    : _fallback(std::move(fallback))
	{
	}

	std::function<bool(entt::entity object, ecs::map_collide::Shape& shape, float& reach)> meshShape;

	[[nodiscard]] bool MeshShape(entt::entity object, ecs::map_collide::Shape& shape, float& reach) const override
	{
		return detail::CallOr(meshShape, _fallback, &Interface::MeshShape, object, shape, reach);
	}

private:
	std::shared_ptr<Interface> _fallback;
};

class FakeMeshBoxProvider final: public ecs::systems::MeshBoxProviderInterface
{
public:
	using Interface = ecs::systems::MeshBoxProviderInterface;

	FakeMeshBoxProvider() = default;
	explicit FakeMeshBoxProvider(std::shared_ptr<Interface> fallback)
	    : _fallback(std::move(fallback))
	{
	}

	std::function<std::optional<AxisAlignedBoundingBox>(entt::id_type meshId)> meshBox;

	[[nodiscard]] std::optional<AxisAlignedBoundingBox> MeshBox(entt::id_type meshId) const override
	{
		return detail::CallOr(meshBox, _fallback, &Interface::MeshBox, meshId);
	}

private:
	std::shared_ptr<Interface> _fallback;
};
} // namespace openblack::test
