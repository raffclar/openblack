/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Rocks.h"

#include <cmath>

#include <algorithm>

#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Audio/Audio.h"
#include "Common/GUtilsAngle.h"
#include "Common/GameRandom.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// info.dat's mobile static mobileType of the Rock class
constexpr int k_RockMobileType = 2;
} // namespace

bool Rocks::IsRock(entt::entity entity)
{
	const auto* statics = Locator::entitiesRegistry::value().TryGet<const MobileStatic>(entity);
	if (statics == nullptr || !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& info = Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(statics->type));
	return static_cast<int>(info.mobileType) == k_RockMobileType;
}

float Rocks::Radius2D(entt::entity entity)
{
	return object::Get2DRadius(entity);
}

float Rocks::Height(entt::entity entity)
{
	return object::GetHeight(entity);
}

bool Rocks::ValidForPlaceInHand(entt::entity entity)
{
	return Radius2D(entity) <= 3.6f;
}

bool Rocks::ValidToTap(entt::entity entity)
{
	return Height(entity) > 0.7f;
}

std::array<entt::entity, 2> Rocks::Tap(entt::entity entity, glm::vec3 handPosition)
{
	const auto halves = SplitInTwo(entity);
	// after SplitInTwo: bank InGame, sample 130 G_RockTap_01 + a counter (0..3 in turn), owner the rock, 3D, not
	// tracked, at the hand's point
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), 130 + audio::NextCounter(audio::Counter::RockTap)};
	options.owner = audio::Owner::Thing(entity);
	options.is3D = true;
	options.track = false;
	options.position = handPosition;
	audio::PlaySoundEffect(options);
	// TODO: the player's creature empathises with CREATURE_DESIRE_TO_PLAY (0.5, at the rock) once creatures have desires
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Rock: tapped at ({:.1f}, {:.1f}, {:.1f})", handPosition.x, handPosition.y,
	                   handPosition.z);
	return halves;
}

std::array<entt::entity, 2> Rocks::SplitInTwo(entt::entity entity, glm::vec3 velocity, glm::vec3 angularMomentum)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<const Transform>(entity);
	const auto type = registry.Get<const MobileStatic>(entity).type;
	constexpr float k_Factor = 0.7935f; // about the cube root of 1/2: each half has half the volume
	const float scale = transform.scale.x * k_Factor;
	const float offset = Radius2D(entity) * k_Factor;
	// Only the Y angle is kept. MobileStaticArchetype's rotation has column 2 = (-sin y cos x, sin x, cos y cos x).
	const float yAngle = std::atan2(-transform.rotation[2].x, transform.rotation[2].z);
	// the first draw: GameFloatRand(2pi)
	const float a = game_random::GameFloatRand(glm::two_pi<float>());
	// o = GetPosFromAngle(a, R2D x 0.7935); the halves at the rock's MapCoords + o and - o: both keep the rock's
	// altitude (o's is 0), so each half is the ground at its own point plus that altitude
	const auto o = gutils::GetPosFromAngle(a, offset);
	const auto centre = map_coords::FromWorld(transform.position);
	std::array<entt::entity, 2> halves {};
	for (size_t i = 0; i < halves.size(); ++i)
	{
		const auto at = map_coords::ToWorld(i == 0 ? centre + o : centre - o);
		halves.at(i) = archetypes::MobileStaticArchetype::Create(at, type, 0.0f, 0.0f, yAngle, 0.0f, scale);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Rock: split type {} scale {:.2f} -> 2 x {:.2f}", static_cast<int>(type),
	                   transform.scale.x, scale);
	// the fire passes on to both halves (fire::CopyFire), then the old rock goes
	for (const auto half : halves)
	{
		fire::CopyFire(entity, half);
	}
	physics::PhysicsObjects::RemoveObject(entity);
	ecs::map_cells::RemoveMapObject(entity); // deleted: out of the map
	registry.Destroy(entity);
	registry.SetDirty();
	// into physics: they fall and settle, or fly on
	for (const auto half : halves)
	{
		if (auto* po = physics::PhysicsObjects::AddObject(half, velocity, glm::vec3(0.0f)))
		{
			po->body.angularMomentum = angularMomentum;
		}
	}
	return halves;
}
