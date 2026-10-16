/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleHeartPassOn.h"

#include <array>
#include <optional>
#include <vector>

#include <entt/core/hashed_string.hpp>

#include "3D/ModelSurface.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/PhysicsHooksWorld.h"
#include "ECS/Registry.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/TempleHeartTowns.h"
#include "Particles/PlasmaCommand.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using physics_hooks::World;

namespace
{
/// A temple heart's five beam sounds, played in turn
constexpr std::array<entt::hashed_string, 5> k_HeartBeamSounds {
    entt::hashed_string("InGame.sad/206"), entt::hashed_string("InGame.sad/207"), entt::hashed_string("InGame.sad/208"),
    entt::hashed_string("InGame.sad/209"), entt::hashed_string("InGame.sad/210")};
static_assert(k_HeartBeamSounds.size() == physics::temple_heart::k_BeamSounds);

/// The temple heart's player's towns as the heart passes a blow on over them: in the order the player gained them, each
/// with its buildings and homeless people, the newest first
std::vector<physics::temple_heart::Town> HeartTowns(World& world, PlayerNames owner)
{
	return temple_heart_towns::Gather(
	    world.Entities(), owner,
	    {
	        .lifeOf = [&world](entt::entity abode) { return world.LifeOf(abode); },
	        .footballPitch =
	            [&world](entt::entity abode) {
		            const auto* info = world.AbodeInfoOf(abode);
		            return info != nullptr && info->abodeType == AbodeType::FootballPitch;
	            },
	        .dying = [&world](entt::entity villager) { return world.VillagerStateOf(villager, true) == VillagerStates::Dying; },
	    });
}

/// The spot visual a temple's heart fires its beams from, made the first time it beams; tried again on a later beam when
/// it couldn't be made
std::optional<uint32_t> HeartBeamSource(World& world, entt::entity heart, Temple& temple)
{
	if (!temple.beamSource.has_value())
	{
		temple.beamSource = world.StartHeartBeamSource(world.Entities().Get<const Transform>(heart).position, temple.owner);
	}
	return temple.beamSource;
}

/// A point's place with an object's height added
glm::vec3 TopOf(World& world, entt::entity object)
{
	auto top = world.Entities().Get<const Transform>(object).position;
	top.y += world.HeightOf(object);
	return top;
}

/// A temple's heart passing a blow that breaks buildings on to a target beams at it every two seconds, with one of its
/// five beam sounds in turn
void BeamAtTarget(World& world, entt::entity heart, Temple& temple, entt::entity target)
{
	namespace temple_heart = physics::temple_heart;
	const auto interval = temple_heart::BeamInterval(static_cast<uint32_t>(TimeSystemInterface::k_TurnDuration.count()));
	temple_heart::Beam beam {.target = temple.beamTarget, .turn = temple.beamTurn};
	const bool due = temple_heart::BeamAtTargetDue(beam, target, world.Turn(), interval);
	temple.beamTarget = beam.target;
	temple.beamTurn = beam.turn;
	if (!due)
	{
		return;
	}
	const auto source = HeartBeamSource(world, heart, temple);
	if (!source.has_value())
	{
		return;
	}
	const auto heartPosition = world.Entities().Get<const Transform>(heart).position;
	// The beam sounds take their turns whether or not they can be heard
	auto& next = world.Entities().Context().nextHeartBeamSound;
	const auto sound = k_HeartBeamSounds.at(next);
	next = (next + 1) % temple_heart::k_BeamSounds;
	world.PlaySound(sound.value(), heartPosition, heart);
	world.AddPlasma(*source, {
	                             .start = TopOf(world, heart),
	                             .end = TopOf(world, target),
	                             .startTangent = particles::k_HeartPlasmaStartTangent,
	                             .endTangent = particles::k_HeartPlasmaEndTangent,
	                             .life = particles::k_HeartPlasmaLife,
	                             .speed = particles::k_HeartPlasmaSpeed,
	                             .alpha = particles::k_HeartPlasmaAlpha,
	                         });
}

/// A temple's heart taking a blow that breaks buildings itself forgets what it beamed at, and on the same wait beams
/// at a point on itself
void BeamAtItself(World& world, entt::entity heart, Temple& temple)
{
	namespace temple_heart = physics::temple_heart;
	const auto interval = temple_heart::BeamInterval(static_cast<uint32_t>(TimeSystemInterface::k_TurnDuration.count()));
	temple_heart::Beam beam {.target = temple.beamTarget, .turn = temple.beamTurn};
	const bool due = temple_heart::BeamAtItselfDue(beam, world.Turn(), interval);
	temple.beamTarget = beam.target;
	temple.beamTurn = beam.turn;
	if (!due)
	{
		return;
	}
	static_cast<void>(HeartBeamSource(world, heart, temple));
	// The beam ends at a random point of the heart's model facing no more than a little downwards, arriving against the
	// surface; the point is drawn even when there is nothing to fire the beam from
	auto* random = world.Random();
	if (random == nullptr)
	{
		return;
	}
	const auto triangles = world.DrawnTrianglesOf(heart);
	const auto point = model_surface::RandomUpwardPoint(triangles, world.PlacementOf(heart), *random);
	if (!temple.beamSource.has_value() || !point.has_value())
	{
		return;
	}
	world.AddPlasma(*temple.beamSource, {
	                                        .start = TopOf(world, heart),
	                                        .end = point->position,
	                                        .startTangent = particles::k_HeartPlasmaStartTangent,
	                                        .endTangent = -point->normal,
	                                        .life = particles::k_HeartPlasmaLife,
	                                        .speed = particles::k_HeartPlasmaSpeed,
	                                        .alpha = particles::k_HeartPlasmaAlpha,
	                                    });
}
} // namespace

physics::temple_heart::Target temple_heart_pass_on::PassOn(World& world, entt::entity heart, bool breaksBuildings)
{
	auto& temple = world.Entities().Get<Temple>(heart);
	const auto target = physics::temple_heart::Choose(HeartTowns(world, temple.owner));
	// While what comes to it breaks buildings, the heart beams at what takes it, or at a point on itself with none
	if (breaksBuildings)
	{
		if (target.kind == physics::temple_heart::TargetKind::Heart)
		{
			BeamAtItself(world, heart, temple);
		}
		else
		{
			BeamAtTarget(world, heart, temple, target.entity);
		}
	}
	return target;
}

entt::entity temple_heart_pass_on::SiteTaker(entt::entity heartChoice, entt::entity heart, entt::entity site, float siteLife)
{
	return heartChoice == heart && siteLife > 0.0f ? site : heartChoice;
}

entt::entity temple_heart_pass_on::TakerOf(World& world, entt::entity object, bool breaksBuildings)
{
	auto& registry = world.Entities();
	if (!registry.Valid(object))
	{
		return object;
	}
	const auto choiceOf = [&world, breaksBuildings](entt::entity heart) {
		const auto target = PassOn(world, heart, breaksBuildings);
		return target.kind == physics::temple_heart::TargetKind::Heart ? heart : target.entity;
	};
	if (registry.AllOf<Temple>(object))
	{
		return choiceOf(object);
	}
	// A worship site asks its temple's heart; a site with no temple takes it itself
	if (const auto* site = registry.TryGet<const WorshipSite>(object);
	    site != nullptr && registry.Valid(site->temple) && registry.AllOf<Temple>(site->temple))
	{
		const auto heart = site->temple;
		return SiteTaker(choiceOf(heart), heart, object, world.LifeOf(object));
	}
	return object;
}
