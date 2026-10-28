/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ComputerMindCommon.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "3D/LandIslandInterface.h"
#include "Commands/PlayerCommands.h"
#include "Common/GUtilsDistance.h"
#include "Common/VirtualInfluence.h"
#include "ComputerMindRuleTable.h"
#include "ECS/AllyInfluence.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipChants.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerMindRules.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/WorshipBattery.h"
#include "VillagerHome.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::computer_world;
using computer_hand::Argument;
using computer_hand::StepKind;

namespace
{

/// The distance in metres from the hand to an icon at which the god draws the miracle's gesture rather than moving the
/// hand to the icon
constexpr float k_GestureDistance = 200.0f;

} // namespace

ecs::Registry& computer_world::Entities()
{
	return Locator::entitiesRegistry::value();
}

std::optional<map_coords::MapCoords> computer_world::CoordsOfThing(entt::entity thing)
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

MagicType computer_world::PlainType(SpellSeedType seed)
{
	if (!Locator::infoConstants::has_value() || seed == SpellSeedType::None)
	{
		return MagicType::None;
	}
	const auto& info = Locator::infoConstants::value();
	return magic::GetMagicTypeFromPowerUpLevel(magic::GetSpellSeedInfo(info, seed), magic::k_BasePowerUpLevel);
}

std::array<entt::entity, CitadelWorship::k_Places> computer_world::SitesOf(PlayerNames player)
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

std::vector<entt::entity> computer_world::IconsOf(entt::entity site)
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

entt::entity computer_world::SiteIconFor(entt::entity site, MagicType type)
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

bool computer_world::BuiltWithDancers(entt::entity site)
{
	auto& sites = Locator::worshipSiteSystem::value();
	return sites.IsBuilt(site) && sites.Dancers(site) != 0;
}

bool computer_world::Repaired(entt::entity site)
{
	return ecs::world_objects::LifeOf(site) >= 1.0f;
}

computer_mind_rules::IconFacts computer_world::FactsOf(entt::entity icon)
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

std::vector<entt::entity> computer_world::TownsOf(PlayerNames player)
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

std::optional<entt::entity> computer_world::WorkingPitOf(entt::entity town)
{
	const auto pit = ecs::construction::StoragePitOf(town);
	return pit.has_value() && ecs::villager_home::IsFunctional(*pit) ? pit : std::nullopt;
}

entt::entity computer_world::StoreOf(entt::entity town, ResourceType resource)
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

entt::entity computer_world::HandOf(PlayerNames player)
{
	if (!Locator::handSystem::has_value())
	{
		return entt::null;
	}
	const auto hands = Locator::handSystem::value().MakeHandsOf(player);
	return hands[static_cast<size_t>(HandSystemInterface::Side::Left)];
}

std::optional<entt::entity> computer_world::HeldBy(PlayerNames player)
{
	if (!Locator::handSystem::has_value())
	{
		return std::nullopt;
	}
	const auto hands = Locator::handSystem::value().GetHandsOf(player);
	if (!hands.has_value())
	{
		return std::nullopt;
	}
	auto& registry = Entities();
	const auto hand = (*hands)[static_cast<size_t>(HandSystemInterface::Side::Left)];
	const auto* grab = registry.Valid(hand) ? registry.TryGet<const HandGrab>(hand) : nullptr;
	if (grab == nullptr || grab->state == HandGrab::State::Empty || !registry.Valid(grab->object))
	{
		return std::nullopt;
	}
	return grab->object;
}

entt::entity computer_world::TownNumbered(int number)
{
	entt::entity found = entt::null;
	Entities().Each<const Town>([&found, number](entt::entity entity, const Town& town) {
		if (static_cast<int>(town.id) == number && found == entt::null)
		{
			found = entity;
		}
	});
	return found;
}

float computer_world::PlacePull(const RuleContext& context, const map_coords::MapCoords& place)
{
	if (!Locator::influenceSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	const auto& influence = Locator::influenceSystem::value();
	// Inside its own influence, or the influence that an ally lends, the place pulls fully
	if (ecs::ally_influence::InfluenceWithAllies(context.player, place) > 0.0f)
	{
		return 1.0f;
	}
	// Outside the influence the place pulls less the further it is from the nearest edge of the citadel's or a town's
	// circle
	std::vector<computer_mind_rules::InfluenceCircle> circles;
	if (Locator::worshipSiteSystem::has_value())
	{
		if (const auto centre = CoordsOfThing(Locator::worshipSiteSystem::value().TempleOf(context.player)))
		{
			circles.push_back({.centre = *centre, .radius = influence.CitadelReachOf(context.player)});
		}
	}
	for (const auto town : TownsOf(context.player))
	{
		const auto centre = CoordsOfThing(town);
		const auto* reach = Entities().TryGet<const TownInfluence>(town);
		if (centre.has_value() && reach != nullptr)
		{
			circles.push_back({.centre = *centre, .radius = reach->radius});
		}
	}
	const auto edge = computer_mind_rules::NearestInfluenceEdge(place, circles);
	if (!edge.has_value())
	{
		return 0.0f;
	}
	const auto& citadel = Locator::infoConstants::value().citadel;
	const float reach = virtual_influence::MaximumDistance(influence.InfluencePower(context.player),
	                                                       {.maxDistance = citadel.virtualInfluenceMaxDistance,
	                                                        .maxTurns = citadel.virtualInfluenceMaxGameTicks,
	                                                        .chantsToDouble = citadel.virtualInfluenceChantsToDouble});
	return computer_mind_rules::OutsidePlacePull(gutils::GetDistanceInMetres(place, *edge), reach);
}

float computer_world::ChargeLevel(RuleContext& context, MagicType type, entt::entity& icon)
{
	icon = entt::null;
	entt::entity found = entt::null;
	for (const auto site : SitesOf(context.player))
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
	++context.state.mind.castRequested.at(static_cast<size_t>(type));
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

void computer_world::ReachIcon(RuleContext& context, entt::entity icon, MagicType type)
{
	const auto iconCoords = CoordsOfThing(icon);
	const float distance =
	    iconCoords.has_value() ? gutils::GetDistanceInMetres(context.state.hand.coords, *iconCoords) : k_GestureDistance;
	if (distance < k_GestureDistance)
	{
		computer_hand::Add(context.state.hand, StepKind::MoveHandToObject, {Argument {.object = icon}});
	}
	else
	{
		computer_hand::Add(context.state.hand, StepKind::MakeGesture,
		                   {Argument {.number = static_cast<int32_t>(type)}, Argument {.number = 0}});
	}
}

void computer_world::ReportHand(PlayerNames player, glm::vec3 hand, glm::vec3 camera)
{
	if (!Locator::handSystem::has_value() || !Locator::handSystem::value().GetHandsOf(player).has_value() ||
	    !Locator::terrainSystem::has_value())
	{
		return;
	}
	// The god points at the land under its hand
	const glm::vec3 under {hand.x, Locator::terrainSystem::value().GetHeightAt({hand.x, hand.z}), hand.z};
	Locator::handSystem::value().Report(player, {.collidePoint = under, .handPoint = hand, .camera = camera, .focus = camera});
}
