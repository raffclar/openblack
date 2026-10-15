/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

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
#include "Camera/Camera.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureActionHistory.h"
#include "Creature/CreatureActivity.h"
#include "Creature/CreatureAttention.h"
#include "Creature/CreatureBringHome.h"
#include "Creature/CreatureDance.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesireSources.h"
#include "Creature/CreatureDesires.h"
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
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreaturePlayerAttention.h"
#include "Creature/CreatureRestAndHome.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureSacrifice.h"
#include "Creature/CreatureScriptPlay.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/CreatureTownCompassion.h"
#include "Creature/CreatureTraining.h"
#include "Creature/CreatureTurnOrder.h"
#include "Creature/CreatureWatching.h"
#include "Creature/LeashOrders.h"
#include "Creature/LeashRules.h"
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
#include "ECS/Components/MagicPile.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureBodyFile.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreatureNaming.h"
#include "ECS/LearningMarker.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/TownDesire.h"
#include "ECS/WorldObjects.h"
#include "Graphics/Moon.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "MagicLiving.h"
#include "ObjectMeasures.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems::mind_detail;
using creature_desires::Desire;
using creature_mind_model::TreeKind;
using creature_plan_actions::Target;

namespace
{
/// The advisor may tell the player of what the creature learnt
void TellHelp(entt::entity creature, const openblack::creature_help::Message& message)
{
	if (openblack::Locator::creatureHelpSystem::has_value())
	{
		openblack::Locator::creatureHelpSystem::value().Raise(creature, message);
	}
}

namespace town_compassion = openblack::creature_town_compassion;
constexpr float k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();
constexpr float k_TurnsPerSecond = 1.0f / k_TurnSeconds;

/// Feedback teaches lessons only once the creature has grown up this far; before, it only stirs its feelings
constexpr uint32_t k_MinLessonPhase = 2;
/// A stroke holds back sadness this long
constexpr float k_StrokeSadnessSeconds = 90.0f;
/// The source of the desire to copy the player, and how much a stroke while copying adds to it
constexpr uint32_t k_FollowPlayerSource = 54;
constexpr float k_MimicStrokeBoost = 0.3f;
/// Noticing a deed it may copy adds this much to the first source of the desire behind the deed, and this much to its
/// wish to follow the player
constexpr float k_NoticedDeedSource = 0.1f;
constexpr float k_NoticedDeedFollow = 0.3f;
/// Starting to copy a deed on a thing teaches what the thing and its whole thing are worth with this feedback
constexpr float k_DeedSeenLesson = 0.5f;
/// Forgiven for running away, the threshold of its wish to run raises this much
constexpr float k_RunAwayThresholdStep = 0.1f;
/// A thing to use is learnt about at most this strongly for poo
constexpr float k_PooUseFeedback = 0.5f;
/// A creature watches skills, miracles and the player's deeds this far away at most
constexpr float k_ViewDistance = 150.0f;
/// Copying the player's desire makes that desire dominant for this many seconds
constexpr float k_MimicDesireSeconds = 120.0f;
/// A creature that copies the player's desire stops when it thought this many times with no plan since its last plan
constexpr uint32_t k_MostThinksWithoutPlanToCopyDesire = 40;
/// After that, it does not follow the player for this many seconds
constexpr float k_NoFollowAfterCopySeconds = 40.0f;

/// The land's script that curses the player's creature, and how tall a creature of size 1 is
constexpr std::string_view k_CurseScript = "CreatureCurse";
constexpr float k_HeightOfSizeOne = 15.0f;
/// A villager hurt to this share of its life or less can be healed by a creature
constexpr float k_HurtEnoughToHeal = 0.7f;
/// A pile of food this many times a storage pit's radius from it is the storage pit's
constexpr float k_StoragePitReach = 1.1f;
/// A toy further than this from its creature's home is one it may take home
constexpr float k_ToyAwayFromHome = 50.0f;

/// The actions that cast a power-up of a miracle, which only a creature grown up enough tries
bool IsPowerUpCast(std::string_view action)
{
	return action.ends_with("PU1") || action.ends_with("PU2");
}

/// While doing these, which it chose with nothing better to do, the creature changes to any plan good enough
bool Interruptible(creature_mind::Activity activity)
{
	using enum creature_mind::Activity;
	return activity == None || activity == BeIdle || activity == Sit || activity == HangAround;
}

/// A creature stroked for what it holds eats it as the action that eats a thing alive
constexpr std::string_view k_EatHeldAction = "EatAlive";
/// A creature that sees a villager do a skill it does not know follows the villager to watch it
constexpr std::string_view k_FollowToWatchAction = "ExamineByFollowing";

/// The action in the table of actions that gets the feedback for an agenda that no plan gave.
/// Only the forced agendas to eat, sleep, poo, be sick or drink are such agendas.
std::string_view ActionForActivity(creature_mind::Activity activity)
{
	using enum creature_mind::Activity;
	switch (activity)
	{
	case Eat:
		return "EatAfterExamining";
	case Sleep:
		return "SleepOnTheSpot";
	case Poo:
		return "Poo";
	case Puke:
		return "Puke";
	case Drink:
		return "DrinkFromTheSea";
	default:
		return {};
	}
}

/// How pressing the reaction a creature holds is, by the game's reaction table; 0 when it holds none
float ReactionPriorityOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* reaction = registry.TryGet<const LivingReaction>(creature);
	if (reaction == nullptr || reaction->type == Reaction::None || !Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	const auto& table = Locator::infoConstants::value().reaction;
	const auto row = static_cast<size_t>(reaction->type);
	return row < table.size() ? static_cast<float>(table.at(row).priority) : 0.0f;
}

std::optional<glm::vec2> PointOf(const ecs::Registry& registry, entt::entity entity)
{
	if (!registry.Valid(entity))
	{
		return std::nullopt;
	}
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	return glm::vec2(transform->position.x, transform->position.z);
}

uint32_t PlayerNumberOf(PlayerNames owner)
{
	return static_cast<uint32_t>(owner);
}

/// A creature takes no tree to plant from within this many metres of where the player did what it last set about copying
constexpr float k_PlantFarFromCopied = 20.0f;

/// Which toy the thing is: the football is a ball, and of the statics those with a toy's model
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

/// The one-shot miracle that a globe holds, as a creature sees it; none for a thing that is not a globe
std::optional<creature_plan_actions::OneShot> OneShotOf(const ecs::Registry& registry, entt::entity entity)
{
	const auto* globe = registry.Valid(entity) ? registry.TryGet<const OneOffSpellSeed>(entity) : nullptr;
	if (globe == nullptr || !Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	// The kinds come from the first miracle of the globe's seed, whatever its power-up
	const auto& info = Locator::infoConstants::value();
	const auto type = magic::GetSpellSeedInfo(info, globe->seedType).magicTypes[0];
	const auto& miracle = magic::GetMagicInfo(info, type);
	return creature_plan_actions::OneShot {.magicType = static_cast<uint32_t>(type),
	                                       .aggressive = miracle.oneOffSpellIsAggressive != 0,
	                                       .compassionate = miracle.oneOffSpellIsCompassionate != 0,
	                                       .playful = miracle.oneOffSpellIsPlayful != 0,
	                                       .restoresHealth = miracle.oneOffSpellIsToRestoreHealth != 0};
}

/// The food pile of a storage pit, while the pile is there
std::optional<entt::entity> StoreFoodPileOf(const ecs::Registry& registry, entt::entity pit)
{
	const auto* store = registry.TryGet<const StoragePit>(pit);
	if (store == nullptr || store->foodPile == entt::null || !registry.Valid(store->foodPile))
	{
		return std::nullopt;
	}
	return store->foodPile;
}

/// Put the food pile of a storage pit, and the point of the pile, in the situation of an action on the pit
void SituateStoreFood(const ecs::Registry& registry, entt::entity pit, creature_plan_actions::Situation& situation)
{
	const auto pile = StoreFoodPileOf(registry, pit);
	const auto at = pile.has_value() ? PointOf(registry, *pile) : std::nullopt;
	if (at.has_value())
	{
		situation.storeFood = creature_plan_actions::Situation::FoodPile {.pile = entt::to_integral(*pile), .point = *at};
	}
}

/// The game's turn now
uint32_t TurnNow()
{
	return Locator::time::has_value() ? static_cast<uint32_t>(Locator::time::value().GetTurn()) : 0;
}

/// Whether the creature can stroke a thing to heal it: a thing that it can pick up and that has lost some of its life
bool Strokeable(const ecs::Registry& registry, entt::entity entity, entt::entity self)
{
	const auto life = registry.Valid(entity) ? magic_living::LifeOf(entity) : std::nullopt;
	return life.has_value() && *life < 1.0f && Locator::creatureObjectActionSystem::has_value() &&
	       Locator::creatureObjectActionSystem::value().CanPickUp(self, entity);
}

creature_activity::Thing KindOfThing(const ecs::Registry& registry, entt::entity thing);
bool DoingSomethingInteresting(const ecs::Registry& registry, const CreatureMindState& mind, entt::entity object);

bool Accepts(const ecs::Registry& registry, Target target, entt::entity entity, entt::entity self)
{
	if (entity == self || !registry.Valid(entity))
	{
		return false;
	}
	const auto* hands =
	    Locator::creatureObjectActionSystem::has_value() ? &Locator::creatureObjectActionSystem::value() : nullptr;
	const bool heldByOther = [&] {
		const auto* held = registry.TryGet<const HeldByCreature>(entity);
		return held != nullptr && held->creature != self;
	}();
	switch (target)
	{
	case Target::None:
		return false;
	case Target::Food:
		return !heldByOther && hands != nullptr && hands->CanEat(self, entity);
	case Target::Pickable:
		return !heldByOther && hands != nullptr && hands->CanPickUp(self, entity);
	case Target::Toy:
		return !heldByOther && ToyOf(registry, entity).has_value() && hands != nullptr && hands->CanPickUp(self, entity);
	case Target::ToyBall:
		return !heldByOther && ToyOf(registry, entity) == creature_object_actions::Toy::Ball;
	case Target::Destroyable:
		return hands != nullptr && hands->CanDestroy(entity);
	case Target::Tree:
		return registry.AllOf<Tree>(entity);
	case Target::Stompable:
		return hands != nullptr && hands->CanStompOn(self, entity);
	case Target::Kickable:
		return hands != nullptr && hands->CanKick(self, entity);
	case Target::StonableAndPickable:
		return !heldByOther && hands != nullptr &&
		       creature_object_actions::CanBeStoned(hands->KindOf(entity).thing,
		                                            registry.AllOf<VillagerDeath>(entity) || magic_living::IsDead(entity)) &&
		       hands->CanPickUp(self, entity);
	case Target::Villager:
		return registry.AllOf<Villager>(entity);
	case Target::DanceVillager:
	{
		// Measured across the land between map positions, as the game measures it
		const auto* at = registry.TryGet<const Transform>(entity);
		const auto* from = registry.TryGet<const Transform>(self);
		return registry.AllOf<Villager>(entity) && at != nullptr && from != nullptr &&
		       gutils::GetDistanceInMetres(from->position, at->position) < creature_dance::k_FurthestVillager;
	}
	case Target::HurtVillager:
	{
		// Hurt to seven tenths of its life or less
		const auto life = registry.AllOf<Villager>(entity) ? magic_living::LifeOf(entity) : std::nullopt;
		return life.has_value() && *life <= k_HurtEnoughToHeal;
	}
	case Target::Strokeable:
		return !heldByOther && Strokeable(registry, entity, self);
	case Target::MagicWoodPile:
	case Target::Cow:
	case Target::VillagerNotWorshipping:
	case Target::BreederVillager:
	case Target::VillagerFarFromHome:
	case Target::PlayerTotem:
		return mind_town_help::Accepts(registry, target, entity, self, heldByOther);
	case Target::Creature:
		return registry.AllOf<Creature>(entity);
	case Target::FightableCreature:
		// A dead creature, or one that lies out cold, is not fought
		return registry.AllOf<Creature>(entity) && Locator::creatureMindSystem::has_value() &&
		       !Locator::creatureMindSystem::value().IsLyingDead(entity);
	case Target::Living:
		return registry.AllOf<Villager>(entity) || registry.AllOf<Creature>(entity);
	case Target::Frightening:
		return registry.AllOf<Creature>(entity) || registry.AllOf<ecs::components::Spell>(entity) ||
		       (registry.AllOf<Animal>(entity) && Locator::animalSystem::has_value() &&
		        Locator::animalSystem::value().IsFrighteningToCreature(entity));
	case Target::Anything:
		return true;
	case Target::Examinable:
	{
		using creature_activity::Thing;
		const auto kind = KindOfThing(registry, entity);
		return kind != Thing::Creature && kind != Thing::Field && kind != Thing::FishFarm && kind != Thing::Spell &&
		       kind != Thing::Other;
	}
	case Target::InterestingVillager:
	{
		const auto* mind = registry.TryGet<const CreatureMindState>(self);
		return mind != nullptr && DoingSomethingInteresting(registry, *mind, entity);
	}
	case Target::PooedOn:
		// A thing that is not alive and is taller than 1.5 m, but not a field or a fish farm
		return !registry.AnyOf<Villager, Animal, Creature, Field, FishFarm>(entity) &&
		       object_measures::Height(registry, entity) > creature_plan_actions::k_PooedOnTallerThan;
	case Target::StoragePit:
		return registry.AllOf<StoragePit>(entity);
	case Target::WorshipSite:
		return registry.AllOf<WorshipSite>(entity);
	case Target::Workshop:
	{
		const auto* abode = registry.TryGet<const Abode>(entity);
		const auto& abodes = Locator::infoConstants::value().abode;
		return abode != nullptr && static_cast<size_t>(abode->info) < abodes.size() &&
		       abodes.at(static_cast<size_t>(abode->info)).abodeType == AbodeType::Workshop;
	}
	case Target::BuiltStoragePit:
	{
		const auto* progress = registry.TryGet<const BuildProgress>(entity);
		return registry.AllOf<StoragePit>(entity) && (progress == nullptr || !(progress->built < 1.0f));
	}
	case Target::AnyTree:
		return registry.AllOf<Tree>(entity);
	case Target::DryField:
	case Target::Waterable:
	{
		// A field has no food to give until it is ripe
		if (const auto* field = registry.TryGet<const Field>(entity))
		{
			const auto& types = Locator::infoConstants::value().fieldType;
			const auto row = static_cast<size_t>(field->type);
			const bool ripe = row < types.size() && !(field->crop.age < types.at(row).ageRecolt);
			return !ripe || field->crop.food == 0.0f;
		}
		return target == Target::Waterable && registry.AllOf<Tree>(entity);
	}
	case Target::BuiltFoodStore:
	{
		// A building stands fully built when it has no build progress left, or its progress is full
		const auto* progress = registry.TryGet<const BuildProgress>(entity);
		return registry.AnyOf<StoragePit, WorshipSite>(entity) && (progress == nullptr || !(progress->built < 1.0f));
	}
	case Target::MagicFoodPile:
	{
		// The food miracle's piles, but not one by the storage pit of the town nearest it (within a tenth more than the
		// pit's radius), which belongs to the town's store
		const auto* pot = registry.TryGet<const Pot>(entity);
		const auto* at = registry.TryGet<const Transform>(entity);
		if (heldByOther || pot == nullptr || pot->type != PotInfo::MagicFood || at == nullptr)
		{
			return false;
		}
		const auto town = NearestTown(registry, at->position);
		const auto pit = town != entt::null ? ecs::construction::StoragePitOf(town) : std::nullopt;
		const auto* pitAt = pit.has_value() ? registry.TryGet<const Transform>(*pit) : nullptr;
		return pitAt == nullptr || !(gutils::GetDistanceInMetres(pitAt->position, at->position) <
		                             object_measures::TwoDRadius(registry, *pit) * k_StoragePitReach);
	}
	case Target::Burning:
	case Target::Unburnt:
	{
		const bool burning = Locator::fireSystem::has_value() && Locator::fireSystem::value().IsOnFire(entity);
		return burning == (target == Target::Burning);
	}
	case Target::AnyLiving:
		return hands != nullptr && creature_object_actions::IsLiving(hands->KindOf(entity).thing);
	case Target::Stonable:
		return hands != nullptr &&
		       creature_object_actions::CanBeStoned(hands->KindOf(entity).thing,
		                                            registry.AllOf<VillagerDeath>(entity) || magic_living::IsDead(entity));
	case Target::ThrowingDamage:
	{
		if (heldByOther || hands == nullptr)
		{
			return false;
		}
		const auto kind = hands->KindOf(entity);
		return creature_object_actions::DoesThrowingDamage(kind.thing, kind.toy) && hands->CanPickUp(self, entity);
	}
	case Target::SeaThrowable:
		return hands != nullptr && creature_object_actions::CanBeThrownInTheSea(hands->KindOf(entity).thing);
	case Target::ThrowableToImpress:
		return !heldByOther && hands != nullptr && hands->CanThrowToImpress(self, entity) && hands->CanPickUp(self, entity);
	case Target::PickableRock:
	case Target::Rock:
	{
		if (hands == nullptr || (target == Target::PickableRock && heldByOther))
		{
			return false;
		}
		const auto thing = hands->KindOf(entity).thing;
		const bool rock = thing == creature_object_actions::Thing::Rock || thing == creature_object_actions::Thing::Bonfire;
		return rock && (target == Target::Rock || hands->CanPickUp(self, entity));
	}
	case Target::BuildingBeingBuilt:
	{
		// Anything built that is still going up (a building, a worship site or the temple), or a building that its town
		// repairs and that is below its full life
		const auto* progress = registry.TryGet<const BuildProgress>(entity);
		return (progress != nullptr && progress->built < 1.0f) ||
		       (registry.AllOf<RepairSite>(entity) && ecs::world_objects::LifeOf(entity) < 1.0f);
	}
	case Target::NeedsRepair:
		// Only a town's buildings say they want repair, while below their full life
		return registry.AllOf<Abode>(entity) && !registry.AllOf<Temple>(entity) && ecs::world_objects::LifeOf(entity) < 1.0f;
	case Target::CuddlyToy:
		return registry.AllOf<MobileStatic>(entity) && ToyOf(registry, entity) == creature_object_actions::Toy::Cuddly;
	case Target::Die:
		return registry.AllOf<MobileStatic>(entity) && ToyOf(registry, entity) == creature_object_actions::Toy::Die;
	case Target::ToyAwayFromHome:
	{
		// Only a static toy: the football is never taken home
		const auto home = ecs::creature_home::HomeOf(registry, self);
		const auto at = PointOf(registry, entity);
		return registry.AllOf<MobileStatic>(entity) && ToyOf(registry, entity).has_value() && home.has_value() &&
		       at.has_value() && gutils::GetDistanceInMetres(*home, glm::vec3(at->x, home->y, at->y)) > k_ToyAwayFromHome;
	}
	case Target::BringableHome:
	{
		// The distance to the home of the creature is across the land, as for a toy
		const auto home = ecs::creature_home::HomeOf(registry, self);
		const auto at = PointOf(registry, entity);
		if (heldByOther || hands == nullptr || !home.has_value() || !at.has_value())
		{
			return false;
		}
		const auto metres = gutils::GetDistanceInMetres(*home, glm::vec3(at->x, home->y, at->y));
		return creature_bring_home::CanBeBroughtHome(metres, creature_object_actions::IsLiving(hands->KindOf(entity).thing),
		                                             hands->CanPickUp(self, entity));
	}
	case Target::OneShotRestoreHealth:
	{
		// A globe that lies about: the game asks only that its miracle restores health
		const auto oneShot = OneShotOf(registry, entity);
		return oneShot.has_value() && oneShot->Is(creature_plan_actions::OneShotKind::RestoreHealth);
	}
	case Target::TemplePart:
		// The temple of any player, or a worship site of any town
		return registry.AllOf<Temple>(entity) || registry.AllOf<WorshipSite>(entity);
	case Target::OneShotAggressive:
	case Target::OneShotCompassionate:
	case Target::OneShotPlayful:
	{
		// A globe that lies about or that it holds, but not one in a player's hand or another creature's hand
		const auto kind = target == Target::OneShotAggressive      ? creature_plan_actions::OneShotKind::Aggressive
		                  : target == Target::OneShotCompassionate ? creature_plan_actions::OneShotKind::Compassionate
		                                                           : creature_plan_actions::OneShotKind::Playful;
		const auto oneShot = OneShotOf(registry, entity);
		return !heldByOther && !registry.AllOf<InHand>(entity) && oneShot.has_value() && oneShot->Is(kind);
	}
	case Target::PickableTree:
		return !heldByOther && registry.AllOf<Tree>(entity) && hands != nullptr && hands->CanPickUp(self, entity);
	case Target::FieldWithFood:
	{
		const auto* field = registry.TryGet<const Field>(entity);
		return field != nullptr && creature_meals::FieldHasFood(field->crop.age, field->crop.food);
	}
	case Target::StoragePitWithFood:
	{
		const auto* body = registry.TryGet<const Creature>(self);
		const auto pile = StoreFoodPileOf(registry, entity);
		const auto* pot = pile.has_value() ? registry.TryGet<const Pot>(*pile) : nullptr;
		return body != nullptr && pot != nullptr &&
		       creature_meals::StoragePitServes(object_measures::TwoDRadius(registry, entity), body->size, pot->amount);
	}
	case Target::TreeBigEnough:
	{
		const auto* body = registry.TryGet<const Creature>(self);
		return body != nullptr && registry.AllOf<Tree>(entity) &&
		       creature_meals::TreeBigEnough(object_measures::Height(registry, entity), body->size);
	}
	case Target::PlantableTree:
	{
		// Not one it planted another by lately, nor one near where the player did what it last set about copying
		const auto* mind = registry.TryGet<const CreatureMindState>(self);
		const auto at = PointOf(registry, entity);
		if (heldByOther || !registry.AllOf<Tree>(entity) || hands == nullptr || mind == nullptr || !at.has_value() ||
		    std::ranges::find(mind->plantedBy, entt::to_integral(entity)) != mind->plantedBy.end() ||
		    !(gutils::GetDistanceInMetres(*at, mind->copiedAt) > k_PlantFarFromCopied))
		{
			return false;
		}
		return hands->CanPickUp(self, entity);
	}
	}
	return false;
}

float Usefulness(const creature_mind_model::Learnt& learnt, Desire desire, const std::optional<creature_tree::Belief>& belief)
{
	if (!belief.has_value())
	{
		return creature_planner::k_DefaultUsefulness;
	}
	const auto& tree = learnt.trees.at(static_cast<size_t>(TreeKind::ActOn)).at(static_cast<size_t>(desire));
	return creature_tree::Usefulness(creature_tree::Evaluate(tree, *belief));
}

std::string ObjectName(const std::optional<creature_tree::Belief>& belief)
{
	return belief.has_value() ? fmt::format("{}s", creature_tree::BeliefName(belief->type)) : "that";
}
/// A creature looks this far, in metres, for a fish farm to fish at
constexpr float k_FishingReach = 600.0f;
/// It goes this near the shoal, or as near as it is tall when taller
constexpr float k_FishingArrival = 15.0f;

/// Where a creature at a point would fish: the shoal of the nearest fish farm (the farm itself without one), going as
/// near as it is tall but at least 15 m, and where it would stand to fish for itself; none without a farm near
std::optional<creature_plan_actions::Situation::Fishing> FishingFor(const ecs::Registry& registry, entt::entity creature,
                                                                    glm::vec2 position)
{
	if (!Locator::fishFarmSystem::has_value())
	{
		return std::nullopt;
	}
	const auto farm = Locator::fishFarmSystem::value().ClosestFarm({position.x, 0.0f, position.y}, k_FishingReach);
	if (!farm.has_value())
	{
		return std::nullopt;
	}
	bool holdingFood = false;
	if (Locator::creatureObjectActionSystem::has_value())
	{
		const auto& hands = Locator::creatureObjectActionSystem::value();
		if (const auto held = hands.GetHeld(creature))
		{
			holdingFood = hands.FoodValueOf(*held).has_value();
		}
	}
	const auto& data = registry.Get<const FishFarm>(*farm);
	const auto& at = registry.Get<const Transform>(*farm).position;
	const glm::vec3 shoal = data.shoal.has_value() ? data.shoal->centre : at;
	const glm::vec2 point {shoal.x, shoal.z};
	// Fishing for itself, where it can't stand at the shoal it goes to the nearest place it can, if near enough
	std::optional<glm::vec2> standAt = point;
	if (Locator::creatureLocomotionSystem::has_value())
	{
		const auto& land = Locator::creatureLocomotionSystem::value().GetWalkableLand();
		if (!land.IsValid(point, creature_route::k_DestinationClearance))
		{
			const auto valid =
			    land.NearestValid(point, creature_route::k_DestinationClearance, creature_route::k_ValidPointSearch);
			standAt = valid.has_value() &&
			                  glm::distance(shoal, glm::vec3(valid->x, 0.0f, valid->y)) < creature_route::k_ValidPointReach
			              ? valid
			              : std::nullopt;
		}
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	const float height = creature_mode::CreatureHeight(body != nullptr ? body->size : 1.0f);
	return creature_plan_actions::Situation::Fishing {.shoal = point,
	                                                  .standAt = standAt,
	                                                  .arriveWithin = std::max(height, k_FishingArrival),
	                                                  .holdingFood = holdingFood,
	                                                  .height = height};
}

/// How much a town feels each of its desires now, and its desires in its own order, most felt first
struct TownFeelings
{
	std::array<float, town_compassion::k_TownDesireCount> felt {};
	std::vector<uint32_t> order;
};
TownFeelings TownFeelingsOf(const ecs::Registry& registry, entt::entity town)
{
	TownFeelings feelings;
	const auto* wants = registry.TryGet<const TownDesire>(town);
	if (wants == nullptr)
	{
		return feelings;
	}
	for (size_t d = 0; d < feelings.felt.size(); ++d)
	{
		feelings.felt.at(d) = ecs::town_desire::GetRawDesire(*wants, d);
	}
	for (const auto& sorted : wants->sortedRaw)
	{
		feelings.order.push_back(sorted.index);
	}
	return feelings;
}

/// Whether compassion is what a creature wants most: no desire it feels is stronger
bool CompassionMostWanted(const creature_desires::Desires& desires)
{
	const auto compassion = desires[Desire::Compassion].value;
	return std::ranges::none_of(desires.desires,
	                            [compassion](const auto& state) { return state.activated && state.value > compassion; });
}

/// The action that heals, which hurt people in a town may make a creature think of first
constexpr std::string_view k_HealAction = "CastHealSpell";

/// The actions a creature's compassion for a town may take now: those for the town desire it helps with, which it may
/// move on from first, or healing first for a town with hurt people
std::vector<uint32_t> TownCompassionActions(const ecs::Registry& registry, entt::entity creature, CreatureMindState& mind,
                                            entt::entity town, const creature_mind_tables::Tables& tables,
                                            const std::function<uint32_t(uint32_t)>& random)
{
	const auto feelings = TownFeelingsOf(registry, town);
	const auto desires = town_compassion::DesiresToHelp(feelings.order, feelings.felt, tables.townActions);
	auto& state = mind.townCompassion;
	const auto before = state.desire;
	const bool beingCompassionate =
	    mind.planActive && mind.planner.current.has_value() && mind.planner.current->desire == Desire::Compassion;
	if (mind.desires.has_value() &&
	    town_compassion::MovesOnWhilePlanning(state, CompassionMostWanted(*mind.desires), beingCompassionate, mind.turn))
	{
		town_compassion::MoveOnWhilePlanning(state, desires, feelings.felt, mind.turn);
	}
	std::optional<uint32_t> heal;
	const auto* data = registry.TryGet<const Town>(town);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (const auto healAction = creature_mind_tables::FindAction(tables, k_HealAction);
	    data != nullptr && body != nullptr && healAction.has_value())
	{
		// TODO(fish): a town desire shown on the leash holds off the healing; the leash can't show one yet
		const auto sightings = MiracleSightings(mind, tables, body->species, MagicType::Heal);
		if (sightings.has_value() &&
		    town_compassion::HealFirst(data->injured, false, sightings->first, sightings->second, random))
		{
			heal = healAction;
		}
	}
	town_compassion::Settle(state, desires, feelings.felt);
	if (state.desire != before)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} helps town {} with its desire {} ({} it wants)",
		                    entt::to_integral(creature), entt::to_integral(town),
		                    state.desire.has_value() ? static_cast<int>(*state.desire) : -1, desires.size());
	}
	return town_compassion::Actions(state, tables.townActions, heal);
}

/// Having finished helping a town, the creature may move on to another of its desires
void FinishedHelpingTown(const ecs::Registry& registry, CreatureMindState& mind, entt::entity town,
                         const creature_mind_tables::Tables& tables, const std::function<uint32_t(uint32_t)>& random)
{
	if (!registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return;
	}
	const auto feelings = TownFeelingsOf(registry, town);
	const auto desires = town_compassion::DesiresToHelp(feelings.order, feelings.felt, tables.townActions);
	town_compassion::FinishedHelping(mind.townCompassion, desires, feelings.felt, mind.turn, random);
}
} // namespace

namespace
{
std::optional<entt::entity> TownOfThing(const ecs::Registry& registry, entt::entity thing,
                                        std::optional<entt::entity> belongsTo);

/// The player number of a town, none when there is no town
std::optional<uint32_t> PlayerOfTown(const ecs::Registry& registry, std::optional<entt::entity> town)
{
	const auto* data = town.has_value() && registry.Valid(*town) ? registry.TryGet<const Town>(*town) : nullptr;
	return data != nullptr ? std::optional(PlayerNumberOf(data->owner)) : std::nullopt;
}

/// The number of the player a thing is of, none for a thing of no player. The things of a town are of its player; a
/// miracle is of the player of its caster; the land's other things are of the neutral player or of none.
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
	if (const auto* icon = registry.TryGet<const TownCentreIcon>(thing))
	{
		return PlayerOfTown(registry, icon->town);
	}
	// A villager, a field or a fish farm with no town is of no player; a building, a totem or a pile with no town is of
	// the neutral player
	if (registry.AnyOf<Villager, Abode, Field, FishFarm, VillageTotem, Pot>(thing))
	{
		if (const auto player = PlayerOfTown(registry, TownOfThing(registry, thing, CreatureMindSystem::BelongsTo(thing))))
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
	const auto* held = registry.TryGet<const CreatureHeldObject>(creature);
	if (held == nullptr || !registry.Valid(held->object))
	{
		return std::nullopt;
	}
	return mind_detail::ObjectSortOf(registry, held->object);
}

/// The attributes only the beliefs about villagers, creatures, buildings, towns and forests have
void SetKindAttributes(const ecs::Registry& registry, entt::entity entity, const Creature* me, creature_tree::Belief& belief)
{
	using creature_tree::Attribute;
	namespace types = creature_tree::belief_types;
	const auto onFire = [&registry, entity]() -> uint32_t {
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
} // namespace

/// What a creature knows of a thing: its kind and attributes, as far as the game's world here tells them
std::optional<creature_tree::Belief> mind_detail::BeliefOf(const ecs::Registry& registry, entt::entity entity,
                                                           entt::entity self)
{
	if (!registry.Valid(entity))
	{
		return std::nullopt;
	}
	const auto type = BeliefKindOf(registry, entity);
	if (!type.has_value())
	{
		return std::nullopt;
	}
	const auto* me = registry.TryGet<const Creature>(self);
	const auto* creature = registry.TryGet<const Creature>(entity);
	auto belief = creature_tree::CommonBelief({
	    .type = *type,
	    .origin = OriginOf(registry, entity),
	    .animate = registry.AnyOf<Villager, Animal, Creature>(entity),
	    .player = BeliefPlayerOf(registry, entity),
	    .creaturesPlayer = me != nullptr ? std::optional(PlayerNumberOf(me->owner)) : std::nullopt,
	    .harderThanMe = creature != nullptr && me != nullptr && ShownSize(*creature) > SizeOfPlayersCreature(registry, *me),
	    .creatureType = creature != nullptr ? static_cast<uint32_t>(creature::InfoRow(creature->species)) : 0,
	});
	SetKindAttributes(registry, entity, me, belief);
	// The kind of thing and some attributes stay as the creature saw them first
	const auto* mind = registry.TryGet<const CreatureMindState>(self);
	const auto* known = mind != nullptr ? mind->beliefs.Find(entity) : nullptr;
	if (known != nullptr && known->firstSight.has_value())
	{
		return creature_tree::WithFirstSight(*known->firstSight, belief);
	}
	return belief;
}

std::optional<creature_tree::Belief> CreatureMindSystem::BeliefAbout(entt::entity creature, entt::entity thing) const
{
	return mind_detail::BeliefOf(Locator::entitiesRegistry::value(), thing, creature);
}

std::optional<creature_tree::Belief> mind_detail::ActedOnBeliefOf(const ecs::Registry& registry, entt::entity entity,
                                                                  entt::entity self)
{
	// The temple here is one thing. Acted on, it stands for its heart: a part of the temple of the temple's player.
	if (const auto* temple = registry.Valid(entity) ? registry.TryGet<const Temple>(entity) : nullptr)
	{
		const auto* me = registry.TryGet<const Creature>(self);
		return creature_tree::CommonBelief({
		    .type = creature_tree::belief_types::k_CitadelPart,
		    .player = PlayerNumberOf(temple->owner),
		    .creaturesPlayer = me != nullptr ? std::optional(PlayerNumberOf(me->owner)) : std::nullopt,
		});
	}
	return BeliefOf(registry, entity, self);
}

std::optional<uint32_t> mind_detail::LessonActivity(const creature_mind_tables::Tables& tables, Desire desire,
                                                    std::optional<uint32_t> object, std::optional<uint32_t> about)
{
	return creature_learning::ActivityOf(tables.rules.at(static_cast<size_t>(desire)).aboutSomething, object, about);
}

uint32_t CreatureMindSystem::Random(uint32_t range)
{
	return range == 0 ? 0u : std::uniform_int_distribution<uint32_t>(0, range - 1)(_random);
}

float CreatureMindSystem::Chance()
{
	return std::uniform_real_distribution<float>(0.0f, 1.0f)(_random);
}

const creature_mind_tables::Tables* CreatureMindSystem::GetTables()
{
	if (!_tables.has_value() && Locator::infoConstants::has_value())
	{
		_tables = creature_mind_tables::Build(Locator::infoConstants::value());
	}
	return _tables.has_value() ? &*_tables : nullptr;
}

void CreatureMindSystem::SetUpMind(entt::entity creature, const Creature& body, CreatureMindState& mind)
{
	if (!mind.desires.has_value())
	{
		mind.desires = creature_desires::Create(SetupFor(body.species), [this](float low, float high) {
			return high > low ? std::uniform_real_distribution<float>(low, high)(_random) : low;
		});
		// It is born with its species' personality, spread at random
		mind.innate = creature_desire_sources::SpreadTraits(InnateTraitsOf(body.species), [this](float most) {
			return most > 0.0f ? std::uniform_real_distribution<float>(0.0f, most)(_random) : 0.0f;
		});
	}
	if (!mind.learnt.has_value())
	{
		SetUpLearning(creature, mind);
	}
}

void CreatureMindSystem::SetUpLearning(entt::entity creature, CreatureMindState& mind)
{
	const auto* tables = GetTables();
	const auto opinions = tables != nullptr ? creature_mind_tables::StartingOpinions(*tables) : std::vector<float> {};
	const auto actionCount = opinions.size();
	auto knowledge = tables != nullptr ? creature_watching::StartKnowledge(tables->skills, tables->miracles)
	                                   : creature_watching::Knowledge {};
	mind.learnt = creature_mind_model::Fresh(*mind.desires, opinions, std::move(knowledge));
	if (tables != nullptr)
	{
		creature_mind_model::TeachStartingLesson(*mind.learnt, tables->attributes.at(static_cast<size_t>(Desire::Hunger)));
	}
	mind.learnt->turnsSinceDone = creature_action_history::Fresh(actionCount, [this](uint32_t range) { return Random(range); });
	// A new mind did each action a short time ago, picked at random for each
	for (auto& turns : mind.learnt->turnsSinceDone)
	{
		turns = Random(creature_desire_sources::k_NewActionTurnsRange);
	}
	if (mind.resourceChecked)
	{
		return;
	}
	mind.resourceChecked = true;
	// A creature made from a mind file takes up what the file holds
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr || body->mind == 0 || !Locator::resources::has_value())
	{
		return;
	}
	const auto& minds = Locator::resources::value().GetCreatureMinds();
	if (minds.Contains(body->mind))
	{
		const auto handle = minds.Handle(body->mind);
		if (handle && handle->Loaded())
		{
			mind.pendingFile = std::make_shared<const creaturemind::MindFileData>(handle->data);
		}
	}
}

void CreatureMindSystem::TakeUpFile(entt::entity creature, CreatureMindState& mind)
{
	const auto file = std::move(mind.pendingFile);
	mind.pendingFile.reset();
	auto* body = Locator::entitiesRegistry::value().TryGet<Creature>(creature);
	if (body == nullptr || !mind.desires.has_value() || !mind.learnt.has_value())
	{
		return;
	}
	// It takes the name the file keeps, unless a computer player owns it
	if (creature_names::TakesNameFromFile(ecs::creature_naming::OwnerOf(body->owner)))
	{
		body->name = file->name;
	}
	const auto fresh =
	    creature_desires::Create(SetupFor(body->species), [](float low, float high) { return 0.5f * (low + high); });
	const auto* tables = GetTables();
	auto knowledge = tables != nullptr ? creature_watching::StartKnowledge(tables->skills, tables->miracles)
	                                   : creature_watching::Knowledge {};
	auto freshLearnt = creature_mind_model::Fresh(
	    fresh, tables != nullptr ? creature_mind_tables::StartingOpinions(*tables) : mind.learnt->opinions,
	    std::move(knowledge));
	if (tables != nullptr)
	{
		creature_mind_model::TeachStartingLesson(freshLearnt, tables->attributes.at(static_cast<size_t>(Desire::Hunger)));
	}
	freshLearnt.turnsSinceDone =
	    creature_action_history::Fresh(freshLearnt.opinions.size(), [this](uint32_t range) { return Random(range); });
	auto loaded = creature_mind_model::FromFile(*file, fresh, freshLearnt);
	if (tables != nullptr)
	{
		creature_mind_model::RebuildTrees(loaded.learnt, tables->attributes);
	}
	Abandon(mind);
	mind.planner = {};
	mind.desires = std::move(loaded.desires);
	mind.learnt = std::move(loaded.learnt);
	mind.developmentPhase = std::min(loaded.developmentPhase, CreatureMindState::k_FullyGrownUp);
	// The file's desires are active as it says, for the stage it gives
	mind.desiresPhase = mind.developmentPhase;
	mind.attitudeToPlayer = loaded.attitudeToPlayer;
	mind.turnsLookedAt = loaded.turnsLookedAt;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} took up a mind file: version {}, stage {}",
	                   entt::to_integral(creature), file->version, mind.developmentPhase);
}

void CreatureMindSystem::Abandon(CreatureMindState& mind)
{
	if (mind.planActive && mind.learnt.has_value() && !mind.learnt->contexts.empty())
	{
		mind.learnt->contexts.back().running = false;
	}
	// Whether carried out or not, the plan's action was done just now, and the other actions of its desire get older
	if (mind.planActive && mind.learnt.has_value() && mind.planner.current.has_value())
	{
		creature_action_history::PlanEnded(mind.learnt->turnsSinceDone, mind.planner.current->action, mind.planActions,
		                                   mind.agendaTurns);
	}
	mind.planActive = false;
	if (mind.planner.current.has_value())
	{
		mind.planner.lastObject = mind.planner.current->object;
		mind.planner.lastAction = mind.planner.current->action;
	}
	mind.planner.current.reset();
	// To stop what it does clears the marks of the plan
	mind.ownPlanSinceStop = false;
	mind.planForced = false;
	mind.forcedPlanCounts = false;
	mind.emergencyRunning = false;
}

namespace
{
/// The temperature of the creature becomes the temperature of the climate where it stands
void SettleTemperature(entt::entity creature)
{
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(creature);
	if (transform == nullptr || !Locator::weatherSystem::has_value() || !Locator::fireSystem::has_value())
	{
		return;
	}
	const auto temperature = static_cast<float>(Locator::weatherSystem::value().GetWeather(transform->position).temperature);
	Locator::fireSystem::value().SetTemperature(creature, temperature, entt::null);
}
} // namespace

void CreatureMindSystem::FollowAgenda(entt::entity creature, CreatureMindState& mind)
{
	if (!mind.learnt.has_value() || !mind.desires.has_value())
	{
		return;
	}
	const auto* tables = GetTables();
	auto& idle = mind.idle;
	if (mind.planActive && (idle.serial != mind.planSerial || idle.step >= idle.agenda.size()))
	{
		// The plan is over. Carried out to its end, its body pays for the action and it counts towards satisfying the
		// urge of the desire it served, unless one of its steps saw to that already; cut short by something else
		// (fainting, a fight, a more pressing plan) or given up, it is still wanted. Once enough plans have been carried
		// out, the desire's clearable sources are cleared, and the desire is less if the game's table says the action
		// lessens it or one of its steps saw to the desire.
		const auto plan = *mind.planner.current;
		const bool carriedOut = idle.serial == mind.planSerial && !idle.gaveUp;
		// A forced plan counts for its desire only when the order that forced it said so
		auto* needs = Locator::entitiesRegistry::value().TryGet<CreatureNeeds>(creature);
		const auto completion = creature_turn_order::CompletionOf(
		    plan.desire, mind.planForced, mind.forcedPlanCounts,
		    tables != nullptr && plan.action < tables->actions.size() && tables->actions[plan.action].fullRecovery,
		    needs != nullptr ? needs->needs.life : creature_turn_order::k_FullLife);
		if (carriedOut)
		{
			NoteDone(mind, plan.action);
			creature_training::CountCarriedOut(mind.actionCounts, plan.action,
			                                   Locator::entitiesRegistry::value().AllOf<ScriptControlled>(creature));
			mind.lastActionDone = plan.action;
			creature_watching::MimicActionDone(mind.learnt->mimicry);
		}
		if (carriedOut && !mind.satisfiedByEffect && tables != nullptr && plan.action < tables->actions.size())
		{
			const auto& action = tables->actions[plan.action];
			BodyPaysFor(creature, action.name);
			auto& served = (*mind.desires)[plan.desire];
			if (completion.counts && creature_desires::CountTowardsUrge(served))
			{
				creature_desires::ClearSourcesAfterSatisfying(served);
				if (action.alwaysApplies || mind.desireSeenTo)
				{
					Lessen(creature, *mind.desires, action.name);
				}
			}
		}
		// After the urge, the desire the plan served changes as the game's rule for that desire says
		if (carriedOut && completion.counts)
		{
			creature_learning::AfterPlanCarriedOut(*mind.desires, plan.desire, mind_detail::DesireFloorOf(creature),
			                                       k_TurnsPerSecond);
		}
		// A plan to restore health heals it, and an action that makes it recover fully gives full life and the
		// temperature of the climate where it stands
		if (carriedOut && completion.life.has_value() && needs != nullptr)
		{
			ecs::world_objects::SetLife(creature, *completion.life);
		}
		if (carriedOut && completion.climateTemperature)
		{
			SettleTemperature(creature);
		}
		// An action that ends well on a creature makes the creature find that creature nicer or nastier, as the game's
		// table says for the action. An action on no thing acts on the creature itself, so it also changes how nice the
		// creature finds itself (a quirk of the game).
		if (carriedOut && tables != nullptr && plan.action < tables->actions.size())
		{
			const auto change = tables->actions[plan.action].creatureNicenessChange;
			const auto& registry = Locator::entitiesRegistry::value();
			const auto other = plan.object.has_value() ? static_cast<entt::entity>(*plan.object) : creature;
			if (change != 0.0f && registry.Valid(other) && registry.AllOf<Creature>(other))
			{
				mind.leash.attitudes.push_back({.creature = entt::to_integral(other), .change = change});
			}
		}
		if (carriedOut && plan.desire == Desire::Compassion && plan.about.has_value() && tables != nullptr)
		{
			FinishedHelpingTown(Locator::entitiesRegistry::value(), mind, static_cast<entt::entity>(*plan.about), *tables,
			                    [this](uint32_t range) { return Random(range); });
		}
		mind.desireSeenTo = false;
		Abandon(mind);
		mind.planner.best.at(static_cast<size_t>(plan.desire)).reset();
	}
	// What the idle mind started is over once its agenda runs out: carried out to its end, its action counts
	if (mind.underway.has_value() && (mind.planActive || mind.underway->serial != idle.serial))
	{
		EndUnderway(mind);
		mind.underway.reset();
	}
	if (mind.underway.has_value() && idle.step >= idle.agenda.size())
	{
		EndUnderway(mind);
		if (!idle.gaveUp)
		{
			NoteDone(mind, mind.underway->action);
			creature_training::CountCarriedOut(mind.actionCounts, mind.underway->action,
			                                   Locator::entitiesRegistry::value().AllOf<ScriptControlled>(creature));
			mind.lastActionDone = mind.underway->action;
			creature_watching::MimicActionDone(mind.learnt->mimicry);
		}
		mind.underway.reset();
	}
	if (!mind.planActive && idle.serial == mind.agendaSeen && idle.step >= idle.agenda.size() && !mind.learnt->contexts.empty())
	{
		mind.learnt->contexts.back().running = false;
	}
	// The trainer judges what it did as soon as it is over
	if (mind.trainer.has_value() && !mind.learnt->contexts.empty())
	{
		const auto& last = mind.learnt->contexts.back();
		if (!last.running && !last.credited.has_value() && last.belief.has_value())
		{
			const bool reward = last.belief->type == *mind.trainer;
			mind.learnt->contexts.back().credited = reward ? 1.0f : -1.0f;
			ReceiveFeedback(creature, reward ? 1.0f : -1.0f);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} {} for a {}", entt::to_integral(creature),
			                   reward ? "stroked" : "slapped", creature_tree::BeliefName(last.belief->type));
		}
	}
	if (idle.serial == mind.agendaSeen)
	{
		return;
	}
	mind.agendaSeen = idle.serial;
	mind.satisfiedByEffect = false;
	mind.desireSeenTo = false;
	if (mind.planActive || tables == nullptr)
	{
		return;
	}
	// Something the idle mind started counts as its action, for feedback
	const auto action = mind.creditedAction.has_value() && mind.creditedSerial == idle.serial
	                        ? mind.creditedAction
	                        : creature_mind_tables::FindAction(*tables, ActionForActivity(idle.activity));
	mind.creditedAction.reset();
	if (!action.has_value() || tables->actions[*action].desire >= creature_desires::k_DesireCount)
	{
		return;
	}
	std::optional<uint32_t> object;
	for (const auto& step : idle.agenda)
	{
		if (step.order.object.has_value())
		{
			object = step.order.object;
			break;
		}
		if (step.object.has_value())
		{
			object = step.object;
			break;
		}
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& info = tables->actions[*action];
	const auto activity = LessonActivity(*tables, static_cast<Desire>(info.desire), object, std::nullopt);
	creature_learning::Remember(
	    mind.learnt->contexts,
	    {
	        .action = *action,
	        .desire = static_cast<Desire>(info.desire),
	        .object = object,
	        .belief =
	            object.has_value() ? ActedOnBeliefOf(registry, static_cast<entt::entity>(*object), creature) : std::nullopt,
	        .activity = activity,
	        .activityBelief =
	            activity.has_value() ? BeliefOf(registry, static_cast<entt::entity>(*activity), creature) : std::nullopt,
	        .learnable = info.learnable,
	        .windowSeconds = info.learningWindowSeconds,
	        .source = mind.desires.has_value()
	                      ? creature_learning::PlanSource((*mind.desires)[static_cast<Desire>(info.desire)])
	                            .value_or(creature_learning::k_NoTellingSource)
	                      : creature_learning::k_NoTellingSource,
	    });
	mind.underway = CreatureMindState::ActionUnderway {.serial = idle.serial, .action = *action};
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

/// Whether a creature kept within an area by its leash has a place farther from the area's centre than the area's radius
bool OutsideConfinedArea(const ecs::Registry& registry, entt::entity creature, glm::vec3 place)
{
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	if (leashes == nullptr)
	{
		return false;
	}
	const bool leashed = leashes->worn.has_value();
	const bool works = !leashed || leashes->worn->works;
	return creature_leash::IsConfined(leashes->confinementRadius, leashed, works) &&
	       gutils::GetDistanceInMetres(place, leashes->confinementCentre) > leashes->confinementRadius;
}

/// Where the creature believes that a thing is: where it last came across the thing, else where the thing is now
std::optional<glm::vec3> BelievedPlaceOf(const ecs::Registry& registry, const CreatureMindState& mind, entt::entity thing)
{
	if (const auto* belief = mind.beliefs.Find(thing))
	{
		return belief->place;
	}
	if (const auto* at = registry.TryGet<const Transform>(thing))
	{
		return at->position;
	}
	return std::nullopt;
}

bool NoWayLately(const ecs::Registry& registry, const CreatureMindState& mind, entt::entity thing);
bool LetsCreatureAct(const ecs::Registry& registry, entt::entity thing);

/// The thing a creature at a point uses for an action, of a kind, other than the goal. It looks only through its
/// beliefs about things that are not whole things, the newest first: what it has not come across, it does not use.
std::optional<creature_planner::UsedChoice> UsedFor(entt::entity creature, Target use, glm::vec2 position, float distanceWeight,
                                                    std::optional<uint32_t> goal)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	if (mind == nullptr || use == Target::None)
	{
		return std::nullopt;
	}
	std::vector<creature_planner::UsedCandidate> candidates;
	for (const auto& belief : mind->beliefs.InListOrder())
	{
		const auto thing = belief.thing;
		// Not a whole thing, nor a thing it found no way to lately, nor one outside the area its leash keeps it within
		if (creature_object_beliefs::InListOfWholeThings(belief.kind) || !registry.Valid(thing) ||
		    NoWayLately(registry, *mind, thing) || OutsideConfinedArea(registry, creature, belief.place) ||
		    !LetsCreatureAct(registry, thing) || !Accepts(registry, use, thing, creature))
		{
			continue;
		}
		const auto* held = registry.TryGet<const HeldByCreature>(thing);
		candidates.push_back({.id = entt::to_integral(thing),
		                      .distance = glm::distance(glm::vec2 {belief.place.x, belief.place.z}, position),
		                      .held = held != nullptr && held->creature == creature});
	}
	return creature_planner::ChooseUsed(candidates, distanceWeight, goal);
}

} // namespace

bool CreatureMindSystem::Adopt(entt::entity creature, CreatureMindState& mind, const creature_planner::Plan& plan,
                               const creature_plan_actions::Situation& situation)
{
	const auto* tables = GetTables();
	if (tables == nullptr || plan.action >= tables->actions.size() || !mind.learnt.has_value() || !mind.desires.has_value())
	{
		return false;
	}
	// Under a script's control it takes up nothing of its own choosing, even once the script's orders are carried out:
	// it stays where the script left it until it is let go
	if (Locator::entitiesRegistry::value().AllOf<ScriptControlled>(creature))
	{
		return false;
	}
	const auto& info = tables->actions[plan.action];
	// Copying the player makes no steps: what the creature did before stops, it copies its last deed again, and it
	// decides again on its next turn. The choice is remembered for feedback, and the desires it opposes are held back.
	if (const auto* executor = creature_plan_actions::For(info.name);
	    executor != nullptr && executor->build == creature_plan_actions::Build::MimicPlayer)
	{
		auto& registry = Locator::entitiesRegistry::value();
		if (registry.AllOf<CreatureFighting>(creature) && Locator::creatureFightSystem::has_value())
		{
			Locator::creatureFightSystem::value().CallOff(creature);
		}
		if (Locator::creatureObjectActionSystem::has_value() &&
		    Locator::creatureObjectActionSystem::value().GetState(creature) ==
		        CreatureObjectActionSystemInterface::State::Busy &&
		    !Locator::creatureObjectActionSystem::value().IsCatching(creature))
		{
			Locator::creatureObjectActionSystem::value().Cancel(creature);
		}
		Abandon(mind);
		creature_mind::Plan(mind.idle, creature_mind::Activity::None, {});
		CopyObservedDeedAgain(creature, mind);
		if (const auto source = creature_learning::TakeUpChosenPlan((*mind.desires)[plan.desire]); source.has_value())
		{
			creature_learning::Remember(mind.learnt->contexts, {
			                                                       .action = plan.action,
			                                                       .desire = plan.desire,
			                                                       .learnable = info.learnable,
			                                                       .windowSeconds = info.learningWindowSeconds,
			                                                       .source = *source,
			                                                   });
		}
		creature_learning::SuppressOpposed(*mind.desires, plan.desire, tables->dependencies, k_TurnsPerSecond);
		return false;
	}
	auto situated = situation;
	Situate(creature, situated);
	// An action done with something takes up the thing to use too, as the plan found it or, for a plan made otherwise
	// (copying the player, doing it again), as the planner would
	auto chosen = plan;
	if (const auto* executor = creature_plan_actions::For(info.name);
	    executor != nullptr && executor->use != Target::None && !chosen.instrument.has_value())
	{
		chosen.instrument = UsedFor(creature, executor->use, situated.position,
		                            tables->rules.at(static_cast<size_t>(plan.desire)).distanceWeight, plan.object)
		                        .transform([](const creature_planner::UsedChoice& used) { return used.id; });
	}
	situated.instrument = chosen.instrument;
	situated.activity = chosen.activityObject.has_value()
	                        ? chosen.activityObject
	                        : mind_detail::LessonActivity(*tables, plan.desire, plan.object, plan.about);
	auto built = PlanAgenda(creature, plan.action, plan.object, situated);
	if (!built.has_value())
	{
		return false;
	}
	auto& [executor, agenda] = *built;
	auto& registry = Locator::entitiesRegistry::value();
	// A new plan ends the fight that the creature started by its own plan, for both creatures
	if (registry.AllOf<CreatureFighting>(creature) && Locator::creatureFightSystem::has_value())
	{
		Locator::creatureFightSystem::value().CallOff(creature);
	}
	// Whatever its hands were doing is given up for the plan, but for a catch, which its body carries on with by itself
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy &&
	    !Locator::creatureObjectActionSystem::value().IsCatching(creature))
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
	if (!Replan(creature, executor->activity, std::move(agenda)))
	{
		return false;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} sets about {} on {} ({} steps)", entt::to_integral(creature),
	                    info.name, plan.object.value_or(0), mind.idle.agenda.size());
	auto& learnt = *mind.learnt;
	mind.planner.current = chosen;
	mind.planActive = true;
	mind.planSerial = mind.idle.serial;
	mind.agendaSeen = mind.idle.serial;
	mind.satisfiedByEffect = false;
	mind.desireSeenTo = false;
	mind.planActions = mind.plannedActions.at(static_cast<size_t>(plan.desire));
	// It is remembered for the player's feedback unless nothing says why it wants what the plan serves, as a forced plan
	// is, and the desires the plan's desire opposes are held back
	if (const auto source = creature_learning::TakeUpChosenPlan((*mind.desires)[plan.desire]); source.has_value())
	{
		const auto activity = LessonActivity(*tables, plan.desire, plan.object, plan.about);
		creature_learning::Remember(
		    learnt.contexts,
		    {
		        .action = plan.action,
		        .desire = plan.desire,
		        .object = plan.object,
		        .belief = plan.object.has_value() ? ActedOnBeliefOf(registry, static_cast<entt::entity>(*plan.object), creature)
		                                          : std::nullopt,
		        .activity = activity,
		        .activityBelief =
		            activity.has_value() ? BeliefOf(registry, static_cast<entt::entity>(*activity), creature) : std::nullopt,
		        .learnable = info.learnable,
		        .windowSeconds = info.learningWindowSeconds,
		        .source = *source,
		    });
	}
	creature_learning::SuppressOpposed(*mind.desires, plan.desire, tables->dependencies, k_TurnsPerSecond);
	return true;
}

std::optional<std::pair<const creature_plan_actions::Executor*, std::vector<creature_mind::Step>>>
CreatureMindSystem::PlanAgenda(entt::entity creature, uint32_t action, std::optional<uint32_t> object,
                               const creature_plan_actions::Situation& situation)
{
	const auto* tables = GetTables();
	if (tables == nullptr || action >= tables->actions.size())
	{
		return std::nullopt;
	}
	const auto* executor = creature_plan_actions::For(tables->actions[action].name);
	if (executor == nullptr)
	{
		return std::nullopt;
	}
	glm::vec2 point {0.0f};
	if (object.has_value())
	{
		const auto at = PointOf(Locator::entitiesRegistry::value(), static_cast<entt::entity>(*object));
		if (!at.has_value())
		{
			return std::nullopt;
		}
		point = *at;
	}
	const auto cast = creature_plan_actions::IsCast(*executor)
	                      ? CastInfoFor(creature, action,
	                                    object.has_value() ? std::optional(static_cast<entt::entity>(*object)) : std::nullopt)
	                      : std::nullopt;
	// Only what varies by chance draws one
	auto situated = situation;
	if (situation.instrument.has_value())
	{
		const auto used = static_cast<entt::entity>(*situation.instrument);
		const auto& registry = Locator::entitiesRegistry::value();
		situated.instrumentPoint = PointOf(registry, used).value_or(glm::vec2(0.0f));
		situated.instrumentRadius = object_measures::TwoDRadius(registry, used);
		situated.usedOneShot = OneShotOf(registry, used);
	}
	if (object.has_value())
	{
		situated.thingHeight = object_measures::Height(Locator::entitiesRegistry::value(), static_cast<entt::entity>(*object));
		situated.thingRadius =
		    object_measures::TwoDRadius(Locator::entitiesRegistry::value(), static_cast<entt::entity>(*object));
		if (executor->build == creature_plan_actions::Build::EatFromStoragePit)
		{
			SituateStoreFood(Locator::entitiesRegistry::value(), static_cast<entt::entity>(*object), situated);
		}
		// Sleeping by a thing, it looks for a clear place four times as wide as it is tall about it; one not under a
		// script's control also wants nothing living there
		if (executor->build == creature_plan_actions::Build::SleepByObject)
		{
			situated.clearArea =
			    ClearAreaFor(Locator::entitiesRegistry::value(), creature, point,
			                 creature_mind::k_SleepByObjectHeights * situation.height, !situation.controlledByScript);
		}
	}
	if (const auto clear = EmoteClearArea(creature, *executor, point, situated))
	{
		situated.clearArea = *clear;
	}
	SituateExploration(creature, *executor, situated);
	if (executor->build == creature_plan_actions::Build::LookButDontApproach ||
	    executor->build == creature_plan_actions::Build::SmileAt)
	{
		situated.chance = Chance();
	}
	if (executor->build == creature_plan_actions::Build::DanceWithVillagers)
	{
		SituateDance(creature, *executor, point, situated);
	}
	mind_town_help::Situate(Locator::entitiesRegistry::value(), creature, *executor, object, situated);
	if (executor->build == creature_plan_actions::Build::Drink)
	{
		situated.drink = DrinkPlaceOf(creature);
	}
	auto agenda = creature_plan_actions::Agenda(
	    *executor, object, point, situated, [this](uint32_t range) { return Random(range); }, cast);
	if (!agenda.has_value())
	{
		// The creature that finds no place to drink does not want to drink for a time
		if (executor->build == creature_plan_actions::Build::Drink)
		{
			if (auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
			    mind != nullptr && mind->desires.has_value())
			{
				creature_drink::NoPlaceFound(*mind->desires, k_TurnsPerSecond);
			}
		}
		return std::nullopt;
	}
	BelieveInTempleToEnter(creature, *executor, situated);
	// Planting by a tree, it remembers that tree, and won't take it to plant elsewhere for a while
	auto& registry = Locator::entitiesRegistry::value();
	if (executor->build == creature_plan_actions::Build::PlantTree && object.has_value() &&
	    registry.AllOf<Tree>(static_cast<entt::entity>(*object)))
	{
		if (auto* mind = registry.TryGet<CreatureMindState>(creature))
		{
			mind->plantedBy.push_back(*object);
			if (mind->plantedBy.size() > creature_mind::k_PlantedRemembered)
			{
				mind->plantedBy.erase(mind->plantedBy.begin());
			}
		}
	}
	return std::pair {executor, std::move(*agenda)};
}

namespace
{
/// Whether a villager does something that the creature finds interesting: its state teaches a skill that the creature
/// does not know. Nothing else is interesting that way.
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

/// Curious, a creature cares little for a kind of thing it has had a good look at, unless the thing does something it
/// finds interesting. Towns aren't things it looks at that way.
float ObservedShare(const ecs::Registry& registry, const CreatureMindState& mind, Desire desire, entt::entity object,
                    const std::optional<creature_tree::Belief>& belief)
{
	if (desire != Desire::Curiosity || !belief.has_value() || registry.AnyOf<Town>(object))
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
                            const glm::vec3& place, float distanceWeight)
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
	const auto* held = registry.TryGet<const HeldByCreature>(object);
	return creature_planner::DistancePriority(held != nullptr && held->creature == creature,
	                                          gutils::GetDistanceInMetres(place, from), distanceWeight);
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
		    gutils::GetDistanceInMetres(at, *place) > leashes->confinementRadius)
		{
			return 0.0f;
		}
	}
	const auto belief = ActedOnBeliefOf(registry, object, creature);
	// Curious, a kind it has had a good look at is a hundredth as useful
	// TODO(creature-reactions-2): the game makes a thing five times as useful to a curious creature while it does
	// something the creature finds interesting (only a villager can), and has a rule of its own for one more desire
	const float share = ObservedShare(registry, mind, desire, object, belief);
	const auto distance =
	    Nearness(registry, creature, at, object, *place, tables->rules.at(static_cast<size_t>(desire)).distanceWeight);
	float learnt = creature_planner::k_DefaultUsefulness;
	if (belief.has_value())
	{
		const auto& tree = mind.learnt->trees.at(static_cast<size_t>(TreeKind::Use)).at(static_cast<size_t>(desire));
		learnt = creature_tree::Usefulness(creature_tree::Evaluate(tree, *belief));
	}
	return learnt * distance * share;
}

namespace
{
/// A town counts as loyal to the player of the creature when its belief in that player is more than this share of its
/// largest belief in another player
constexpr float k_HalfOfMostOtherBelief = 0.5f;

/// The player of a thing that can belong to a player: a town, a villager or a building by its town, a creature or a
/// temple. A villager or a building has the player of the town that it belongs to. None for any other thing, or for a
/// villager or a building with no town.
std::optional<PlayerNames> PlayerOfThing(const ecs::Registry& registry, entt::entity thing,
                                         std::optional<entt::entity> belongsTo)
{
	if (!registry.Valid(thing))
	{
		return std::nullopt;
	}
	if (const auto* creature = registry.TryGet<const Creature>(thing))
	{
		return creature->owner;
	}
	if (const auto* temple = registry.TryGet<const Temple>(thing))
	{
		return temple->owner;
	}
	if (const auto* town = registry.TryGet<const Town>(thing))
	{
		return town->owner;
	}
	if (const auto owner = belongsTo; owner.has_value() && *owner != thing)
	{
		if (const auto* town = registry.TryGet<const Town>(*owner))
		{
			return town->owner;
		}
	}
	return std::nullopt;
}

/// What a creature knows about a whole thing that it examines a desire about
creature_activity::WholeFacts WholeFactsOf(const ecs::Registry& registry, entt::entity creature, entt::entity whole,
                                           creature_activity::Whole kind, const creature_mind_tables::Tables& tables)
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
	// Only the player of the creature looks at it
	if (self.owner == PlayerNames::PLAYER_ONE && Locator::camera::has_value())
	{
		const auto& camera = Locator::camera::value();
		const auto* at = registry.TryGet<const Transform>(creature);
		facts.lookedAt = at != nullptr && creature_attention::IsLookingAt(camera.GetOrigin(), camera.GetFocus(), at->position);
	}
	return facts;
}

/// The town that a building belongs to, none when it has none
std::optional<entt::entity> TownOfBuilding(const ecs::Registry& registry, uint32_t townId)
{
	const auto& towns = registry.Context().towns;
	const auto found = towns.find(townId);
	return found != towns.end() && registry.Valid(found->second) ? std::optional(found->second) : std::nullopt;
}

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

/// The town of a village totem: the town of the village centre that it stands on
std::optional<entt::entity> TownOfTotem(const ecs::Registry& registry, entt::entity totem)
{
	const auto& self = registry.Get<const VillageTotem>(totem);
	const auto* centre = registry.Valid(self.townCentre) ? registry.TryGet<const Abode>(self.townCentre) : nullptr;
	return centre != nullptr ? TownOfBuilding(registry, centre->townId) : std::nullopt;
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

/// Whether the creature found no way to a thing lately. A thing fixed in the land is left alone for longer.
bool NoWayLately(const ecs::Registry& registry, const CreatureMindState& mind, entt::entity thing)
{
	const auto* belief = mind.beliefs.Find(thing);
	const bool fixed =
	    registry.AnyOf<Abode, Tree, Feature, Field, FishFarm, AnimatedStatic, Flowers, Temple, WorshipSite, VillageTotem>(
	        thing);
	return belief != nullptr && creature_object_beliefs::BlockedRecently(*belief, mind.turn, fixed);
}

/// A hungry creature does not eat a thing of a town for which it learnt a compassion worth more than this
constexpr float k_MostCompassionToEatFrom = 0.1f;

/// Whether a creature can act on a thing at all. The living and miracles can always have an action. A whole thing, a
/// worship site and a thing without data cannot. Any other thing lets a creature act on it when its data says so. The
/// temple here is one thing, which stands for its heart: the data of the heart says if a creature can act on it.
// TODO(creature-belief-types-in-trees): the game also has the creature pens of a temple as parts that a creature can act
// on. Here the pens are not things of their own.
bool LetsCreatureAct(const ecs::Registry& registry, entt::entity thing)
{
	if (registry.AnyOf<Villager, Animal, Creature, ecs::components::Spell>(thing))
	{
		return true;
	}
	if (registry.AllOf<Temple>(thing))
	{
		return Locator::infoConstants::has_value() &&
		       Locator::infoConstants::value().citadelHeart.canCreatureInteractWithMe != 0;
	}
	if (registry.AnyOf<WorshipSite, Town, Flock, ForestTurns>(thing))
	{
		return false;
	}
	// A field has the data of its town's field building, and a village totem the data of its statue
	const GObjectInfo* info = nullptr;
	if (registry.AllOf<Field>(thing))
	{
		info = FieldBuildingInfo(registry, thing);
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

/// Whether an animal of this kind frightens a creature: the lions, tigers, wolves and leopards, the bats and the vultures
bool Frightens(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::Lion:
	case AnimalInfo::Tiger:
	case AnimalInfo::Wolf:
	case AnimalInfo::Leopard:
	case AnimalInfo::SpellWolf:
	case AnimalInfo::Bat:
	case AnimalInfo::SpellBat:
	case AnimalInfo::Vulture:
		return true;
	default:
		return false;
	}
}

/// The data that tells if a creature can act on a thing. A field has the data of its town's field building, a village
/// totem the data of its statue, and the temple the data of its heart.
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
	if (registry.AllOf<Field>(thing))
	{
		return FieldBuildingInfo(registry, thing);
	}
	if (registry.AllOf<VillageTotem>(thing))
	{
		return TotemStatueInfo(registry, thing);
	}
	if (const auto* animal = registry.TryGet<const Animal>(thing))
	{
		const auto row = static_cast<size_t>(animal->type);
		return row < info.animal.size() ? &info.animal.at(row) : nullptr;
	}
	return ecs::world_objects::InfoOf(thing);
}

/// The kind of a thing, by how the desires examine it
creature_activity::Thing KindOfThing(const ecs::Registry& registry, entt::entity thing)
{
	using creature_activity::Thing;
	if (registry.AllOf<Villager>(thing))
	{
		return Thing::Villager;
	}
	if (registry.AllOf<Animal>(thing))
	{
		return Thing::Animal;
	}
	if (registry.AllOf<Creature>(thing))
	{
		return Thing::Creature;
	}
	if (registry.AllOf<ecs::components::Spell>(thing))
	{
		return Thing::Spell;
	}
	if (registry.AllOf<Tree>(thing))
	{
		return Thing::Tree;
	}
	const auto* still = registry.TryGet<const MobileStatic>(thing);
	if (registry.AnyOf<Rock, DeadTree>(thing) || (still != nullptr && still->type == MobileStaticInfo::Bonfire))
	{
		return Thing::Rock;
	}
	if (registry.AnyOf<MobileObject, Pot, Ball>(thing))
	{
		return Thing::MobileObject;
	}
	if (still != nullptr)
	{
		return Thing::MobileStatic;
	}
	if (registry.AllOf<Field>(thing))
	{
		return Thing::Field;
	}
	if (registry.AllOf<FishFarm>(thing))
	{
		return Thing::FishFarm;
	}
	if (registry.AllOf<VillageTotem>(thing))
	{
		return Thing::TotemStatue;
	}
	if (registry.AnyOf<Abode, Temple, WorshipSite, Feature, AnimatedStatic, Flowers, BigForest>(thing))
	{
		return Thing::Building;
	}
	return Thing::Other;
}

/// What a creature knows about a thing when it examines a desire for it. Only the facts that the desire uses cost time
/// to find: the height for poo, and the pick-up rule for bringing stuff home.
creature_activity::ThingFacts ThingFactsOf(const ecs::Registry& registry, entt::entity creature, Desire desire,
                                           entt::entity thing, const creature_mind_tables::Tables* tables)
{
	using creature_activity::Thing;
	const auto kind = KindOfThing(registry, thing);
	const auto* info = ActingInfoOf(registry, thing);
	creature_activity::ThingFacts facts {
	    .kind = kind,
	    .canActOn = LetsCreatureAct(registry, thing),
	    .attackable = info != nullptr && info->canCreatureAttackMe != 0,
	    .playable = info != nullptr && info->canCreaturePlayWithMe != 0,
	    .inspectable = info != nullptr && info->canCreatureInspectMe != 0,
	    .itself = thing == creature,
	};
	if (const auto* villager = registry.TryGet<const Villager>(thing))
	{
		// A villager in its home, or one that may not be picked up, cannot have help
		facts.dead = registry.AllOf<VillagerDeath>(thing) || !(villager->life > 0.0f);
		facts.dancing = registry.AllOf<Dancer>(thing);
		facts.pickable = !registry.AnyOf<AtHome, CannotBePickedUp>(thing);
	}
	else if (const auto* animal = registry.TryGet<const Animal>(thing))
	{
		facts.dead = animal->Dead();
		facts.frightening = Frightens(animal->type);
	}
	else if (registry.AllOf<Creature>(thing))
	{
		facts.dead = tables != nullptr && mind_detail::IsDead(registry, thing, *tables);
		facts.friendOfCreature = mind_detail::AreFriends(registry, thing, creature);
	}
	const auto* feature = registry.TryGet<const Feature>(thing);
	const auto* mobile = registry.TryGet<const MobileObject>(thing);
	facts.mushroom =
	    (feature != nullptr && feature->type == FeatureInfo::MagicMushroom) ||
	    (mobile != nullptr && (mobile->type == MobileObjectInfo::MagicMushroom || mobile->type == MobileObjectInfo::Toadstool));
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

/// The town of a thing: the town of a villager, a building, a field or a fish farm, of a pile in a storage pit, or of
/// a village totem. None for any other thing.
std::optional<entt::entity> TownOfThing(const ecs::Registry& registry, entt::entity thing,
                                        std::optional<entt::entity> belongsTo)
{
	const auto town = [&registry](entt::entity found) {
		return registry.Valid(found) && registry.AllOf<Town>(found) ? std::optional(found) : std::nullopt;
	};
	if (registry.AnyOf<Villager, Abode, Field>(thing) && !registry.AllOf<WorshipSite>(thing))
	{
		return belongsTo.has_value() ? town(*belongsTo) : std::nullopt;
	}
	if (const auto* farm = registry.TryGet<const FishFarm>(thing))
	{
		return town(farm->town);
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
} // namespace

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
	for (const auto& belief : mind.beliefs.NewestFirst())
	{
		const auto kind = creature_activity::WholeOf(belief.kind);
		if (!kind.has_value() || belief.thing == creature || !registry.Valid(belief.thing))
		{
			continue;
		}
		const auto facts = WholeFactsOf(registry, creature, belief.thing, *kind, *tables);
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
		if (creature_activity::ThingSuits(desire, ThingFactsOf(registry, creature, desire, thing, GetTables())))
		{
			parts.push_back(GoalWorth(creature, mind, desire, thing, activity));
		}
	};
	for (const auto& belief : mind.beliefs.PartsOf(activity.whole))
	{
		add(belief.thing);
	}
	// The temple is its own heart, the part that the creature came across first
	if (activity.kind == creature_activity::Whole::Citadel)
	{
		add(activity.whole);
	}
	return parts;
}

bool CreatureMindSystem::SuitsDesire(entt::entity creature, CreatureMindState& mind, Desire desire, entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(thing) || NoWayLately(registry, mind, thing))
	{
		return false;
	}
	// Kept within an area, it does not choose a thing that it believes is outside the area
	if (const auto place = BelievedPlaceOf(registry, mind, thing);
	    place.has_value() && OutsideConfinedArea(registry, creature, *place))
	{
		return false;
	}
	if (desire == Desire::Hunger)
	{
		if (const auto town = TownOfThing(registry, thing, BelongsTo(thing)))
		{
			ComeAcross(creature, mind, *town);
			if (ActivityUsefulness(creature, mind, Desire::Compassion, *town) > k_MostCompassionToEatFrom)
			{
				return false;
			}
		}
	}
	return creature_activity::ThingSuits(desire, ThingFactsOf(registry, creature, desire, thing, GetTables()));
}

std::vector<creature_planner::ObjectCandidate>
CreatureMindSystem::BelievedThingsToActOn(entt::entity creature, CreatureMindState& mind, Desire desire,
                                          const std::function<bool(entt::entity)>& suitsAction)
{
	std::vector<creature_planner::ObjectCandidate> found;
	for (const auto& belief : mind.beliefs.InListOrder())
	{
		if (belief.thing == creature || !SuitsDesire(creature, mind, desire, belief.thing) || !suitsAction(belief.thing))
		{
			continue;
		}
		found.push_back(GoalWorth(creature, mind, desire, belief.thing, std::nullopt));
	}
	return found;
}

std::optional<entt::entity> CreatureMindSystem::WholeOfShownThing(entt::entity creature, CreatureMindState& mind,
                                                                  entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(thing))
	{
		return std::nullopt;
	}
	ComeAcross(creature, mind, thing);
	// A creature is part of no whole thing
	if (registry.AllOf<Creature>(thing))
	{
		return std::nullopt;
	}
	// The temple is its own heart, a part of itself
	if (registry.AllOf<Temple>(thing))
	{
		return thing;
	}
	const auto whole = BelongsTo(thing);
	if (!whole.has_value() || *whole == thing || !registry.Valid(*whole))
	{
		return std::nullopt;
	}
	ComeAcross(creature, mind, *whole);
	return whole;
}

std::optional<entt::entity> CreatureMindSystem::WholeOfForcedPlan(entt::entity creature, CreatureMindState& mind, Desire desire,
                                                                  entt::entity thing)
{
	if (auto whole = WholeOfShownThing(creature, mind, thing))
	{
		return whole;
	}
	// A desire about something, with no whole thing, is about the nearest town
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	const auto* at = registry.TryGet<const Transform>(creature);
	if (tables == nullptr || at == nullptr || !tables->rules.at(static_cast<size_t>(desire)).aboutSomething)
	{
		return std::nullopt;
	}
	const auto town = mind_detail::NearestTown(registry, at->position);
	if (town == entt::null)
	{
		return std::nullopt;
	}
	ComeAcross(creature, mind, town);
	return town;
}

std::optional<CreatureMindSystem::ChosenActivity>
CreatureMindSystem::ShownActivity(entt::entity creature, CreatureMindState& mind, Desire desire, entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	const auto* at = registry.TryGet<const Transform>(creature);
	const auto whole = WholeOfShownThing(creature, mind, thing);
	const auto* belief = whole.has_value() ? mind.beliefs.Find(*whole) : nullptr;
	const auto kind = belief != nullptr ? creature_activity::WholeOf(belief->kind) : std::nullopt;
	if (tables == nullptr || at == nullptr || !kind.has_value())
	{
		return std::nullopt;
	}
	const auto facts = WholeFactsOf(registry, creature, *whole, *kind, *tables);
	if (!creature_activity::CanBeAbout(desire, facts, mind.mayAttackOwnPlayer))
	{
		return std::nullopt;
	}
	const auto usefulness = ActivityUsefulness(creature, mind, desire, *whole) *
	                        mind_detail::Nearness(registry, creature, at->position, *whole, belief->place,
	                                              tables->rules.at(static_cast<size_t>(desire)).distanceWeight);
	return ChosenActivity {.whole = *whole, .kind = *kind, .usefulness = usefulness};
}

std::vector<creature_planner::ObjectCandidate> CreatureMindSystem::ShownParts(entt::entity creature, CreatureMindState& mind,
                                                                              Desire desire, entt::entity thing,
                                                                              const ChosenActivity& activity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto suits = [&](entt::entity part) {
		return SuitsDesire(creature, mind, desire, part) && ObjectToActOnUsefulness(creature, mind, desire, part) > 0.0f;
	};
	if (suits(thing))
	{
		return {GoalWorth(creature, mind, desire, thing, activity)};
	}
	std::vector<creature_planner::ObjectCandidate> parts;
	for (const auto& belief : mind.beliefs.PartsOf(activity.whole))
	{
		if (belief.thing != creature && registry.Valid(belief.thing) && suits(belief.thing))
		{
			parts.push_back(GoalWorth(creature, mind, desire, belief.thing, activity));
		}
	}
	return parts;
}

std::vector<creature_planner::ObjectCandidate>
CreatureMindSystem::ShownThingsWithoutWhole(entt::entity creature, CreatureMindState& mind, Desire desire, entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto suits = [&](entt::entity found) {
		return registry.Valid(found) && found != creature && SuitsDesire(creature, mind, desire, found) &&
		       ObjectToActOnUsefulness(creature, mind, desire, found) > 0.0f;
	};
	if (suits(thing))
	{
		return {GoalWorth(creature, mind, desire, thing, std::nullopt)};
	}
	std::vector<creature_planner::ObjectCandidate> parts;
	if (const auto whole = WholeOfShownThing(creature, mind, thing))
	{
		for (const auto& belief : mind.beliefs.PartsOf(*whole))
		{
			if (suits(belief.thing))
			{
				parts.push_back(GoalWorth(creature, mind, desire, belief.thing, std::nullopt));
			}
		}
	}
	return parts;
}

namespace
{
/// Whether a fight lets the creature choose a new plan. Only the creature that started the fight by its own plan
/// chooses, and only while that plan is the fight: a plan that it wants much more ends the fight. The other creature
/// obeys, a creature made to fight or made to do something after a knock-out does not choose, and a creature that lies
/// out cold does not choose.
bool FightLetsChoose(const ecs::Registry& registry, entt::entity creature, const CreatureMindState& mind)
{
	const auto* fighting = registry.TryGet<const CreatureFighting>(creature);
	return creature_fight::LetsChoose({
	    .knockedOut = registry.AllOf<CreatureKnockedOut>(creature),
	    .fighting = fighting != nullptr,
	    .startedIt = fighting != nullptr && fighting->madeArena,
	    .planned = fighting != nullptr && fighting->planned,
	    .waitsForFight = mind.planActive && mind.idle.step < mind.idle.agenda.size() &&
	                     mind.idle.agenda[mind.idle.step].kind == creature_mind::Step::Kind::Fight,
	    .won = fighting != nullptr && fighting->stage == CreatureFighting::Stage::Celebrate,
	});
}
} // namespace

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

void CreatureMindSystem::PlanCreature(entt::entity creature, CreatureMindState& mind, bool everyDesire,
                                      std::optional<Desire> only, bool adopt)
{
	const auto* tables = GetTables();
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (tables == nullptr || transform == nullptr || !mind.desires.has_value() || !mind.learnt.has_value() || mind.paused ||
	    creature_mind::IsUnconscious(mind.idle) || !FightLetsChoose(registry, creature, mind))
	{
		return;
	}
	if (mind.plannedTurn == mind.turn)
	{
		return;
	}
	mind.plannedTurn = mind.turn;
	// Copying the player comes first. With no agenda, it does the action of the copy stage. While it notices the deed or
	// copies the action, it does not choose a new plan when it has one.
	if (!only.has_value() && mind.learnt->mimicry.has_value())
	{
		const bool running = mind.idle.step < mind.idle.agenda.size();
		if (!running)
		{
			ProcessMimicry(creature, mind);
		}
		if (running && mind.learnt->mimicry.has_value() &&
		    mind.learnt->mimicry->stage != creature_watching::MimicStage::CopyDesire)
		{
			return;
		}
	}
	// When it completes its work, the thing that waits replaces its next decision: it becomes high after a magic
	// mushroom, or it reacts to a stroke or a slap
	if (adopt && mind.followUp.has_value() && mind.idle.step >= mind.idle.agenda.size() &&
	    !registry.AllOf<ScriptControlled>(creature))
	{
		const auto followUp = *mind.followUp;
		mind.followUp.reset();
		DoFollowUp(creature, mind, followUp);
		return;
	}
	const glm::vec2 position {transform->position.x, transform->position.z};
	const auto& desires = *mind.desires;
	const auto& learnt = *mind.learnt;

	creature_plan_actions::Situation situation;
	if (Locator::camera::has_value())
	{
		const auto eye = Locator::camera::value().GetOrigin();
		situation.camera = glm::vec2(eye.x, eye.z);
	}
	Situate(creature, situation);
	const auto random = [this](uint32_t range) { return Random(range); };
	bool fishingLooked = false;
	const auto prepare = [&](const creature_plan_actions::Executor& executor) {
		const bool fishing = executor.build == creature_plan_actions::Build::FishAndEat ||
		                     executor.build == creature_plan_actions::Build::GiveFishToStore ||
		                     executor.build == creature_plan_actions::Build::TakeFishHome ||
		                     executor.build == creature_plan_actions::Build::PutFishBySite;
		if (fishing && !fishingLooked)
		{
			fishingLooked = true;
			situation.fishing = FishingFor(registry, creature, position);
			if (const auto home = ecs::creature_home::HomeOf(registry, creature))
			{
				situation.home = glm::vec2(home->x, home->z);
			}
		}
	};

	std::array<float, creature_desires::k_DesireCount> strengths {};
	std::array<bool, creature_desires::k_DesireCount> eligible {};
	for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
	{
		const auto& state = desires.desires.at(d);
		const auto strength = state.activated && state.suppressedTurns == 0 ? state.value : 0.0f;
		strengths.at(d) = strength;
		eligible.at(d) = strength > 0.0f && std::ranges::any_of(tables->desireActions.at(d), [tables](uint32_t action) {
			                 return creature_plan_actions::For(tables->actions.at(action).name) != nullptr;
		                 });
	}
	// The desire to explore has a condition of its own: a young creature, or one that is not too exhausted
	{
		const auto* body = registry.TryGet<const CreatureNeeds>(creature);
		const float exhaustion = body != nullptr ? body->needs.exhaustion : 0.0f;
		auto& wander = eligible.at(static_cast<size_t>(Desire::Wanderlust));
		wander = wander && creature_exploration::WanderlustAllowed(mind.developmentPhase, exhaustion);
	}

	// Something shown on the leash is the only goal there is this turn, for every desire
	std::optional<entt::entity> leashTarget;
	if (!mind.leash.actOn.empty())
	{
		leashTarget = static_cast<entt::entity>(mind.leash.actOn.back());
		mind.leash.actOn.clear();
	}
	auto goals =
	    leashTarget.has_value() || everyDesire
	        ? std::vector<Desire> {}
	        : creature_planner::NextGoals(mind.planner, eligible, creature_planner::GoalsPerTurn(registry.Size<Creature>()));
	if (leashTarget.has_value() || everyDesire)
	{
		for (size_t d = 0; d < eligible.size(); ++d)
		{
			if (eligible.at(d))
			{
				goals.push_back(static_cast<Desire>(d));
			}
			else
			{
				mind.planner.best.at(d).reset();
			}
		}
	}

	if (only.has_value())
	{
		goals.clear();
		if (eligible.at(static_cast<size_t>(*only)))
		{
			goals.push_back(*only);
		}
	}

	for (const auto desire : goals)
	{
		const auto d = static_cast<size_t>(desire);
		// A desire about something is about a whole thing: a town, a forest, a temple, a flock or a creature. The plan
		// acts on a part of the whole thing. Without a whole thing, the desire has no plan.
		std::span<const uint32_t> actions = tables->desireActions.at(d);
		std::vector<uint32_t> townActions;
		std::optional<ChosenActivity> activity;
		std::vector<creature_planner::ObjectCandidate> parts;
		const bool aboutSomething = tables->rules.at(d).aboutSomething;
		if (aboutSomething && (!leashTarget.has_value() || desire == Desire::Compassion))
		{
			activity = ChooseActivity(creature, mind, desire);
			// Compassion does what the town desire that it helps with asks for
			// TODO(creature-activity-object): find what compassion does for a creature that it is about
			if (desire == Desire::Compassion && activity.has_value() && activity->kind != creature_activity::Whole::Town)
			{
				activity.reset();
			}
			if (!activity.has_value())
			{
				mind.planner.best.at(d).reset();
				continue;
			}
			if (desire == Desire::Compassion)
			{
				townActions = TownCompassionActions(registry, creature, mind, activity->whole, *tables, random);
				actions = townActions;
			}
			parts = PartsToActOn(creature, mind, desire, *activity);
			// Anger chooses the part to act on before the action: the part that is most useful to act on
			if (desire == Desire::Anger)
			{
				std::vector<float> partWorth;
				std::ranges::transform(parts, std::back_inserter(partWorth), &creature_planner::ObjectCandidate::usefulness);
				const auto chosen = creature_activity::MostUseful(partWorth);
				parts =
				    chosen.has_value() ? std::vector {parts.at(*chosen)} : std::vector<creature_planner::ObjectCandidate> {};
			}
		}
		else if (aboutSomething)
		{
			// The thing shown on the leash makes the desire about the whole thing that the shown thing is part of. The
			// plan acts on the shown thing when it suits the desire and is of use, else on the first part of the whole
			// thing that is.
			activity = ShownActivity(creature, mind, desire, *leashTarget);
			if (!activity.has_value())
			{
				mind.planner.best.at(d).reset();
				continue;
			}
			parts = ShownParts(creature, mind, desire, *leashTarget, *activity);
		}
		else if (leashTarget.has_value())
		{
			// A desire not about a whole thing acts on the shown thing when it suits the desire and is of use. If not, it
			// acts on the first part of the whole thing that suits the desire and is of use.
			parts = ShownThingsWithoutWhole(creature, mind, desire, *leashTarget);
		}
		const auto worth = [&](entt::entity thing) { return GoalWorth(creature, mind, desire, thing, activity); };
		// The actions of the desire, in the order of its list
		std::vector<creature_planner::ActionCandidate> candidates;
		std::vector<const creature_plan_actions::Executor*> executors;
		for (const auto action : actions)
		{
			const auto* executor = creature_plan_actions::For(tables->actions.at(action).name);
			if (executor == nullptr)
			{
				continue;
			}
			prepare(*executor);
			candidates.push_back({
			    .action = action,
			    .needsObject = tables->actions.at(action).actsOnThing,
			    .usesThing = executor->use != Target::None,
			    .opinion = learnt.opinions.at(action),
			    .turnsSinceDone = learnt.turnsSinceDone.at(action),
			    .servesFirstDesire = tables->actions.at(action).desire == 0,
			    .disabled =
			        !creature_plan_actions::Possible(*executor, situation) ||
			        !creature_plan_actions::Allowed(*executor, situation) ||
			        creature_plan_actions::DoneTooRecently(*executor, learnt.turnsSinceDone.at(action),
			                                               action < mind.lastDoneTurn.size() ? mind.lastDoneTurn.at(action) : 0,
			                                               TurnNow(), static_cast<uint32_t>(k_TurnsPerSecond)) ||
			        (executor->build == creature_plan_actions::Build::DanceWithVillagers &&
			         !MayDanceAgain(creature, executor->danceType)) ||
			        !creature_watching::KnowsWhatItNeeds(learnt.knowledge, tables->actions.at(action).needs) ||
			        (creature_plan_actions::IsCast(*executor) &&
			         !MayCast(creature, mind, action, IsPowerUpCast(tables->actions.at(action).name))),
			});
			executors.push_back(executor);
		}
		const auto executorOf = [&](const creature_planner::ActionCandidate& candidate) {
			const auto at = std::ranges::find(candidates, candidate.action, &creature_planner::ActionCandidate::action);
			return executors.at(static_cast<size_t>(std::distance(candidates.begin(), at)));
		};
		const auto heldByItself = [&](entt::entity thing) {
			const auto* held = registry.TryGet<const HeldByCreature>(thing);
			return held != nullptr && held->creature == creature;
		};
		// The thing that an action can act on: the most useful, the first of equals, and only one that is worth more than
		// nothing. An action that must not act on what the creature holds does not take a thing that it holds.
		const auto mostUseful = [](const std::vector<creature_planner::ObjectCandidate>& found) {
			std::optional<creature_planner::ObjectCandidate> best;
			for (const auto& object : found)
			{
				if (object.usefulness > (best.has_value() ? best->usefulness : 0.0f))
				{
					best = object;
				}
			}
			return best;
		};
		const auto findObject =
		    [&](const creature_planner::ActionCandidate& candidate) -> std::optional<creature_planner::ObjectCandidate> {
			const auto* executor = executorOf(candidate);
			const auto suits = [&](entt::entity thing) {
				return (executor->target == Target::None || Accepts(registry, executor->target, thing, creature)) &&
				       !(executor->notWhatItHolds && heldByItself(thing));
			};
			std::vector<creature_planner::ObjectCandidate> found;
			if (leashTarget.has_value() && desire != Desire::Compassion && (activity.has_value() || !aboutSomething))
			{
				// The first of the shown parts that the action can act on
				const auto first = std::ranges::find_if(parts, [&](const creature_planner::ObjectCandidate& part) {
					return suits(static_cast<entt::entity>(part.id));
				});
				return first != parts.end() ? std::optional(*first) : std::nullopt;
			}
			if (leashTarget.has_value())
			{
				if (suits(*leashTarget) && (!activity.has_value() || BelongsTo(*leashTarget) == activity->whole))
				{
					found.push_back(worth(*leashTarget));
				}
			}
			else if (activity.has_value())
			{
				std::ranges::copy_if(parts, std::back_inserter(found), [&](const creature_planner::ObjectCandidate& part) {
					return suits(static_cast<entt::entity>(part.id));
				});
			}
			else
			{
				found = BelievedThingsToActOn(creature, mind, desire, suits);
			}
			return mostUseful(found);
		};
		const auto distanceWeight = tables->rules.at(d).distanceWeight;
		const auto findUsed = [&](const creature_planner::ActionCandidate& candidate, std::optional<uint32_t> object) {
			return UsedFor(creature, executorOf(candidate)->use, position, distanceWeight, object);
		};
		// A desire about a whole thing finds the action before the part, but anger finds the part first
		auto order = creature_planner::PlanOrder::ActionFirst;
		if (leashTarget.has_value() && desire != Desire::Compassion && (activity.has_value() || !aboutSomething))
		{
			order = creature_planner::PlanOrder::PartThenAction;
		}
		else if (activity.has_value() && !leashTarget.has_value())
		{
			order = desire == Desire::Anger ? creature_planner::PlanOrder::PartThenAction
			                                : creature_planner::PlanOrder::ActionThenPart;
		}
		auto best = creature_planner::PlanDesire({
		    .desire = desire,
		    .strength = strengths.at(d),
		    .order = order,
		    .actions = candidates,
		    .activityUsefulness = activity.has_value() ? std::optional(activity->usefulness) : std::nullopt,
		    .findObject = findObject,
		    .findUsed = findUsed,
		    .itself = entt::to_integral(creature),
		});
		if (best.has_value() && activity.has_value())
		{
			best->about = entt::to_integral(activity->whole);
		}
		mind.planner.best.at(d) = best;
		mind.plannedActions.at(d).assign(actions.begin(), actions.end());
	}

	// When the planner examines only one desire, the creature starts the plan immediately, if there is a plan
	if (only.has_value())
	{
		if (const auto plan = mind.planner.best.at(static_cast<size_t>(*only)); adopt && plan.has_value())
		{
			Adopt(creature, mind, *plan, situation);
		}
		return;
	}
	// Doing something it chose for itself that isn't idling, it carries on
	if (!mind.planActive && !Interruptible(mind.idle.activity) && mind.idle.step < mind.idle.agenda.size())
	{
		return;
	}
	const auto choice = creature_planner::Choose(mind.planner);
	if (choice.has_value() && !Adopt(creature, mind, *choice, situation))
	{
		mind.planner.best.at(static_cast<size_t>(choice->desire)).reset();
	}
}

void CreatureMindSystem::AteMagicMushroom(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	// Every magic mushroom it eats counts, also while it is high
	if (auto* body = registry.Valid(creature) ? registry.TryGet<Creature>(creature) : nullptr)
	{
		++body->magicMushroomsEaten;
	}
	if (auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr)
	{
		mind->followUp = creature_follow_up::AfterMushroom(mind->followUp);
	}
}

void CreatureMindSystem::DoFollowUp(entt::entity creature, CreatureMindState& mind, creature_follow_up::FollowUp followUp)
{
	using creature_follow_up::FollowUp;
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} does what waited: {}", entt::to_integral(creature),
	                    static_cast<int>(followUp));
	switch (followUp)
	{
	case FollowUp::BecomeHigh:
		BecomeHigh(creature);
		return;
	case FollowUp::ShowHappy:
		static_cast<void>(ForcePlan(creature, {.desire = Desire::ManifestState,
		                                       .action = "CommunicateState",
		                                       .object = creature,
		                                       .agenda = creature_follow_up::ShowHappy()}));
		return;
	case FollowUp::FrightenedOfPlayer:
		static_cast<void>(ForcePlan(
		    creature, {.desire = Desire::Fear, .action = creature_follow_up::FrightenedAction(Random(3)), .object = creature}));
		return;
	case FollowUp::DoLessonAgain:
		DoLessonAgain(creature, mind);
		return;
	case FollowUp::ShowSlapFeeling:
	{
		const creature_follow_up::Feelings feelings {
		    .compassion = mind.desires.has_value() ? (*mind.desires)[Desire::Compassion].value : 0.0f,
		    .anger = mind.desires.has_value() ? (*mind.desires)[Desire::Anger].value : 0.0f,
		};
		auto agenda = creature_follow_up::ShowSlapFeeling(
		    feelings, [this](uint32_t range) { return Random(range); }, Chance(),
		    static_cast<uint32_t>(std::lround(k_TurnsPerSecond)));
		static_cast<void>(ForcePlan(
		    creature,
		    {.desire = Desire::ManifestState, .action = "CommunicateState", .object = creature, .agenda = std::move(agenda)}));
		return;
	}
	}
}

void CreatureMindSystem::BecomeHigh(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (transform == nullptr)
	{
		return;
	}
	const glm::vec2 position {transform->position.x, transform->position.z};
	const auto radius = object_measures::TwoDRadius(registry, creature);
	const auto findSeat = [&]() -> std::optional<creature_mind::HighSeat> {
		const auto place = ClearAreaFor(registry, creature, position, creature_mind::k_HighSeatRadii * radius, false);
		return place.has_value() ? std::optional(creature_mind::HighSeat {.place = *place, .radius = radius}) : std::nullopt;
	};
	auto agenda = creature_mind::BecomeHigh(
	    findSeat, [this](uint32_t range) { return Random(range); }, [this](float most) { return Chance() * most; });
	static_cast<void>(
	    ForcePlan(creature, {.desire = Desire::GetHigh, .action = "GetHigh", .object = creature, .agenda = std::move(agenda)}));
}

bool CreatureMindSystem::ForcePlan(entt::entity creature, const ForcedPlan& plan)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	const auto* transform = registry.Valid(creature) ? registry.TryGet<const Transform>(creature) : nullptr;
	const auto* tables = GetTables();
	if (mind == nullptr || body == nullptr || transform == nullptr || tables == nullptr)
	{
		return false;
	}
	// A creature told to do something as soon as it is made has a mind ready for it
	SetUpMind(creature, *body, *mind);
	const glm::vec2 position {transform->position.x, transform->position.z};
	const auto height = k_HeightOfSizeOne * ShownSize(*body);
	const auto random = [this](uint32_t range) { return Random(range); };
	auto activity = plan.activity.value_or(creature_mind::Activity::Told);
	auto agenda = plan.agenda;
	if (!agenda.has_value())
	{
		if (plan.action == creature_leash_orders::k_HoldAction && plan.object.has_value())
		{
			// It plays with what it is told to hold only the first times that it holds a thing of that sort. A thing
			// with no sort gets no play.
			std::optional<uint32_t> playWith;
			if (const auto sort = mind_detail::ObjectSortOf(registry, *plan.object); sort.has_value())
			{
				auto& held = mind->heldSortCounts[*sort];
				if (held < creature_leash_orders::k_HoldPlayLimit)
				{
					playWith = Random(static_cast<uint32_t>(creature_object_actions::k_KeepAnimationCount));
					++held;
				}
			}
			const bool alreadyHeld = Locator::creatureObjectActionSystem::has_value() &&
			                         Locator::creatureObjectActionSystem::value().GetHeld(creature) == plan.object;
			const auto eye = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : transform->position;
			agenda = creature_leash_orders::Hold(
			    entt::to_integral(*plan.object), alreadyHeld, glm::vec2(eye.x, eye.z), playWith,
			    creature_leash_orders::HoldSeconds(Chance() * creature_leash_orders::k_HoldExtraSeconds, k_TurnsPerSecond));
		}
		else if (plan.action == creature_leash_orders::k_ReflectionAction)
		{
			const auto water = NearestWater(position);
			if (water.has_value())
			{
				agenda = creature_leash_orders::LookAtReflection(water->water, water->shore, height, k_TurnsPerSecond);
			}
		}
		else if (plan.action == creature_leash_orders::k_SleepAction && plan.point.has_value())
		{
			activity = creature_mind::Activity::Sleep;
			agenda = creature_leash_orders::SleepAtHome(*plan.point, height, creature_mind::Sleep(random));
		}
		else if (const auto* executor = creature_plan_actions::For(plan.action))
		{
			creature_plan_actions::Situation situation;
			if (executor->build == creature_plan_actions::Build::Drink)
			{
				situation.drink = DrinkPlaceOf(creature);
			}
			if (Locator::camera::has_value())
			{
				const auto eye = Locator::camera::value().GetOrigin();
				situation.camera = glm::vec2(eye.x, eye.z);
			}
			Situate(creature, situation);
			situation.chance = Chance();
			if (plan.instrument.has_value())
			{
				situation.instrument = entt::to_integral(*plan.instrument);
				situation.instrumentPoint = PointOf(registry, *plan.instrument).value_or(position);
				situation.instrumentRadius = object_measures::TwoDRadius(registry, *plan.instrument);
				situation.usedOneShot = OneShotOf(registry, *plan.instrument);
			}
			situation.handFull = registry.AllOf<CreatureHeldObject>(creature);
			if (plan.activityObject.has_value())
			{
				situation.activity = entt::to_integral(*plan.activityObject);
			}
			if (plan.object.has_value() && registry.Valid(*plan.object))
			{
				situation.thingHeight = object_measures::Height(registry, *plan.object);
				situation.thingRadius = object_measures::TwoDRadius(registry, *plan.object);
				if (executor->build == creature_plan_actions::Build::EatFromStoragePit)
				{
					SituateStoreFood(registry, *plan.object, situation);
				}
			}
			if (const auto home = ecs::creature_home::HomeOf(registry, creature))
			{
				situation.home = glm::vec2(home->x, home->z);
			}
			const auto point =
			    plan.object.has_value() ? PointOf(registry, *plan.object).value_or(position) : plan.point.value_or(position);
			// Sleeping by a thing, it looks for a clear place four times as wide as it is tall about it; one not under a
			// script's control also wants nothing living there
			if (executor->build == creature_plan_actions::Build::SleepByObject)
			{
				situation.clearArea =
				    ClearAreaFor(registry, creature, point, creature_mind::k_SleepByObjectHeights * situation.height,
				                 !situation.controlledByScript);
			}
			if (const auto clear = EmoteClearArea(creature, *executor, point, situation))
			{
				situation.clearArea = *clear;
			}
			SituateExploration(creature, *executor, situation);
			if (executor->build == creature_plan_actions::Build::DanceWithVillagers)
			{
				SituateDance(creature, *executor, point, situation);
			}
			mind_town_help::Situate(registry, creature, *executor,
			                        plan.object.has_value() ? std::optional(entt::to_integral(*plan.object)) : std::nullopt,
			                        situation);
			const auto actionIndex = creature_mind_tables::FindAction(*tables, plan.action);
			const auto cast = creature_plan_actions::IsCast(*executor) && actionIndex.has_value()
			                      ? CastInfoFor(creature, *actionIndex, plan.object)
			                      : std::nullopt;
			agenda = creature_plan_actions::Agenda(
			    *executor, plan.object.has_value() ? std::optional(entt::to_integral(*plan.object)) : std::nullopt, point,
			    situation, random, cast);
			if (agenda.has_value())
			{
				BelieveInTempleToEnter(creature, *executor, situation);
			}
			else if (executor->build == creature_plan_actions::Build::Drink && mind->desires.has_value())
			{
				creature_drink::NoPlaceFound(*mind->desires, k_TurnsPerSecond);
			}
			activity = executor->activity;
		}
	}
	if (!agenda.has_value() || !Replan(creature, activity, std::move(*agenda)))
	{
		return false;
	}
	// Forced, nothing it wants is ever more pressing than the plan until it is done
	const auto action = creature_mind_tables::FindAction(*tables, plan.action);
	if (!action.has_value())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} carries out {}, which the action table hasn't",
		                    entt::to_integral(creature), plan.action);
		return true;
	}
	const auto object = plan.object.has_value() ? std::optional(entt::to_integral(*plan.object)) : std::nullopt;
	mind->planner.current = creature_planner::Plan {
	    .desire = plan.desire, .action = *action, .object = object, .priority = std::numeric_limits<float>::max()};
	mind->planActive = true;
	mind->planForced = true;
	mind->planSerial = mind->idle.serial;
	mind->agendaSeen = mind->idle.serial;
	mind->satisfiedByEffect = false;
	mind->desireSeenTo = false;
	mind->planActions = tables->desireActions.at(static_cast<size_t>(plan.desire));
	auto& learnt = *mind->learnt;
	const auto& info = tables->actions[*action];
	// TODO(usefulness-use-tree): the game's forced plans each name what their desire is about; only the give passes it
	// here yet, and the rest count as the planner's own plans do
	const auto lessonActivity = plan.activityObject.has_value() ? std::optional(entt::to_integral(*plan.activityObject))
	                                                            : LessonActivity(*tables, plan.desire, object, std::nullopt);
	// It is remembered for the player's feedback, unless nothing says why it wants what the plan serves
	if (const auto source = creature_learning::PlanSource((*mind->desires)[plan.desire]); source.has_value())
	{
		creature_learning::Remember(
		    learnt.contexts,
		    {
		        .action = *action,
		        .desire = plan.desire,
		        .object = object,
		        .belief = plan.object.has_value() ? ActedOnBeliefOf(registry, *plan.object, creature) : std::nullopt,
		        .activity = lessonActivity,
		        .activityBelief = lessonActivity.has_value()
		                              ? BeliefOf(registry, static_cast<entt::entity>(*lessonActivity), creature)
		                              : std::nullopt,
		        .learnable = info.learnable,
		        .windowSeconds = info.learningWindowSeconds,
		        .source = *source,
		    });
	}
	creature_learning::SuppressOpposed(*mind->desires, plan.desire, tables->dependencies, k_TurnsPerSecond);
	return true;
}

bool CreatureMindSystem::CarryOutForScript(entt::entity creature, std::vector<creature_mind::Step> agenda)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind == nullptr)
	{
		return false;
	}
	// The script takes it from a fight or from lying knocked out first
	if (Locator::creatureFightSystem::has_value())
	{
		Locator::creatureFightSystem::value().TakeForScript(creature);
	}
	if (!Replan(creature, creature_mind::Activity::Told, std::move(agenda)))
	{
		return false;
	}
	registry.AssignOrReplace<ScriptControlled>(creature);
	// It obeys its player by following them, as far as its plan goes, which nothing it wants replaces until the agenda
	// is over; unlike an order, it learns nothing from it
	if (const auto* tables = GetTables(); tables != nullptr)
	{
		if (const auto action = creature_mind_tables::FindAction(*tables, "FollowPlayer"); action.has_value())
		{
			mind->planner.current = creature_planner::Plan {.desire = creature_desires::Desire::ObeyPlayer,
			                                                .action = *action,
			                                                .object = entt::to_integral(creature),
			                                                .priority = std::numeric_limits<float>::max()};
		}
	}
	mind->planActive = true;
	mind->planSerial = mind->idle.serial;
	mind->agendaSeen = mind->idle.serial;
	mind->satisfiedByEffect = false;
	mind->desireSeenTo = false;
	return true;
}

void CreatureMindSystem::SetScriptFocus(entt::entity creature, std::optional<creature_mind::Gaze> focus)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr; mind != nullptr)
	{
		mind->scriptFocus = focus;
	}
}

namespace
{
/// Where the altar stands of the worship site that a creature of the player takes a sacrifice to: the site in the first
/// place around the player's temple that has a site. None when the player has no temple, or the temple has no site.
std::optional<glm::vec3> SacrificeAltarOf(const ecs::Registry& registry, PlayerNames player)
{
	std::optional<glm::vec3> altar;
	registry.Each<const Temple, const CitadelWorship>([&](const Temple& temple, const CitadelWorship& worship) {
		if (temple.owner != player || altar.has_value())
		{
			return;
		}
		std::array<bool, CitadelWorship::k_Places> placeHasSite {};
		std::ranges::transform(worship.sites, placeHasSite.begin(),
		                       [&registry](entt::entity site) { return site != entt::null && registry.Valid(site); });
		const auto place = creature_sacrifice::SitePlace(placeHasSite);
		if (!place.has_value())
		{
			return;
		}
		const auto* site = registry.TryGet<const WorshipSite>(worship.sites.at(*place));
		const auto* at =
		    site != nullptr && registry.Valid(site->altar) ? registry.TryGet<const Transform>(site->altar) : nullptr;
		if (at != nullptr)
		{
			altar = at->position;
		}
	});
	return altar;
}
} // namespace

void CreatureMindSystem::Situate(entt::entity creature, creature_plan_actions::Situation& situation)
{
	const auto& registry = Locator::entitiesRegistry::value();
	situation.self = entt::to_integral(creature);
	if (const auto* mind = registry.TryGet<const CreatureMindState>(creature); mind != nullptr)
	{
		situation.stage = mind->developmentPhase;
	}
	if (Locator::camera::has_value())
	{
		situation.eye = Locator::camera::value().GetOrigin();
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	situation.hasPlayer = body != nullptr && body->owner != PlayerNames::NEUTRAL;
	situation.controlledByScript = registry.AllOf<ScriptControlled>(creature);
	const auto* needs = registry.TryGet<const CreatureNeeds>(creature);
	situation.healthyEnoughToFight = creature_fight::HealthyEnoughToFight(needs != nullptr ? needs->needs.life : 1.0f);
	situation.radius = object_measures::TwoDRadius(registry, creature);
	situation.height = body != nullptr ? k_HeightOfSizeOne * ShownSize(*body) : 0.0f;
	const auto held = Locator::creatureObjectActionSystem::has_value()
	                      ? Locator::creatureObjectActionSystem::value().GetHeld(creature)
	                      : std::nullopt;
	situation.handFull = held.has_value();
	if (held.has_value())
	{
		situation.holdingEatable = Locator::creatureObjectActionSystem::value().CanEatHeld(creature);
		situation.heldOneShot = OneShotOf(registry, *held);
		const auto thing = Locator::creatureObjectActionSystem::value().KindOf(*held).thing;
		const auto* mobile = registry.TryGet<const MobileObject>(*held);
		situation.holdingTree = thing == creature_object_actions::Thing::Tree;
		situation.holdingLiving = creature_object_actions::IsLiving(thing);
		situation.holdingRock =
		    thing == creature_object_actions::Thing::Rock || thing == creature_object_actions::Thing::Bonfire;
		situation.holdingMushroom = thing == creature_object_actions::Thing::MagicMushroom ||
		                            (mobile != nullptr && (mobile->type == MobileObjectInfo::MagicMushroom ||
		                                                   mobile->type == MobileObjectInfo::Toadstool));
	}
	if (const auto* at = registry.TryGet<const Transform>(creature))
	{
		situation.position = {at->position.x, at->position.z};
		// Only the local player's hands are about to throw to
		const bool local = body != nullptr && Locator::playerSystem::has_value() &&
		                   body->owner == Locator::playerSystem::value().GetLocalPlayer();
		if (local && Locator::handSystem::has_value())
		{
			for (const auto& hand : Locator::handSystem::value().GetPlayerHandPositions())
			{
				if (hand.has_value() &&
				    (!situation.playerHand.has_value() ||
				     glm::distance(*hand, at->position) < glm::distance(*situation.playerHand, at->position)))
				{
					situation.playerHand = hand;
				}
			}
		}
	}
	if (const auto* transform = registry.TryGet<const Transform>(creature); transform != nullptr && situation.eye.has_value())
	{
		situation.eyeDistance = glm::distance(*situation.eye, transform->position);
	}
	situation.hangAround = HangAroundPlacesOf(creature);
	situation.turnSeconds = k_TurnSeconds;
	situation.turnsPerSecond = k_TurnsPerSecond;
	SituatePlayer(creature, situation);
	SituateHome(creature, situation);
	SituateEmotes(creature, situation);
	if (body != nullptr)
	{
		situation.sacrificeAltar = SacrificeAltarOf(registry, body->owner);
	}
}

void CreatureMindSystem::SituatePlayer(entt::entity creature, creature_plan_actions::Situation& situation)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* at = registry.TryGet<const Transform>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	// Only the local player's camera looks at creatures here
	if (at != nullptr && body != nullptr && Locator::camera::has_value() && Locator::playerSystem::has_value() &&
	    body->owner == Locator::playerSystem::value().GetLocalPlayer())
	{
		const auto& camera = Locator::camera::value();
		situation.lookedAt = creature_attention::IsLookingAt(camera.GetOrigin(), camera.GetFocus(), at->position);
	}
	if (const auto* mind = registry.TryGet<const CreatureMindState>(creature); mind != nullptr)
	{
		situation.handMoving = situation.playerHand.has_value() && creature_player_attention::IsHandMoving(mind->handSpeed);
	}
	if (at != nullptr && situation.playerHand.has_value() && Locator::camera::has_value())
	{
		const auto& camera = Locator::camera::value();
		situation.awayFromHand =
		    creature_player_attention::AwayFromHand(at->position, *situation.playerHand, camera.GetOrigin(), camera.GetFocus());
	}
}

void CreatureMindSystem::SituateEmotes(entt::entity creature, creature_plan_actions::Situation& situation)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (Locator::skySystem::has_value())
	{
		const auto& clock = Locator::skySystem::value().GetClock();
		situation.night = clock.IsVisualNight();
		// The creature looks for the moon at the place that the sky draws it at from the camera, taken as a place on
		// the land
		situation.moon = graphics::moon::Offset(clock.GetScriptTime());
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	situation.evil = body != nullptr && body->alignment < 0.0f;
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	situation.leashed = leashes != nullptr && leashes->worn.has_value();
	const auto* at = registry.TryGet<const Transform>(creature);
	if (situation.playerHand.has_value() && at != nullptr)
	{
		situation.handDistance = glm::distance(*situation.playerHand, at->position);
	}
	if (const auto* mind = registry.TryGet<const CreatureMindState>(creature))
	{
		situation.attitudeToPlayer = mind->attitudeToPlayer;
	}
	if (Locator::creatureObjectActionSystem::has_value())
	{
		if (const auto held = Locator::creatureObjectActionSystem::value().GetHeld(creature))
		{
			const auto life = magic_living::LifeOf(*held);
			situation.holdingStrokeable =
			    life.has_value() && *life < 1.0f && Locator::creatureObjectActionSystem::value().CanPickUp(creature, *held);
		}
	}
}

std::optional<glm::vec2> CreatureMindSystem::EmoteClearArea(entt::entity creature,
                                                            const creature_plan_actions::Executor& executor, glm::vec2 point,
                                                            const creature_plan_actions::Situation& situation)
{
	const auto& registry = Locator::entitiesRegistry::value();
	switch (executor.build)
	{
	case creature_plan_actions::Build::Sleep:
		// Sleeping on the spot, it looks for a clear place about itself four times as wide as it is tall; one not under a
		// script's control also wants nothing living there
		return ClearAreaFor(registry, creature, situation.position, creature_emote::k_SleepOnTheSpotHeights * situation.height,
		                    !situation.controlledByScript);
	case creature_plan_actions::Build::SitDown:
		// Sitting down, it looks for a clear place about the thing four times as wide as its own radius
		return ClearAreaFor(registry, creature, point, creature_emote::k_SitDownRadii * situation.radius, false);
	default:
		return std::nullopt;
	}
}

void CreatureMindSystem::NoteDone(CreatureMindState& mind, uint32_t action)
{
	if (mind.lastDoneTurn.size() <= action)
	{
		mind.lastDoneTurn.resize(static_cast<size_t>(action) + 1, 0);
	}
	mind.lastDoneTurn.at(action) = TurnNow();
}

void CreatureMindSystem::EndUnderway(CreatureMindState& mind)
{
	const auto* tables = GetTables();
	if (!mind.underway.has_value() || !mind.learnt.has_value() || tables == nullptr ||
	    mind.underway->action >= tables->actions.size())
	{
		return;
	}
	const auto desire = tables->actions.at(mind.underway->action).desire;
	const auto list = desire < tables->desireActions.size() ? std::span<const uint32_t>(tables->desireActions.at(desire))
	                                                        : std::span<const uint32_t> {};
	creature_action_history::PlanEnded(mind.learnt->turnsSinceDone, mind.underway->action, list, mind.agendaTurns);
}

void CreatureMindSystem::BelieveInTempleToEnter(entt::entity creature, const creature_plan_actions::Executor& executor,
                                                const creature_plan_actions::Situation& situation)
{
	// As it sets off into its temple, the creature comes to believe in the temple's heart
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (executor.build != creature_plan_actions::Build::EnterTemple || !situation.templeHeart.has_value() || mind == nullptr)
	{
		return;
	}
	ComeAcross(creature, *mind, static_cast<entt::entity>(situation.templeHeart->temple));
}

void CreatureMindSystem::SituateHome(entt::entity creature, creature_plan_actions::Situation& situation)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* at = registry.TryGet<const Transform>(creature);
	if (body == nullptr || at == nullptr)
	{
		return;
	}
	situation.standing = at->position;
	if (const auto home = ecs::creature_home::HomeOf(registry, creature))
	{
		situation.home = glm::vec2(home->x, home->z);
		situation.homeDistance = gutils::GetDistanceInMetres(situation.position, *situation.home);
	}
	// The heart of its player's temple is the temple itself, on the ground
	registry.Each<const Temple, const Transform>([&](entt::entity temple, const Temple& building, const Transform& place) {
		if (building.owner != body->owner || situation.templeHeart.has_value())
		{
			return;
		}
		const auto ground = Locator::terrainSystem::has_value()
		                        ? Locator::terrainSystem::value().GetHeightAt({place.position.x, place.position.z})
		                        : place.position.y;
		situation.templeHeart = creature_plan_actions::Situation::TempleHeart {
		    .temple = entt::to_integral(temple),
		    .point = {place.position.x, ground, place.position.z},
		    .radius = object_measures::TwoDRadius(registry, temple),
		};
	});
	situation.hasTemple = situation.templeHeart.has_value();
	// Every other creature counts, of any player, dead or alive
	std::vector<float> others;
	registry.Each<const Creature, const Transform>([&](entt::entity other, const Creature&, const Transform& place) {
		if (other != creature)
		{
			others.push_back(gutils::GetDistanceInMetres(place.position, at->position));
		}
	});
	situation.nothingFrighteningNear = creature_mind::NothingFrighteningNear(situation.height, others);
}

creature_mind::HangAroundPlaces CreatureMindSystem::HangAroundPlacesOf(entt::entity creature)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto onGround = [](glm::vec3 point) {
		const auto ground =
		    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt({point.x, point.z}) : point.y;
		return glm::vec3(point.x, ground, point.z);
	};
	creature_mind::HangAroundPlaces places;
	const auto* body = registry.TryGet<const Creature>(creature);
	places.height = body != nullptr ? k_HeightOfSizeOne * ShownSize(*body) : 0.0f;
	// TODO(creature-hang-around): the game keeps the place where a creature without a temple was made as its home.
	// openblack does not keep that place, so a creature without a home hangs around where it stands.
	if (const auto home = ecs::creature_home::HomeOf(registry, creature))
	{
		places.home = onGround(*home);
	}
	else if (const auto* at = registry.TryGet<const Transform>(creature))
	{
		places.home = onGround(at->position);
	}
	if (const auto* leash = registry.TryGet<const CreatureLeash>(creature))
	{
		places.confinementRadius = leash->confinementRadius;
	}
	if (body != nullptr)
	{
		registry.Each<const Temple, const Transform>([&](entt::entity, const Temple& temple, const Transform& transform) {
			if (temple.owner == body->owner && !places.temple.has_value())
			{
				places.temple = onGround(transform.position);
			}
		});
	}
	return places;
}

bool CreatureMindSystem::HasPlayed(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	return mind == nullptr || creature_script_play::Played(mind->idle);
}

std::optional<Desire> CreatureMindSystem::DesireForShownThing(entt::entity creature, CreatureMindState& mind,
                                                              entt::entity thing, std::span<const Desire> tried)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	const auto* at = registry.TryGet<const Transform>(creature);
	if (tables == nullptr || at == nullptr || !mind.desires.has_value() || !registry.AllOf<Creature>(creature))
	{
		return std::nullopt;
	}
	// The whole thing that the shown thing is part of, when there is one
	const auto whole = WholeOfShownThing(creature, mind, thing);
	const auto* wholeBelief = whole.has_value() ? mind.beliefs.Find(*whole) : nullptr;
	const auto kind = wholeBelief != nullptr ? creature_activity::WholeOf(wholeBelief->kind) : std::nullopt;
	std::vector<creature_planner::DesireOnThing> aboutWhole;
	std::vector<creature_planner::DesireOnThing> withoutWhole;
	for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
	{
		const auto desire = static_cast<Desire>(d);
		const auto& state = mind.desires->desires.at(d);
		if (!state.activated || state.suppressedTurns != 0 || std::ranges::find(tried, desire) != tried.end())
		{
			continue;
		}
		const auto& rule = tables->rules.at(d);
		if (rule.aboutSomething)
		{
			if (!kind.has_value() ||
			    !creature_activity::CanBeAbout(desire, WholeFactsOf(registry, creature, *whole, *kind, *tables),
			                                   mind.mayAttackOwnPlayer))
			{
				continue;
			}
			const auto nearness =
			    mind_detail::Nearness(registry, creature, at->position, *whole, wholeBelief->place, rule.distanceWeight);
			aboutWhole.push_back({.desire = desire,
			                      .strength = state.value,
			                      .usefulness = ActivityUsefulness(creature, mind, desire, *whole) * nearness});
		}
		else if (creature_planner::TestsThingActedOn(desire))
		{
			withoutWhole.push_back({.desire = desire, .strength = state.value});
		}
	}
	return creature_planner::DesireForShownThing(aboutWhole, withoutWhole);
}

std::optional<creature_desires::Desire> CreatureMindSystem::ForcePlanOn(entt::entity creature, entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind == nullptr || !mind->desires.has_value() || !mind->learnt.has_value() || !registry.Valid(object))
	{
		return std::nullopt;
	}
	// What it was doing is given up, so that the thing is planned for at once, whatever it was doing
	Abandon(*mind);
	creature_mind::Plan(mind->idle, creature_mind::Activity::None, {});
	ComeAcross(creature, *mind, object);
	// It finds the desire that it most wants for the shown thing, and plans that desire alone. With no plan, it leaves
	// that desire out and tries the next desire.
	std::vector<Desire> tried;
	for (size_t attempt = 0; attempt < creature_planner::k_ShownThingTries; ++attempt)
	{
		const auto desire = DesireForShownThing(creature, *mind, object, tried);
		if (!desire.has_value())
		{
			break;
		}
		mind->leash.actOn = {entt::to_integral(object)};
		mind->plannedTurn = mind->turn + 1;
		PlanCreature(creature, *mind, false, desire);
		mind->leash.actOn.clear();
		if (mind->planActive && mind->planner.current.has_value() && mind->planner.current->desire == *desire)
		{
			mind->planner.current->priority = std::numeric_limits<float>::max();
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} acts on the shown thing {} for desire {} after {} tries",
			                    entt::to_integral(creature), entt::to_integral(object), static_cast<int>(*desire), attempt + 1);
			return desire;
		}
		tried.push_back(*desire);
	}
	return std::nullopt;
}

std::optional<Desire> CreatureMindSystem::DesireForThingInHand(entt::entity creature, CreatureMindState& mind,
                                                               entt::entity thing)
{
	if (!mind.desires.has_value() || !mind.learnt.has_value())
	{
		return std::nullopt;
	}
	// The creature comes across the thing, as when the leash shows it the thing
	ComeAcross(creature, mind, thing);
	// It looks for a plan in a copy of its mind. Thus the plan that it does now does not change.
	auto trial = mind;
	std::vector<Desire> tried;
	for (size_t attempt = 0; attempt < creature_planner::k_ShownThingTries; ++attempt)
	{
		const auto desire = DesireForShownThing(creature, trial, thing, tried);
		if (!desire.has_value())
		{
			break;
		}
		trial.leash.actOn = {entt::to_integral(thing)};
		trial.plannedTurn = trial.turn + 1;
		PlanCreature(creature, trial, false, desire, false);
		trial.leash.actOn.clear();
		if (trial.planner.best.at(static_cast<size_t>(*desire)).has_value())
		{
			return desire;
		}
		tried.push_back(*desire);
	}
	return std::nullopt;
}

void CreatureMindSystem::LetGoOfHeld(entt::entity creature, CreatureMindState& mind, creature_mind::LetGo how)
{
	if (!Locator::creatureObjectActionSystem::has_value())
	{
		return;
	}
	auto& hands = Locator::creatureObjectActionSystem::value();
	const auto held = hands.GetHeld(creature);
	if (!held.has_value())
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	bool gently = how == creature_mind::LetGo::PutDown;
	std::optional<Desire> use;
	if (!gently)
	{
		// It does not look for a plan for a creature that it holds
		const bool heldIsCreature = registry.AllOf<Creature>(*held);
		if (!heldIsCreature)
		{
			use = DesireForThingInHand(creature, mind, *held);
		}
		gently = creature_mind::PutsDownGently(heldIsCreature, use);
	}
	[[maybe_unused]] const bool started = gently ? hands.PutDown(creature) : hands.Discard(creature);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} lets go of {} before a step that needs empty hands: {} ({}), {}",
	                    entt::to_integral(creature), entt::to_integral(*held), gently ? "puts it down" : "tosses it away",
	                    use.has_value() ? static_cast<int>(*use) : -1, started ? "started" : "can't");
}

void CreatureMindSystem::PlanTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> creatures;
	registry.Each<const Creature, CreatureMindState>(
	    [&creatures](entt::entity entity, const Creature&, CreatureMindState&) { creatures.push_back(entity); });
	for (const auto entity : creatures)
	{
		if (auto* mind = registry.TryGet<CreatureMindState>(entity))
		{
			PlanCreature(entity, *mind);
		}
	}
}

void CreatureMindSystem::LearnFromFeedback(entt::entity creature, CreatureMindState& mind, float feedback)
{
	namespace sources = creature_desires::sources;
	if (!mind.desires.has_value() || !mind.learnt.has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	auto& desires = *mind.desires;
	auto& learnt = *mind.learnt;
	const auto contextIndex = creature_learning::BestContext(learnt.contexts);
	auto* context = contextIndex.has_value() ? &learnt.contexts.at(*contextIndex) : nullptr;
	const auto* info = context != nullptr && tables != nullptr && context->action < tables->actions.size()
	                       ? &tables->actions[context->action]
	                       : nullptr;

	// Stroked for holding something while doing an action that allows it, it learns to eat from the hand
	if (feedback > 0.0f && mind.developmentPhase > 0 && info != nullptr && info->eatWhenStroked &&
	    Locator::creatureObjectActionSystem::has_value())
	{
		if (const auto held = Locator::creatureObjectActionSystem::value().GetHeld(creature);
		    held.has_value() && FoodValueOf(*held).has_value())
		{
			creature_learning::MakeFullyDominant(desires, Desire::Hunger, mind_detail::DesireFloorOf(creature));
			if (const auto belief = BeliefOf(registry, *held, creature); belief.has_value() && tables != nullptr)
			{
				creature_mind_model::Learn(learnt, TreeKind::Use, Desire::Hunger, {.belief = *belief, .feedback = 1.0f},
				                           tables->attributes.at(static_cast<size_t>(Desire::Hunger)));
			}
			creature_mind_model::Think(learnt, "I've learnt to eat what I'm given");
			// It must eat the thing alive. The feedback for this meal goes to eating that thing alive.
			static_cast<void>(ForcePlan(creature, {.desire = Desire::Hunger,
			                                       .action = k_EatHeldAction,
			                                       .object = *held,
			                                       .agenda = creature_mind::EatHeld(),
			                                       .activity = creature_mind::Activity::Eat}));
			// When the meal is done, it does again what it was stroked for. This replaces a thing that waits.
			mind.followUp = creature_follow_up::FollowUp::DoLessonAgain;
			return;
		}
	}
	// Stroked for running away from the player, it is forgiven and wants to run away less
	if (feedback > 0.0f && context != nullptr && context->desire == Desire::RunAwayFromPlayer)
	{
		auto& runAway = desires[Desire::RunAwayFromPlayer];
		creature_learning::MakeLeastDominant(desires, Desire::RunAwayFromPlayer, creature_learning::k_LeastDominantFactor,
		                                     mind_detail::DesireFloorOf(creature));
		for (auto& source : runAway.sources)
		{
			if (source.type == sources::k_RunAwayFromPlayer)
			{
				source.value = 0.0f;
				source.threshold = std::min(source.threshold + k_RunAwayThresholdStep, creature_learning::k_MaxThreshold);
			}
		}
		Abandon(mind);
		return;
	}

	// A slap makes it show how it feels when it completes its current work
	if (feedback < 0.0f)
	{
		mind.followUp = creature_follow_up::FollowUp::ShowSlapFeeling;
	}
	// With nothing it did lately to credit the feedback to, it learns nothing from it
	if (context == nullptr)
	{
		return;
	}
	const auto strength = creature_learning::Strengthened(feedback);
	if (feedback > 0.0f)
	{
		if (learnt.mimicry.has_value() && tables != nullptr)
		{
			creature_watching::StrokedWhileMimicking(learnt.mimicry, tables->mimics,
			                                         [this](uint32_t range) { return Random(range); });
			creature_desires::ChangeSource(desires, k_FollowPlayerSource, k_MimicStrokeBoost);
		}
		creature_desires::Suppress(desires, Desire::Sadness, k_StrokeSadnessSeconds, k_TurnsPerSecond);
	}
	// Slapped, it gives up the newest thing that it did and wants that desire least of all, young or not. This needs a
	// plan that it could carry out: a desire about a whole thing needs that whole thing.
	const auto giveUpAfterSlap = [&]() {
		if (feedback > 0.0f || learnt.contexts.empty() || tables == nullptr)
		{
			return;
		}
		const auto& newest = learnt.contexts.back();
		if (tables->rules.at(static_cast<size_t>(newest.desire)).aboutSomething && !newest.activityBelief.has_value())
		{
			return;
		}
		creature_learning::MakeLeastDominant(desires, newest.desire, creature_learning::k_LeastDominantFactor,
		                                     mind_detail::DesireFloorOf(creature));
		for (auto& state : desires.desires)
		{
			state.suppressedTurns = 0;
		}
		// This slap also clears the mark that the hand held it
		mind.heldSinceRestart = false;
		Abandon(mind);
	};

	if (info == nullptr || mind.developmentPhase < k_MinLessonPhase)
	{
		// Nothing to learn from yet: stroking makes it playful, showy and kind; slapping angry and fearful
		const auto amount = 0.5f * std::abs(feedback);
		constexpr std::array k_Stroked {sources::k_PlayFromWatchingPlayer, sources::k_ManifestState,
		                                sources::k_CompassionFromWatchingPlayer};
		constexpr std::array k_Slapped {sources::k_AngerFromDamage, sources::k_FearFromDamage};
		const auto pushed = feedback > 0.0f ? std::span<const uint32_t>(k_Stroked) : std::span<const uint32_t>(k_Slapped);
		for (const auto type : pushed)
		{
			creature_desires::ChangeSource(desires, type, amount);
		}
		TellHelp(creature, creature_help::TooYoungToLearn(feedback));
		mind.followUp = creature_follow_up::AfterYoungFeedback(mind.followUp, feedback, Random(2));
		giveUpAfterSlap();
		// A young creature that is stroked also does its action again, as an older one does
		if (feedback > 0.0f)
		{
			DoLessonAgain(creature, mind);
		}
		return;
	}

	const auto desire = context->desire;
	const auto d = static_cast<size_t>(desire);
	const auto& texts = tables->texts.at(d);
	context->credited = strength;
	// It is taught: the player has dealt with it
	mind.attention.turnsSinceInteraction = 0;
	if (tables->rules.at(d).learnable)
	{
		creature_learning::LearnDesireLesson(desires, desire, creature_learning::DominantSource(desires[desire]), strength,
		                                     tables->rules, tables->dependencies);
		creature_mind_model::Think(learnt, creature_learning::DesireLessonText(strength, texts.desire));
		// What the desire was about teaches what such things are worth as its point; what it acted on, when that was
		// something else and not itself, what such things are worth acting on (soiling only a little either way)
		const auto lessons = creature_learning::ThingLessonsFor(*context, entt::to_integral(creature));
		if (lessons.activity)
		{
			for (const auto& [other, share] : creature_learning::Spread(desire, strength, tables->dependencies))
			{
				creature_mind_model::Learn(learnt, TreeKind::ActOn, other,
				                           {.belief = *context->activityBelief, .feedback = std::clamp(share, -1.0f, 1.0f)},
				                           tables->attributes.at(static_cast<size_t>(other)));
			}
		}
		if (lessons.actedOn)
		{
			const auto acted = desire == Desire::Poo ? std::clamp(strength, -k_PooUseFeedback, k_PooUseFeedback) : strength;
			for (const auto& [other, share] : creature_learning::Spread(desire, acted, tables->dependencies))
			{
				creature_mind_model::Learn(learnt, TreeKind::Use, other,
				                           {.belief = *context->belief, .feedback = std::clamp(share, -1.0f, 1.0f)},
				                           tables->attributes.at(static_cast<size_t>(other)));
			}
		}
		if (context->belief.has_value())
		{
			creature_mind_model::Think(
			    learnt, creature_learning::ObjectLessonText(strength, ObjectName(context->belief), texts.trying));
		}
		// The lesson changed its opinion of doing it to such things when what it knows of them has split up
		const auto belowRoot = [&learnt, desire](TreeKind kind, const creature_tree::Belief& belief) {
			const auto& tree = learnt.trees.at(static_cast<size_t>(kind)).at(static_cast<size_t>(desire));
			const auto leaf = creature_tree::LeafOf(tree, belief);
			return leaf.has_value() && *leaf != 0;
		};
		const bool opinionChanged = (lessons.activity && belowRoot(TreeKind::ActOn, *context->activityBelief)) ||
		                            (lessons.actedOn && belowRoot(TreeKind::Use, *context->belief));
		auto& opinion = learnt.opinions.at(context->action);
		opinion = creature_learning::OpinionAfter(opinion, strength);
		creature_mind_model::Think(
		    learnt, creature_learning::ActionLessonText(strength, info->name, texts.trying, ObjectName(context->belief)));
		TellHelp(creature, creature_help::FeedbackLesson(strength, opinionChanged, desire, context->source));
	}

	if (feedback <= 0.0f)
	{
		giveUpAfterSlap();
		return;
	}
	// Stroked for something it did to a thing, it must do it again to something like it
	DoLessonAgain(creature, mind);
}

void CreatureMindSystem::LearnTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	if (tables == nullptr)
	{
		return;
	}
	std::vector<entt::entity> creatures;
	registry.Each<const Creature, CreatureMindState>(
	    [&creatures](entt::entity entity, const Creature&, CreatureMindState&) { creatures.push_back(entity); });
	for (const auto creature : creatures)
	{
		auto* mind = registry.TryGet<CreatureMindState>(creature);
		if (mind == nullptr || !mind->learnt.has_value() || !mind->desires.has_value())
		{
			continue;
		}
		auto& learnt = *mind->learnt;
		// What the player showed it on a leash: anger or compassion towards such things
		for (const auto& shown : mind->leash.shown)
		{
			const auto belief = BeliefOf(registry, static_cast<entt::entity>(shown.object), creature);
			if (!belief.has_value())
			{
				continue;
			}
			for (const auto& lesson : shown.lessons)
			{
				creature_mind_model::Learn(learnt, TreeKind::ActOn, lesson.desire,
				                           {.belief = *belief, .feedback = lesson.change},
				                           tables->attributes.at(static_cast<size_t>(lesson.desire)));
				creature_mind_model::Think(
				    learnt, creature_learning::ObjectLessonText(lesson.change, ObjectName(belief),
				                                                tables->texts.at(static_cast<size_t>(lesson.desire)).trying));
			}
		}
		mind->leash.shown.clear();
		// Leashed to another creature, it comes to find it nicer or nastier, and learns about creatures like it
		for (const auto& change : mind->leash.attitudes)
		{
			auto& attitude = creature_learning::AttitudeTo(learnt.creatures, change.creature);
			const auto lessons = creature_learning::ChangeHowNice(attitude, change.change);
			if (const auto belief = BeliefOf(registry, static_cast<entt::entity>(change.creature), creature))
			{
				for (const auto& [desire, amount] : lessons)
				{
					creature_mind_model::Learn(learnt, TreeKind::ActOn, desire, {.belief = *belief, .feedback = amount},
					                           tables->attributes.at(static_cast<size_t>(desire)));
				}
			}
		}
		mind->leash.attitudes.clear();
	}
}

namespace
{
/// The kind of a thing for copying the player
creature_watching::MimicKind MimicKindOf(const ecs::Registry& registry, entt::entity thing)
{
	using Class = creature_watching::MimicFacts::Class;
	creature_watching::MimicFacts facts;
	if (const auto* villager = registry.TryGet<const Villager>(thing))
	{
		facts.kind = Class::Villager;
		facts.child = villager->lifeStage == Villager::LifeStage::Child;
		facts.female = villager->sex == Villager::Sex::FEMALE;
	}
	else if (registry.AllOf<Animal>(thing))
	{
		const auto* info = ecs::world_objects::InfoOf(thing);
		facts.kind = Class::Animal;
		facts.nasty = info != nullptr && info->alignmentType == AlignmentType::AnimalNasty;
	}
	else if (registry.AllOf<Creature>(thing))
	{
		facts.kind = Class::Creature;
	}
	else if (registry.AnyOf<WorshipSite, VillageTotem, FishFarm, Temple, Town>(thing))
	{
		facts.kind = Class::Other;
	}
	else if (registry.AnyOf<Abode, Field>(thing))
	{
		facts.kind = Class::Building;
	}
	else if (registry.AllOf<Tree>(thing))
	{
		facts.kind = Class::Tree;
	}
	else if (registry.AnyOf<Rock, DeadTree>(thing))
	{
		facts.kind = Class::Rock;
	}
	else if (registry.AnyOf<MobileStatic, MobileObject, Ball, Pot>(thing))
	{
		facts.kind = Class::LooseObject;
	}
	return creature_watching::KindToCopy(facts);
}
} // namespace

void CreatureMindSystem::CopyPlayersDeed(entt::entity creature, CreatureMindState& mind)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	if (tables == nullptr || !mind.learnt.has_value() || !mind.learnt->mimicry.has_value() ||
	    !registry.AllOf<Transform>(creature))
	{
		return;
	}
	auto& learnt = *mind.learnt;
	const auto& rule = tables->mimics.at(learnt.mimicry->rule);
	// Every copy is for the desire the player seemed to have
	if (rule.desire >= creature_desires::k_DesireCount)
	{
		return;
	}
	const auto desire = static_cast<Desire>(rule.desire);
	std::optional<entt::entity> players;
	if (learnt.mimicry->object.has_value())
	{
		const auto thing = static_cast<entt::entity>(*learnt.mimicry->object);
		if (registry.Valid(thing))
		{
			ComeAcross(creature, mind, thing);
			if (mind.beliefs.Find(thing) != nullptr)
			{
				// The player still holds the thing in the hand: it stops copying
				if (registry.AllOf<InHand>(thing))
				{
					learnt.mimicry.reset();
					return;
				}
				players = thing;
			}
		}
	}

	// It does what the player did, in three passes: to the player's thing, to the nearest thing of the same kind, and to
	// the thing it finds most useful to act on. It may do it each time it can; after each time, it stops by random(2).
	enum class Pass : uint8_t
	{
		PlayersThing,
		SameKind,
		MostUseful,
	};
	const auto* at = registry.TryGet<const Transform>(creature);
	const auto distanceWeight = tables->rules.at(static_cast<size_t>(desire)).distanceWeight;
	const auto choose = [&](Pass pass, Target target) -> std::optional<entt::entity> {
		const auto suits = [&](entt::entity thing) { return Accepts(registry, target, thing, creature); };
		switch (pass)
		{
		case Pass::PlayersThing:
			return players.has_value() && suits(*players) ? players : std::nullopt;
		case Pass::SameKind:
		{
			if (!mind.copiedKind.has_value())
			{
				return std::nullopt;
			}
			std::vector<entt::entity> things;
			std::vector<creature_watching::MimicCandidate> candidates;
			for (const auto& belief : mind.beliefs.InListOrder())
			{
				if (belief.thing == creature || !SuitsDesire(creature, mind, desire, belief.thing) || !suits(belief.thing))
				{
					continue;
				}
				things.push_back(belief.thing);
				candidates.push_back({
				    .kind = MimicKindOf(registry, belief.thing),
				    .nearness =
				        mind_detail::Nearness(registry, creature, at->position, belief.thing, belief.place, distanceWeight),
				});
			}
			const auto nearest = creature_watching::NearestOfKind(candidates, *mind.copiedKind);
			return nearest.has_value() ? std::optional(things.at(*nearest)) : std::nullopt;
		}
		case Pass::MostUseful:
		{
			std::optional<creature_planner::ObjectCandidate> best;
			for (const auto& thing : BelievedThingsToActOn(creature, mind, desire, suits))
			{
				if (thing.usefulness > (best.has_value() ? best->usefulness : 0.0f))
				{
					best = thing;
				}
			}
			return best.has_value() ? std::optional(static_cast<entt::entity>(best->id)) : std::nullopt;
		}
		}
		return std::nullopt;
	};
	const auto copyActions =
	    creature_watching::CopyActionsOf(rule, learnt.mimicry->rule, learnt.mimicry->magic, tables->miracles);
	for (const auto pass : {Pass::PlayersThing, Pass::SameKind, Pass::MostUseful})
	{
		for (const auto action : copyActions)
		{
			const auto* executor = creature_plan_actions::For(tables->actions.at(action).name);
			if (executor == nullptr || !creature_watching::KnowsWhatItNeeds(learnt.knowledge, tables->actions.at(action).needs))
			{
				continue;
			}
			// An action that needs no thing acts on the creature itself
			std::optional<entt::entity> object;
			if (executor->target != Target::None)
			{
				object = choose(pass, executor->target);
				if (!object.has_value())
				{
					continue;
				}
			}
			const auto whole = WholeOfForcedPlan(creature, mind, desire, object.value_or(creature));
			creature_plan_actions::Situation situation;
			if (!Adopt(creature, mind,
			           {.desire = desire,
			            .action = action,
			            .object = object.has_value() ? std::optional(entt::to_integral(*object)) : std::nullopt,
			            .about = whole.has_value() ? std::optional(entt::to_integral(*whole)) : std::nullopt,
			            // Copying counts as pressing as a desire at its strongest, with nothing learnt
			            .priority = creature_planner::Priority(1.0f, std::nullopt,
			                                                   creature_planner::ActionPriority(learnt.opinions.at(action)),
			                                                   creature_planner::k_DefaultUsefulness, std::nullopt)},
			           situation))
			{
				continue;
			}
			creature_mind_model::Think(learnt, fmt::format("I'm copying you: {}", tables->actions.at(action).name));
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} copies the player in pass {}: {} on {}",
			                    entt::to_integral(creature), static_cast<int>(pass), tables->actions.at(action).name,
			                    object.has_value() ? static_cast<int64_t>(entt::to_integral(*object)) : -1);
			if (Random(2) == 0)
			{
				return;
			}
		}
	}
}

bool CreatureMindSystem::ContinueKeptPlanWith(entt::entity creature, CreatureMindState& mind, entt::entity gift)
{
	const auto* tables = GetTables();
	const auto& kept = mind.keptPlan;
	if (tables == nullptr || !kept.has_value() || kept->action >= tables->actions.size())
	{
		return false;
	}
	const auto& action = tables->actions.at(kept->action);
	const auto* executor = creature_plan_actions::For(action.name);
	// Only the actions that a creature can do here can continue
	if (executor == nullptr)
	{
		return false;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* held = registry.TryGet<const HeldByCreature>(gift);
	const bool holdsGift = held != nullptr && held->creature == creature;
	const auto whole = kept->about.has_value() ? kept->about : kept->activityObject;
	const bool needsThingToUse = creature_plan_actions::NeedsThingToUse(action.name);
	const auto use = executor->use != Target::None ? executor->use : Target::Pickable;
	const creature_giving::KeptPlanFacts facts {
	    .complete = kept->action != 0 && kept->object.has_value(),
	    .missesWhole = tables->rules.at(static_cast<size_t>(kept->desire)).aboutSomething && !whole.has_value(),
	    .missesThingToUse = needsThingToUse && !kept->instrument.has_value(),
	    .actsOnThing = action.actsOnThing,
	    .needsThingToUse = needsThingToUse,
	    .giftSuitsActOn = !(action.notOnWhatItHolds && holdsGift) &&
	                      (executor->target == Target::None || Accepts(registry, executor->target, gift, creature)),
	    .giftSuitsUse = !NoWayLately(registry, mind, gift) &&
	                    !BelievedPlaceOf(registry, mind, gift)
	                         .transform([&](glm::vec3 place) { return OutsideConfinedArea(registry, creature, place); })
	                         .value_or(false) &&
	                    LetsCreatureAct(registry, gift) && Accepts(registry, use, gift, creature),
	};
	const auto chosen = creature_giving::UseForKeptPlan(facts);
	if (chosen == creature_giving::KeptPlanUse::None)
	{
		return false;
	}
	const auto toEntity = [](std::optional<uint32_t> number) {
		return number.has_value() ? std::optional(static_cast<entt::entity>(*number)) : std::nullopt;
	};
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} continues {} with the thing it was given ({}) as the thing {}",
	                    entt::to_integral(creature), action.name, entt::to_integral(gift),
	                    chosen == creature_giving::KeptPlanUse::ActOn ? "to act on" : "to use");
	const bool actsOnGift = chosen == creature_giving::KeptPlanUse::ActOn;
	return ForceActivity(creature, {
	                                   .desire = kept->desire,
	                                   .activityObject = toEntity(whole),
	                                   .action = action.name,
	                                   .actionObject = actsOnGift ? gift : static_cast<entt::entity>(*kept->object),
	                                   .instrument = actsOnGift ? toEntity(kept->instrument) : std::optional(gift),
	                               });
}

void CreatureMindSystem::ProcessMimicry(entt::entity creature, CreatureMindState& mind)
{
	const auto* tables = GetTables();
	if (tables == nullptr || !mind.learnt.has_value() || !mind.learnt->mimicry.has_value() || !mind.desires.has_value())
	{
		return;
	}
	auto& mimicry = mind.learnt->mimicry;
	if (!creature_watching::StageContinues(*mimicry, mind.turn, k_TurnsPerSecond))
	{
		creature_watching::NextMimicStage(mimicry, tables->mimics, mind.turn, [this](uint32_t range) { return Random(range); });
		if (mimicry.has_value())
		{
			const auto& next = tables->mimics.at(mimicry->rule);
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} copies the player's {}: stage {}",
			                    entt::to_integral(creature), next.name, creature_watching::Name(mimicry->stage));
			if (mimicry->stage == creature_watching::MimicStage::CopyDesire && next.desire < tables->texts.size())
			{
				creature_mind_model::Think(mind.learnt.value(),
				                           fmt::format("I want what you wanted: {}", tables->texts.at(next.desire).desire));
			}
		}
		return;
	}
	const auto& rule = tables->mimics.at(mimicry->rule);
	auto& registry = Locator::entitiesRegistry::value();
	switch (mimicry->stage)
	{
	case creature_watching::MimicStage::Notice:
	{
		// The creature shows the player that it saw the deed, if the deed had a thing
		const auto object = mimicry->object.has_value() ? static_cast<entt::entity>(*mimicry->object) : entt::null;
		if (registry.Valid(object) && rule.noticeAction < tables->actions.size())
		{
			ComeAcross(creature, mind, object);
			ForceActivity(
			    creature,
			    {.desire = Desire::Curiosity, .action = tables->actions.at(rule.noticeAction).name, .actionObject = object});
		}
		break;
	}
	case creature_watching::MimicStage::CopyAction:
		CopyPlayersDeed(creature, mind);
		break;
	case creature_watching::MimicStage::CopyDesire:
	{
		if (rule.desire >= creature_desires::k_DesireCount)
		{
			break;
		}
		// It comes to want what the player seemed to want. Its hunger and tiredness stay as they were.
		auto& desires = *mind.desires;
		const auto hunger = desires[Desire::Hunger].value;
		const auto tiredness = desires[Desire::Tiredness].value;
		auto* spells = registry.TryGet<CreatureSpells>(creature);
		if (spells == nullptr)
		{
			spells = &registry.Assign<CreatureSpells>(creature);
		}
		spells->cheat = creature_spell_mind::SetCheatDominant(
		    desires, static_cast<Desire>(rule.desire),
		    {.all = false, .seconds = k_MimicDesireSeconds, .floor = mind_detail::DesireFloorOf(creature)}, k_TurnsPerSecond);
		desires[Desire::FollowPlayerDesire].suppressedTurns = 0;
		// A creature that stood with nothing to do for too long stops copying, and does not follow the player for a time
		if (mind.thinksWithoutPlan > k_MostThinksWithoutPlanToCopyDesire)
		{
			mimicry.reset();
			creature_desires::Suppress(desires, Desire::FollowPlayerDesire, k_NoFollowAfterCopySeconds, k_TurnsPerSecond);
		}
		desires[Desire::Hunger].value = std::clamp(hunger, 0.0f, 1.0f);
		desires[Desire::Tiredness].value = std::clamp(tiredness, 0.0f, 1.0f);
		break;
	}
	}
}

void CreatureMindSystem::LoadMind(entt::entity creature, std::shared_ptr<const creaturemind::MindFileData> mind)
{
	if (auto* state = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature))
	{
		state->pendingFile = std::move(mind);
	}
}

std::optional<creaturemind::MindFileData> CreatureMindSystem::SaveMind(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (mind == nullptr || body == nullptr || !mind->desires.has_value() || !mind->learnt.has_value())
	{
		return std::nullopt;
	}
	auto file = creature_mind_model::ToFile(*mind->desires, *mind->learnt, mind->developmentPhase, mind->attitudeToPlayer,
	                                        mind->turnsLookedAt, static_cast<uint32_t>(creature::InfoRow(body->species)));
	// It is saved as itself: as it was before any spell on it changed it
	const creature_spells::SavedBody now {.size = body->size, .strength = body->strength, .alignment = body->alignment};
	const auto* spells = registry.TryGet<const CreatureSpells>(creature);
	auto saved = spells != nullptr ? creature_spells::ValuesToSave(spells->spells, now) : now;
	// While the land's curse on the player's creature runs, it is saved as the curse found it
	if (body->owner == PlayerNames::PLAYER_ONE && Locator::vm::has_value())
	{
		const auto& vm = Locator::vm::value();
		const bool cursed =
		    std::ranges::any_of(vm.GetTasks(), [](const auto& entry) { return entry.second.name == k_CurseScript; });
		if (cursed)
		{
			const auto global = [&vm](std::string_view name) {
				const auto& variables = vm.GetVariables();
				const auto found = std::ranges::find(variables, name, &lhvm::VMVar::name);
				return found != variables.end() ? found->value.floatVal : 0.0f;
			};
			saved.size = global("OriginalSizeOfMyCreature") / k_HeightOfSizeOne;
			saved.strength = global("OriginalStrengthOfMyCreature");
			saved.alignment = global("OriginalAlignmentOfMyCreature");
		}
	}
	// Its body is saved with its mind as it is now, but for what the spells changed
	auto kept = creature_body_file::Capture(registry, creature);
	kept.size = saved.size;
	kept.strength = saved.strength;
	kept.alignment = saved.alignment;
	creature_mind_body::ToMindFile(kept, file);
	// Its name is saved with its mind
	file.name = body->name;
	return file;
}

void CreatureMindSystem::ClearLearning(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (mind == nullptr || body == nullptr)
	{
		return;
	}
	const auto name = mind->learnt.has_value() ? mind->learnt->name : std::u16string {};
	Abandon(*mind);
	mind->desires = creature_desires::Create(SetupFor(body->species), [this](float low, float high) {
		return high > low ? std::uniform_real_distribution<float>(low, high)(_random) : low;
	});
	mind->desiresPhase.reset();
	mind->planner = {};
	mind->pendingFile.reset();
	mind->resourceChecked = true;
	SetUpLearning(creature, *mind);
	mind->learnt->name = name;
}

void CreatureMindSystem::SeeSkill(const glm::vec3& point, size_t skill)
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> watching;
	registry.Each<const Creature, const CreatureMindState, const Transform>(
	    [&](entt::entity entity, const Creature&, const CreatureMindState& mind, const Transform& at) {
		    if (mind.learnt.has_value() && glm::distance(at.position, point) <= k_ViewDistance)
		    {
			    watching.push_back(entity);
		    }
	    });
	for (const auto entity : watching)
	{
		WatchSkill(entity, skill);
	}
}

namespace
{
/// The game's turn now
uint32_t GameTurn()
{
	return Locator::time::has_value() ? static_cast<uint32_t>(Locator::time::value().GetTurn()) : 0;
}

/// Where a creature's head is as posed this frame: its head bone, else its height over its feet
glm::vec3 HeadPoint(const ecs::Registry& registry, entt::entity creature)
{
	const auto& transform = registry.Get<const Transform>(creature);
	const auto& body = registry.Get<const Creature>(creature);
	const auto fallback = transform.position + glm::vec3(0.0f, ShownSize(body) * creature_mode::k_HeightPerSize, 0.0f);
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(body.species);
	if (animation == nullptr || animation->boneMatrices.empty() || !rigs.Contains(rigId))
	{
		return fallback;
	}
	const auto& points = rigs.Handle(rigId)->actionPoints;
	if (!points.has_value() || points->head >= animation->boneMatrices.size())
	{
		return fallback;
	}
	const auto placement = creature::PlacementMatrix(transform.position, transform.rotation, transform.scale);
	return glm::vec3(creature::PosedBone(points->head, animation->boneMatrices, placement)[3]);
}
} // namespace

void CreatureMindSystem::WatchSkill(entt::entity creature, size_t skill)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	// A mind stilled, as by the freeze spell, learns nothing
	if (tables == nullptr || mind == nullptr || !mind->learnt.has_value() || mind->paused)
	{
		return;
	}
	// The clock of a skill runs on the game's turns
	const auto turn = GameTurn();
	const auto progress = creature_watching::SeeSkill(mind->learnt->knowledge, skill, tables->skills, mind->developmentPhase,
	                                                  turn, k_TurnsPerSecond);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} sees skill {} (turn {}): event {}, meter {}",
	                    entt::to_integral(creature), skill, turn,
	                    progress.event.has_value() ? static_cast<int>(*progress.event) : -1, progress.meter.value_or(-1.0f));
	if (progress.event == creature_watching::LearningEvent::Learnt)
	{
		creature_mind_model::Think(*mind->learnt, fmt::format("I've learnt to {}", tables->skills.at(skill).name));
	}
	ShowLearningMeter(creature, *mind, progress.meter);
	// The advisor tells of it: too young to learn it, learnt it now, or still learning it
	if (progress.event.has_value())
	{
		using creature_help::Kind;
		using creature_watching::LearningEvent;
		const auto kind = *progress.event == LearningEvent::TooYoung       ? Kind::CantLearnNormalActionYet
		                  : *progress.event == LearningEvent::NearlyLearnt ? Kind::NearlyLearntNormalAction
		                                                                   : Kind::LessonNormalAction;
		TellHelp(creature, {.kind = kind, .parameter = static_cast<uint32_t>(skill)});
	}
}

void CreatureMindSystem::WatchVillager(entt::entity creature, CreatureMindState& mind, entt::entity villager)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* person = registry.TryGet<const Villager>(villager);
	if (person == nullptr)
	{
		return;
	}
	const creature_watching::Watcher watcher {
	    .phase = mind.developmentPhase,
	    .scriptControlled = registry.AllOf<ScriptControlled>(creature),
	    .planDesire = mind.planner.current.has_value() ? std::optional(mind.planner.current->desire) : std::nullopt,
	};
	if (!creature_watching::WatchesVillagers(watcher))
	{
		return;
	}
	WatchVillagerSkill(creature, mind, villager);
	// A villager of another player's village, or of a village of no player, makes it want to impress more
	const auto* town = registry.Valid(person->town) ? registry.TryGet<const Town>(person->town) : nullptr;
	const auto* body = registry.TryGet<const Creature>(creature);
	if (town != nullptr && (town->owner == PlayerNames::NEUTRAL || body == nullptr || town->owner != body->owner))
	{
		mind.impressFromSeeing = creature_desire_sources::ImpressAfterSighting(mind.impressFromSeeing);
	}
}

void CreatureMindSystem::WatchVillagerSkill(entt::entity creature, CreatureMindState& mind, entt::entity villager)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.TryGet<const LivingAction>(villager);
	if (!mind.learnt.has_value() || !Locator::infoConstants::has_value() || action == nullptr)
	{
		return;
	}
	// How far a walking villager still has to go
	float toGoal = 0.0f;
	if (const auto* walk = registry.TryGet<const WallHug>(villager); walk != nullptr)
	{
		toGoal = glm::distance(glm::xz(registry.Get<const Transform>(villager).position), walk->goal);
	}
	const auto state = creature_watching::StateToLearnFrom({
	    .state = action->states.at(static_cast<size_t>(LivingAction::Index::Top)),
	    .finalState = action->states.at(static_cast<size_t>(LivingAction::Index::Final)),
	    .distanceToGoal = toGoal,
	});
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	if (state >= table.size())
	{
		return;
	}
	const auto skill = table.at(state).teachesSkill;
	const auto& known = mind.learnt->knowledge.skillsKnown;
	if (skill >= known.size() || known.at(skill))
	{
		return;
	}
	// It takes in what the villager does, and goes after it to see more
	WatchSkill(creature, skill);
	ForceActivity(creature,
	              {.desire = creature_desires::Desire::Curiosity, .action = k_FollowToWatchAction, .actionObject = villager});
}

void CreatureMindSystem::ShowLearningMeter(entt::entity creature, CreatureMindState& mind, std::optional<float> share)
{
	const auto turn = GameTurn();
	if (!share.has_value() || !creature_watching::MeterShows(mind.lastMeterTurn, turn))
	{
		return;
	}
	mind.lastMeterTurn = turn;
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (body == nullptr || transform == nullptr || !Locator::resources::has_value())
	{
		return;
	}
	const auto marker = registry.Create();
	registry.Assign<LearningMarker>(
	    marker,
	    ecs::learning_marker::Start(HeadPoint(registry, creature), ShownSize(*body) * creature_mode::k_HeightPerSize, *share));
}

void CreatureMindSystem::SeeMiracle(const glm::vec3& point, size_t miracle)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const Creature, CreatureMindState, const Transform>(
	    [&](entt::entity entity, const Creature& creature, CreatureMindState& mind, const Transform& at) {
		    if (glm::distance(at.position, point) <= k_ViewDistance)
		    {
			    CountMiracleSighting(entity, creature, mind, miracle);
		    }
	    });
}

void CreatureMindSystem::CountMiracleSighting(entt::entity entity, const Creature& creature, CreatureMindState& mind,
                                              size_t miracle)
{
	const auto* tables = GetTables();
	// A mind stilled, as by the freeze spell, learns nothing
	if (tables == nullptr || !mind.learnt.has_value() || mind.paused)
	{
		return;
	}
	const auto progress = creature_watching::SeeMiracle(
	    mind.learnt->knowledge, miracle, tables->miracles, mind.developmentPhase, mind.turn, mind.leash.miracleSightingWeight,
	    creature_mind_tables::MiracleMultiplier(creature::InfoRow(creature.species)));
	if (progress.learnt && !progress.ignored)
	{
		creature_mind_model::Think(*mind.learnt, fmt::format("I've learnt the miracle {}", tables->miracles.at(miracle).name));
	}
	ShowLearningMeter(entity, mind, progress.meter);
	// The advisor tells of what the sighting showed
	if (progress.event.has_value())
	{
		using creature_help::Kind;
		using creature_watching::LearningEvent;
		const auto kind = *progress.event == LearningEvent::TooYoung       ? Kind::CantLearnMagicActionYet
		                  : *progress.event == LearningEvent::NearlyLearnt ? Kind::NearlyLearntMagicAction
		                                                                   : Kind::LessonMagicAction;
		TellHelp(entity, {.kind = kind, .parameter = static_cast<uint32_t>(miracle)});
	}
}

void CreatureMindSystem::PlayerDid(size_t deed, const glm::vec3& point, std::optional<entt::entity> object,
                                   std::optional<PlayerNames> player)
{
	NoticeDeed(deed, point, object, player, 0);
}

void CreatureMindSystem::PlayerCastDeed(size_t deed, const glm::vec3& point, std::optional<entt::entity> object,
                                        std::optional<PlayerNames> player, MagicType magic)
{
	NoticeDeed(deed, point, object, player, static_cast<uint32_t>(magic));
}

void CreatureMindSystem::NoticeDeed(size_t deed, const glm::vec3& point, std::optional<entt::entity> object,
                                    std::optional<PlayerNames> player, uint32_t magic)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	if (tables == nullptr || deed >= tables->mimics.size())
	{
		return;
	}
	std::vector<entt::entity> mayCopy;
	registry.Each<const Creature, CreatureMindState, const Transform>([&](entt::entity entity, const Creature& creature,
	                                                                      CreatureMindState& mind, const Transform& at) {
		if (!mind.learnt.has_value() || (player.has_value() && (creature.owner != *player || !creature.leashable)))
		{
			return;
		}
		// It must see where the thing is, or the point when there is no thing, and be near enough to it
		const glm::vec2 here(at.position.x, at.position.z);
		const auto seen =
		    object.has_value() ? PointOf(registry, *object).value_or(glm::vec2(point.x, point.z)) : glm::vec2(point.x, point.z);
		const auto* animation = registry.TryGet<const CreatureAnimation>(entity);
		const auto from = map_coords::FromMetres(here);
		const auto to = map_coords::FromMetres(seen);
		const auto looking = LookingAngle(at, animation != nullptr ? animation->lookAt : std::nullopt);
		const bool inView = creature_object_beliefs::CanSee(looking, gutils::GetAngleFromXZ(from, to),
		                                                    map_coords::Cell(from) == map_coords::Cell(to));
		const bool nearEnough = creature_watching::WithinCopyReach(gutils::GetDistanceInMetres(to, from),
		                                                           creature_object_beliefs::SweepCells(ShownSize(creature)));
		const creature_watching::MimicConditions conditions {
		    .phase = mind.developmentPhase,
		    .learningLeashInHand = mind.leash.learningInHand,
		    .reactionPriority = ReactionPriorityOf(registry, entity),
		    .canSee = inView && nearEnough,
		};
		if (creature_watching::MayStartMimicry(mind.learnt->mimicry, deed, tables->mimics, conditions))
		{
			mayCopy.push_back(entity);
		}
	});
	const auto target = object.has_value() ? std::optional(entt::to_integral(*object)) : std::nullopt;
	const auto random = [this](uint32_t range) { return Random(range); };
	for (const auto entity : mayCopy)
	{
		auto* mind = registry.TryGet<CreatureMindState>(entity);
		const auto* creature = registry.TryGet<const Creature>(entity);
		if (mind == nullptr || creature == nullptr || !mind->learnt.has_value())
		{
			continue;
		}
		// A deed done with a miracle that the creature has not seen often enough counts as a sighting of the miracle. With
		// more than one sighting still needed it stops there; with one, it goes on to copy one time in two.
		if (magic != 0 && magic < tables->miracles.size() && magic < mind->learnt->knowledge.miraclesSeen.size())
		{
			const auto step = creature_watching::CastDeedGate(
			    mind->learnt->knowledge.miraclesSeen.at(magic).count, tables->miracles.at(magic).timesToSee,
			    creature_mind_tables::MiracleMultiplier(creature::InfoRow(creature->species)));
			if (step != creature_watching::CastDeedStep::Copy)
			{
				CountMiracleSighting(entity, *creature, *mind, magic);
				if (step == creature_watching::CastDeedStep::CountAndStop || Random(2) == 0)
				{
					continue;
				}
			}
		}
		if (object.has_value() && registry.Valid(*object))
		{
			LearnFromDeedSeen(entity, *mind, deed, *object, magic);
		}
		creature_watching::BeginMimicry(mind->learnt->mimicry, deed, tables->mimics, target, mind->turn, random);
		mind->learnt->mimicry->magic = magic;
		mind->observedDeed = creature_watching::ObservedDeed {.rule = deed, .object = target, .magic = magic};
		// It forgets the trees that it planted others by
		mind->plantedBy.clear();
		// When the player is angry with a thing of the player, the creature that copies the player can be angry with the
		// things of its player from now on
		if (object.has_value() && tables->mimics.at(deed).desire == static_cast<uint32_t>(Desire::Anger) &&
		    PlayerOfThing(registry, *object, BelongsTo(*object)) == creature->owner)
		{
			mind->mayAttackOwnPlayer = true;
		}
		if (const auto copied = object.has_value() ? PointOf(registry, *object) : std::nullopt)
		{
			mind->copiedAt = *copied;
		}
		if (object.has_value() && registry.Valid(*object))
		{
			mind->copiedKind = MimicKindOf(registry, *object);
		}
		// Seeing the deed stirs the first source of the desire behind it, and its wish to follow the player
		const auto desire = tables->mimics.at(deed).desire;
		if (mind->desires.has_value() && desire < creature_desires::k_DesireCount)
		{
			const auto& sources = (*mind->desires)[static_cast<Desire>(desire)].sources;
			if (!sources.empty())
			{
				creature_desires::ChangeSource(*mind->desires, sources.front().type, k_NoticedDeedSource);
			}
			creature_desires::ChangeSource(*mind->desires, k_FollowPlayerSource, k_NoticedDeedFollow);
		}
		// It knows of the miracle of the deed from now on, and the advisor tells of it when this is new
		auto& known = mind->learnt->knowledge.miraclesKnown;
		if (magic != 0 && magic < known.size() && !known.at(magic))
		{
			known.at(magic) = true;
			TellHelp(entity, {.kind = creature_help::Kind::LessonMagicAction, .parameter = magic});
		}
		// What it was doing stops as a failure. With no plan, it then notices the deed (see ProcessMimicry).
		{
			creature_mind_model::Think(*mind->learnt, fmt::format("I saw you: {}", tables->mimics.at(deed).name));
			if (Locator::creatureObjectActionSystem::has_value() &&
			    Locator::creatureObjectActionSystem::value().GetState(entity) ==
			        CreatureObjectActionSystemInterface::State::Busy)
			{
				Locator::creatureObjectActionSystem::value().Cancel(entity);
			}
			Abandon(*mind);
			if (Locator::magicSystem::has_value())
			{
				Locator::magicSystem::value().ReleaseCreatureCast(entity);
			}
			creature_mind::Plan(mind->idle, creature_mind::Activity::None, {});
		}
	}
}

void CreatureMindSystem::DoLessonAgain(entt::entity creature, CreatureMindState& mind)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (tables == nullptr || transform == nullptr || !mind.learnt.has_value() || !mind.desires.has_value())
	{
		return;
	}
	const auto& learnt = *mind.learnt;
	const auto index = creature_learning::BestContext(learnt.contexts);
	if (!index.has_value())
	{
		return;
	}
	const auto context = learnt.contexts.at(*index);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} looks to do its lesson again: action {} on {}",
	                    entt::to_integral(creature), context.action, context.object.value_or(0));
	// It needs a thing that it acted on that is not itself. The thing can be gone, as a thing that it ate is: the kind of
	// thing stays known. It does nothing while it copies the player.
	if (!creature_follow_up::MayDoAgain(context.object, entt::to_integral(creature), learnt.mimicry.has_value()) ||
	    context.action >= tables->actions.size())
	{
		return;
	}
	const auto actedOn = static_cast<entt::entity>(*context.object);
	const auto action = creature_follow_up::ActionToDoAgain(tables->actions.at(context.action).name);
	const auto actionIndex = creature_mind_tables::FindAction(*tables, action);
	const auto* executor = creature_plan_actions::For(action);
	if (!actionIndex.has_value() || executor == nullptr)
	{
		return;
	}
	const glm::vec2 position {transform->position.x, transform->position.z};
	creature_plan_actions::Situation situation;
	if (Locator::camera::has_value())
	{
		const auto eye = Locator::camera::value().GetOrigin();
		situation.camera = glm::vec2(eye.x, eye.z);
	}
	Situate(creature, situation);
	const auto desire = context.desire;
	// It finds the most useful thing of the same kind as the thing it acted on. An action that acts on nothing acts on
	// the creature itself.
	std::vector<creature_planner::ObjectCandidate> sameKind;
	if (executor->target != Target::None)
	{
		const auto* known = mind.beliefs.Find(actedOn);
		if (known == nullptr && !context.belief.has_value())
		{
			return;
		}
		const auto kind = known != nullptr ? known->kind : context.belief->type;
		sameKind = BelievedThingsToActOn(creature, mind, desire, [&](entt::entity candidate) {
			const auto* belief = mind.beliefs.Find(candidate);
			return belief != nullptr && belief->kind == kind && Accepts(registry, executor->target, candidate, creature);
		});
	}
	const auto chosen =
	    creature_follow_up::ThingToDoAgainTo(executor->target != Target::None, entt::to_integral(creature), sameKind);
	if (!chosen.has_value())
	{
		return;
	}
	const auto object = static_cast<entt::entity>(*chosen);
	// It must be able to do the action now and know what the action needs
	if (!creature_plan_actions::Allowed(*executor, situation) ||
	    !creature_watching::KnowsWhatItNeeds(learnt.knowledge, tables->actions.at(*actionIndex).needs))
	{
		return;
	}
	const auto whole = WholeOfForcedPlan(creature, mind, desire, object);
	std::optional<entt::entity> instrument;
	if (executor->use != Target::None)
	{
		const auto used = UsedFor(creature, executor->use, position,
		                          tables->rules.at(static_cast<size_t>(desire)).distanceWeight, entt::to_integral(object));
		if (!used.has_value())
		{
			return;
		}
		instrument = static_cast<entt::entity>(used->id);
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} does its action again: {} on {}", entt::to_integral(creature), action,
	                    entt::to_integral(object));
	static_cast<void>(ForcePlan(
	    creature, {.desire = desire, .action = action, .object = object, .instrument = instrument, .activityObject = whole}));
}

void CreatureMindSystem::LearnFromDeedSeen(entt::entity creature, CreatureMindState& mind, size_t deed, entt::entity thing,
                                           uint32_t magic)
{
	const auto* tables = GetTables();
	if (tables == nullptr || !mind.learnt.has_value() || deed >= tables->mimics.size())
	{
		return;
	}
	const auto& rule = tables->mimics.at(deed);
	const auto actions = creature_watching::CopyActionsOf(rule, deed, magic, tables->miracles);
	if (actions.empty() || rule.desire >= creature_desires::k_DesireCount)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto desire = static_cast<Desire>(rule.desire);
	ComeAcross(creature, mind, thing);
	// A creature is its own whole thing
	const auto whole = registry.AllOf<Creature>(thing) ? std::optional(thing) : WholeOfShownThing(creature, mind, thing);
	const creature_learning::Context context {
	    .action = actions.front(),
	    .desire = desire,
	    .object = entt::to_integral(thing),
	    .belief = ActedOnBeliefOf(registry, thing, creature),
	    .activity = whole.transform([](entt::entity entity) { return entt::to_integral(entity); }),
	    .activityBelief = whole.has_value() ? BeliefOf(registry, *whole, creature) : std::nullopt,
	};
	// It learns nothing from a deed that it could not plan to do itself: on a thing it knows nothing of, or for a desire
	// about a whole thing when the thing is part of none
	if (!context.belief.has_value() || (tables->rules.at(rule.desire).aboutSomething && !context.activityBelief.has_value()))
	{
		return;
	}
	auto& learnt = *mind.learnt;
	const auto lessons = creature_learning::ThingLessonsFor(context, entt::to_integral(creature));
	const auto teach = [&](creature_mind_model::TreeKind kind, const creature_tree::Belief& belief) {
		for (const auto& [other, share] : creature_learning::Spread(desire, k_DeedSeenLesson, tables->dependencies))
		{
			creature_mind_model::Learn(learnt, kind, other, {.belief = belief, .feedback = std::clamp(share, -1.0f, 1.0f)},
			                           tables->attributes.at(static_cast<size_t>(other)));
		}
	};
	if (lessons.activity)
	{
		teach(creature_mind_model::TreeKind::ActOn, *context.activityBelief);
	}
	if (lessons.actedOn)
	{
		teach(creature_mind_model::TreeKind::Use, *context.belief);
	}
	creature_mind_model::Think(learnt, creature_learning::ObjectLessonText(k_DeedSeenLesson, ObjectName(context.belief),
	                                                                       tables->texts.at(rule.desire).trying));
}

void CreatureMindSystem::CopyObservedDeedAgain([[maybe_unused]] entt::entity creature, CreatureMindState& mind)
{
	const auto* tables = GetTables();
	if (tables == nullptr || !mind.learnt.has_value())
	{
		return;
	}
	// A creature that never started to copy a deed has nothing to copy
	if (!mind.observedDeed.has_value())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} wants to copy the player but saw no deed to copy",
		                    entt::to_integral(creature));
		return;
	}
	auto observed = *mind.observedDeed;
	// The thing that the player did it to can be gone
	auto& registry = Locator::entitiesRegistry::value();
	if (observed.object.has_value() && !registry.Valid(static_cast<entt::entity>(*observed.object)))
	{
		observed.object.reset();
		mind.observedDeed->object.reset();
	}
	auto copy = creature_watching::CopyObservedDeedAgain(observed, tables->mimics, mind.turn,
	                                                     [this](uint32_t range) { return Random(range); });
	if (!copy.has_value())
	{
		return;
	}
	mind.learnt->mimicry = copy;
	if (observed.object.has_value())
	{
		const auto thing = static_cast<entt::entity>(*observed.object);
		if (const auto at = PointOf(registry, thing))
		{
			mind.copiedAt = *at;
		}
		mind.copiedKind = MimicKindOf(registry, thing);
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} copies the player's {} again", entt::to_integral(creature),
	                    tables->mimics.at(observed.rule).name);
	creature_mind_model::Think(*mind.learnt, fmt::format("I'll copy you again: {}", tables->mimics.at(observed.rule).name));
}
