/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "Common/X87Angle.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/TurnAngles.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Registry.h"

/// How a placed thing turns and leans. The game keeps the angles of a placed thing and makes its turn from them.
namespace openblack::ecs::placed_turn
{

/// Whether a thing can lean about x and z as well as turn about the upright axis. These things can lean:
/// - mobile statics, rocks, dead trees, vortexes, teleport stones and the pieces that break off a building
/// - mobile objects, pots, footballs, one-shot miracle globes and reward chests
/// All other things stand upright. Their angles about x and z are always 0.
[[nodiscard]] inline bool CanLean(const Registry& registry, entt::entity entity)
{
	return registry.AnyOf<components::MobileStatic, components::MobileObject, components::Pot, components::DeadTree,
	                      components::Ball, components::OneOffSpellSeed, components::BuildingPiece, components::Reward,
	                      components::Vortex, components::TeleportStone>(entity);
}

/// The angles that a thing keeps from the given angles. A thing that does not lean keeps only the angle about y.
[[nodiscard]] constexpr glm::vec3 KeptAngles(const glm::vec3& angles, bool canLean)
{
	return canLean ? angles : glm::vec3(0.0f, angles.y, 0.0f);
}

/// The game's turn of a thing from its kept angles. A thing that leans turns about z, then x, then y. A thing that does
/// not lean turns only about y.
[[nodiscard]] inline glm::mat3 TurnOf(const glm::vec3& kept, bool canLean)
{
	return canLean ? x87::GameTurnMatrix(kept.x, kept.y, kept.z) : x87::GameUprightTurn(kept.y);
}

/// The angles and the turn that a thing takes from the axes of its body, each time the physics moves it
struct BodyTurn
{
	glm::vec3 angles {0.0f};
	glm::mat3 turn {1.0f};
};

/// The angles and the turn of a thing from the axes of its body. The game finds the angles of the body's matrix, keeps
/// the angles that the thing can have, and makes the thing's turn again from them. A thing that does not lean keeps
/// only its angle about y, thus it stands upright with the heading of its body.
[[nodiscard]] inline BodyTurn TurnFromBody(const glm::mat3& axes, bool canLean)
{
	const auto kept = KeptAngles(x87::GameTurnMatrixAngles(axes), canLean);
	return {.angles = kept, .turn = TurnOf(kept, canLean)};
}

/// The angles of a thing with this turn. The kept angles are correct while the thing has the turn that they made. When
/// a different turn replaces it, the angles come from the turn, as the game finds the angles of a matrix.
[[nodiscard]] inline glm::vec3 AnglesOf(const components::TurnAngles* kept, const glm::mat3& turn, bool canLean)
{
	if (kept != nullptr && kept->turn == turn)
	{
		return kept->angles;
	}
	return KeptAngles(x87::GameTurnMatrixAngles(turn), canLean);
}

/// The angles of a thing, in radians about x, y and z, as the scripts read them
[[nodiscard]] inline glm::vec3 AnglesOf(const Registry& registry, entt::entity entity)
{
	const auto* transform = registry.TryGet<const components::Transform>(entity);
	if (transform == nullptr)
	{
		return glm::vec3(0.0f);
	}
	return AnglesOf(registry.TryGet<const components::TurnAngles>(entity), transform->rotation, CanLean(registry, entity));
}

/// Keep the angles of a thing that has the turn from these angles now. A thing that does not lean keeps only the angle
/// about y.
inline void KeepAngles(Registry& registry, entt::entity entity, const glm::vec3& angles)
{
	const auto* transform = registry.TryGet<const components::Transform>(entity);
	if (transform == nullptr)
	{
		return;
	}
	const components::TurnAngles value {.angles = KeptAngles(angles, CanLean(registry, entity)), .turn = transform->rotation};
	if (auto* kept = registry.TryGet<components::TurnAngles>(entity); kept != nullptr)
	{
		*kept = value;
		return;
	}
	registry.Assign<components::TurnAngles>(entity, value);
}

/// Turn a thing to the given angles with the game's turn, and keep the angles. A thing that does not lean keeps only the
/// angle about y.
inline void SetAngles(Registry& registry, entt::entity entity, const glm::vec3& angles)
{
	auto* transform = registry.TryGet<components::Transform>(entity);
	if (transform == nullptr)
	{
		return;
	}
	const bool canLean = CanLean(registry, entity);
	const auto kept = KeptAngles(angles, canLean);
	transform->rotation = TurnOf(kept, canLean);
	KeepAngles(registry, entity, kept);
	registry.SetDirty();
}

/// The angles of a thing after a turn about the upright axis by `radians`, in the direction of `glm::eulerAngleY`. The
/// game adds the turn to the angle about y and keeps the leans.
[[nodiscard]] constexpr glm::vec3 TurnedAboutUpright(const glm::vec3& angles, float radians)
{
	return {angles.x, angles.y - radians, angles.z};
}

} // namespace openblack::ecs::placed_turn
