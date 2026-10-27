/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ComputerMindMiracles.h"

#include <cstdint>

#include <optional>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ComputerMindCommon.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/ComputerHand.h"
#include "ECS/Map.h"
#include "ECS/PlayerMiracles.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TownAreaRules.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "ObjectMeasures.h"
#include "VillagerHome.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::computer_world;
using computer_hand::Argument;
using computer_hand::StepKind;
using computer_mind::MindDomain;
using computer_mind::NodeKind;
using computer_mind_rules::SpellKind;

namespace
{

/// The seconds that the hand stays over a thing after it cast a miracle at it
constexpr float k_CastOnThingSeconds = 8.0f;
/// The number of map cells round a place in which the god counts the people who see a miracle
constexpr int32_t k_ImpressCells = 9;

/// The ranges of magic types that the god casts in the same way
enum class CastWay : uint8_t
{
	/// An angry miracle that flies from the hand toward its target
	Toward,
	/// A miracle that goes on its target
	AtTarget,
	/// A storm, a tornado or a shield, which the god casts at a place with a radius
	Shield,
	/// A flock, which the god casts only at a place
	Flock,
	/// A miracle of a creature
	Creature,
	None,
};

CastWay WayOf(MagicType type)
{
	const auto number = static_cast<uint32_t>(type);
	const auto in = [number](MagicType first, MagicType last) {
		return number >= static_cast<uint32_t>(first) && number <= static_cast<uint32_t>(last);
	};
	if (in(MagicType::Fireball, MagicType::LightningBoltPowerUpTwo))
	{
		return CastWay::Toward;
	}
	if (in(MagicType::ExplosionOne, MagicType::FoodPowerUpOne) || in(MagicType::Wood, MagicType::WaterPowerUpOne))
	{
		return CastWay::AtTarget;
	}
	if (in(MagicType::StormWindRain, MagicType::PhysicalShield))
	{
		return CastWay::Shield;
	}
	if (in(MagicType::FlockFlying, MagicType::FlockGround))
	{
		return CastWay::Flock;
	}
	if (in(MagicType::CreatureSpellFreeze, MagicType::CreatureSpellItchy))
	{
		return CastWay::Creature;
	}
	return CastWay::None;
}

/// The first object above the node, none when no node above has one
entt::entity ObjectAbove(const computer_mind::Mind& mind, uint16_t node)
{
	for (auto at = mind.nodes[node].parent; at.has_value(); at = mind.nodes[*at].parent)
	{
		if (mind.nodes[*at].object != entt::null)
		{
			return mind.nodes[*at].object;
		}
	}
	return entt::null;
}

/// The node's object, or the first object above it
entt::entity ObjectOfNodeOrAbove(const computer_mind::Mind& mind, uint16_t node)
{
	return mind.nodes[node].object != entt::null ? mind.nodes[node].object : ObjectAbove(mind, node);
}

/// The thing is in the world and has the component
template <typename T>
bool Is(entt::entity thing)
{
	auto& registry = Entities();
	return thing != entt::null && registry.Valid(thing) && registry.AllOf<T>(thing);
}

/// The player of the god, none when it has gone
const Player* PlayerOf(PlayerNames name)
{
	const Player* found = nullptr;
	Entities().Each<const Player>([&found, name](const Player& each) {
		if (each.name == name && found == nullptr)
		{
			found = &each;
		}
	});
	return found;
}

/// The place in metres for a step of the hand, on the land
glm::vec3 PlaceOn(const RuleContext& context, const map_coords::MapCoords& coords)
{
	return {map_coords::ToMetres(coords.x), context.hand.moves.LandHeight(coords) + coords.altitude,
	        map_coords::ToMetres(coords.z)};
}

/// The place itself when it is in the god's influence, else the nearest point on the edge of the citadel's or a town's
/// circle. None when the god has no circle.
std::optional<map_coords::MapCoords> InfluenceEdgeFor(const RuleContext& context, const map_coords::MapCoords& place)
{
	if (!Locator::influenceSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& influence = Locator::influenceSystem::value();
	if (influence.PlayerInfluence(context.player, place) > 0.0f)
	{
		return place;
	}
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
	return computer_mind_rules::NearestInfluenceEdge(place, circles);
}

/// The steps that cast an angry miracle that flies. The hand goes to the edge of the influence. Then it goes toward the
/// place while it stays in the influence, and casts toward the place.
void CastToward(RuleContext& context, const map_coords::MapCoords& place, MagicType type, entt::entity icon)
{
	const auto edge = InfluenceEdgeFor(context, place);
	if (!edge.has_value())
	{
		return;
	}
	auto& hand = context.state.hand;
	computer_hand::Add(hand, StepKind::MoveHandToPos, {Argument {.place = PlaceOn(context, *edge)}});
	computer_hand::Add(hand, StepKind::MoveHandToPosStopWhenOutOfInfluence, {Argument {.place = PlaceOn(context, place)}});
	computer_hand::Add(hand, StepKind::CastSpellTowardPos,
	                   {Argument {.place = PlaceOn(context, place)}, Argument {.number = static_cast<int32_t>(type)},
	                    Argument {.object = icon}});
}

/// The steps that cast a shield at the place with the radius
void CastShield(RuleContext& context, const map_coords::MapCoords& place, MagicType type, entt::entity icon, float radius)
{
	auto& hand = context.state.hand;
	computer_hand::Add(hand, StepKind::MoveHandToPos, {Argument {.place = PlaceOn(context, place)}});
	computer_hand::Add(hand, StepKind::CastShieldAtPos,
	                   {Argument {.place = PlaceOn(context, place)}, Argument {.number = static_cast<int32_t>(type)},
	                    Argument {.value = radius}, Argument {.object = icon}});
}

/// The magic type of the miracle of the icon
MagicType TypeOfIcon(entt::entity icon)
{
	return Is<WorshipIcon>(icon) ? PlainType(Entities().Get<const WorshipIcon>(icon).seed) : MagicType::None;
}

/// The centre of the town, none when it has none
entt::entity TownCentreOf(entt::entity town)
{
	auto& registry = Entities();
	for (const auto abode : registry.Get<const Town>(town).abodes)
	{
		const auto* info = registry.Valid(abode) ? ecs::world_objects::AbodeInfoOf(abode) : nullptr;
		if (info != nullptr && info->abodeType == AbodeType::TownCentre)
		{
			return abode;
		}
	}
	return entt::null;
}

/// The villagers in the first cells of a spiral out from the place
uint32_t VillagersNear(const map_coords::MapCoords& place)
{
	if (!Locator::entitiesMap::has_value())
	{
		return 0;
	}
	auto& registry = Entities();
	uint32_t count = 0;
	auto coords = place;
	map_coords::Spiral spiral;
	for (int32_t cells = k_ImpressCells; cells != 0; --cells)
	{
		if (const glm::ivec2 cell = map_coords::Cell(coords); map_coords::InBounds(cell))
		{
			for (const auto thing : Locator::entitiesMap::value().GetAllInCell(cell))
			{
				if (registry.AllOf<Villager>(thing))
				{
					++count;
				}
			}
		}
		map_coords::AddCells(coords, spiral.Next());
	}
	return count;
}

/// The place of a town where the most people see a miracle, and its value
struct ImpressPlace
{
	float value {0.0f};
	map_coords::MapCoords place;
};

/// The best place of the town for an impressive miracle. None when the people want to sleep.
std::optional<ImpressPlace> ImpressPlaceOf(entt::entity town, const map_coords::MapCoords& hand)
{
	if (!Locator::townDesireSystem::has_value() || !Locator::townSystem::has_value())
	{
		return std::nullopt;
	}
	const auto sleep = static_cast<double>(Locator::townDesireSystem::value().GetRawDesire(town, TownDesireInfo::ForSleep));
	if (!(sleep <= computer_mind_rules::k_TooSleepyToImpress))
	{
		return std::nullopt;
	}
	const auto fromMetres = [](glm::vec2 metres) {
		return map_coords::MapCoords {.x = map_coords::ToFixed(metres.x), .z = map_coords::ToFixed(metres.y), .altitude = 0.0f};
	};
	// The god looks at the places in this order: the door of the storage pit, the gathering place, the town centre,
	// then each field of the town
	std::vector<map_coords::MapCoords> places;
	if (const auto pit = ecs::construction::StoragePitOf(town))
	{
		places.push_back(fromMetres(ecs::villager_home::DoorPosition(*pit)));
	}
	places.push_back(fromMetres(Locator::townSystem::value().GetCongregationPos(town)));
	if (const auto centre = CoordsOfThing(TownCentreOf(town)))
	{
		places.push_back(*centre);
	}
	const auto number = static_cast<int>(Entities().Get<const Town>(town).id);
	Entities().Each<const Field>([&places, number](entt::entity entity, const Field& field) {
		if (field.town != number)
		{
			return;
		}
		if (const auto coords = CoordsOfThing(entity))
		{
			places.push_back(*coords);
		}
	});
	std::vector<float> values;
	values.reserve(places.size());
	for (const auto& place : places)
	{
		values.push_back(
		    computer_mind_rules::ImpressPlaceValue(VillagersNear(place), gutils::GetDistanceInMetres(hand, place)));
	}
	const auto best = computer_mind_rules::BestImpressPlace(values);
	if (!best.has_value())
	{
		return std::nullopt;
	}
	return ImpressPlace {.value = values[*best], .place = places[*best]};
}

/// The building of the god's towns that is the most damaged, against the pull of its place
float CheckTownForDamage(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	std::vector<entt::entity> abodes;
	std::vector<float> shares;
	for (const auto town : TownsOf(context.player))
	{
		for (const auto abode : Entities().Get<const Town>(town).abodes)
		{
			const auto coords = CoordsOfThing(abode);
			if (!coords.has_value())
			{
				continue;
			}
			abodes.push_back(abode);
			shares.push_back(ecs::world_objects::LifeOf(abode) * PlacePull(context, *coords));
		}
	}
	const auto choice = computer_mind_rules::MostDamaged(shares);
	if (!choice.has_value())
	{
		return 0.0f;
	}
	mind.nodes[node].object = abodes[choice->index];
	return computer_mind_rules::DamageNeed(choice->share);
}

/// The building above the node burns: the god wants all of it
float FindBurningAbode(RuleContext& /*context*/, computer_mind::Mind& mind, uint16_t node)
{
	const auto abode = ObjectAbove(mind, node);
	if (!Is<Abode>(abode) || !Locator::fireSystem::has_value())
	{
		return 0.0f;
	}
	const auto reach = Locator::fireSystem::value().GetReach(abode);
	if (!reach.has_value() || !reach->aboveReactionTemperature)
	{
		return 0.0f;
	}
	mind.nodes[node].object = abode;
	return 1.0f;
}

/// The wish for the miracle, times two. When the first wish is not full, the god finds the wish again.
float DoubleWish(RuleContext& context, computer_mind::Mind& mind, uint16_t node, MagicType type,
                 const std::optional<map_coords::MapCoords>& place)
{
	const auto wish = [&context, &mind, node, type, &place]() {
		entt::entity icon = entt::null;
		const float value = SpellWish(context, type, place, icon);
		if (icon != entt::null)
		{
			mind.nodes[node].object = icon;
		}
		return value;
	};
	const float first = wish() * 2.0f;
	return first < 1.0f ? wish() * 2.0f : 1.0f;
}

/// How much the god wants to put out the fire of the building above with a water miracle
float CastWaterSpell(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return DoubleWish(context, mind, node, MagicType::Water, std::nullopt);
}

/// The god charges the miracle of the node's icon and casts it at the thing above. The hand stays over the thing for
/// eight seconds, then the miracle ends.
bool CastOnThing(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto icon = ObjectOfNodeOrAbove(mind, node);
	const auto target = ObjectAbove(mind, node);
	if (!Is<WorshipIcon>(icon) || !CoordsOfThing(target).has_value())
	{
		return false;
	}
	ReachAndChant(context, icon);
	CastAtThing(context, target, icon, 0.0f);
	computer_hand::Add(context.state.hand, StepKind::Wait, {Argument {.value = k_CastOnThingSeconds}});
	computer_hand::Add(context.state.hand, StepKind::CloseDownCurrentSpell, {});
	return true;
}

/// The building above the node has no shield of its own: the god wants all of it
float FindUnprotectedAbode(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto abode = ObjectAbove(mind, node);
	const auto coords = CoordsOfThing(abode);
	if (!Is<Abode>(abode) || !coords.has_value())
	{
		return 0.0f;
	}
	// A burning building needs the miracle shield, another building the physical shield
	const auto type = Entities().AllOf<Fire>(abode) ? MagicType::Shield : MagicType::PhysicalShield;
	if (OwnCastAt(context, *coords, type))
	{
		return 0.0f;
	}
	mind.nodes[node].object = abode;
	return 1.0f;
}

/// The god casts the shield of the node's icon over the town above, as wide as the town
bool CastShieldToProtect(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto icon = ObjectOfNodeOrAbove(mind, node);
	const auto town = ObjectAbove(mind, node);
	if (!Is<WorshipIcon>(icon) || !Is<Town>(town))
	{
		return false;
	}
	const auto& area = Entities().Get<const Town>(town).area;
	const auto centre = ecs::town_area::Centre(area);
	if (OwnCastAt(context, centre, TypeOfIcon(icon)))
	{
		return false;
	}
	ReachAndChant(context, icon);
	CastAtPlace(context, centre, icon, ecs::town_area::Radius(area));
	return true;
}

/// How much the god wants a physical shield over the thing above, when it has none there
float CastPhysicalShieldToProtect(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto coords = CoordsOfThing(ObjectAbove(mind, node));
	if (!coords.has_value() || OwnCastAt(context, *coords, MagicType::PhysicalShield))
	{
		return 0.0f;
	}
	return DoubleWish(context, mind, node, MagicType::PhysicalShield, coords);
}

/// The god casts the shield of the node's icon over the thing above, a little wider than the thing
bool PerformPhysicalShield(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto icon = ObjectOfNodeOrAbove(mind, node);
	const auto thing = ObjectAbove(mind, node);
	const auto coords = CoordsOfThing(thing);
	if (!Is<WorshipIcon>(icon) || !coords.has_value() || OwnCastAt(context, *coords, TypeOfIcon(icon)))
	{
		return false;
	}
	ReachAndChant(context, icon);
	const float radius = object_measures::TwoDRadius(Entities(), thing) * computer_mind_rules::k_PhysicalShieldScale;
	CastAtPlace(context, *coords, icon, radius);
	return true;
}

/// The worship site's icon that the god wants to charge most is the node's object. How much it wants it.
float ChargeUpSpell(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	std::vector<entt::entity> icons;
	std::vector<computer_mind_rules::ChargeCandidate> candidates;
	for (const auto site : SitesOf(context.player))
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
				     .requests = context.state.mind.castRequested.at(static_cast<size_t>(PlainType(component.seed)))});
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

/// The god's worshippers chant for the miracle of the node's icon
bool PerformCharge(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
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
	ReachIcon(context, icon, type);
	computer_hand::Add(context.state.hand, StepKind::MakeWorshippersChantForSpellNoWait,
	                   {Argument {.number = static_cast<int32_t>(type)}, Argument {.object = icon}});
	return true;
}

/// The node's object becomes the icon of the best miracle, when there is one
float KeepIcon(computer_mind::Mind& mind, uint16_t node, float value, entt::entity icon)
{
	if (icon != entt::null)
	{
		mind.nodes[node].object = icon;
	}
	return value;
}

} // namespace

float computer_world::SpellWish(RuleContext& context, MagicType type, const std::optional<map_coords::MapCoords>& place,
                                entt::entity& icon)
{
	icon = entt::null;
	const auto* player = PlayerOf(context.player);
	if (player == nullptr || !player_miracles::IsMagicTypeEnabled(player->miracles, type))
	{
		return 0.0f;
	}
	float charge = ChargeLevel(context, type, icon);
	// Short of a full charge the god asks again, so each look counts two requests
	charge = charge < 1.0f ? ChargeLevel(context, type, icon) : 1.0f;
	if (icon == entt::null)
	{
		return 0.0f;
	}
	const float turns = computer_mind_rules::TurnsToCharge(FactsOf(icon));
	if (turns < 0.0f)
	{
		return 0.0f;
	}
	const float pull = place.has_value() ? PlacePull(context, *place) : 1.0f;
	return computer_mind_rules::SpellValue(charge, turns, pull);
}

float computer_world::BestSpellOfKind(RuleContext& context, SpellKind kind, const std::optional<map_coords::MapCoords>& place,
                                      entt::entity& icon)
{
	icon = entt::null;
	if (!Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	const auto& info = Locator::infoConstants::value();
	float best = 0.0f;
	for (auto number = computer_mind_rules::k_FirstChoiceType; number <= computer_mind_rules::k_LastChoiceType; ++number)
	{
		const auto type = static_cast<MagicType>(number);
		const auto& effect = magic::GetMagicEffectInfo(info, type);
		const float score = computer_mind_rules::KindScore(kind, effect.perceivedPlayerDesire.at(0), effect.cpImpressiveBalance,
		                                                   effect.aggressiveAttackValue);
		if (score == 0.0f)
		{
			continue;
		}
		entt::entity found = entt::null;
		const float value = computer_mind_rules::KindWish(SpellWish(context, type, place, found), score);
		if (value > best)
		{
			best = value;
			icon = found;
		}
	}
	return best;
}

float computer_world::AggressiveSpell(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	const auto target = ObjectAbove(mind, node);
	const auto targetCoords = Is<Creature>(target) ? CoordsOfThing(target) : std::nullopt;
	const auto own =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetPrimaryCreature(context.player) : std::nullopt;
	// The god does not cast an angry miracle at a creature that is near its own creature
	if (own.has_value() && Is<Creature>(*own) && targetCoords.has_value())
	{
		if (const auto ownCoords = CoordsOfThing(*own))
		{
			const float distance = gutils::GetDistanceInMetres(*ownCoords, *targetCoords);
			if (computer_mind_rules::TooNearOwnCreature(distance, registry.Get<const Creature>(*own).size,
			                                            registry.Get<const Creature>(target).size))
			{
				return 0.0f;
			}
		}
	}
	entt::entity icon = entt::null;
	const float best = BestSpellOfKind(context, SpellKind::Aggressive, targetCoords, icon);
	return KeepIcon(mind, node, best, icon);
}

float computer_world::CompassionateSpell(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	entt::entity icon = entt::null;
	const float best = BestSpellOfKind(context, SpellKind::Compassionate, std::nullopt, icon);
	return KeepIcon(mind, node, best, icon);
}

float computer_world::ImpressiveSpell(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto town = ObjectAbove(mind, node);
	if (!Is<Town>(town))
	{
		return 0.0f;
	}
	entt::entity icon = entt::null;
	const float best = BestSpellOfKind(context, SpellKind::Impressive, CoordsOfThing(town), icon);
	auto place = ImpressPlaceOf(town, context.state.hand.coords);
	float value = place.has_value() ? place->value : 0.0f;
	// Short of a full value the god looks at the places again
	if (value < 1.0f)
	{
		place = ImpressPlaceOf(town, context.state.hand.coords);
		value = place.has_value() ? place->value : 0.0f;
	}
	else
	{
		value = 1.0f;
	}
	if (icon != entt::null)
	{
		mind.nodes[node].object = icon;
		if (place.has_value())
		{
			mind.nodes[node].place = {map_coords::ToMetres(place->place.x), place->place.altitude,
			                          map_coords::ToMetres(place->place.z)};
		}
	}
	return value * best;
}

std::optional<computer_world::ImpressSpot> computer_world::TownImpressPlace(entt::entity town,
                                                                            const map_coords::MapCoords& hand)
{
	const auto place = ImpressPlaceOf(town, hand);
	if (!place.has_value())
	{
		return std::nullopt;
	}
	return ImpressSpot {.value = place->value, .place = place->place};
}

std::optional<map_coords::MapCoords> computer_world::InfluenceEdgeOf(const RuleContext& context,
                                                                     const map_coords::MapCoords& place)
{
	return InfluenceEdgeFor(context, place);
}

bool computer_world::OwnCastAt(const RuleContext& context, const map_coords::MapCoords& place, MagicType type)
{
	auto& registry = Entities();
	for (const auto cast : context.state.casts)
	{
		const auto* spell = registry.Valid(cast) ? registry.TryGet<const Spell>(cast) : nullptr;
		if (spell == nullptr || spell->magicType != type)
		{
			continue;
		}
		const map_coords::MapCoords at {
		    .x = map_coords::ToFixed(spell->position.x), .z = map_coords::ToFixed(spell->position.z), .altitude = 0.0f};
		if (computer_mind_rules::SameCastPlace(at, place))
		{
			return true;
		}
	}
	return false;
}

void computer_world::ReachAndChant(RuleContext& context, entt::entity icon)
{
	const auto type = TypeOfIcon(icon);
	ReachIcon(context, icon, type);
	computer_hand::Add(context.state.hand, StepKind::MakeWorshippersChantForSpell,
	                   {Argument {.number = static_cast<int32_t>(type)}, Argument {.object = icon}});
}

void computer_world::CastAtThing(RuleContext& context, entt::entity target, entt::entity icon, float radius)
{
	const auto type = TypeOfIcon(icon);
	const auto coords = CoordsOfThing(target);
	if (!coords.has_value())
	{
		return;
	}
	switch (WayOf(type))
	{
	case CastWay::Toward:
	{
		// The hand casts toward the side of the thing that faces it
		const float angle = gutils::Get3DAngleFromXZ(*coords, context.state.hand.coords);
		const auto side = *coords + gutils::GetPosFromAngle(angle, object_measures::TwoDRadius(Entities(), target));
		CastToward(context, side, type, icon);
		break;
	}
	case CastWay::AtTarget:
	case CastWay::Creature:
		computer_hand::Add(
		    context.state.hand, StepKind::CastSpellAtObject,
		    {Argument {.object = target}, Argument {.number = static_cast<int32_t>(type)}, Argument {.object = icon}});
		break;
	case CastWay::Shield:
		CastShield(context, *coords, type, icon, radius);
		break;
	case CastWay::Flock:
	case CastWay::None:
		break;
	}
}

void computer_world::CastAtPlace(RuleContext& context, const map_coords::MapCoords& place, entt::entity icon, float radius)
{
	const auto type = TypeOfIcon(icon);
	// The miracle goes on the side of the place that faces the hand
	const float angle = gutils::Get3DAngleFromXZ(place, context.state.hand.coords);
	const auto side = place + gutils::GetPosFromAngle(angle, radius);
	switch (WayOf(type))
	{
	case CastWay::Toward:
		CastToward(context, side, type, icon);
		break;
	case CastWay::AtTarget:
		computer_hand::Add(context.state.hand, StepKind::CastSpellAtPos,
		                   {Argument {.place = PlaceOn(context, side)}, Argument {.number = static_cast<int32_t>(type)},
		                    Argument {.object = icon}});
		// The god then also casts the same miracle as a shield at the middle of the place (see docs/vanilla-bugs.md)
		CastShield(context, place, type, icon, radius);
		break;
	case CastWay::Shield:
	case CastWay::Flock:
		CastShield(context, place, type, icon, radius);
		break;
	case CastWay::Creature:
	case CastWay::None:
		break;
	}
}

bool computer_world::CarryNear(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	// The thing is the object of the leaf at the end of the line that the node chose
	uint16_t leaf = node;
	while (mind.nodes[leaf].bestLeaf.has_value() && *mind.nodes[leaf].bestLeaf != leaf)
	{
		leaf = *mind.nodes[leaf].bestLeaf;
	}
	const auto thing = ObjectOfNodeOrAbove(mind, leaf);
	const auto target = ObjectOfNodeOrAbove(mind, node);
	const auto thingCoords = CoordsOfThing(thing);
	const auto targetCoords = CoordsOfThing(target);
	if (!thingCoords.has_value() || !targetCoords.has_value() || !Locator::gameRandom::has_value())
	{
		return false;
	}
	// The hand puts the thing on the far side of the target, as seen from the thing
	const float angle = gutils::Get3DAngleFromXZ(*thingCoords, *targetCoords);
	const float random = Locator::gameRandom::value().GameFloatRand(computer_mind_rules::k_CarryRandom);
	const float distance = computer_mind_rules::CarryDistance(random, object_measures::TwoDRadius(Entities(), target),
	                                                          object_measures::TwoDRadius(Entities(), thing));
	const auto place = *targetCoords + gutils::GetPosFromAngle(angle, distance);
	auto& hand = context.state.hand;
	computer_hand::Add(hand, StepKind::PickUpObject, {Argument {.object = thing}, Argument {.value = hand.speed}});
	computer_hand::Add(hand, StepKind::DropObjectInHandAtPos,
	                   {Argument {.place = PlaceOn(context, place)}, Argument {.value = hand.speed}});
	return true;
}

void computer_world::RegisterMiraclesRules(RuleTable& table)
{
	constexpr auto k_Miracles = MindDomain::Miracles;
	AddRule(table, k_Miracles, NodeKind::ActionChargeUpSpell, {.intensity = ChargeUpSpell, .perform = PerformCharge});
	AddRule(table, k_Miracles, NodeKind::NeedCheckTownForDamage, {.intensity = CheckTownForDamage});
	AddRule(table, k_Miracles, NodeKind::ActionFindBurningAbode, {.intensity = FindBurningAbode, .perform = CarryNear});
	AddRule(table, k_Miracles, NodeKind::ObjectCastWaterSpell, {.intensity = CastWaterSpell, .perform = CastOnThing});
	AddRule(table, k_Miracles, NodeKind::ActionFindUnprotectedAbode, {.intensity = FindUnprotectedAbode});
	// The god does not keep the attacks on its towns yet, so it does not find how much it wants a shield over a town
	AddRule(table, k_Miracles, NodeKind::ObjectCastShieldToProtect, {.perform = CastShieldToProtect});
	AddRule(table, k_Miracles, NodeKind::ObjectCastPhysicalShieldToProtect,
	        {.intensity = CastPhysicalShieldToProtect, .perform = PerformPhysicalShield});
}
