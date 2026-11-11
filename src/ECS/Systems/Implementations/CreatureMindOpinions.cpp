/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a creature believes about a thing, and how useful it finds the thing for a desire

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

namespace town_compassion = openblack::creature_town_compassion;

namespace openblack::ecs::systems::mind_detail
{
uint32_t PlayerNumberOf(PlayerNames owner)
{
	return static_cast<uint32_t>(owner);
}

std::optional<creature_object_actions::Toy> ToyOf(const ecs::Registry& registry, entt::entity entity)
{
	if (registry.AllOf<Ball>(entity))
	{
		return creature_object_actions::Toy::Ball;
	}
	const auto* still = registry.TryGet<const MobileStatic>(entity);
	if (still == nullptr || !Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& statics = Locator::infoConstants::value().mobileStatic;
	const auto kind = static_cast<size_t>(still->type);
	return kind < statics.size() ? creature_object_actions::ToyOf(statics.at(kind).meshId) : std::nullopt;
}
} // namespace openblack::ecs::systems::mind_detail

namespace
{
float Usefulness(const creature_mind_model::Learnt& learnt, Desire desire, const std::optional<creature_tree::Belief>& belief)
{
	if (!belief.has_value())
	{
		return creature_planner::k_DefaultUsefulness;
	}
	const auto& tree = learnt.trees.at(static_cast<size_t>(TreeKind::Activity)).at(static_cast<size_t>(desire));
	return creature_tree::Usefulness(creature_tree::Evaluate(tree, *belief));
}

/// The player number of a town, none when there is no town
std::optional<uint32_t> PlayerOfTown(const ecs::Registry& registry, std::optional<entt::entity> town)
{
	const auto* data = town.has_value() && registry.Valid(*town) ? registry.TryGet<const Town>(*town) : nullptr;
	return data != nullptr ? std::optional(PlayerNumberOf(data->owner)) : std::nullopt;
}
} // namespace

namespace openblack::ecs::systems::mind_detail
{
std::optional<uint32_t> BeliefPlayerOf(const ecs::Registry& registry, entt::entity thing)
{
	if (const auto* creature = registry.TryGet<const Creature>(thing))
	{
		return PlayerNumberOf(creature->owner);
	}
	if (const auto* town = registry.TryGet<const Town>(thing))
	{
		return PlayerNumberOf(town->owner);
	}
	if (const auto* temple = registry.TryGet<const Temple>(thing))
	{
		return PlayerNumberOf(temple->owner);
	}
	if (const auto* site = registry.TryGet<const WorshipSite>(thing))
	{
		return PlayerNumberOf(site->player);
	}
	// An altar is of the player of its site's temple
	if (const auto* altar = registry.TryGet<const WorshipAltar>(thing))
	{
		const auto* site = registry.Valid(altar->site) ? registry.TryGet<const WorshipSite>(altar->site) : nullptr;
		return PlayerNumberOf(site != nullptr ? site->player : PlayerNames::NEUTRAL);
	}
	if (const auto* spell = registry.TryGet<const ecs::components::Spell>(thing))
	{
		switch (spell->caster.kind)
		{
		case SpellCaster::Kind::Player:
			return PlayerNumberOf(spell->caster.player);
		case SpellCaster::Kind::Creature:
		case SpellCaster::Kind::Object:
			return registry.Valid(spell->caster.entity) && spell->caster.entity != thing
			           ? BeliefPlayerOf(registry, spell->caster.entity)
			           : std::nullopt;
		default:
			return std::nullopt;
		}
	}
	if (const auto* pile = registry.TryGet<const MagicPile>(thing))
	{
		return PlayerNumberOf(pile->player);
	}
	// A shield is of the player of the miracle that keeps it up, and of the neutral player when the miracle let go of it
	if (const auto* shield = registry.TryGet<const MagicShield>(thing))
	{
		return registry.Valid(shield->spell) && shield->spell != thing ? BeliefPlayerOf(registry, shield->spell)
		                                                               : PlayerNumberOf(PlayerNames::NEUTRAL);
	}
	// A fireball is of the player of its miracle, and of the neutral player when its miracle has no player
	if (const auto* fireball = registry.TryGet<const MagicFireBall>(thing))
	{
		return PlayerNumberOf(fireball->hasPlayer ? fireball->player : PlayerNames::NEUTRAL);
	}
	if (const auto* icon = registry.TryGet<const TownCentreIcon>(thing))
	{
		return PlayerOfTown(registry, icon->town);
	}
	// An icon at a worship site is of the player of its site
	if (registry.AllOf<WorshipIcon>(thing))
	{
		return PlayerNumberOf(ecs::world_objects::IconPlayer(registry, thing));
	}
	// A villager, a field or a fish farm with no town is of no player; a building, a totem or a pile with no town is of
	// the neutral player
	if (registry.AnyOf<Villager, Abode, Field, FishFarm, VillageTotem, Pot>(thing))
	{
		if (const auto player = PlayerOfTown(registry, TownOfThing(registry, thing)))
		{
			return player;
		}
		if (registry.AnyOf<Villager, Field, FishFarm>(thing))
		{
			return std::nullopt;
		}
		return PlayerNumberOf(PlayerNames::NEUTRAL);
	}
	// Trees, forests and flocks have no player
	if (registry.AnyOf<Tree, ForestTurns, Flock>(thing))
	{
		return std::nullopt;
	}
	return PlayerNumberOf(PlayerNames::NEUTRAL);
}
} // namespace openblack::ecs::systems::mind_detail

namespace
{
/// Whether a thing is natural, made by people, or made by magic
uint32_t OriginOf(const ecs::Registry& registry, entt::entity thing)
{
	namespace origin = creature_tree::origin;
	if (registry.AnyOf<Temple, ecs::components::Spell>(thing))
	{
		return origin::k_Magic;
	}
	// A worship site is a part of the temple, which is natural
	if (registry.AllOf<WorshipSite>(thing))
	{
		return origin::k_Natural;
	}
	return registry.AnyOf<Town, Abode, Field>(thing) ? origin::k_ManMade : origin::k_Natural;
}

/// The size that the creature of a creature's player shows: the first creature of the player, else the creature itself
float SizeOfPlayersCreature(const ecs::Registry& registry, const Creature& me)
{
	if (Locator::playerSystem::has_value())
	{
		if (const auto primary = Locator::playerSystem::value().GetPrimaryCreature(me.owner))
		{
			if (const auto* body = registry.TryGet<const Creature>(*primary))
			{
				return ShownSize(*body);
			}
		}
	}
	return ShownSize(me);
}

/// How many trees a forest has, grown and growing
size_t TreesInForest(entt::entity forest)
{
	if (!Locator::forestSystem::has_value())
	{
		return 0;
	}
	const auto& forests = Locator::forestSystem::value();
	return forests.TreesOf(forest, false).size() + forests.TreesOf(forest, true).size();
}

/// The desire that another creature shows: the desire that the action of its plan shows, or of its last plan
uint32_t ShownDesireOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	const auto* tables = Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().GetTables() : nullptr;
	if (mind == nullptr || tables == nullptr)
	{
		return 0;
	}
	const auto action = mind->planner.current.has_value() ? mind->planner.current->action : mind->planner.lastAction;
	return action < tables->actions.size() ? tables->actions[action].shownDesire : 0;
}

/// How many miracles another creature knows
size_t MiraclesKnownBy(const ecs::Registry& registry, entt::entity creature)
{
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	if (mind == nullptr || !mind->learnt.has_value())
	{
		return 0;
	}
	return static_cast<size_t>(std::ranges::count(mind->learnt->knowledge.miraclesKnown, true));
}

/// The object type of the thing that another creature carries, none when it carries no thing
std::optional<uint32_t> CarriedObjectTypeOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto held = ecs::creature_carried::CarriedBy(registry, creature);
	if (!held.has_value())
	{
		return std::nullopt;
	}
	return mind_detail::ObjectSortOf(registry, *held);
}

/// The attributes only the beliefs about villagers, creatures, buildings, towns and forests have
void SetKindAttributes(const ecs::Registry& registry, entt::entity entity, const Creature* me, creature_tree::Belief& belief)
{
	using creature_tree::Attribute;
	namespace types = creature_tree::belief_types;
	const auto onFire = [entity]() -> uint32_t {
		return creature_tree::OnFireValue(Locator::fireSystem::has_value() && Locator::fireSystem::value().IsOnFire(entity));
	};
	switch (belief.type)
	{
	case types::k_Villager:
		if (const auto* villager = registry.TryGet<const Villager>(entity))
		{
			belief.Set(Attribute::Sex, static_cast<uint32_t>(villager->sex));
			belief.Set(Attribute::VillagerJob, static_cast<uint32_t>(villager->number));
			belief.Set(Attribute::Life, creature_tree::LifeValue(villager->life));
			belief.Set(Attribute::OnFire, onFire());
			belief.Set(Attribute::Tribe, static_cast<uint32_t>(villager->tribe));
		}
		break;
	case types::k_Creature:
		if (const auto* creature = registry.TryGet<const Creature>(entity))
		{
			belief.Set(Attribute::Height, creature_tree::CreatureHeight(ShownSize(*creature)));
			belief.Set(Attribute::SpellKnowledge, creature_tree::SpellKnowledgeValue(MiraclesKnownBy(registry, entity)));
			belief.Set(Attribute::Carrying, creature_tree::CarryingValue(CarriedObjectTypeOf(registry, entity)));
			belief.Set(Attribute::DominantDesire, creature_tree::DominantDesireValue(ShownDesireOf(registry, entity)));
		}
		break;
	case types::k_Abode:
		if (const auto* abode = registry.TryGet<const Abode>(entity))
		{
			belief.Set(Attribute::AbodeType, static_cast<uint32_t>(abode->type));
			belief.Set(Attribute::Life, creature_tree::LifeValue(ecs::world_objects::LifeOf(entity)));
			belief.Set(Attribute::OnFire, onFire());
			belief.Set(Attribute::AbodeBeingBuilt, creature_tree::BeingBuiltValue(ecs::construction::IsBuilt(entity)));
		}
		break;
	case types::k_Town:
	{
		float faith = 0.0f;
		if (const auto* impression = registry.TryGet<const TownImpression>(entity); impression != nullptr && me != nullptr)
		{
			faith = impression->belief.belief.at(static_cast<size_t>(me->owner));
		}
		belief.Set(Attribute::TownReligiousBelief, town_compassion::ReligiousBelief(faith));
		const auto* wants = registry.TryGet<const TownDesire>(entity);
		belief.Set(Attribute::TownNeedsMost,
		           town_compassion::NeedsMost(wants != nullptr ? ecs::town_desire::GetMostDesired(*wants) : -1));
		const auto* stats = registry.TryGet<const TownStats>(entity);
		belief.Set(Attribute::TownSize, town_compassion::TownSize(stats != nullptr ? stats->adults + stats->children : 0));
		const auto* tribe = registry.TryGet<const Tribe>(entity);
		// No tribe counts as the last
		belief.Set(Attribute::Tribe,
		           tribe != nullptr ? std::min(static_cast<uint32_t>(*tribe), static_cast<uint32_t>(Tribe::TIBETAN)) : 0);
		break;
	}
	case types::k_Forest:
		belief.Set(Attribute::ForestSize, creature_tree::ForestSize(TreesInForest(entity)));
		break;
	default:
		break;
	}
}

/// The belief about the heart of a temple: a part of the temple of the temple's player, at the heart's exact map position
std::optional<creature_tree::Belief> HeartBeliefOf(const ecs::Registry& registry, entt::entity temple, entt::entity heart,
                                                   entt::entity self)
{
	const auto* building = registry.Valid(temple) ? registry.TryGet<const Temple>(temple) : nullptr;
	if (building == nullptr)
	{
		return std::nullopt;
	}
	const auto* me = registry.TryGet<const Creature>(self);
	auto belief = creature_tree::CommonBelief({
	    .type = creature_tree::belief_types::k_CitadelPart,
	    .player = PlayerNumberOf(building->owner),
	    .creaturesPlayer = me != nullptr ? std::optional(PlayerNumberOf(me->owner)) : std::nullopt,
	});
	const auto place = ecs::thing_map_place::Of(registry, heart);
	belief.x = place.x;
	belief.z = place.z;
	return belief;
}
} // namespace

std::optional<creature_tree::Belief> mind_detail::BeliefOf(const ecs::Registry& registry, entt::entity entity,
                                                           entt::entity self)
{
	const auto* mind = registry.Valid(entity) ? registry.TryGet<const CreatureMindState>(self) : nullptr;
	if (const auto* known = mind != nullptr ? mind->beliefs.Find(entity) : nullptr;
	    known != nullptr && known->attributes.has_value())
	{
		auto belief = *known->attributes;
		belief.x = known->place.x;
		belief.z = known->place.z;
		return belief;
	}
	return BeliefNowOf(registry, entity, self);
}

std::optional<creature_tree::Belief> mind_detail::BeliefNowOf(const ecs::Registry& registry, entt::entity entity,
                                                              entt::entity self)
{
	if (!registry.Valid(entity))
	{
		return std::nullopt;
	}
	// A temple's heart has a kind of belief, thus the search below gives it to the function that finds the heart
	const auto type = BeliefKindOf(registry, entity);
	if (!type.has_value())
	{
		return std::nullopt;
	}
	return BeliefNowOf(registry, entity, self, *type, ecs::thing_map_place::Of(registry, entity));
}

std::optional<creature_tree::Belief> mind_detail::BeliefNowOf(const ecs::Registry& registry, entt::entity entity,
                                                              entt::entity self, uint32_t kind,
                                                              const map_coords::MapCoords& place)
{
	if (const auto* heart = registry.TryGet<const TempleHeart>(entity))
	{
		return HeartBeliefOf(registry, heart->temple, entity, self);
	}
	const auto* me = registry.TryGet<const Creature>(self);
	const auto* creature = registry.TryGet<const Creature>(entity);
	auto belief = creature_tree::CommonBelief({
	    .type = kind,
	    .origin = OriginOf(registry, entity),
	    .animate = registry.AnyOf<Villager, Animal, Creature>(entity),
	    .player = BeliefPlayerOf(registry, entity),
	    .creaturesPlayer = me != nullptr ? std::optional(PlayerNumberOf(me->owner)) : std::nullopt,
	    .harderThanMe = creature != nullptr && me != nullptr && ShownSize(*creature) > SizeOfPlayersCreature(registry, *me),
	    .creatureType = creature != nullptr ? static_cast<uint32_t>(creature::InfoRow(creature->species)) : 0,
	});
	SetKindAttributes(registry, entity, me, belief);
	belief.x = place.x;
	belief.z = place.z;
	return belief;
}

std::optional<creature_tree::Belief> CreatureMindSystem::BeliefAbout(entt::entity creature, entt::entity thing) const
{
	return mind_detail::BeliefOf(Locator::entitiesRegistry::value(), thing, creature);
}

std::optional<creature_tree::Belief> CreatureMindSystem::ActedOnBeliefAbout(entt::entity creature, entt::entity thing) const
{
	return mind_detail::ActedOnBeliefOf(Locator::entitiesRegistry::value(), thing, creature);
}

std::optional<creature_tree::Belief> mind_detail::ActedOnBeliefOf(const ecs::Registry& registry, entt::entity entity,
                                                                  entt::entity self)
{
	// Acted on, the temple stands for its heart: the creature's belief about the heart
	if (registry.Valid(entity) && registry.AllOf<Temple>(entity))
	{
		const auto heart = ecs::temple_heart::Of(registry, entity);
		return heart != entt::null ? BeliefOf(registry, heart, self) : HeartBeliefOf(registry, entity, entity, self);
	}
	return BeliefOf(registry, entity, self);
}

std::optional<uint32_t> mind_detail::LessonActivity(const creature_mind_tables::Tables& tables, Desire desire,
                                                    std::optional<uint32_t> object, std::optional<uint32_t> about)
{
	return creature_learning::ActivityOf(tables.rules.at(static_cast<size_t>(desire)).aboutSomething, object, about);
}

namespace
{
/// Whether a creature kept within an area by its leash has the thing farther from it than the area's radius
bool OutsideConfinement(const ecs::Registry& registry, entt::entity creature, entt::entity object)
{
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	if (leashes == nullptr)
	{
		return false;
	}
	const bool leashed = leashes->worn.has_value();
	const bool works = !leashed || leashes->worn->works;
	const auto* at = registry.TryGet<const Transform>(creature);
	const auto* objectAt = registry.TryGet<const Transform>(object);
	return creature_leash::IsConfined(leashes->confinementRadius, leashed, works) && at != nullptr && objectAt != nullptr &&
	       gutils::GetDistanceInMetres(at->position, objectAt->position) > leashes->confinementRadius;
}
} // namespace

namespace openblack::ecs::systems::mind_detail
{
bool DoingSomethingInteresting(const ecs::Registry& registry, const CreatureMindState& mind, entt::entity object)
{
	const auto* action = registry.TryGet<const LivingAction>(object);
	if (!registry.AllOf<Villager>(object) || action == nullptr || !mind.learnt.has_value() ||
	    !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto state = action->states.at(static_cast<size_t>(LivingAction::Index::Top));
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	if (state >= table.size())
	{
		return false;
	}
	return creature_mind::DoingSomethingInteresting(table.at(state).teachesSkill, mind.learnt->knowledge.skillsKnown);
}
} // namespace openblack::ecs::systems::mind_detail

namespace
{
/// Curious, a creature cares little for a kind of thing it has had a good look at, unless the thing does something it
/// finds interesting. Only objects are looked at that way.
float ObservedShare(const ecs::Registry& registry, const CreatureMindState& mind, Desire desire, entt::entity object,
                    const std::optional<creature_tree::Belief>& belief)
{
	if (desire != Desire::Curiosity || !belief.has_value() ||
	    !creature_mind::IsLookedOverAsAKind(belief->type, registry.AllOf<Mist>(object)))
	{
		return 1.0f;
	}
	return creature_mind::CuriosityShare(mind.observedKinds.contains(belief->type),
	                                     DoingSomethingInteresting(registry, mind, object));
}

/// How useful a thing is to act on for a desire, by what the creature has learnt and what it has looked over
float ObjectUsefulness(const ecs::Registry& registry, const creature_mind_model::Learnt& learnt, const CreatureMindState& mind,
                       Desire desire, entt::entity object, entt::entity creature)
{
	const auto belief = BeliefOf(registry, object, creature);
	return Usefulness(learnt, desire, belief) * ObservedShare(registry, mind, desire, object, belief);
}
} // namespace

float mind_detail::Nearness(const ecs::Registry& registry, entt::entity creature, const glm::vec3& at, entt::entity object,
                            const map_coords::MapCoords& place, float distanceWeight)
{
	glm::vec3 from = at;
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	if (leashes != nullptr && leashes->worn.has_value() && leashes->worn->tiedTo.has_value() &&
	    registry.Valid(*leashes->worn->tiedTo))
	{
		if (const auto* tied = registry.TryGet<const Transform>(*leashes->worn->tiedTo))
		{
			from = tied->position;
		}
	}
	return creature_planner::DistancePriority(ecs::creature_carried::Carries(registry, creature, object),
	                                          gutils::GetDistanceInMetres(place, map_coords::FromMetres(glm::xz(from))),
	                                          distanceWeight);
}

float CreatureMindSystem::ActivityUsefulness(entt::entity creature, const CreatureMindState& mind, Desire desire,
                                             entt::entity object) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!mind.learnt.has_value() || !registry.Valid(object))
	{
		return 0.0f;
	}
	// The game also counts a thing useless for a few desires when nothing it knows of belongs to it. Nothing reaches
	// here that way: what a fire belongs to always has the burning thing belonging to it, or is the creature itself.
	if (const auto* leashes = registry.TryGet<const CreatureLeash>(creature))
	{
		// Leashed and tied to a thing, it only acts on what that thing belongs to
		if (leashes->worn.has_value() && leashes->worn->tiedTo.has_value())
		{
			const auto tiedOwner = BelongsTo(*leashes->worn->tiedTo);
			if (tiedOwner.has_value() && *tiedOwner != object)
			{
				return 0.0f;
			}
		}
		// Kept within an area, it doesn't act on what lies farther from it than the area's radius
		if (OutsideConfinement(registry, creature, object))
		{
			return 0.0f;
		}
	}
	// A creature is never angry with a friend
	if (desire == Desire::Anger && registry.AllOf<Creature>(object) && mind_detail::AreFriends(registry, object, creature))
	{
		return 0.0f;
	}
	return ObjectUsefulness(registry, *mind.learnt, mind, desire, object, creature);
}

float CreatureMindSystem::ObjectToActOnUsefulness(entt::entity creature, const CreatureMindState& mind, Desire desire,
                                                  entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto place = BelievedPlace(mind, object);
	const auto* tables = GetTables();
	if (!mind.learnt.has_value() || !place.has_value() || tables == nullptr || !registry.AllOf<Transform>(creature))
	{
		return 0.0f;
	}
	const auto& at = registry.Get<const Transform>(creature).position;
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	// Kept within an area, it doesn't act on what it believes lies farther from it than the area's radius
	// TODO(creature-reactions-2): the game makes one exception, for one desire and a sort of thing that answers a test of
	// its own; no creature, villager, animal or other living thing answers it, and the rest aren't mapped yet
	if (leashes != nullptr)
	{
		const bool leashed = leashes->worn.has_value();
		const bool works = !leashed || leashes->worn->works;
		if (creature_leash::IsConfined(leashes->confinementRadius, leashed, works) &&
		    gutils::GetDistanceInMetres(map_coords::FromMetres(glm::xz(at)), *place) > leashes->confinementRadius)
		{
			return 0.0f;
		}
	}
	const auto belief = ActedOnBeliefOf(registry, object, creature);
	// Curious, it finds a thing that does something interesting five times as useful. Playful, it finds a villager that
	// plays football five times as useful, and a toy three times as useful. Curious, it finds a kind that it has had a
	// good look at a hundredth as useful, in place of the multiplier.
	const float observed = ObservedShare(registry, mind, desire, object, belief);
	const float share =
	    observed < 1.0f ? observed
	                    : creature_mind::DesireMultiplier(
	                          desire, {.doingSomethingInteresting =
	                                       desire == Desire::Curiosity && DoingSomethingInteresting(registry, mind, object),
	                                   .playingFootball = desire == Desire::Play && mind_football::IsPlaying(registry, object),
	                                   .toy = desire == Desire::Play && ToyOf(registry, object).has_value()});
	const auto distance =
	    Nearness(registry, creature, at, object, *place, tables->rules.at(static_cast<size_t>(desire)).distanceWeight);
	float learnt = creature_planner::k_DefaultUsefulness;
	if (belief.has_value())
	{
		const auto& tree = mind.learnt->trees.at(static_cast<size_t>(TreeKind::ActOn)).at(static_cast<size_t>(desire));
		learnt = creature_tree::Usefulness(creature_tree::Evaluate(tree, *belief));
	}
	return learnt * distance * share;
}

namespace openblack::ecs::systems::mind_detail
{
std::optional<entt::entity> TownOfBuilding(const ecs::Registry& registry, uint32_t townId)
{
	const auto& towns = registry.Context().towns;
	const auto found = towns.find(townId);
	return found != towns.end() && registry.Valid(found->second) ? std::optional(found->second) : std::nullopt;
}

std::optional<entt::entity> TownOfTotem(const ecs::Registry& registry, entt::entity totem)
{
	const auto& self = registry.Get<const VillageTotem>(totem);
	const auto* centre = registry.Valid(self.townCentre) ? registry.TryGet<const Abode>(self.townCentre) : nullptr;
	return centre != nullptr ? TownOfBuilding(registry, centre->townId) : std::nullopt;
}

std::optional<entt::entity> TownOfThing(const ecs::Registry& registry, entt::entity thing)
{
	const auto town = [&registry](entt::entity found) {
		return registry.Valid(found) && registry.AllOf<Town>(found) ? std::optional(found) : std::nullopt;
	};
	if (registry.AnyOf<Villager, Abode, Field>(thing) && !registry.AllOf<WorshipSite>(thing))
	{
		const auto belongsTo = CreatureMindSystem::BelongsTo(thing);
		return belongsTo.has_value() ? town(*belongsTo) : std::nullopt;
	}
	if (const auto* farm = registry.TryGet<const FishFarm>(thing))
	{
		return town(farm->town);
	}
	// A town's desire flag is of its town
	if (const auto* flag = registry.TryGet<const TownDesireFlag>(thing))
	{
		return town(flag->town);
	}
	if (registry.AllOf<VillageTotem>(thing))
	{
		return TownOfTotem(registry, thing);
	}
	if (registry.AllOf<Pot>(thing))
	{
		std::optional<entt::entity> pit;
		registry.Each<const StoragePit>([&](entt::entity entity, const StoragePit& store) {
			if (store.foodPile == thing || std::ranges::find(store.woodPiles, thing) != store.woodPiles.end())
			{
				pit = entity;
			}
		});
		if (const auto* abode = pit.has_value() ? registry.TryGet<const Abode>(*pit) : nullptr)
		{
			const auto& towns = registry.Context().towns;
			const auto found = towns.find(abode->townId);
			return found != towns.end() ? town(found->second) : std::nullopt;
		}
	}
	return std::nullopt;
}
} // namespace openblack::ecs::systems::mind_detail
