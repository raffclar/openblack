/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The running state of the miracles in a saved game.

#define LOCATOR_IMPLEMENTATIONS

#include <optional>
#include <utility>

#include <glm/gtx/euler_angles.hpp>

#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Map.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"
#include "MagicSpellLinks.h"
#include "MagicSystem.h"
#include "Sound/SoundRequestInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
ecs::Registry& EntityRegistry()
{
	return Locator::entitiesRegistry::value();
}
} // namespace

// The running state in a saved game (deviation 83)

void MagicSystem::SaveState(reflection::BinaryWriter& out) const
{
	// The miracles go in the order that they are processed. Each one makes its link to its effect again at the load.
	reflection::Save(out, std::vector<entt::entity>(_spells.begin(), _spells.end()));
	reflection::Save(out, _creatureCasts);
	reflection::Save(out, _creatureBeams);
	reflection::Save(out, _world.PuttingOutFire());
	// The movement of this computer's hand and the place of its effect: only the frames change them, and only the hand's
	// own effect and the casts that the hand sends read them
	out.BeginPresentation();
	reflection::Save(out, _handVelocity);
	reflection::Save(out, _lastHandPosition);
	out.WriteRaw(_handSpin);
	reflection::Save(out, _spinningSeed);
	reflection::Save(out, _handEffectPoint);
	reflection::Save(out, _handScale);
	reflection::Save(out, _handAtStep);
	out.EndPresentation();
	// The hands keep their seeds (HandHeldSeed). An older save kept the seed of the first player here.
	reflection::Save(out, std::optional<entt::entity> {});
	// The press of this computer's hand, its sound and its pour: the hand sends its casts as commands, which the turn does
	out.BeginPresentation();
	out.WriteRaw(_input);
	reflection::Save(out, _actionDown);
	out.WriteRaw(_fireBallPress);
	reflection::Save(out, _holdLoop != entt::null);
	out.WriteRaw(_pour);
	out.EndPresentation();
	reflection::Save(out, _rainWatchers);
	// The last circle that this computer's hand drew. The cast sends its place in the command.
	out.BeginPresentation();
	out.WriteRaw(_circle);
	out.EndPresentation();
	reflection::Save(out, _turn);
	// Only the tools read the result of the last action of the hand
	reflection::SavePresentation(out, _lastHandResult);
	reflection::Save(out, _ignoreInfluence);
}

bool MagicSystem::LoadState(reflection::BinaryReader& in)
{
	return LoadBody(in, false);
}

bool MagicSystem::LoadOldState(reflection::BinaryReader& in)
{
	return LoadBody(in, true);
}

bool MagicSystem::LoadBody(reflection::BinaryReader& in, bool hasSpellGrid)
{
	std::vector<entt::entity> spells;
	reflection::Load(in, spells);
	reflection::Load(in, _creatureCasts);
	reflection::Load(in, _creatureBeams);
	std::unordered_map<entt::entity, uint32_t> watchers;
	reflection::Load(in, watchers);
	_world.RestorePuttingOutFire(std::move(watchers));
	if (hasSpellGrid)
	{
		// An older save kept a grid of where miracles ran lately. Nothing reads it, so the load reads past it.
		std::array<std::array<uint8_t, k_OldSpellGridSide>, k_OldSpellGridSide> grid {};
		reflection::Load(in, grid);
	}
	reflection::Load(in, _handVelocity);
	reflection::Load(in, _lastHandPosition);
	in.ReadRaw(_handSpin);
	reflection::Load(in, _spinningSeed);
	reflection::Load(in, _handEffectPoint);
	reflection::Load(in, _handScale);
	reflection::Load(in, _handAtStep);
	std::optional<entt::entity> olderHeld;
	reflection::Load(in, olderHeld);
	in.ReadRaw(_input);
	reflection::Load(in, _actionDown);
	in.ReadRaw(_fireBallPress);
	bool holdLoop = false;
	reflection::Load(in, holdLoop);
	in.ReadRaw(_pour);
	reflection::Load(in, _rainWatchers);
	in.ReadRaw(_circle);
	reflection::Load(in, _turn);
	reflection::Load(in, _lastHandResult);
	reflection::Load(in, _ignoreInfluence);
	if (in.Failed())
	{
		return false;
	}
	// A save older than the seeds of the hands kept the seed of the first player
	if (olderHeld.has_value() && !HeldOf(PlayerNames::PLAYER_ONE).has_value())
	{
		const ActingAs acting(*this, PlayerNames::PLAYER_ONE);
		SetHeld(olderHeld);
	}
	// Each miracle links to its effect again, and the effect finds its miracle with no new event
	_spells.clear();
	_links.clear();
	if (spells.empty() && !Held().has_value())
	{
		if (holdLoop)
		{
			StartHoldLoop();
		}
		return true;
	}
	auto& registry = EntityRegistry();
	auto* effects = Locator::effectRequestSystem::has_value() ? &Locator::effectRequestSystem::value() : nullptr;
	for (const auto entity : spells)
	{
		const auto* spell = registry.Valid(entity) ? registry.TryGet<const Spell>(entity) : nullptr;
		if (spell == nullptr)
		{
			continue;
		}
		auto link = std::make_unique<SpellLink>(*this, entity);
		if (effects != nullptr && spell->effect != k_NoEffectHandle)
		{
			effects->RestoreSink(spell->effect, link.get());
		}
		_links.insert_or_assign(entity, std::move(link));
		_spells.push_back(entity);
	}
	if (Held().has_value())
	{
		const auto* seed = registry.Valid(*Held()) ? registry.TryGet<const SpellSeed>(*Held()) : nullptr;
		if (seed == nullptr)
		{
			SetHeld(std::nullopt);
		}
		else if (effects != nullptr && seed->handEffect != k_NoEffectHandle)
		{
			effects->RestoreSink(seed->handEffect, _handEffectLink.get());
		}
	}
	// The hum of an armed seed plays again
	if (holdLoop)
	{
		StartHoldLoop();
	}
	return true;
}
