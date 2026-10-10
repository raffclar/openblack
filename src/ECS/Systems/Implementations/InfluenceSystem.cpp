/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "InfluenceSystem.h"

#include <cmath>

#include <algorithm>
#include <chrono>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/Sound.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
/// Where an influence of its own is: about the object it goes with while there is one, otherwise where it was put
glm::vec3 SourcePosition(const ecs::Registry& registry, const InfluenceSource& source, const Transform& transform)
{
	if (source.follows != entt::null && registry.Valid(source.follows))
	{
		if (const auto* followed = registry.TryGet<const Transform>(source.follows); followed != nullptr)
		{
			return followed->position;
		}
	}
	return transform.position;
}

/// The border is drawn again only on every tenth turn, and only once a reach has moved by more than this
constexpr uint32_t k_RedrawTurns = 10;
constexpr float k_RedrawReach = 0.01f;

/// What the story gives for a land of the story, from the five lands' values and the one after them
float ForLand(const std::array<float, 5>& story, float after, int32_t land)
{
	if (land >= 1 && land <= static_cast<int32_t>(story.size()))
	{
		return story.at(static_cast<size_t>(land - 1));
	}
	return after;
}

const MapScriptGlobals& Globals()
{
	return Locator::entitiesRegistry::value().Context().mapScriptGlobals;
}

/// A building's own info, by its number and its mesh, else the first of its tribe
const GAbodeInfo* AbodeInfoOf(const Abode& abode, entt::id_type mesh, Tribe tribe)
{
	const GAbodeInfo* byTribe = nullptr;
	for (const auto& info : Locator::infoConstants::value().abode)
	{
		if (info.abodeNumber != abode.type)
		{
			continue;
		}
		if (resources::HashIdentifier(info.meshId) == mesh)
		{
			return &info;
		}
		if (byTribe == nullptr && info.tribeType == tribe)
		{
			byTribe = &info;
		}
	}
	return byTribe;
}

/// A building adds its own influence, by its size, once for itself and once for each of its people
float AbodeInfluence(entt::entity entity, const Abode& abode, Tribe tribe)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mesh = registry.TryGet<const Mesh>(entity);
	const auto* info = AbodeInfoOf(abode, mesh != nullptr ? mesh->id : 0, tribe);
	if (info == nullptr)
	{
		return 0.0f;
	}
	const auto* transform = registry.TryGet<const Transform>(entity);
	const float scale = transform != nullptr ? transform->scale.x : 1.0f;
	const auto people = std::ranges::count_if(
	    abode.inhabitants, [&registry](entt::entity villager) { return registry.AnyOf<Villager>(villager); });
	return scale * info->influence * static_cast<float>(people + 1);
}

/// The first temple of each player, which is their citadel
std::array<entt::entity, static_cast<size_t>(PlayerNames::_COUNT)> Citadels()
{
	std::array<entt::entity, static_cast<size_t>(PlayerNames::_COUNT)> citadels {};
	citadels.fill(entt::null);
	Locator::entitiesRegistry::value().Each<const Temple>([&citadels](entt::entity entity, const Temple& temple) {
		const auto index = static_cast<size_t>(temple.owner);
		if (index < citadels.size() && citadels.at(index) == entt::null)
		{
			citadels.at(index) = entity;
		}
	});
	return citadels;
}

/// How far a citadel reaches: its heart's reach for the land, fixed the first time, times the land's multiplier
float CitadelReach(entt::entity temple)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* stored = registry.TryGet<const CitadelInfluence>(temple);
	if (stored == nullptr)
	{
		const auto& heart = Locator::infoConstants::value().citadelHeart;
		const auto land = Globals().landNumber;
		const float reach = land != 0 ? ForLand(heart.storyInfluence, heart.transferedDamageMultiplier, land) : heart.influence;
		stored = &registry.Assign<CitadelInfluence>(temple, reach);
	}
	return Globals().playerInfluenceMultiplier * stored->reach;
}

/// The player's hand in the world, if it is in it
std::optional<glm::vec3> HandPosition()
{
	if (!Locator::handSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto hand = Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
	const auto* transform = registry.TryGet<const Transform>(hand);
	if (transform == nullptr || transform->position == glm::vec3(0.0f))
	{
		return std::nullopt;
	}
	return transform->position;
}

/// Where a player's hand is in the world: this computer's player's is the hand the mouse moves. The computer's gods
/// have no hand in the world yet, so theirs keep nothing past the border.
std::optional<glm::vec3> HandOf(PlayerNames player)
{
	if (!Locator::playerSystem::has_value() || Locator::playerSystem::value().GetLocalPlayer() != player)
	{
		return std::nullopt;
	}
	return HandPosition();
}

/// A player's entity on this land, if they are on it
std::optional<entt::entity> PlayerEntityOf(PlayerNames player)
{
	std::optional<entt::entity> found;
	Locator::entitiesRegistry::value().Each<const Player>([&found, player](entt::entity entity, const Player& each) {
		if (each.name == player)
		{
			found = entity;
		}
	});
	return found;
}

influence::Ground LandHeight()
{
	return [](glm::vec2 point) {
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
	};
}
} // namespace

void InfluenceSystem::Reset()
{
	_circles.clear();
	_ripples.clear();
	_borderShown.fill(false);
	_bordersDirty = true;
}

void InfluenceSystem::NoteReach(float reach, float drawn)
{
	if (std::fabs(reach - drawn) > k_RedrawReach)
	{
		_bordersDirty = true;
	}
}

void InfluenceSystem::ProcessTowns()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::unordered_map<uint32_t, Tribe> tribes;
	registry.Each<const Town, const Tribe>([&](const Town& town, const Tribe& tribe) { tribes.emplace(town.id, tribe); });
	std::unordered_map<uint32_t, float> buildings;
	registry.Each<const Abode>([&](entt::entity entity, const Abode& abode) {
		if (const auto tribe = tribes.find(abode.townId); tribe != tribes.end())
		{
			buildings[abode.townId] += AbodeInfluence(entity, abode, tribe->second);
		}
	});
	const auto& info = Locator::infoConstants::value().town;
	const auto land = Globals().landNumber;
	const float base = land != 0 ? ForLand(info.storyInfluence, info.maxForTimeWillWorkUntil, land) : info.influence;
	const float multiplier = Globals().townInfluenceMultiplier;
	registry.Each<const Town>([&](entt::entity entity, const Town& town) {
		auto& influence = registry.AnyOf<TownInfluence>(entity) ? registry.Get<TownInfluence>(entity)
		                                                        : registry.Assign<TownInfluence>(entity);
		influence.radius = (base + buildings[town.id]) * multiplier;
		NoteReach(influence.radius, influence.drawnRadius);
	});
}

void InfluenceSystem::ProcessCitadels()
{
	const auto citadels = Citadels();
	auto& registry = Locator::entitiesRegistry::value();
	for (size_t player = 0; player < citadels.size(); ++player)
	{
		const auto citadel = citadels.at(player);
		if (citadel == entt::null)
		{
			continue;
		}
		// A player's border shows once their citadel stands
		if (player < _borderShown.size())
		{
			_borderShown.at(player) = true;
		}
		const float reach = CitadelReach(citadel);
		NoteReach(reach, registry.Get<const CitadelInfluence>(citadel).drawnRadius);
	}
}

void InfluenceSystem::DrawBorders()
{
	_circles.clear();
	auto& registry = Locator::entitiesRegistry::value();
	const auto citadels = Citadels();
	const auto ground = LandHeight();
	// Every player's but the neutral one's: a circle about their citadel, then one about each of their towns
	for (size_t player = 0; player < static_cast<size_t>(PlayerNames::NEUTRAL); ++player)
	{
		const auto name = static_cast<PlayerNames>(player);
		if (const auto citadel = citadels.at(player); citadel != entt::null)
		{
			const float reach = CitadelReach(citadel);
			if (const auto* transform = registry.TryGet<const Transform>(citadel); transform != nullptr && reach != 0.0f)
			{
				influence::AddCircle(_circles, name, transform->position, reach, ground);
			}
			registry.Get<CitadelInfluence>(citadel).drawnRadius = reach;
		}
		registry.Each<const Town, TownInfluence, const Transform>(
		    [&](const Town& town, TownInfluence& influence, const Transform& transform) {
			    if (town.owner != name)
			    {
				    return;
			    }
			    if (influence.radius != 0.0f)
			    {
				    influence::AddCircle(_circles, name, transform.position, influence.radius, ground);
			    }
			    influence.drawnRadius = influence.radius;
		    });
		registry.Each<const InfluenceSource, const Transform>([&](const InfluenceSource& source, const Transform& transform) {
			if (source.player == name && source.radius > 0.0f && !source.anti)
			{
				influence::AddCircle(_circles, name, SourcePosition(registry, source, transform), source.radius, ground);
			}
		});
	}
	_bordersDirty = false;
}

void InfluenceSystem::ProcessTurn(uint32_t turn)
{
	ProcessTowns();
	ProcessCitadels();
	if (_bordersDirty && turn % k_RedrawTurns == 0)
	{
		DrawBorders();
	}
	ProcessVirtualInfluence(turn);
}

bool InfluenceSystem::Shielded(PlayerNames player, const glm::vec3& point)
{
	return Shielded(player, map_coords::FromMetres({point.x, point.z}));
}

bool InfluenceSystem::Shielded(PlayerNames player, const map_coords::MapCoords& position)
{
	bool shielded = false;
	Locator::entitiesRegistry::value().Each<const AntiInfluence, const Transform>(
	    [&](const AntiInfluence& ring, const Transform& transform) {
		    shielded =
		        shielded || (ring.owner != player &&
		                     gutils::GetDistanceInMetres(map_coords::FromMetres({transform.position.x, transform.position.z}),
		                                                 position) < ring.radius);
	    });
	return shielded;
}

float InfluenceSystem::InfluencePower(PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	float power = 0.0f;
	if (const auto citadel = Citadels().at(static_cast<size_t>(player)); citadel != entt::null)
	{
		power = CitadelReach(citadel);
	}
	registry.Each<const Town, const TownInfluence>([&](const Town& town, const TownInfluence& influence) {
		if (town.owner == player)
		{
			power += influence.radius;
		}
	});
	registry.Each<const InfluenceSource>([&](const InfluenceSource& source) {
		if (source.player == player && !source.anti)
		{
			power += source.radius;
		}
	});
	return power;
}

void InfluenceSystem::ProcessVirtualInfluence(uint32_t turn)
{
	_turn = turn;
	if (!Locator::playerSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	// Every player's hand keeps a share of their influence past the border, the computer's gods' too
	std::vector<std::pair<entt::entity, PlayerNames>> players;
	Locator::entitiesRegistry::value().Each<const Player>(
	    [&players](entt::entity entity, const Player& player) { players.emplace_back(entity, player.name); });
	for (const auto& [entity, player] : players)
	{
		if (const auto hand = HandOf(player); hand.has_value())
		{
			ProcessVirtualInfluence(entity, player, *hand, turn);
		}
	}
}

void InfluenceSystem::HeldThingUsedOnLand(PlayerNames player)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	// Measured from where the hand was at the last turn
	const auto* state = VirtualStateOf(player);
	const auto entity = PlayerEntityOf(player);
	if (state == nullptr || !state->turnHand.has_value() || !entity.has_value())
	{
		return;
	}
	const auto hand = *state->turnHand;
	ProcessVirtualInfluence(*entity, player, hand, _turn);
}

void InfluenceSystem::ProcessVirtualInfluence(entt::entity entity, PlayerNames player, glm::vec3 hand, uint32_t turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& virtualInfluence = registry.AnyOf<VirtualInfluence>(entity) ? registry.Get<VirtualInfluence>(entity)
	                                                                  : registry.Assign<VirtualInfluence>(entity);
	const auto& citadel = Locator::infoConstants::value().citadel;
	const bool shielded = Shielded(player, hand);
	// Only the player's own influence counts here, not what the hand keeps
	const bool inInfluence = !shielded && PlayerRawInfluence(player, hand) > 0.0f;
	const auto before = virtualInfluence.state;
	// TODO(raffclar): the chants waiting at the player's worship sites, which slow the waning and are used up by it, once
	// worship sites gather chants; until then there are none
	virtual_influence::ProcessTurn(virtualInfluence.state,
	                               {
	                                   .hand = hand,
	                                   .turn = turn,
	                                   .handShielded = shielded,
	                                   .handInInfluence = inInfluence,
	                                   .influencePower = InfluencePower(player),
	                                   .chants = 0.0f,
	                               },
	                               {
	                                   .maxDistance = citadel.virtualInfluenceMaxDistance,
	                                   .maxTurns = citadel.virtualInfluenceMaxGameTicks,
	                                   .chantsToDouble = citadel.virtualInfluenceChantsToDouble,
	                               });
	// The log follows the strength the hand keeps past the border, a tenth at a time
	const auto& after = virtualInfluence.state;
	if (static_cast<int>(before.fraction * 10.0f) != static_cast<int>(after.fraction * 10.0f) ||
	    before.anchor.has_value() != after.anchor.has_value())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Player {}'s hand {} their influence at ({:.1f}, {:.1f}), strength {:.2f}",
		                    static_cast<int>(player), inInfluence ? "is in" : (shielded ? "is shielded from" : "is out of"),
		                    hand.x, hand.z, after.fraction);
	}
}

const virtual_influence::State* InfluenceSystem::VirtualStateOf(PlayerNames player)
{
	const auto entity = PlayerEntityOf(player);
	if (!entity.has_value())
	{
		return nullptr;
	}
	const auto* virtualInfluence = Locator::entitiesRegistry::value().TryGet<const VirtualInfluence>(*entity);
	return virtualInfluence != nullptr ? &virtualInfluence->state : nullptr;
}

void InfluenceSystem::ShowHandInfluence(std::chrono::duration<float, std::milli> gameTime)
{
	if (!Locator::playerSystem::has_value() || !Locator::audio::has_value())
	{
		return;
	}
	const auto player = Locator::playerSystem::value().GetLocalPlayer();
	const auto entity = Locator::playerSystem::value().GetPlayer(player);
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AnyOf<VirtualInfluence>(entity))
	{
		return;
	}
	auto& virtualInfluence = registry.Get<VirtualInfluence>(entity);
	auto& state = virtualInfluence.state;
	auto& audio = Locator::audio::value();
	const auto hand = HandPosition();
	const auto inInfluence = [this, player](const glm::vec3& point) { return PlayerRawInfluence(player, point) > 0.0f; };
	const bool plays = hand.has_value() && state.anchor.has_value() &&
	                   virtual_influence::HumPlays(state, Shielded(player, *hand), inInfluence(*hand),
	                                               Shielded(player, *state.anchor), inInfluence(*state.anchor));
	if (!plays)
	{
		if (state.soundStarted)
		{
			// A land change takes the sound with it
			if (audio.EmitterExists(virtualInfluence.hum))
			{
				audio.DestroyEmitter(virtualInfluence.hum);
			}
			virtualInfluence.hum = entt::null;
			state.soundStarted = false;
		}
		return;
	}
	// Past the border the hand hums, heard alike from both sides, lower the less of its strength is left; it is started
	// again whenever it has played out
	if (!state.soundStarted)
	{
		state.soundFraction = state.fraction;
		state.soundStarted = true;
	}
	// The mana path runs from the hand back to halfway to where it last was in influence, worked out once for each trip
	// out, its sparks in the player's colour dimmed by the strength left
	const auto handCoords = map_coords::FromMetres({hand->x, hand->z});
	if (!state.manaPathStart.has_value())
	{
		state.manaPathStart =
		    virtual_influence::ManaPathStart(handCoords, map_coords::FromMetres({state.anchor->x, state.anchor->z}));
	}
	if (const auto scale = virtual_influence::EmitManaPath(state, gameTime.count());
	    scale.has_value() && Locator::particleSystem::has_value() && Locator::terrainSystem::has_value())
	{
		const auto& land = Locator::terrainSystem::value();
		const auto colour = Player::k_Colours.at(static_cast<size_t>(player) & (Player::k_Colours.size() - 1));
		Locator::particleSystem::value().AddHandManaPathSpark({
		    .from = map_coords::ToWorld(land, handCoords),
		    .to = map_coords::ToWorld(land, *state.manaPathStart),
		    .rgb = virtual_influence::ScaleColour(colour, *scale) & 0xFFFFFFu,
		});
	}
	const auto pitch = virtual_influence::HumPitchPercent(state);
	if (!audio.EmitterExists(virtualInfluence.hum))
	{
		virtualInfluence.hum =
		    audio.StartSoundEffect(static_cast<entt::id_type>(audio::SoundId::G_VirtualInfluence_04), {.pitchPercent = pitch});
	}
	audio.SetEmitterPitch(virtualInfluence.hum, pitch);
}

void InfluenceSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	_scrollRemainder += gameTime.count();
	const auto step = static_cast<int32_t>(_scrollRemainder);
	_scrollRemainder -= static_cast<float>(step);
	_scrollClock = (_scrollClock + step) % influence::k_ScrollWrap;

	std::erase_if(_ripples,
	              [&gameTime](influence::Ripple& ripple) { return !influence::AdvanceRipple(ripple, gameTime.count()); });

	// While the game runs, the hand crossing a border sounds once, at the hand
	if (Locator::time::value().IsPaused())
	{
		return;
	}
	if (const auto hand = HandPosition(); hand.has_value() && CrossBorders(*hand) && Locator::soundTagSystem::has_value())
	{
		Locator::soundTagSystem::value().CreatePointSound(static_cast<entt::id_type>(audio::SoundId::G_HandThroughInfluence_01),
		                                                  *hand, false);
	}
}

bool InfluenceSystem::CrossBorders(const glm::vec3& hand)
{
	std::array<bool, k_Players> inside {};
	for (const auto& circle : _circles)
	{
		if (influence::Inside(circle.centre, circle.radius, hand))
		{
			inside.at(static_cast<size_t>(circle.player)) = true;
		}
	}
	bool crossed = false;
	if (!_handSeen)
	{
		// The first time, only where it is
		_handWasInside = inside;
		_handSeen = true;
	}
	else
	{
		for (size_t player = 0; player < k_Players; ++player)
		{
			if (inside.at(player) == _handWasInside.at(player))
			{
				continue;
			}
			_handWasInside.at(player) = inside.at(player);
			// The circle whose edge the hand went over: none if the border moved under a still hand
			const auto circle = std::ranges::find_if(_circles, [&](const influence::Circle& c) {
				return static_cast<size_t>(c.player) == player &&
				       influence::Inside(c.centre, c.radius, _handBefore) != influence::Inside(c.centre, c.radius, hand);
			});
			if (circle == _circles.end() || !IsBorderShown(circle->player))
			{
				continue;
			}
			_ripples.insert(_ripples.begin(),
			                influence::MakeRipple(*circle, _handBefore, hand, LandHeight(), [](float a, float b) {
				                return Locator::gameRandom::value().CrtRandom(a, b);
			                }));
			crossed = true;
		}
	}
	_handBefore = {hand.x, 0.0f, hand.z};
	return crossed;
}

float InfluenceSystem::PlayerInfluence(PlayerNames player, const map_coords::MapCoords& position) const
{
	if (const auto* state = VirtualStateOf(player); state != nullptr)
	{
		// The questions of a game turn measure from where the hand was at the turn; between turns this computer's
		// player's are measured from where their hand is now
		auto hand = state->turnHand;
		if (!_inGameTurn && Locator::playerSystem::has_value() && Locator::playerSystem::value().GetLocalPlayer() == player)
		{
			if (const auto live = HandPosition(); live.has_value())
			{
				hand = live;
			}
		}
		if (hand.has_value() && !Shielded(player, position))
		{
			if (const auto granted = virtual_influence::Grant(*state, map_coords::FromMetres({hand->x, hand->z}), position);
			    granted.has_value())
			{
				return *granted;
			}
		}
	}
	return PlayerRawInfluence(player, position);
}

float InfluenceSystem::HandPointInfluence(PlayerNames player, const map_coords::MapCoords& hand) const
{
	// The place asked is the hand's own, so the hand is inside while it keeps anything
	if (const auto* state = VirtualStateOf(player); state != nullptr && !Shielded(player, hand))
	{
		if (const auto granted = virtual_influence::Grant(*state, hand, hand); granted.has_value())
		{
			return *granted;
		}
	}
	return PlayerRawInfluence(player, hand);
}

float InfluenceSystem::PlayerRawInfluence(PlayerNames player, const map_coords::MapCoords& position) const
{
	auto& registry = Locator::entitiesRegistry::value();
	// Each is measured from its map position, in the game's map units
	const auto distanceTo = [&position](const glm::vec3& point) {
		return gutils::GetDistanceInMetres(map_coords::FromMetres({point.x, point.z}), position);
	};
	// Under another player's shield a player has no influence at all, nor within an anti-influence a script made for them
	if (Shielded(player, position))
	{
		return 0.0f;
	}
	bool anti = false;
	registry.Each<const InfluenceSource, const Transform>([&](const InfluenceSource& source, const Transform& transform) {
		anti = anti || (source.anti && source.player == player &&
		                distanceTo(SourcePosition(registry, source, transform)) <= source.radius);
	});
	if (anti)
	{
		return 0.0f;
	}
	float sum = 0.0f;
	// The citadel's reach, where it reaches
	if (const auto citadel = Citadels().at(static_cast<size_t>(player)); citadel != entt::null)
	{
		const float reach = CitadelReach(citadel);
		if (const auto* transform = registry.TryGet<const Transform>(citadel);
		    transform != nullptr && distanceTo(transform->position) < reach)
		{
			sum = reach;
		}
	}
	// And each of the player's towns', where it reaches
	registry.Each<const Town, const TownInfluence, const Transform>(
	    [&](const Town& town, const TownInfluence& influence, const Transform& transform) {
		    if (town.owner == player && distanceTo(transform.position) < influence.radius)
		    {
			    sum += influence.radius;
		    }
	    });
	// And any other source of the player's, where it reaches
	registry.Each<const InfluenceSource, const Transform>([&](const InfluenceSource& source, const Transform& transform) {
		if (source.player == player && !source.anti && distanceTo(SourcePosition(registry, source, transform)) < source.radius)
		{
			sum += source.radius;
		}
	});
	return std::clamp(sum, -1.0f, 1.0f);
}

bool InfluenceSystem::IsBorderShown(PlayerNames player) const
{
	const auto index = static_cast<size_t>(player);
	return index < _borderShown.size() && _borderShown.at(index);
}
