/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ComputerMindWorld.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include <LNDFile.h>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipChants.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ComputerMindRules.h"
#include "ECS/PlayerMiracles.h"
#include "ECS/Registry.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/SpellRules.h"
#include "Magic/WorshipBattery.h"
#include "VillagerHome.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::computer_world;
using computer_hand::Argument;
using computer_hand::StepKind;
using computer_mind::NodeKind;

namespace
{

/// The distance in metres from the hand to an icon at which the god draws the miracle's gesture rather than moving the
/// hand to the icon
constexpr float k_GestureDistance = 200.0f;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// The map position of a thing that is still in the world
std::optional<map_coords::MapCoords> CoordsOfThing(entt::entity thing)
{
	auto& registry = Entities();
	if (thing == entt::null || !registry.Valid(thing) || !Locator::terrainSystem::has_value())
	{
		return std::nullopt;
	}
	const auto* transform = registry.TryGet<const Transform>(thing);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	return map_coords::FromWorld(Locator::terrainSystem::value(), transform->position);
}

/// The plain magic type of a miracle seed, the one with no power-up
MagicType PlainType(SpellSeedType seed)
{
	if (!Locator::infoConstants::has_value() || seed == SpellSeedType::None)
	{
		return MagicType::None;
	}
	const auto& info = Locator::infoConstants::value();
	return magic::GetMagicTypeFromPowerUpLevel(magic::GetSpellSeedInfo(info, seed), magic::k_BasePowerUpLevel);
}

/// The worship sites around the player's temple, in the order of their places; none where a place is empty
std::array<entt::entity, CitadelWorship::k_Places> SitesOf(PlayerNames player)
{
	std::array<entt::entity, CitadelWorship::k_Places> sites {};
	sites.fill(entt::null);
	if (!Locator::worshipSiteSystem::has_value())
	{
		return sites;
	}
	auto& registry = Entities();
	const auto temple = Locator::worshipSiteSystem::value().TempleOf(player);
	if (temple == entt::null || !registry.Valid(temple))
	{
		return sites;
	}
	if (const auto* citadel = registry.TryGet<const CitadelWorship>(temple); citadel != nullptr)
	{
		for (size_t i = 0; i < sites.size(); ++i)
		{
			if (registry.Valid(citadel->sites.at(i)) && registry.AllOf<WorshipSite>(citadel->sites.at(i)))
			{
				sites.at(i) = citadel->sites.at(i);
			}
		}
	}
	return sites;
}

/// The icons of a worship site, in their order
std::vector<entt::entity> IconsOf(entt::entity site)
{
	auto& registry = Entities();
	std::vector<entt::entity> icons;
	if (const auto* list = registry.TryGet<const SiteIcons>(site); list != nullptr)
	{
		std::ranges::copy_if(list->icons, std::back_inserter(icons), [&registry](entt::entity icon) {
			return registry.Valid(icon) && registry.AllOf<WorshipIcon>(icon);
		});
	}
	return icons;
}

/// The first icon of a site that holds the first seed of the magic type
entt::entity SiteIconFor(entt::entity site, MagicType type)
{
	if (!Locator::infoConstants::has_value())
	{
		return entt::null;
	}
	const auto seed = magic::FindFirstSpellSeedForMagicType(Locator::infoConstants::value(), type);
	if (!seed.has_value())
	{
		return entt::null;
	}
	for (const auto icon : IconsOf(site))
	{
		if (Entities().Get<const WorshipIcon>(icon).seed == *seed)
		{
			return icon;
		}
	}
	return entt::null;
}

/// The site has its building done and dancers at its altar
bool BuiltWithDancers(entt::entity site)
{
	auto& sites = Locator::worshipSiteSystem::value();
	return sites.IsBuilt(site) && sites.Dancers(site) != 0;
}

/// The site has its life full again
bool Repaired(entt::entity site)
{
	return ecs::world_objects::LifeOf(site) >= 1.0f;
}

/// What the god knows of an icon and its site
computer_mind_rules::IconFacts FactsOf(entt::entity icon)
{
	auto& registry = Entities();
	const auto& component = registry.Get<const WorshipIcon>(icon);
	computer_mind_rules::IconFacts facts {.store = component.store};
	if (Locator::infoConstants::has_value())
	{
		facts.cost = magic::GetChantsRequiredToCreate(Locator::infoConstants::value(), PlainType(component.seed));
	}
	const auto* chants = registry.Valid(component.site) ? registry.TryGet<const WorshipChants>(component.site) : nullptr;
	if (chants != nullptr && Locator::worshipSiteSystem::has_value() && Locator::infoConstants::has_value())
	{
		const auto& site = registry.Get<const WorshipSite>(component.site);
		facts.site = true;
		facts.infinite = chants->infinite;
		facts.battery = chants->battery;
		facts.available = chants->available;
		facts.used = chants->used;
		const auto dancers = Locator::worshipSiteSystem::value().Dancers(component.site);
		// The dancers chant with the player's Aztec power, whatever the site's tribe
		const auto& rows = Locator::infoConstants::value().worshipSite;
		const auto row = static_cast<size_t>(site.tribe);
		const float power =
		    Locator::magicSystem::has_value() ? Locator::magicSystem::value().GetTribalPower(site.player, Tribe::AZTEC) : 1.0f;
		if (row < rows.size())
		{
			facts.capacity = magic::WorshipCapacity(magic::WorshipBatteryRulesFor(rows.at(row), power), dancers);
		}
	}
	return facts;
}

/// The towns of the player, in the order that the player gained them
std::vector<entt::entity> TownsOf(PlayerNames player)
{
	auto& registry = Entities();
	std::vector<std::pair<uint32_t, entt::entity>> towns;
	registry.Each<const Town>([&towns, player](entt::entity entity, const Town& town) {
		if (town.owner == player)
		{
			towns.emplace_back(town.gained, entity);
		}
	});
	std::ranges::sort(towns, {}, &std::pair<uint32_t, entt::entity>::first);
	std::vector<entt::entity> ordered;
	std::ranges::transform(towns, std::back_inserter(ordered), &std::pair<uint32_t, entt::entity>::second);
	return ordered;
}

/// The storage pit of the town that works, none when it has none
std::optional<entt::entity> WorkingPitOf(entt::entity town)
{
	const auto pit = ecs::construction::StoragePitOf(town);
	return pit.has_value() && ecs::villager_home::IsFunctional(*pit) ? pit : std::nullopt;
}

/// Where the town keeps food or wood: its storage pit that works, else its temporary pile, which is made if missing
entt::entity StoreOf(entt::entity town, ResourceType resource)
{
	if (const auto pit = WorkingPitOf(town))
	{
		return *pit;
	}
	if (!Locator::townSystem::has_value())
	{
		return entt::null;
	}
	glm::vec2 from {0.0f};
	if (const auto* transform = Entities().TryGet<const Transform>(town); transform != nullptr)
	{
		from = {transform->position.x, transform->position.z};
	}
	const auto pile = Locator::townSystem::value().TemporaryStorePile(town, from, resource);
	return pile.has_value() ? pile->pile : entt::null;
}

/// How much the town wants a thing: what its people want, and the two boosts
float DesireOf(entt::entity town, TownDesireInfo desire)
{
	const auto* desires = Entities().TryGet<const TownDesire>(town);
	if (desires == nullptr)
	{
		return 0.0f;
	}
	const auto index = static_cast<size_t>(desire);
	return desires->desire.at(index) + desires->boost.at(index) + desires->boostA.at(index);
}

} // namespace

GameHandWorld::GameHandWorld(PlayerNames player, ComputerPlayer& state)
    : _player(player)
    , _state(state)
{
}

std::optional<map_coords::MapCoords> GameHandWorld::CoordsOf(entt::entity thing) const
{
	return CoordsOfThing(thing);
}

float GameHandWorld::HeightOf(entt::entity thing) const
{
	return Entities().Valid(thing) ? ecs::world_objects::SizeOf(thing).height : 0.0f;
}

float GameHandWorld::LandHeight(const map_coords::MapCoords& coords) const
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetAltitude(coords.x, coords.z) : 0.0f;
}

bool GameHandWorld::IconHasSite(entt::entity icon) const
{
	auto& registry = Entities();
	const auto* component = registry.Valid(icon) ? registry.TryGet<const WorshipIcon>(icon) : nullptr;
	return component != nullptr && registry.Valid(component->site) && registry.AllOf<WorshipSite>(component->site);
}

bool GameHandWorld::IconCharging(entt::entity icon) const
{
	auto& registry = Entities();
	const auto* component = registry.Valid(icon) ? registry.TryGet<const WorshipIcon>(icon) : nullptr;
	return component != nullptr && component->charging;
}

bool GameHandWorld::StartChant(entt::entity icon, MagicType /*type*/)
{
	return Locator::spellIconSystem::has_value() && Locator::spellIconSystem::value().StartChargeForGod(icon, _player);
}

float GameHandWorld::ChantNeeded(entt::entity icon) const
{
	return Locator::spellIconSystem::has_value() ? Locator::spellIconSystem::value().ChantNeeded(icon) : 0.0f;
}

bool GameHandWorld::Angry(MagicType type) const
{
	return Locator::infoConstants::has_value() &&
	       magic::GetMagicEffectInfo(Locator::infoConstants::value(), type).perceivedPlayerDesire.at(0) ==
	           CreatureDesires::Anger;
}

bool GameHandWorld::Cast(MagicType type, entt::entity target, entt::entity icon, glm::vec3 hand)
{
	if (!Locator::infoConstants::has_value() || !Locator::magicSystem::has_value())
	{
		return false;
	}
	// The miracle must be charged full
	if (icon != entt::null && ChantNeeded(icon) > 0.0f)
	{
		return false;
	}
	const auto targetCoords = CoordsOfThing(target);
	if (!targetCoords.has_value())
	{
		return false;
	}
	// A food or wood miracle lands only on land
	if (type == MagicType::Food || type == MagicType::Wood)
	{
		const auto* cell = Locator::terrainSystem::value().FindCell(glm::u16vec2(map_coords::Cell(*targetCoords)));
		if (cell == nullptr || cell->properties.hasWater != 0)
		{
			return false;
		}
	}
	const auto& info = Locator::infoConstants::value();
	const auto& effect = magic::GetMagicEffectInfo(info, type);
	const map_coords::MapCoords handCoords {
	    .x = map_coords::ToFixed(hand.x), .z = map_coords::ToFixed(hand.z), .altitude = hand.y};
	const auto from = glm::vec3(map_coords::ToMetres(handCoords.x), handCoords.altitude, map_coords::ToMetres(handCoords.z));
	const auto at =
	    glm::vec3(map_coords::ToMetres(targetCoords->x), targetCoords->altitude, map_coords::ToMetres(targetCoords->z));
	const magic::SpellCastData cast {.magnitude = 1.0f,
	                                 .chants = effect.initialChants,
	                                 .duration = effect.timerWhenComputerPlayerCasting,
	                                 .maxObjectsToCreate = -1};
	const particles::ProcessInfo process {.handPosition = from,
	                                      .cameraForward = at - from,
	                                      .direction = glm::vec3(0.0f),
	                                      .power = 1.0f,
	                                      .enabled = true,
	                                      .spin = 0.0f};
	const auto seed = magic::FindFirstSpellSeedForMagicType(info, type);
	const bool onObject = seed.has_value() && magic::GetSpellSeedInfo(info, *seed).castOnObject != 0;
	auto& magicSystem = Locator::magicSystem::value();
	const auto spell = onObject ? magicSystem.CastOnObject(type, _player, target, cast, process)
	                            : magicSystem.CastAtPoint(type, _player, at, cast, process);
	if (spell == entt::null)
	{
		return false;
	}
	if (icon != entt::null && Locator::spellIconSystem::has_value())
	{
		Locator::spellIconSystem::value().TakeCastCharge(icon);
	}
	_state.casts.insert(_state.casts.begin(), spell);
	return true;
}

float GameHandWorld::CastSeconds(MagicType type) const
{
	return Locator::infoConstants::has_value()
	           ? magic::GetMagicEffectInfo(Locator::infoConstants::value(), type).computerCastDuration
	           : 0.0f;
}

GameMindWorld::GameMindWorld(PlayerNames player, ComputerPlayer& state)
    : _player(player)
    , _state(state)
    , _hand(player, state)
{
}

float GameMindWorld::Intensity(computer_mind::Mind& mind, uint16_t node)
{
	switch (mind.nodes[node].kind)
	{
	case NodeKind::ActionTownWithHighestFoodDesire:
		return TownWanting(mind, node, TownDesireInfo::ForFood);
	case NodeKind::ActionTownWithHighestWoodDesire:
		return TownWanting(mind, node, TownDesireInfo::ForWood);
	case NodeKind::ObjectShallCastFoodSpell:
		return ShallCast(mind, node, MagicType::Food, ResourceType::Food);
	case NodeKind::ObjectShallCastWoodSpell:
		return ShallCast(mind, node, MagicType::Wood, ResourceType::Wood);
	case NodeKind::ActionChargeUpSpell:
		return ChargeUpSpell(mind, node);
	default:
		// The rules of the other needs are not made yet: the god does not want them
		return 0.0f;
	}
}

bool GameMindWorld::Perform(computer_mind::Mind& mind, uint16_t node)
{
	bool started = false;
	switch (mind.nodes[node].kind)
	{
	case NodeKind::ObjectShallCastFoodSpell:
	case NodeKind::ObjectShallCastWoodSpell:
		started = CastFromIcon(mind, node);
		break;
	case NodeKind::ActionChargeUpSpell:
		started = PerformCharge(mind, node);
		break;
	default:
		break;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Computer god {} action {} ({:.3f}) started {}", static_cast<int>(_player),
	                    computer_mind::InfoOf(mind.nodes[node].kind).name, mind.nodes[0].value, started);
	return started;
}

bool GameMindWorld::ActionBusy() const
{
	return computer_hand::Busy(_state.hand);
}

bool GameMindWorld::StepAction()
{
	return computer_hand::Process(_state.hand, _hand);
}

void GameMindWorld::ClearAction()
{
	computer_hand::Clear(_state.hand);
}

void GameMindWorld::StartAction() {}

bool GameMindWorld::ActionLocked() const
{
	return _state.hand.locked;
}

void GameMindWorld::UnlockAction()
{
	_state.hand.locked = false;
}

bool GameMindWorld::ForceQueued()
{
	return false;
}

bool GameMindWorld::HeldByHelp() const
{
	return false;
}

float GameMindWorld::TownWanting(computer_mind::Mind& mind, uint16_t node, TownDesireInfo desire)
{
	// The wood need looks only at towns with a storage pit that works
	std::vector<entt::entity> towns;
	std::vector<float> desires;
	for (const auto town : TownsOf(_player))
	{
		if (desire == TownDesireInfo::ForWood && !WorkingPitOf(town).has_value())
		{
			continue;
		}
		towns.push_back(town);
		desires.push_back(DesireOf(town, desire));
	}
	const auto best = computer_mind_rules::MostWanting(desires);
	if (!best.has_value())
	{
		return 0.0f;
	}
	mind.nodes[node].object = towns[*best];
	return desires[*best];
}

float GameMindWorld::ChargeLevel(MagicType type, entt::entity& icon)
{
	icon = entt::null;
	entt::entity found = entt::null;
	for (const auto site : SitesOf(_player))
	{
		if (site != entt::null)
		{
			found = SiteIconFor(site, type);
			if (found != entt::null)
			{
				break;
			}
		}
	}
	if (found == entt::null)
	{
		return 0.0f;
	}
	++_state.mind.castRequested.at(static_cast<size_t>(type));
	const float level = computer_mind_rules::ChargeShare(FactsOf(found));
	if (level < 1.0f)
	{
		// The dancers must be able to charge the rest
		const auto site = Entities().Get<const WorshipIcon>(found).site;
		if (!Entities().Valid(site) || !BuiltWithDancers(site) || !Repaired(site))
		{
			return 0.0f;
		}
	}
	icon = found;
	return level;
}

float GameMindWorld::PlacePull(const map_coords::MapCoords& place) const
{
	if (!Locator::influenceSystem::has_value())
	{
		return 0.0f;
	}
	return Locator::influenceSystem::value().PlayerInfluence(_player, place) > 0.0f ? 1.0f : 0.0f;
}

float GameMindWorld::ShallCast(computer_mind::Mind& mind, uint16_t node, MagicType type, ResourceType resource)
{
	// The place is the store of the town above, or the place of the thing above
	std::optional<map_coords::MapCoords> place;
	entt::entity above = entt::null;
	for (auto at = mind.nodes[node].parent; at.has_value(); at = mind.nodes[*at].parent)
	{
		if (mind.nodes[*at].object != entt::null)
		{
			above = mind.nodes[*at].object;
			break;
		}
	}
	if (above != entt::null && Entities().Valid(above) && Entities().AllOf<Town>(above))
	{
		place = CoordsOfThing(StoreOf(above, resource));
	}
	else
	{
		place = CoordsOfThing(above);
	}

	const auto* player = [this]() -> const Player* {
		const Player* found = nullptr;
		Entities().Each<const Player>([&found, this](const Player& each) {
			if (each.name == _player && found == nullptr)
			{
				found = &each;
			}
		});
		return found;
	}();
	if (player == nullptr || !player_miracles::IsMagicTypeEnabled(player->miracles, type))
	{
		return 0.0f;
	}
	entt::entity icon = entt::null;
	float charge = ChargeLevel(type, icon);
	// Short of a full charge the god asks again, so each look counts two requests
	charge = charge < 1.0f ? ChargeLevel(type, icon) : 1.0f;
	if (icon == entt::null)
	{
		return 0.0f;
	}
	const float turns = computer_mind_rules::TurnsToCharge(FactsOf(icon));
	if (turns < 0.0f)
	{
		return 0.0f;
	}
	const float pull = place.has_value() ? PlacePull(*place) : 1.0f;
	mind.nodes[node].object = icon;
	return computer_mind_rules::SpellValue(charge, turns, pull);
}

float GameMindWorld::ChargeUpSpell(computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	std::vector<entt::entity> icons;
	std::vector<computer_mind_rules::ChargeCandidate> candidates;
	for (const auto site : SitesOf(_player))
	{
		if (site == entt::null)
		{
			continue;
		}
		if (BuiltWithDancers(site))
		{
			for (const auto icon : IconsOf(site))
			{
				const auto& component = registry.Get<const WorshipIcon>(icon);
				icons.push_back(icon);
				candidates.push_back(
				    {.charging = component.charging,
				     .store = component.store,
				     .requests = _state.mind.castRequested.at(static_cast<size_t>(PlainType(component.seed)))});
			}
		}
		else if (!Locator::worshipSiteSystem::value().IsBuilt(site) && Repaired(site))
		{
			// The god finishes a site that is whole at once
			Locator::worshipSiteSystem::value().BuildBy(site, 1.0f);
		}
	}
	const auto choice = computer_mind_rules::ChooseCharge(candidates);
	if (!choice.has_value())
	{
		return 0.0f;
	}
	mind.nodes[node].object = icons[choice->index];
	return choice->value;
}

void GameMindWorld::ReachIcon(entt::entity icon, MagicType type)
{
	const auto iconCoords = CoordsOfThing(icon);
	const float distance =
	    iconCoords.has_value() ? gutils::GetDistanceInMetres(_state.hand.coords, *iconCoords) : k_GestureDistance;
	if (distance < k_GestureDistance)
	{
		computer_hand::Add(_state.hand, StepKind::MoveHandToObject, {Argument {.object = icon}});
	}
	else
	{
		computer_hand::Add(_state.hand, StepKind::MakeGesture,
		                   {Argument {.number = static_cast<int32_t>(type)}, Argument {.number = 0}});
	}
}

bool GameMindWorld::CastFromIcon(computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	// The icon is the node's object, or the object of the first node above that has one
	entt::entity icon = mind.nodes[node].object;
	for (auto at = mind.nodes[node].parent; icon == entt::null && at.has_value(); at = mind.nodes[*at].parent)
	{
		icon = mind.nodes[*at].object;
	}
	entt::entity town = entt::null;
	for (auto at = mind.nodes[node].parent; at.has_value(); at = mind.nodes[*at].parent)
	{
		if (mind.nodes[*at].object != entt::null)
		{
			town = mind.nodes[*at].object;
			break;
		}
	}
	if (icon == entt::null || !registry.Valid(icon) || !registry.AllOf<WorshipIcon>(icon))
	{
		return false;
	}
	const auto type = PlainType(registry.Get<const WorshipIcon>(icon).seed);
	if (town == entt::null || !registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return false;
	}
	const auto store = StoreOf(town, type == MagicType::Wood ? ResourceType::Wood : ResourceType::Food);
	if (store == entt::null)
	{
		return false;
	}
	ReachIcon(icon, type);
	computer_hand::Add(_state.hand, StepKind::MakeWorshippersChantForSpell,
	                   {Argument {.number = static_cast<int32_t>(type)}, Argument {.object = icon}});
	// Food, wood, heal, water and forest miracles go on the thing
	computer_hand::Add(
	    _state.hand, StepKind::CastSpellAtObject,
	    {Argument {.object = store}, Argument {.number = static_cast<int32_t>(type)}, Argument {.object = icon}});
	return true;
}

bool GameMindWorld::PerformCharge(computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	entt::entity icon = mind.nodes[node].object;
	for (auto at = mind.nodes[node].parent; icon == entt::null && at.has_value(); at = mind.nodes[*at].parent)
	{
		icon = mind.nodes[*at].object;
	}
	if (icon == entt::null || !registry.Valid(icon) || !registry.AllOf<WorshipIcon>(icon))
	{
		return false;
	}
	const auto type = PlainType(registry.Get<const WorshipIcon>(icon).seed);
	ReachIcon(icon, type);
	computer_hand::Add(_state.hand, StepKind::MakeWorshippersChantForSpellNoWait,
	                   {Argument {.number = static_cast<int32_t>(type)}, Argument {.object = icon}});
	return true;
}

void computer_world::Validate(ComputerPlayer& state)
{
	auto& registry = Entities();
	std::erase_if(state.casts, [&registry](entt::entity spell) { return !registry.Valid(spell); });
	// An action whose thing has gone stops
	const bool gone = std::ranges::any_of(state.hand.steps, [&registry](const computer_hand::Step& step) {
		return std::ranges::any_of(step.arguments, [&registry](const Argument& argument) {
			return argument.object != entt::null && !registry.Valid(argument.object);
		});
	});
	if (gone)
	{
		computer_hand::Clear(state.hand);
	}
}

std::optional<glm::vec3> computer_world::StartingHand(PlayerNames player)
{
	if (!Locator::worshipSiteSystem::has_value())
	{
		return std::nullopt;
	}
	const auto coords = CoordsOfThing(Locator::worshipSiteSystem::value().TempleOf(player));
	if (!coords.has_value())
	{
		return std::nullopt;
	}
	return glm::vec3(map_coords::ToMetres(coords->x), coords->altitude, map_coords::ToMetres(coords->z));
}
