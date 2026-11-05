/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How a creature chooses what to act on for a desire: the activity, the things it knows of that suit the desire, and
// what each choice is worth

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Animals/AnimalRules.h"
#include "Camera/Camera.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Common/X87Vector.h"
#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureActionHistory.h"
#include "Creature/CreatureActivity.h"
#include "Creature/CreatureAgendaArea.h"
#include "Creature/CreatureAttention.h"
#include "Creature/CreatureBringHome.h"
#include "Creature/CreatureDance.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesireSources.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureExplorationMap.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureGiving.h"
#include "Creature/CreatureKindAgendas.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMeals.h"
#include "Creature/CreatureMind.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureMode.h"
#include "Creature/CreatureMusicReaction.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreaturePlayerAttention.h"
#include "Creature/CreatureRestAndHome.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureSacrifice.h"
#include "Creature/CreatureScriptAgendas.h"
#include "Creature/CreatureScriptPlay.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/CreatureTownCompassion.h"
#include "Creature/CreatureTownTheft.h"
#include "Creature/CreatureTraining.h"
#include "Creature/CreatureTurnOrder.h"
#include "Creature/CreatureWatching.h"
#include "Creature/LeashOrders.h"
#include "Creature/LeashRules.h"
#include "CreatureMindFootball.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "CreatureMindTownHelp.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LandForest.h"
#include "ECS/Components/LearningMarker.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicPile.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureBodyFile.h"
#include "ECS/CreatureCarried.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreatureNaming.h"
#include "ECS/FixedKinds.h"
#include "ECS/LearningMarker.h"
#include "ECS/PlayerTowns.h"
#include "ECS/Registry.h"
#include "ECS/ReportedCamera.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MusicMoodSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/TempleHeartOf.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/TownDesire.h"
#include "ECS/WallHugRules.h"
#include "ECS/WorldObjects.h"
#include "Graphics/Moon.h"
#include "Gui/TextDatabase.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "MagicLiving.h"
#include "ObjectMeasures.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/MusicMood.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems::mind_detail;
using creature_desires::Desire;
using creature_mind_model::TreeKind;
using creature_plan_actions::Target;

namespace openblack::ecs::systems::mind_detail
{
bool OutsideConfinedArea(const ecs::Registry& registry, entt::entity creature, const map_coords::MapCoords& place)
{
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	if (leashes == nullptr)
	{
		return false;
	}
	const bool leashed = leashes->worn.has_value();
	const bool works = !leashed || leashes->worn->works;
	return creature_leash::IsConfined(leashes->confinementRadius, leashed, works) &&
	       gutils::GetDistanceInMetres(place, map_coords::FromMetres(glm::xz(leashes->confinementCentre))) >
	           leashes->confinementRadius;
}

std::optional<map_coords::MapCoords> BelievedPlaceOf(const ecs::Registry& registry,
                                                     const creature_object_beliefs::Belief* belief, entt::entity thing)
{
	if (belief != nullptr)
	{
		return belief->place;
	}
	if (registry.AllOf<Transform>(thing))
	{
		return ecs::thing_map_place::Of(registry, thing);
	}
	return std::nullopt;
}

std::optional<map_coords::MapCoords> BelievedPlaceOf(const ecs::Registry& registry, const CreatureMindState& mind,
                                                     entt::entity thing)
{
	return BelievedPlaceOf(registry, mind.beliefs.Find(thing), thing);
}

std::optional<entt::entity> ActedThingOf(const ecs::Registry& registry, entt::entity thing)
{
	if (registry.Valid(thing) && registry.AllOf<Temple>(thing) && ecs::temple_heart::Of(registry, thing) != entt::null)
	{
		return std::nullopt;
	}
	return ecs::temple_heart::Acted(registry, thing);
}
} // namespace openblack::ecs::systems::mind_detail

namespace
{
/// A town counts as loyal to the player of the creature when its belief in that player is more than this share of its
/// largest belief in another player
constexpr float k_HalfOfMostOtherBelief = 0.5f;
} // namespace

namespace openblack::ecs::systems::mind_detail
{
creature_activity::WholeFacts WholeFactsOf(const ecs::Registry& registry, entt::entity creature, entt::entity whole,
                                           creature_activity::Whole kind, const creature_mind_tables::Tables& tables,
                                           bool lookedAt)
{
	const auto& self = registry.Get<const Creature>(creature);
	creature_activity::WholeFacts facts {.kind = kind};
	switch (kind)
	{
	case creature_activity::Whole::Town:
		if (const auto* town = registry.TryGet<const Town>(whole))
		{
			facts.samePlayer = town->owner == self.owner;
		}
		if (const auto* impression = registry.TryGet<const TownImpression>(whole))
		{
			const auto& belief = impression->belief.belief;
			const auto mine = static_cast<size_t>(self.owner);
			float mostOther = 0.0f;
			for (size_t player = 0; player < belief.size(); ++player)
			{
				if (player != mine && belief.at(player) > mostOther)
				{
					mostOther = belief.at(player);
				}
			}
			facts.believesInPlayer = mine < belief.size() && mostOther * k_HalfOfMostOtherBelief < belief.at(mine);
		}
		break;
	case creature_activity::Whole::Creature:
		facts.samePlayer = registry.Get<const Creature>(whole).owner == self.owner;
		facts.attackable = mind_detail::MayBeAngryWith(registry, whole, creature, tables);
		break;
	case creature_activity::Whole::Citadel:
		if (const auto* temple = registry.TryGet<const Temple>(whole))
		{
			facts.samePlayer = temple->owner == self.owner;
		}
		break;
	case creature_activity::Whole::Forest:
	case creature_activity::Whole::Flock:
		// A forest and a flock have the neutral player
		facts.samePlayer = self.owner == PlayerNames::NEUTRAL;
		break;
	}
	facts.lookedAt = lookedAt;
	return facts;
}
} // namespace openblack::ecs::systems::mind_detail

namespace
{
/// The tribe of a town; none for no town or a town of no tribe
std::optional<Tribe> TribeOfTown(const ecs::Registry& registry, std::optional<entt::entity> town)
{
	const auto* tribe = town.has_value() ? registry.TryGet<const Tribe>(*town) : nullptr;
	return tribe != nullptr ? std::optional(*tribe) : std::nullopt;
}

/// A field has the data of the field building of its town's tribe, or of the first building for a field of no town
const GAbodeInfo* FieldBuildingInfo(const ecs::Registry& registry, entt::entity field)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& info = Locator::infoConstants::value();
	const auto& self = registry.Get<const Field>(field);
	const auto tribe =
	    TribeOfTown(registry, self.town >= 0 ? TownOfBuilding(registry, static_cast<uint32_t>(self.town)) : std::nullopt);
	if (!tribe.has_value())
	{
		return &info.abode.at(0);
	}
	const auto row = static_cast<size_t>(GAbodeInfo::Find(*tribe, AbodeNumber::Field));
	return row < info.abode.size() ? &info.abode.at(row) : nullptr;
}

/// The data of a village totem's statue, by the tribe of its town
const GTotemStatueInfo* TotemStatueInfo(const ecs::Registry& registry, entt::entity totem)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& statues = Locator::infoConstants::value().totemStatue;
	const auto tribe = TribeOfTown(registry, TownOfTotem(registry, totem));
	const auto row = tribe.has_value() ? static_cast<size_t>(*tribe) : statues.size();
	return row < statues.size() ? &statues.at(row) : nullptr;
}
} // namespace

namespace openblack::ecs::systems::mind_detail
{
bool NoWayLately(const ecs::Registry& registry, const CreatureMindState& mind, entt::entity thing)
{
	return NoWayLately(registry, mind, mind.beliefs.Find(thing), thing);
}

bool NoWayLately(const ecs::Registry& registry, const CreatureMindState& mind, const creature_object_beliefs::Belief* belief,
                 entt::entity thing)
{
	// Only a creature that found no way to the thing asks if the thing is fixed in the land
	if (belief == nullptr || !belief->blockedTurn.has_value())
	{
		return false;
	}
	return creature_object_beliefs::BlockedRecently(*belief, mind.turn, ecs::fixed_kinds::IsFixedKind(registry, thing));
}
} // namespace openblack::ecs::systems::mind_detail

namespace
{
/// A hungry creature does not eat a thing of a town for which it learnt a compassion worth more than this
constexpr float k_MostCompassionToEatFrom = 0.1f;

/// Whether a creature can act on a thing at all. The living and miracles can always have an action. A whole thing, a
/// worship site and a thing without data cannot. Any other thing lets a creature act on it when its data says so. The
/// temple here is one thing, which stands for its heart: the data of the heart says if a creature can act on it. The
/// altar of a worship site has the data of its site. The shipped lands make no creature pen as a thing of its own.
bool LetsCreatureActOnUnliving(const ecs::Registry& registry, entt::entity thing);
} // namespace

namespace openblack::ecs::systems::mind_detail
{
bool LetsCreatureAct(const ecs::Registry& registry, entt::entity thing)
{
	return registry.AnyOf<Villager, Animal, Creature, ecs::components::Spell>(thing) ||
	       LetsCreatureActOnUnliving(registry, thing);
}
} // namespace openblack::ecs::systems::mind_detail

namespace
{
/// Whether a creature can act on a thing that is not a villager, an animal, a creature or a miracle
bool LetsCreatureActOnUnliving(const ecs::Registry& registry, entt::entity thing)
{
	if (registry.AllOf<Temple>(thing))
	{
		return Locator::infoConstants::has_value() &&
		       Locator::infoConstants::value().citadelHeart.canCreatureInteractWithMe != 0;
	}
	if (registry.AllOf<WorshipAltar>(thing))
	{
		const auto* info = mind_detail::AltarInfoOf(registry, thing);
		return info != nullptr && info->canCreatureInteractWithMe != 0;
	}
	if (registry.AnyOf<WorshipSite, Town, Flock, ForestTurns>(thing))
	{
		return false;
	}
	// A field has the data of its town's field building, a village totem the data of its statue, and a town's desire
	// flag the data of the prayer site
	const GObjectInfo* info = nullptr;
	if (registry.AllOf<Field>(thing))
	{
		info = FieldBuildingInfo(registry, thing);
	}
	else if (registry.AllOf<TownDesireFlag>(thing))
	{
		info = Locator::infoConstants::has_value() ? &Locator::infoConstants::value().prayerSite : nullptr;
	}
	else if (registry.AllOf<VillageTotem>(thing))
	{
		info = TotemStatueInfo(registry, thing);
	}
	else
	{
		info = ecs::world_objects::InfoOf(thing);
	}
	return info != nullptr && info->canCreatureInteractWithMe != 0;
}

/// The data that tells if a creature can act on a thing. A field has the data of its town's field building, a village
/// totem the data of its statue, the temple the data of its heart, and the altar of a worship site the data of its site.
/// A town's desire flag has the data of the prayer site.
const GObjectInfo* ActingInfoOf(const ecs::Registry& registry, entt::entity thing)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& info = Locator::infoConstants::value();
	if (registry.AllOf<Temple>(thing))
	{
		return &info.citadelHeart;
	}
	if (registry.AllOf<WorshipAltar>(thing))
	{
		return mind_detail::AltarInfoOf(registry, thing);
	}
	if (registry.AllOf<Field>(thing))
	{
		return FieldBuildingInfo(registry, thing);
	}
	if (registry.AllOf<VillageTotem>(thing))
	{
		return TotemStatueInfo(registry, thing);
	}
	if (registry.AllOf<TownDesireFlag>(thing))
	{
		return &info.prayerSite;
	}
	if (const auto* animal = registry.TryGet<const Animal>(thing))
	{
		const auto row = static_cast<size_t>(animal->type);
		return row < info.animal.size() ? &info.animal.at(row) : nullptr;
	}
	return ecs::world_objects::InfoOf(thing);
}

/// The data that tells if a creature can act on a thing (ActingInfoOf), and whether it can act on it at all
/// (LetsCreatureAct)
struct ActingOf
{
	const GObjectInfo* info {nullptr};
	bool canActOn {false};
};

/// The same answers as ActingInfoOf and LetsCreatureAct, for a thing that is not a villager, an animal, a creature or
/// a miracle, with one look at each component
ActingOf ActingOfUnliving(const ecs::Registry& registry, entt::entity thing)
{
	if (!Locator::infoConstants::has_value())
	{
		return {.info = nullptr, .canActOn = LetsCreatureActOnUnliving(registry, thing)};
	}
	const auto& constants = Locator::infoConstants::value();
	const auto acts = [](const GObjectInfo* info) { return info != nullptr && info->canCreatureInteractWithMe != 0; };
	if (registry.AllOf<Temple>(thing))
	{
		return {.info = &constants.citadelHeart, .canActOn = acts(&constants.citadelHeart)};
	}
	if (registry.AllOf<WorshipAltar>(thing))
	{
		const auto* info = mind_detail::AltarInfoOf(registry, thing);
		return {.info = info, .canActOn = acts(info)};
	}
	// A creature cannot act on a whole thing, but the data of the thing stays
	const bool whole = registry.AnyOf<WorshipSite, Town, Flock, ForestTurns>(thing);
	if (registry.AllOf<Field>(thing))
	{
		const auto* info = FieldBuildingInfo(registry, thing);
		return {.info = info, .canActOn = !whole && acts(info)};
	}
	if (registry.AllOf<VillageTotem>(thing))
	{
		// A village totem that is also a town's desire flag lets a creature act on it as the flag does
		const auto* info = TotemStatueInfo(registry, thing);
		const GObjectInfo* actsAs =
		    registry.AllOf<TownDesireFlag>(thing) ? static_cast<const GObjectInfo*>(&constants.prayerSite) : info;
		return {.info = info, .canActOn = !whole && acts(actsAs)};
	}
	if (registry.AllOf<TownDesireFlag>(thing))
	{
		return {.info = &constants.prayerSite, .canActOn = !whole && acts(&constants.prayerSite)};
	}
	const auto* info = ecs::world_objects::InfoOf(thing);
	return {.info = info, .canActOn = !whole && acts(info)};
}

/// The kind of a thing, by how the desires examine it. The components give the same answers as the registry: the
/// registry itself, or the stores that KindStores finds.
template <typename Components>
creature_activity::Thing KindOf(const Components& components, entt::entity thing)
{
	using creature_activity::Thing;
	if (components.template AllOf<Villager>(thing))
	{
		return Thing::Villager;
	}
	if (components.template AllOf<Animal>(thing))
	{
		return Thing::Animal;
	}
	if (components.template AllOf<Creature>(thing))
	{
		return Thing::Creature;
	}
	if (components.template AllOf<ecs::components::Spell>(thing))
	{
		return Thing::Spell;
	}
	if (components.template AllOf<Tree>(thing))
	{
		return Thing::Tree;
	}
	const auto* still = components.template TryGet<const MobileStatic>(thing);
	if (components.template AnyOf<Rock, DeadTree>(thing) || (still != nullptr && still->type == MobileStaticInfo::Bonfire))
	{
		return Thing::Rock;
	}
	// A scaffold is a loose object
	if (components.template AnyOf<MobileObject, Pot, Ball, Scaffold>(thing))
	{
		return Thing::MobileObject;
	}
	if (still != nullptr)
	{
		return Thing::MobileStatic;
	}
	if (components.template AllOf<Field>(thing))
	{
		return Thing::Field;
	}
	if (components.template AllOf<FishFarm>(thing))
	{
		return Thing::FishFarm;
	}
	if (components.template AllOf<VillageTotem>(thing))
	{
		return Thing::TotemStatue;
	}
	if (components.template AnyOf<Abode, Temple, WorshipSite, WorshipAltar, Feature, AnimatedStatic, Flowers, BigForest>(thing))
	{
		return Thing::Building;
	}
	if (components.template AllOf<TownDesireFlag>(thing))
	{
		return Thing::PlainObject;
	}
	return Thing::Other;
}

/// The stores of the components that tell the kind of a thing (KindOf), found one time for a walk of many things. No
/// test of a desire makes the first component of one of these kinds.
using KindStores =
    ecs::ComponentLookups<Villager, Animal, Creature, ecs::components::Spell, Tree, MobileStatic, Rock, DeadTree, MobileObject,
                          Pot, Ball, Scaffold, Field, FishFarm, VillageTotem, Abode, Temple, WorshipSite, WorshipAltar, Feature,
                          AnimatedStatic, Flowers, BigForest, TownDesireFlag>;

/// What a creature knows about a thing of a kind when it examines a desire for it. The function finds only the facts
/// that the desire reads. The other facts keep their default values.
creature_activity::ThingFacts ThingFactsOf(const ecs::Registry& registry, entt::entity creature, Desire desire,
                                           entt::entity thing, creature_activity::Thing kind,
                                           const creature_mind_tables::Tables* tables)
{
	using creature_activity::Thing;
	const auto read = creature_activity::FactsReadBy(desire);
	// The kind comes from the first of these components that the thing has: a villager, an animal, a creature, a
	// miracle. Thus the kind tells which of them the thing has, with no search of the registry.
	const bool living = kind == Thing::Villager || kind == Thing::Animal || kind == Thing::Creature || kind == Thing::Spell;
	const auto acting =
	    living ? ActingOf {.info = ActingInfoOf(registry, thing), .canActOn = true} : ActingOfUnliving(registry, thing);
	const auto* info = acting.info;
	creature_activity::ThingFacts facts {
	    .kind = kind,
	    .canActOn = acting.canActOn,
	    .attackable = info != nullptr && info->canCreatureAttackMe != 0,
	    .playable = info != nullptr && info->canCreaturePlayWithMe != 0,
	    .inspectable = info != nullptr && info->canCreatureInspectMe != 0,
	    .itself = thing == creature,
	};
	if (read.living)
	{
		if (kind == Thing::Villager)
		{
			// A villager in its home, or one that may not be picked up, cannot have help
			const auto& villager = registry.Get<const Villager>(thing);
			facts.dead = registry.AllOf<VillagerDeath>(thing) || !(villager.life > 0.0f);
			facts.dancing = registry.AllOf<Dancer>(thing);
			facts.pickable = !registry.AnyOf<AtHome, CannotBePickedUp>(thing);
		}
		else if (kind == Thing::Animal)
		{
			const auto& animal = registry.Get<const Animal>(thing);
			facts.dead = animal.Dead();
			facts.frightening = animals::FrightensCreatures(animal.type);
		}
		else if (kind == Thing::Creature)
		{
			facts.dead = tables != nullptr && mind_detail::IsDead(registry, thing, *tables);
			facts.friendOfCreature = mind_detail::AreFriends(registry, thing, creature);
		}
	}
	if (read.mushroom)
	{
		const auto* feature = registry.TryGet<const Feature>(thing);
		const auto* mobile = registry.TryGet<const MobileObject>(thing);
		// A scaffold has the loose kind of the one row of the scaffolds
		std::optional<MobileObjectInfo> looseKind;
		if (mobile != nullptr)
		{
			looseKind = mobile->type;
		}
		else if (registry.AllOf<Scaffold>(thing) && Locator::infoConstants::has_value())
		{
			looseKind = Locator::infoConstants::value().scaffold.mobileType;
		}
		facts.mushroom = (feature != nullptr && feature->type == FeatureInfo::MagicMushroom) ||
		                 looseKind == MobileObjectInfo::MagicMushroom || looseKind == MobileObjectInfo::Toadstool;
	}
	if (desire == Desire::Poo && kind != Thing::Spell && kind != Thing::Other)
	{
		facts.height = object_measures::Height(registry, thing);
	}
	if (desire == Desire::BringStuffHome && Locator::creatureObjectActionSystem::has_value())
	{
		facts.creatureCanPickUp = Locator::creatureObjectActionSystem::value().CanPickUp(creature, thing);
	}
	return facts;
}
} // namespace

creature_activity::Thing mind_detail::KindOfThing(const ecs::Registry& registry, entt::entity thing)
{
	return KindOf(registry, thing);
}

std::optional<CreatureMindSystem::ChosenActivity>
CreatureMindSystem::ChooseActivity(entt::entity creature, const CreatureMindState& mind, Desire desire)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	const auto* at = registry.TryGet<const Transform>(creature);
	if (tables == nullptr || at == nullptr || !registry.AllOf<Creature>(creature))
	{
		return std::nullopt;
	}
	const auto& rules = tables->rules.at(static_cast<size_t>(desire));
	std::vector<ChosenActivity> found;
	std::vector<float> worth;
	const bool lookedAt = LookedAtByItsPlayer(creature);
	for (const auto* held : mind.beliefs.HeldNewestFirst())
	{
		const auto& belief = *held;
		const auto kind = creature_activity::WholeOf(belief.kind);
		if (!kind.has_value() || belief.thing == creature || !registry.Valid(belief.thing))
		{
			continue;
		}
		const auto facts = WholeFactsOf(registry, creature, belief.thing, *kind, *tables, lookedAt);
		if (!creature_activity::CanBeAbout(desire, facts, mind.mayAttackOwnPlayer))
		{
			continue;
		}
		const auto usefulness =
		    ActivityUsefulness(creature, mind, desire, belief.thing) *
		    mind_detail::Nearness(registry, creature, at->position, belief.thing, belief.place, rules.distanceWeight);
		found.push_back({.whole = belief.thing, .kind = *kind, .usefulness = usefulness});
		worth.push_back(usefulness);
	}
	const auto best = creature_activity::MostUseful(worth);
	return best.has_value() ? std::optional(found.at(*best)) : std::nullopt;
}

std::vector<creature_planner::ObjectCandidate> CreatureMindSystem::PartsToActOn(entt::entity creature,
                                                                                const CreatureMindState& mind, Desire desire,
                                                                                const ChosenActivity& activity)
{
	// A creature is its own part, whatever the creature can do to it
	if (activity.kind == creature_activity::Whole::Creature)
	{
		return {GoalWorth(creature, mind, desire, activity.whole, activity)};
	}
	const auto& registry = Locator::entitiesRegistry::value();
	std::vector<creature_planner::ObjectCandidate> parts;
	const auto add = [&](entt::entity thing) {
		if (thing == creature || !registry.Valid(thing) || NoWayLately(registry, mind, thing))
		{
			return;
		}
		const auto kind = mind_detail::KindOfThing(registry, thing);
		if (creature_activity::KindMaySuit(desire, kind) &&
		    creature_activity::ThingSuits(desire, ThingFactsOf(registry, creature, desire, thing, kind, GetTables())))
		{
			parts.push_back(GoalWorth(creature, mind, desire, thing, activity));
		}
	};
	for (const auto& belief : mind.beliefs.PartsOf(activity.whole))
	{
		if (const auto acted = ActedThingOf(registry, belief.thing))
		{
			add(*acted);
		}
	}
	return parts;
}

bool CreatureMindSystem::SuitsDesire(entt::entity creature, CreatureMindState& mind, Desire desire, entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(thing) && SuitsDesireOfKind(creature, mind, desire, thing, mind_detail::KindOfThing(registry, thing));
}

bool CreatureMindSystem::SuitsDesireOfKind(entt::entity creature, CreatureMindState& mind, Desire desire, entt::entity thing,
                                           creature_activity::Thing kind)
{
	const auto& registry = Locator::entitiesRegistry::value();
	// The test of the kind comes first because it costs less. The other tests only read the world and the beliefs, but
	// the test for hunger can change the beliefs. Any kind can suit hunger, thus that test stays in its sequence.
	if (!creature_activity::KindMaySuit(desire, kind))
	{
		return false;
	}
	// The two tests below read the same belief, and nothing between them changes the beliefs
	const auto* belief = mind.beliefs.Find(thing);
	if (NoWayLately(registry, mind, belief, thing))
	{
		return false;
	}
	// Kept within an area, it does not choose a thing that it believes is outside the area
	if (const auto place = BelievedPlaceOf(registry, belief, thing);
	    place.has_value() && OutsideConfinedArea(registry, creature, *place))
	{
		return false;
	}
	if (desire == Desire::Hunger)
	{
		if (const auto town = TownOfThing(registry, thing))
		{
			ComeAcross(creature, mind, *town);
			if (ActivityUsefulness(creature, mind, Desire::Compassion, *town) > k_MostCompassionToEatFrom)
			{
				return false;
			}
		}
	}
	return creature_activity::ThingSuits(desire, ThingFactsOf(registry, creature, desire, thing, kind, GetTables()));
}

std::vector<creature_planner::ObjectCandidate>
CreatureMindSystem::BelievedThingsToActOn(entt::entity creature, CreatureMindState& mind, Desire desire,
                                          const std::function<bool(entt::entity)>& suitsAction)
{
	BelievedFacts unkept;
	return BelievedThingsToActOn(creature, mind, desire, suitsAction, unkept);
}

std::vector<creature_planner::ObjectCandidate>
CreatureMindSystem::BelievedThingsToActOn(entt::entity creature, CreatureMindState& mind, Desire desire,
                                          const std::function<bool(entt::entity)>& suitsAction, BelievedFacts& kept)
{
	std::vector<creature_planner::ObjectCandidate> found;
	// The facts stay true while no come-across changes what the tests read. The test of a thing for the desire reads only
	// the beliefs and the world, and nothing else in the planning of a desire changes them. The tests came across some
	// things that changed nothing that they read. The creature comes across these things again in the same sequence, so
	// that what else the come-acrosses change, such as the desire sources, changes as when the tests run again.
	if (kept.foundAt == _comeAcrossCount)
	{
		for (const auto thing : kept.comeAcross)
		{
			ComeAcross(creature, mind, thing);
		}
		if (kept.foundAt != _comeAcrossCount)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"),
			                    "A come-across that changed nothing changed the beliefs when it came again");
		}
		for (auto& suiting : kept.suiting)
		{
			if (!suitsAction(suiting.thing))
			{
				continue;
			}
			if (!suiting.worth.has_value())
			{
				suiting.worth = GoalWorth(creature, mind, desire, suiting.thing, std::nullopt);
			}
			found.push_back(*suiting.worth);
		}
		return found;
	}
	// The tests come in the same sequence as without the kept facts
	const auto startAt = _comeAcrossCount;
	kept.suiting.clear();
	kept.comeAcross.clear();
	auto* const outerLog = std::exchange(_comeAcrossLog, &kept.comeAcross);
	const auto& registry = Locator::entitiesRegistry::value();
	// The tests can come across things and change the beliefs. Thus the code takes the things to examine first.
	const auto held = mind.beliefs.HeldInListOrder();
	kept.things.clear();
	kept.things.reserve(held.size());
	std::ranges::transform(held, std::back_inserter(kept.things), &creature_object_beliefs::Belief::thing);
	kept.suiting.reserve(kept.things.size());
	const KindStores kinds(registry);
	for (const auto believedThing : kept.things)
	{
		const auto thing = ActedThingOf(registry, believedThing);
		if (!thing.has_value() || *thing == creature || !registry.Valid(*thing) ||
		    !SuitsDesireOfKind(creature, mind, desire, *thing, KindOf(kinds, *thing)))
		{
			continue;
		}
		auto& suiting = kept.suiting.emplace_back(BelievedFacts::Suiting {.thing = *thing});
		if (!suitsAction(*thing))
		{
			continue;
		}
		suiting.worth = GoalWorth(creature, mind, desire, *thing, std::nullopt);
		found.push_back(*suiting.worth);
	}
	_comeAcrossLog = outerLog;
	// When a come-across during this search changes what the tests read, the facts can change. The next action then does
	// not use them.
	kept.foundAt = _comeAcrossCount == startAt ? std::optional(startAt) : std::nullopt;
	if (outerLog != nullptr)
	{
		outerLog->insert(outerLog->end(), kept.comeAcross.begin(), kept.comeAcross.end());
	}
	return found;
}

creature_planner::ObjectCandidate CreatureMindSystem::GoalWorth(entt::entity creature, const CreatureMindState& mind,
                                                                Desire desire, entt::entity thing,
                                                                const std::optional<ChosenActivity>& activity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	creature_planner::ObjectCandidate candidate {.id = entt::to_integral(thing)};
	const auto* at = registry.TryGet<const Transform>(creature);
	if (tables == nullptr || at == nullptr || !mind.learnt.has_value())
	{
		return candidate;
	}
	// A part of the whole thing that the desire is about has the worth that the creature learnt for things to act on.
	// The worth of the whole thing counts for the plan. A creature is its own part, with the worth of the whole thing.
	if (activity.has_value())
	{
		const auto partUsefulness = ObjectToActOnUsefulness(creature, mind, desire, thing);
		candidate.usefulness = activity->kind == creature_activity::Whole::Creature ? activity->usefulness : partUsefulness;
		candidate.activityUsefulness = activity->usefulness;
		candidate.actedOnUsefulness = creature_activity::ActedOnUsefulness(desire, partUsefulness);
		return candidate;
	}
	// Without a whole thing, a thing has the worth that the creature learnt for things to act on, for the distance
	candidate.usefulness = ObjectToActOnUsefulness(creature, mind, desire, thing);
	candidate.actedOnUsefulness = candidate.usefulness;
	return candidate;
}
