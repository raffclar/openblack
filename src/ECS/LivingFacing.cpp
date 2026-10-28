/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LivingFacing.h"

#include <glm/trigonometric.hpp>

#include "Common/GUtilsAngle.h"
#include "Common/PortableRotations.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/ThingMapPlace.h"

namespace openblack::ecs
{

namespace
{
/// Turns the walker to an angle across the land. The walker comes out of its map cell and goes in again only when the
/// angle is not the angle that it kept, as the game compares the two angles.
void Turn(Registry& registry, entt::entity walker, float yAngle, components::WallHug* walk)
{
	auto* transform = registry.TryGet<components::Transform>(walker);
	if (transform == nullptr)
	{
		return;
	}
	const bool turned = walk == nullptr || walk->yAngle != yAngle;
	if (walk != nullptr)
	{
		walk->yAngle = yAngle;
	}
	transform->rotation = portable_math::EulerAngleY(-yAngle - glm::radians(90.0f));
	registry.SetDirty();
	if (turned)
	{
		thing_map_place::Reinsert(walker);
	}
}
} // namespace

void living_facing::FaceGameAngle(Registry& registry, entt::entity walker, uint16_t gameAngle)
{
	auto* walk = registry.TryGet<components::WallHug>(walker);
	const auto angle = static_cast<uint16_t>(gameAngle & gutils::k_GameAngleMask);
	if (walk != nullptr)
	{
		walk->gameAngle = angle;
	}
	Turn(registry, walker, gutils::ConvertGameAngleTo3D(angle), walk);
}

void living_facing::FaceAngle(Registry& registry, entt::entity walker, float yAngle)
{
	auto* walk = registry.TryGet<components::WallHug>(walker);
	Turn(registry, walker, yAngle, walk);
	if (walk != nullptr)
	{
		walk->gameAngle = static_cast<uint16_t>(gutils::ConvertAngle3DToGame(yAngle));
	}
}

void living_facing::FaceOnly(Registry& registry, entt::entity walker, float yAngle)
{
	Turn(registry, walker, yAngle, registry.TryGet<components::WallHug>(walker));
}

} // namespace openblack::ecs
