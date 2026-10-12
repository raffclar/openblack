/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TempleDestructionSystem.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>
#include <vector>

#include <entt/core/hashed_string.hpp>

#include "3D/ModelSurface.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageLight.h"
#include "ECS/Registry.h"
#include "GameTempleDestructionWorld.h"
#include "Physics/TempleHeart.h"
#include "Temple/TempleDestruction.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The sound that loops over the temple until the explosion, and the explosion's
constexpr entt::hashed_string k_LoopSound = entt::hashed_string("InGame.sad/171");
constexpr entt::hashed_string k_ExplosionSound = entt::hashed_string("InGame.sad/168");
/// A hurt heart's five beam sounds, in the order the samples 206 to 210 go
constexpr std::array<entt::hashed_string, 5> k_DamageSounds {
    entt::hashed_string("InGame.sad/206"), entt::hashed_string("InGame.sad/207"), entt::hashed_string("InGame.sad/208"),
    entt::hashed_string("InGame.sad/209"), entt::hashed_string("InGame.sad/210"),
};
static_assert(k_DamageSounds.size() == physics::temple_heart::k_BeamSounds);
constexpr uint32_t k_FirstDamageSample = 206;
/// The script the game starts when the local player's temple is destroyed
constexpr std::string_view k_GameOverScript = "GameOver";
} // namespace

TempleDestructionSystem::TempleDestructionSystem()
    : TempleDestructionSystem(std::make_unique<GameTempleDestructionWorld>())
{
}

TempleDestructionSystem::TempleDestructionSystem(std::unique_ptr<temple_world::World> world)
    : _world(std::move(world))
{
}

TempleDestructionSystem::~TempleDestructionSystem() = default;

void TempleDestructionSystem::RemoveTemple(entt::entity temple)
{
	auto& registry = *_world->Entities();
	std::vector<entt::entity> entrances;
	registry.Each<const TempleEntrance>([temple, &entrances](entt::entity entity, const TempleEntrance& entrance) {
		if (entrance.temple == temple)
		{
			entrances.push_back(entity);
		}
	});
	for (const auto entrance : entrances)
	{
		_world->Remove(entrance);
	}
	RemoveLanterns(temple);
	// Any worship site made for it since its destruction began goes with it
	_world->RemoveWorshipSites(temple);
	_world->Remove(temple);
}

void TempleDestructionSystem::RemoveLanterns(entt::entity temple)
{
	auto& registry = *_world->Entities();
	// Its two entrance lanterns, with their village lights and the lights' flames and glow
	std::vector<std::pair<entt::entity, entt::entity>> lanterns;
	registry.Each<const TempleLantern>([temple, &lanterns](entt::entity entity, const TempleLantern& lantern) {
		if (lantern.temple == temple)
		{
			lanterns.emplace_back(entity, lantern.light);
		}
	});
	for (const auto& [lantern, light] : lanterns)
	{
		std::vector<entt::entity> sprites;
		registry.Each<const VillageLightSprite>(
		    [light = light, &sprites](entt::entity entity, const VillageLightSprite& sprite) {
			    if (sprite.light == light)
			    {
				    sprites.push_back(entity);
			    }
		    });
		for (const auto sprite : sprites)
		{
			_world->Remove(sprite);
		}
		_world->Remove(light);
		_world->Remove(lantern);
	}
}

void TempleDestructionSystem::DamageEffect(entt::entity entity, Temple& temple)
{
	namespace temple_heart = physics::temple_heart;
	auto& registry = *_world->Entities();
	// Only a heart with some life lost does it; one with none left does it at its fiercest
	const auto* objectLife = registry.TryGet<const ObjectLife>(entity);
	const auto life = temple_heart::DamageEffectLife(objectLife != nullptr ? objectLife->life : 1.0f);
	if (!life.has_value())
	{
		return;
	}
	const auto interval = temple_heart::DamageInterval(_world->MillisecondsPerTurn(), *life);
	if (!temple_heart::DamageBeamDue(temple.beamTurn, _world->Turn(), interval))
	{
		return;
	}
	// TODO(temple-dressing): the beam itself, plasma from a point of the heart's model, isn't made yet; only its sound
	if (temple_heart::DamageBeamSounds(*life))
	{
		// The sound takes the next of the five whether or not it can be heard
		const auto sample = temple_heart::NextDamageSample(registry.Context().nextDamageBeamSound);
		_world->PlayOnce(k_DamageSounds.at(sample - k_FirstDamageSample).value(),
		                 registry.Get<const Transform>(entity).position, entity);
	}
}

void TempleDestructionSystem::Beam(entt::entity entity, Temple& temple, glm::vec3 position)
{
	namespace td = temple_destruction;
	if (!td::Beaming(temple.destructionClock))
	{
		return;
	}
	// As many beams as the beam clock falls behind the clock, each between two random points of the heart's model facing
	// no more than a little down, leaving along the first's normal and arriving against the second's
	std::vector<model_surface::Triangle> triangles;
	while (temple.destructionBeamClock < temple.destructionClock)
	{
		const float share = td::BeamShare(temple.destructionClock);
		temple.destructionBeamClock = td::NextBeam(temple.destructionBeamClock, share);
		if (!temple.destructionBeamSource.has_value())
		{
			temple.destructionBeamSource =
			    _world->StartSpotVisual(SpotVisualType::MagicBeamOnCitadel, position,
			                            td::TurnsFor(_world->MillisecondsPerTurn(), td::k_BeamsOver), temple.owner);
		}
		auto* random = _world->Random();
		if (random == nullptr)
		{
			continue;
		}
		if (triangles.empty())
		{
			triangles = _world->DrawnTrianglesOf(entity);
		}
		const auto placement = _world->PlacementOf(entity);
		const auto from = model_surface::RandomUpwardPoint(triangles, placement, *random);
		const auto to = model_surface::RandomUpwardPoint(triangles, placement, *random);
		if (!temple.destructionBeamSource.has_value() || !from.has_value() || !to.has_value())
		{
			continue;
		}
		const auto look = td::BeamLookAt(share);
		_world->AddPlasma(*temple.destructionBeamSource, {
		                                                     .start = from->position,
		                                                     .end = to->position,
		                                                     .startTangent = from->normal,
		                                                     .endTangent = -to->normal,
		                                                     .life = look.life,
		                                                     .speed = look.speed,
		                                                     .alpha = look.alpha,
		                                                 });
	}
}

void TempleDestructionSystem::Step(entt::entity entity, Temple& temple)
{
	namespace td = temple_destruction;
	const auto millisecondsPerTurn = _world->MillisecondsPerTurn();
	const float before = temple.destructionClock;
	constexpr float k_SecondsPerMillisecond = 0.001f;
	temple.destructionClock += static_cast<float>(millisecondsPerTurn) * k_SecondsPerMillisecond;
	const auto events = td::Between(before, temple.destructionClock);
	const auto position = _world->Entities()->Get<const Transform>(entity).position;
	// The beams' spot visual is let go once it has ended
	if (temple.destructionBeamSource.has_value() && !_world->SpotVisualRunning(*temple.destructionBeamSource))
	{
		temple.destructionBeamSource.reset();
	}
	// The local player's heartbeat quickens with the clock (the heartbeat system reads it)
	// TODO(physics): the heart fades out from fourteen seconds under a shell drawn over it; openblack draws no heart fade
	// or shell yet
	// A clock started again while the loop still plays starts another loop, sounding over the first, as the game makes a
	// new sound each time
	if (events.loopStarts)
	{
		const auto loop = _world->StartLoop(k_LoopSound.value(), position, entity);
		if (loop != entt::null)
		{
			temple.destructionLoops.push_back(loop);
			_loops.emplace_back(entity, loop);
		}
	}
	if (events.loopStarts)
	{
		temple.destructionBeamClock = temple.destructionClock;
	}
	Beam(entity, temple, position);
	// The lanterns are not drawn past 21.8 seconds
	if (!td::LanternsShown(temple.destructionClock))
	{
		RemoveLanterns(entity);
	}
	if (events.glow)
	{
		temple.destructionGlow = _world->StartSpotVisual(SpotVisualType::MagicFxOnCitadel, position,
		                                                 td::TurnsFor(millisecondsPerTurn, td::k_GlowSeconds), temple.owner);
		if (temple.destructionGlow.has_value())
		{
			// The glow is over the heart
			_world->FollowWithSpotVisual(*temple.destructionGlow, entity);
		}
	}
	if (events.explosion)
	{
		// Every loop over the temple stops, and the explosion sounds
		for (const auto loop : temple.destructionLoops)
		{
			_world->StopSound(loop);
			std::erase(_loops, std::pair {entity, loop});
		}
		temple.destructionLoops.clear();
		_world->PlayOnce(k_ExplosionSound.value(), position, entity);
		_world->StartSpotVisual(SpotVisualType::ExplosionCitadel, position,
		                        td::TurnsFor(millisecondsPerTurn, td::k_ExplosionSeconds), temple.owner);
	}
	if (events.smoke)
	{
		const float share = _world->RandomShare(td::k_SmokeShareSpread);
		// The smoke is made at the usual magnitude, then made ten times as big
		const auto smoke = _world->StartSpotVisual(SpotVisualType::EvilSmoke, position,
		                                           td::SmokeTurns(millisecondsPerTurn, share), temple.owner);
		if (smoke.has_value())
		{
			_world->SetSpotVisualMagnitude(*smoke, td::k_SmokeMagnitude);
		}
	}
	if (events.end)
	{
		temple.destroying = false;
		temple.destructionClock = 0.0f;
		RemoveTemple(entity);
	}
}

void TempleDestructionSystem::Start(entt::entity temple)
{
	auto* registry = _world->Entities();
	auto* component = registry != nullptr && registry->Valid(temple) ? registry->TryGet<Temple>(temple) : nullptr;
	if (component == nullptr)
	{
		return;
	}
	// Started again, as by a heart healed and destroyed anew while it goes, its clock starts again from nothing
	component->destroying = true;
	component->destructionClock = 0.0f;
	// Its worship sites go at once, the first turn of its destruction
	_world->RemoveWorshipSites(temple);
}

void TempleDestructionSystem::EndTurn()
{
	auto* registry = _world->Entities();
	const auto local = _world->LocalPlayer();
	if (registry == nullptr || !local.has_value())
	{
		return;
	}
	// The local player whose temple is being destroyed has lost: the game's end is played out by its script, once, and
	// never in a skirmish
	auto& context = registry->Context();
	bool destroying = false;
	registry->Each<const Temple>([&local, &destroying](entt::entity, const Temple& temple) {
		destroying = destroying || (temple.owner == *local && temple.destroying);
	});
	if (temple_destruction::GameOverStarts({
	        .over = context.gameOver,
	        .skirmish = context.skirmish,
	        .multiplayer = false,
	        .localTempleDestroying = destroying,
	    }))
	{
		context.gameOver = true;
		_world->StartScript(k_GameOverScript);
	}
}

void TempleDestructionSystem::ProcessTurn()
{
	auto* registry = _world->Entities();
	if (registry == nullptr)
	{
		return;
	}
	// A loop whose temple went some other way than its destruction's end stops with it
	std::erase_if(_loops, [this, registry](const std::pair<entt::entity, entt::entity>& loop) {
		const auto* temple = registry->Valid(loop.first) ? registry->TryGet<const Temple>(loop.first) : nullptr;
		if (temple != nullptr && std::ranges::find(temple->destructionLoops, loop.second) != temple->destructionLoops.end())
		{
			return false;
		}
		_world->StopSound(loop.second);
		return true;
	});
	std::vector<entt::entity> destroying;
	registry->Each<const Temple>([&destroying](entt::entity entity, const Temple& temple) {
		if (temple.destroying)
		{
			destroying.push_back(entity);
		}
	});
	for (const auto entity : destroying)
	{
		if (registry->Valid(entity))
		{
			Step(entity, registry->Get<Temple>(entity));
		}
	}
	// Then every heart that is hurt beams, its destruction under way or not
	std::vector<entt::entity> temples;
	registry->Each<const Temple>([&temples](entt::entity entity, const Temple&) { temples.push_back(entity); });
	for (const auto entity : temples)
	{
		if (registry->Valid(entity))
		{
			DamageEffect(entity, registry->Get<Temple>(entity));
		}
	}
}
