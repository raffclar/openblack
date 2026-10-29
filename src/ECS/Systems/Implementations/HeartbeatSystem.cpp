/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HeartbeatSystem.h"

#include <optional>
#include <unordered_map>
#include <vector>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/GameSoundEffects.h"
#include "Audio/HeaderOverrides.h"
#include "Audio/Sound.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Heartbeat.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/PlayerTowns.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "Help/VillageVoices.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// A town as the danger is measured: its player, the cell it stands on and how many people it has
struct TownSeen
{
	entt::entity entity;
	PlayerNames owner;
	glm::ivec2 cell;
	uint32_t people;
};

std::vector<TownSeen> SeeTowns(const ecs::Registry& registry)
{
	std::unordered_map<entt::entity, uint32_t> people;
	registry.Each<const Villager>([&people](const Villager& villager) {
		if (villager.town != entt::null)
		{
			++people[villager.town];
		}
	});
	// The town list of each player in turn and the neutral list last, so that the first of two towns as near is the first
	// met in these lists
	std::vector<TownSeen> towns;
	for (const auto entity : ecs::player_towns::AllInOrder(registry))
	{
		const auto* transform = registry.TryGet<const Transform>(entity);
		if (transform == nullptr)
		{
			continue;
		}
		const auto found = people.find(entity);
		towns.push_back({.entity = entity,
		                 .owner = registry.Get<const Town>(entity).owner,
		                 .cell = map_coords::CellOf(transform->position),
		                 .people = found != people.end() ? found->second : 0});
	}
	return towns;
}

/// The town nearest a cell, of any player's, the first found of those as near
const TownSeen* NearestTown(const std::vector<TownSeen>& towns, glm::ivec2 cell)
{
	const TownSeen* nearest = nullptr;
	uint32_t best = 0;
	for (const auto& town : towns)
	{
		const auto distance = ecs::heartbeat::CellDistance(cell, town.cell);
		if (nearest == nullptr || distance < best)
		{
			nearest = &town;
			best = distance;
		}
	}
	return nearest;
}

/// What a player's danger is measured from now
ecs::heartbeat::Danger DangerOf(const ecs::Registry& registry, PlayerNames player, const std::vector<TownSeen>& towns,
                                uint32_t worldPopulation, float totalInfluence, std::vector<uint32_t>& creatureDistances)
{
	ecs::heartbeat::Danger danger;
	uint32_t believers = 0;
	for (const auto& town : towns)
	{
		if (town.owner != player)
		{
			continue;
		}
		believers += town.people;
		if (Locator::townDesireSystem::has_value())
		{
			danger.protectionDesire +=
			    Locator::townDesireSystem::value().GetRawDesire(town.entity, TownDesireInfo::ForProtection);
		}
	}
	if (worldPopulation != 0 && believers != 0)
	{
		danger.believerShare = static_cast<float>(believers) / static_cast<float>(worldPopulation);
	}
	const float own = Locator::influenceSystem::has_value() ? Locator::influenceSystem::value().InfluencePower(player) : 0.0f;
	if (own != 0.0f)
	{
		danger.influenceShare = totalInfluence / own;
	}
	// Each other player's creature whose nearest town is this player's
	creatureDistances.clear();
	if (Locator::playerSystem::has_value())
	{
		registry.Each<const Player>([&](const Player& other) {
			if (other.name == player)
			{
				return;
			}
			const auto creature = Locator::playerSystem::value().GetPrimaryCreature(other.name);
			const auto* transform = creature.has_value() ? registry.TryGet<const Transform>(*creature) : nullptr;
			if (transform == nullptr)
			{
				return;
			}
			const auto cell = map_coords::CellOf(transform->position);
			if (const auto* nearest = NearestTown(towns, cell); nearest != nullptr && nearest->owner == player)
			{
				creatureDistances.push_back(ecs::heartbeat::CellDistance(cell, nearest->cell));
			}
		});
	}
	danger.creatureDistances = creatureDistances;
	return danger;
}

/// The beats a second this computer's player's temple holds their heart at while it is destroyed, none otherwise
std::optional<float> DestructionBeatsOf(const ecs::Registry& registry, PlayerNames player)
{
	if (!Locator::playerSystem::has_value() ||
	    (Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE) != player)
	{
		return std::nullopt;
	}
	std::optional<float> beats;
	registry.Each<const Temple>([&beats, player](const Temple& temple) {
		if (temple.owner == player && temple.destroying)
		{
			beats = ecs::heartbeat::DestructionBeats(temple.destructionClock);
		}
	});
	return beats;
}
} // namespace

void HeartbeatSystem::ProcessTurn(uint32_t turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	// Every player but the neutral one has a heart
	std::vector<entt::entity> players;
	registry.Each<const Player>([&players](entt::entity entity, const Player& player) {
		if (player.name != PlayerNames::NEUTRAL)
		{
			players.push_back(entity);
		}
	});
	// A player's heart beats on from the land before, when they kept one, else it starts calm
	for (const auto entity : players)
	{
		if (!registry.AllOf<Heartbeat>(entity))
		{
			const auto kept = _kept.Take(registry.Get<const Player>(entity).name);
			registry.Assign<Heartbeat>(entity, kept.value_or(ecs::heartbeat::State {}));
		}
	}

	const bool measure = heartbeat::IsMeasureTurn(turn);
	std::vector<TownSeen> towns;
	uint32_t worldPopulation = 0;
	float totalInfluence = 0.0f;
	if (measure)
	{
		towns = SeeTowns(registry);
		registry.Each<const Villager>([&worldPopulation](const Villager& /*unused*/) { ++worldPopulation; });
		// The influence of the players that play and of the neutral player counts towards everyone's. A player that
		// nobody plays does not count.
		if (Locator::influenceSystem::has_value())
		{
			registry.Each<const Player>([&totalInfluence](const Player& player) {
				if (player.name == PlayerNames::NEUTRAL ||
				    (Locator::playerSystem::has_value() && Locator::playerSystem::value().PlaysInGame(player.name)))
				{
					totalInfluence += Locator::influenceSystem::value().InfluencePower(player.name);
				}
			});
		}
	}
	constexpr float k_SecondsPerMillisecond = 0.001f;
	const float secondsPerTurn = static_cast<float>(TimeSystemInterface::k_TurnDuration.count()) * k_SecondsPerMillisecond;
	std::vector<uint32_t> creatureDistances;
	for (const auto entity : players)
	{
		const auto name = registry.Get<const Player>(entity).name;
		auto& state = registry.Get<Heartbeat>(entity).state;
		if (measure)
		{
			heartbeat::Measure(state, DangerOf(registry, name, towns, worldPopulation, totalInfluence, creatureDistances));
		}
		state.destructionBeats = heartbeat::HeldBeats(state.destructionBeats, DestructionBeatsOf(registry, name));
		heartbeat::Step(state, secondsPerTurn);
	}
	SoundHeart();
}

void HeartbeatSystem::SoundHeart()
{
	if (!Locator::playerSystem::has_value() || !Locator::audio::has_value())
	{
		return;
	}
	const auto player =
	    (Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE);
	const auto state = StateOf(player);
	auto& registry = Locator::entitiesRegistry::value();
	std::optional<glm::vec3> place;
	std::optional<uint32_t> pitch;
	registry.Each<const Temple, const Transform>([&](entt::entity temple, const Temple& owner, const Transform& at) {
		if (owner.owner != player || place.has_value() || !state.has_value())
		{
			return;
		}
		const auto* progress = registry.TryGet<const BuildProgress>(temple);
		const auto* life = registry.TryGet<const ObjectLife>(temple);
		place = at.position;
		pitch = heartbeat::SoundPitchPercent(*state, progress == nullptr || progress->built >= 1.0f,
		                                     life != nullptr ? life->life : 1.0f);
	});
	if (!pitch.has_value())
	{
		return;
	}
	// The heart sounds from the land at its temple, heard fully to a third of its reach. The bank makes it loop, and a
	// heartbeat that plays already is not started again. Each turn it beats at the rate of the heart.
	if (Locator::terrainSystem::has_value())
	{
		place->y = Locator::terrainSystem::value().GetHeightAt(glm::xz(*place));
	}
	auto& audio = Locator::audio::value();
	if (!audio.EmitterExists(_sound))
	{
		_sound = audio::StartGameSoundEffect(static_cast<entt::id_type>(audio::SoundId::G_Heartbeat_01),
		                                     {.position = place,
		                                      .pitchPercent = pitch,
		                                      .playType = audio::PlayType::Repeat,
		                                      .voicePlayType = pack::AudioBankLoop::Once,
		                                      .voiceOwner = audio::VoiceOwner::Number,
		                                      .ownerNumber = static_cast<uint32_t>(player),
		                                      .minDistance = heartbeat::k_SoundReach * guidance::k_VoiceNearShare,
		                                      .maxDistance = heartbeat::k_SoundReach,
		                                      .callerWins = audio::k_CallerDistancesWin});
	}
	audio.SetEmitterPitch(_sound, *pitch);
}

float HeartbeatSystem::Pulse(PlayerNames player, float turnFraction) const
{
	const auto state = StateOf(player);
	return state.has_value() ? heartbeat::Pulse(*state, turnFraction) : 0.0f;
}

std::optional<ecs::heartbeat::State> HeartbeatSystem::StateOf(PlayerNames player) const
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	std::optional<heartbeat::State> found;
	Locator::entitiesRegistry::value().Each<const Player, const Heartbeat>(
	    [&found, player](const Player& owner, const Heartbeat& heartbeat) {
		    if (owner.name == player)
		    {
			    found = heartbeat.state;
		    }
	    });
	return found;
}

void HeartbeatSystem::KeepForNextLand()
{
	_kept.Forget();
	Locator::entitiesRegistry::value().Each<const Player, const Heartbeat>(
	    [this](const Player& player, const Heartbeat& heartbeat) { _kept.Keep(player.name, heartbeat.state); });
}

void HeartbeatSystem::ForgetKept()
{
	_kept.Forget();
}

void HeartbeatSystem::EndDestruction(PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const Player, Heartbeat>([player](const Player& owner, Heartbeat& heartbeat) {
		if (owner.name == player)
		{
			heartbeat.state.destructionBeats = 0.0f;
		}
	});
}
