/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How a creature chooses a plan: the desire to act on, the activity and the thing to act on, and the agenda of the
// plan

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

namespace
{
constexpr float k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();
constexpr float k_TurnsPerSecond = 1.0f / k_TurnSeconds;

/// A villager hurt to this share of its life or less can be healed by a creature
constexpr float k_HurtEnoughToHeal = 0.7f;
/// A pile of food this many times a storage pit's radius from it is the storage pit's
constexpr float k_StoragePitReach = 1.1f;
/// A toy further than this from its creature's home is one it may take home
constexpr float k_ToyAwayFromHome = 50.0f;
} // namespace

namespace openblack::ecs::systems::mind_detail
{
bool IsPowerUpCast(std::string_view action)
{
	return action.ends_with("PU1") || action.ends_with("PU2");
}
} // namespace openblack::ecs::systems::mind_detail

namespace
{
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
} // namespace

namespace openblack::ecs::systems::mind_detail
{
std::optional<glm::vec2> PointOf(const ecs::Registry& registry, entt::entity entity)
{
	const auto position = object_measures::PositionOf(registry, entity);
	return position.has_value() ? std::optional(glm::vec2(position->x, position->z)) : std::nullopt;
}
} // namespace openblack::ecs::systems::mind_detail

namespace
{
/// A creature takes no tree to plant from within this many metres of where the player did what it last set about copying
constexpr float k_PlantFarFromCopied = 20.0f;
} // namespace

namespace openblack::ecs::systems::mind_detail
{
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
} // namespace openblack::ecs::systems::mind_detail

namespace
{
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
} // namespace

namespace openblack::ecs::systems::mind_detail
{
void SituateStoreFood(const ecs::Registry& registry, entt::entity pit, creature_plan_actions::Situation& situation)
{
	const auto pile = StoreFoodPileOf(registry, pit);
	const auto at = pile.has_value() ? PointOf(registry, *pile) : std::nullopt;
	if (at.has_value())
	{
		situation.storeFood = creature_plan_actions::Situation::FoodPile {.pile = entt::to_integral(*pile), .point = *at};
	}
}

uint32_t TurnNow()
{
	return Locator::time::has_value() ? static_cast<uint32_t>(Locator::time::value().GetTurn()) : 0;
}
} // namespace openblack::ecs::systems::mind_detail

namespace
{
/// Whether the creature can stroke a thing to heal it: a thing that it can pick up and that has lost some of its life
bool Strokeable(const ecs::Registry& registry, entt::entity entity, entt::entity self)
{
	const auto life = registry.Valid(entity) ? magic_living::LifeOf(entity) : std::nullopt;
	return life.has_value() && *life < 1.0f && Locator::creatureObjectActionSystem::has_value() &&
	       Locator::creatureObjectActionSystem::value().CanPickUp(self, entity);
}
} // namespace

namespace openblack::ecs::systems::mind_detail
{
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
	case Target::Footballer:
	case Target::FootballerMySideScored:
	case Target::FootballerOtherSideScored:
		return mind_football::Accepts(registry, target, entity, self);
	case Target::Creature:
		return registry.AllOf<Creature>(entity);
	case Target::CreatureFreeForJoint:
		return registry.AllOf<Creature>(entity) && !registry.AllOf<CreaturePartner>(entity);
	case Target::CreatureNotFreeForJoint:
		return registry.AllOf<Creature>(entity) && registry.AllOf<CreaturePartner>(entity);
	case Target::FightableCreature:
		// A dead creature, or one that lies out cold, is not fought
		return registry.AllOf<Creature>(entity) && Locator::creatureMindSystem::has_value() &&
		       !Locator::creatureMindSystem::value().IsLyingDead(entity);
	case Target::FriendlyCreature:
		return registry.AllOf<Creature>(entity) && Locator::creatureMindSystem::has_value() &&
		       Locator::creatureMindSystem::value().SeemsFriendly(self, entity);
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
		const auto kind = mind_detail::KindOfThing(registry, entity);
		return kind != Thing::Creature && kind != Thing::Field && kind != Thing::FishFarm && kind != Thing::Spell &&
		       kind != Thing::Other;
	}
	case Target::InterestingVillager:
	{
		const auto* mind = registry.TryGet<const CreatureMindState>(self);
		return mind != nullptr && DoingSomethingInteresting(registry, *mind, entity);
	}
	case Target::PooedOn:
		return creature_plan_actions::CanPooBy(registry.AnyOf<Villager, Animal, Creature>(entity),
		                                       registry.AnyOf<Field, FishFarm>(entity),
		                                       object_measures::Height(registry, entity));
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
		return creature_object_actions::IsRock(hands->KindOf(entity).thing) &&
		       (target == Target::Rock || hands->CanPickUp(self, entity));
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
	case Target::Stealable:
		return CreatureMindSystem::CanSteal(self, entity);
	case Target::OwnStoragePit:
	case Target::OthersStoragePit:
	{
		const auto* progress = registry.TryGet<const BuildProgress>(entity);
		const auto* me = registry.TryGet<const Creature>(self);
		if (!registry.AllOf<StoragePit>(entity) || me == nullptr)
		{
			return false;
		}
		const bool built = progress == nullptr || !(progress->built < 1.0f);
		const auto pitPlayer = BeliefPlayerOf(registry, entity);
		const auto creaturePlayer = std::optional(PlayerNumberOf(me->owner));
		return target == Target::OwnStoragePit ? creature_town_theft::IsOwnStoragePit(built, pitPlayer, creaturePlayer)
		                                       : creature_town_theft::IsOthersStoragePit(built, pitPlayer, creaturePlayer);
	}
	case Target::OthersField:
	{
		const auto* me = registry.TryGet<const Creature>(self);
		return registry.AllOf<Field>(entity) && me != nullptr &&
		       creature_town_theft::IsOthersField(BeliefPlayerOf(registry, entity), PlayerNumberOf(me->owner));
	}
	case Target::OneShotRestoreHealth:
	{
		// A globe that lies about: the game asks only that its miracle restores health
		const auto oneShot = OneShotOf(registry, entity);
		return oneShot.has_value() && oneShot->Is(creature_plan_actions::OneShotKind::RestoreHealth);
	}
	case Target::TemplePart:
		// The temple of any player, a worship site of any town or the altar at the head of a worship site
		return registry.AnyOf<Temple, WorshipSite, WorshipAltar>(entity);
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
} // namespace openblack::ecs::systems::mind_detail

namespace
{
/// A creature looks this far, in metres, for a fish farm to fish at
constexpr float k_FishingReach = 600.0f;
/// It goes this near the shoal, or as near as it is tall when taller
constexpr float k_FishingArrival = 15.0f;
} // namespace

namespace openblack::ecs::systems::mind_detail
{
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
		if (const auto held = hands.GetCarried(creature))
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
			const auto valid = land.NearestStandable(point);
			// The game measures the reach in three parts, with the squares added from z to x
			standAt = valid.has_value() &&
			                  x87::LengthZyx(shoal - glm::vec3(valid->x, 0.0f, valid->y)) < creature_route::k_ValidPointReach
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
} // namespace openblack::ecs::systems::mind_detail

namespace
{
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
	for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
	{
		if (desires.IsActivated(d) && desires.desires.at(d).value > compassion)
		{
			return false;
		}
	}
	return true;
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
		// A leash tied to a town's desire flag stops the healing choice
		const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
		const bool leashedToTownDesire = leashes != nullptr && leashes->worn.has_value() && leashes->worn->tiedTo.has_value() &&
		                                 registry.Valid(*leashes->worn->tiedTo) &&
		                                 registry.AllOf<TownDesireFlag>(*leashes->worn->tiedTo);
		const auto sightings = MiracleSightings(mind, tables, body->species, MagicType::Heal);
		if (sightings.has_value() &&
		    town_compassion::HealFirst(data->injured, leashedToTownDesire, sightings->first, sightings->second, random))
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
	// A plan whose steps a script stopped is not over: it stays with no steps. A creature free to think makes its choice
	// in the first turn after the stop against the plan as it was. After that turn, the plan is not forced and has
	// priority 0, so the next choice replaces it. Under a script's control, the plan stays as it is until the script gives
	// an order or lets the creature go.
	const bool keptWithNoSteps = mind.planActive && idle.serial == mind.planSerial && idle.step >= idle.agenda.size() &&
	                             idle.stoppedByScript && mind.planner.current.has_value();
	if (keptWithNoSteps && !Locator::entitiesRegistry::value().AllOf<ScriptControlled>(creature))
	{
		if (idle.stoppedSeen)
		{
			mind.planner.current->priority = 0.0f;
			mind.planForced = false;
			mind.forcedPlanCounts = false;
		}
		idle.stoppedSeen = true;
	}
	if (mind.planActive && !keptWithNoSteps && (idle.serial != mind.planSerial || idle.step >= idle.agenda.size()))
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
			// The newest action it remembers for feedback is done, unless a script controls it
			if (!Locator::entitiesRegistry::value().AllOf<ScriptControlled>(creature))
			{
				MarkNewestActionDone(creature, mind);
			}
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
			if (!Locator::entitiesRegistry::value().AllOf<ScriptControlled>(creature))
			{
				MarkNewestActionDone(creature, mind);
			}
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
	RememberAction(creature, mind,
	               {
	                   .action = *action,
	                   .desire = static_cast<Desire>(info.desire),
	                   .object = object,
	                   .belief = object.has_value() ? ActedOnBeliefOf(registry, static_cast<entt::entity>(*object), creature)
	                                                : std::nullopt,
	                   .activity = activity,
	                   .activityBelief = activity.has_value()
	                                         ? BeliefOf(registry, static_cast<entt::entity>(*activity), creature)
	                                         : std::nullopt,
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
/// The action that takes the creature to a teleport stone, whose walks the area that holds the creature never refuses
constexpr std::string_view k_GoToTeleportAction = "GoToTeleport";
} // namespace

namespace openblack::ecs::systems::mind_detail
{
std::optional<std::pair<creature_leash::AgendaArea, glm::vec2>>
AgendaAreaOf(const ecs::Registry& registry, entt::entity creature, std::string_view action, bool walksBack)
{
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	const auto* at = registry.TryGet<const Transform>(creature);
	if (leashes == nullptr || at == nullptr)
	{
		return std::nullopt;
	}
	const bool leashed = leashes->worn.has_value();
	const creature_leash::AgendaArea area {
	    .radius = leashes->confinementRadius,
	    .leashed = leashed,
	    .leashWorks = !leashed || leashes->worn->works,
	    .centre = glm::xz(leashes->confinementCentre),
	    .walkingBack = walksBack || leashes->returnTo.has_value(),
	    .scriptControlled = registry.AllOf<ScriptControlled>(creature),
	    .toTeleport = action == k_GoToTeleportAction,
	};
	return std::pair {area, glm::vec2(glm::xz(at->position))};
}
} // namespace openblack::ecs::systems::mind_detail

namespace
{
/// Whether the area that holds the creature refuses a walk of a new agenda for an action. The area refuses only a walk
/// to a point, not a walk to a thing.
bool AgendaLeavesArea(const ecs::Registry& registry, entt::entity creature, std::string_view action,
                      std::span<const creature_mind::Step> agenda)
{
	const auto area = AgendaAreaOf(registry, creature, action);
	return area.has_value() && creature_agenda_area::RefusesAgenda(area->first, area->second, agenda);
}
} // namespace

namespace openblack::ecs::systems::mind_detail
{
bool LeftAreaFor(const ecs::Registry& registry, entt::entity creature, const CreatureMindState& mind, uint32_t action)
{
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	if (leashes == nullptr || action >= mind.leftAreaActions.size() || !mind.leftAreaActions[action])
	{
		return false;
	}
	const bool leashed = leashes->worn.has_value();
	return creature_leash::IsConfined(leashes->confinementRadius, leashed, !leashed || leashes->worn->works);
}

std::optional<uint32_t> AgendaThingOf(const ecs::Registry& registry, const creature_plan_actions::Executor& executor,
                                      std::optional<uint32_t> thing)
{
	if (!thing.has_value() || executor.build != creature_plan_actions::Build::LookAtTemple)
	{
		return thing;
	}
	return entt::to_integral(ecs::temple_heart::MapThingOf(registry, static_cast<entt::entity>(*thing)));
}

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
	for (const auto* kept : mind->beliefs.HeldInListOrder())
	{
		const auto& belief = *kept;
		const auto acted = ActedThingOf(registry, belief.thing);
		if (!acted.has_value())
		{
			continue;
		}
		const auto thing = *acted;
		// Not a whole thing, nor a thing it found no way to lately, nor one outside the area its leash keeps it within
		if (creature_object_beliefs::InListOfWholeThings(belief.kind) || !registry.Valid(thing) ||
		    NoWayLately(registry, *mind, thing) || OutsideConfinedArea(registry, creature, belief.place) ||
		    !LetsCreatureAct(registry, thing) || !Accepts(registry, use, thing, creature))
		{
			continue;
		}
		candidates.push_back({.id = entt::to_integral(thing),
		                      .distance = gutils::GetDistanceInMetres(map_coords::ToMetres(belief.place), position),
		                      .held = ecs::creature_carried::Carries(registry, creature, thing)});
	}
	return creature_planner::ChooseUsed(candidates, distanceWeight, goal);
}

void DrawAgendaLengths(const creature_plan_actions::Executor& executor, creature_plan_actions::Situation& situation,
                       const std::function<float(float)>& upTo)
{
	if (executor.build == creature_plan_actions::Build::LookButDontApproach)
	{
		situation.lookExtraSeconds = upTo(creature_mind::k_LookButDontApproachExtraSeconds);
	}
	if (executor.build == creature_plan_actions::Build::SmileAt)
	{
		situation.smileExtraSeconds = upTo(creature_mind::k_SmileExtraSeconds);
	}
	if (executor.build == creature_plan_actions::Build::SitDown &&
	    (situation.controlledByScript || situation.clearArea.has_value()))
	{
		situation.sitExtraSeconds = upTo(creature_emote::k_SitExtraSeconds);
	}
}
} // namespace openblack::ecs::systems::mind_detail

bool CreatureMindSystem::Adopt(entt::entity creature, CreatureMindState& mind, const creature_planner::Plan& plan,
                               const creature_plan_actions::Situation& situation, bool fightBodyEndsAgain)
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
			StopFightForPlan(creature, fightBodyEndsAgain);
		}
		if (Locator::creatureObjectActionSystem::has_value() &&
		    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy)
		{
			Locator::creatureObjectActionSystem::value().StopForPlan(creature, true);
		}
		Abandon(mind);
		creature_mind::Plan(mind.idle, creature_mind::Activity::None, {});
		CopyObservedDeedAgain(creature, mind);
		if (const auto source = creature_learning::TakeUpChosenPlan((*mind.desires)[plan.desire]); source.has_value())
		{
			RememberAction(creature, mind,
			               {
			                   .action = plan.action,
			                   .desire = plan.desire,
			                   .learnable = info.learnable,
			                   .windowSeconds = info.learningWindowSeconds,
			                   .source = *source,
			                   .ordered = _orderedPlanFor == creature,
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
		StopFightForPlan(creature, fightBodyEndsAgain);
	}
	// Whatever its hands were doing stops for the plan. An action that the body started plays on, and a point plays its
	// end.
	if (Locator::creatureObjectActionSystem::has_value() &&
	    Locator::creatureObjectActionSystem::value().GetState(creature) == CreatureObjectActionSystemInterface::State::Busy)
	{
		Locator::creatureObjectActionSystem::value().StopForPlan(creature, true);
	}
	const bool refused = mind.agendaRefused;
	if (!Replan(creature, executor->activity, std::move(agenda)))
	{
		return false;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} sets about {} on {} ({} steps)", entt::to_integral(creature),
	                    info.name, plan.object.value_or(0), mind.idle.agenda.size());
	mind.planner.current = chosen;
	mind.planActive = true;
	mind.planSerial = mind.idle.serial;
	mind.agendaSeen = mind.idle.serial;
	mind.satisfiedByEffect = false;
	mind.desireSeenTo = false;
	mind.planActions = mind.plannedActions.at(static_cast<size_t>(plan.desire));
	// It is remembered for the player's feedback unless nothing says why it wants what the plan serves, as a forced plan
	// is, and the desires the plan's desire opposes are held back
	// A plan that failed at once because the area refused a walk is not remembered and holds back no desire.
	const auto source = creature_learning::TakeUpChosenPlan((*mind.desires)[plan.desire]);
	if (refused)
	{
		return true;
	}
	if (source.has_value())
	{
		const auto activity = LessonActivity(*tables, plan.desire, plan.object, plan.about);
		RememberAction(
		    creature, mind,
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
		        .ordered = _orderedPlanFor == creature,
		    });
	}
	creature_learning::SuppressOpposed(*mind.desires, plan.desire, tables->dependencies, k_TurnsPerSecond);
	return true;
}

std::optional<std::pair<const creature_plan_actions::Executor*, std::vector<creature_mind::Step>>>
CreatureMindSystem::PlanAgenda(entt::entity creature, uint32_t action, std::optional<uint32_t> object,
                               const creature_plan_actions::Situation& situation)
{
	if (auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature))
	{
		mind->agendaRefused = false;
	}
	const auto* tables = GetTables();
	if (tables == nullptr || action >= tables->actions.size())
	{
		return std::nullopt;
	}
	const auto* executor = ExecutorOf(action);
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
		situated.thingLiving =
		    Locator::creatureObjectActionSystem::has_value() &&
		    creature_object_actions::IsLiving(
		        Locator::creatureObjectActionSystem::value().KindOf(static_cast<entt::entity>(*object)).thing);
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
	if (executor->build == creature_plan_actions::Build::DanceWithVillagers)
	{
		SituateDance(creature, *executor, point, situated);
	}
	mind_town_help::Situate(Locator::entitiesRegistry::value(), creature, *executor, object, situated);
	mind_football::Situate(Locator::entitiesRegistry::value(), creature, *executor, object, situated);
	if (object.has_value())
	{
		SituateFriend(creature, *executor, static_cast<entt::entity>(*object), situated);
	}
	if (executor->build == creature_plan_actions::Build::Drink)
	{
		situated.drink = DrinkPlaceOf(creature);
	}
	DrawAgendaLengths(*executor, situated, _random.GameFloat());
	auto agenda = creature_plan_actions::Agenda(
	    *executor, AgendaThingOf(Locator::entitiesRegistry::value(), *executor, object), point, situated,
	    [this](uint32_t range) { return Random(range); }, cast);
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
	// The area that holds the creature refuses a walk of the agenda: the plan fails as the creature takes it, and the
	// creature does not want the action while the area holds it
	if (auto* mind = registry.TryGet<CreatureMindState>(creature))
	{
		mind->agendaRefused = AgendaLeavesArea(registry, creature, tables->actions[action].name, *agenda);
		if (mind->agendaRefused)
		{
			mind->leftAreaActions.resize(std::max(mind->leftAreaActions.size(), tables->actions.size()), false);
			mind->leftAreaActions[action] = true;
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} cannot walk out of its area to {}",
			                    entt::to_integral(creature), tables->actions[action].name);
		}
	}
	return std::pair {executor, std::move(*agenda)};
}

namespace
{
/// Whether a fight lets the creature choose a new plan. The creature that started the fight by its own plan chooses,
/// but only while that plan is the fight: a plan that it wants much more ends the fight. The other creature chooses
/// until the first creature tells it to stand still, and a plan that it chooses ends the fight. Then it obeys. A
/// creature made to fight or made to do something after a knock-out does not choose, and a creature that lies out cold
/// does not choose.
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
	    .onlyAsked = fighting != nullptr && fighting->part == CreatureFighting::Part::Asked,
	    .mindFree = fighting != nullptr && fighting->mindFree,
	});
}
} // namespace

void CreatureMindSystem::PlanCreature(entt::entity creature, CreatureMindState& mind, bool everyDesire,
                                      std::optional<Desire> only, bool adopt)
{
	const auto* tables = GetTables();
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(creature);
	// A partner of a joint action whose mind stands still chooses nothing
	if (tables == nullptr || transform == nullptr || !mind.desires.has_value() || !mind.learnt.has_value() || mind.paused ||
	    creature_mind::IsUnconscious(mind.idle) || !FightLetsChoose(registry, creature, mind) ||
	    registry.AllOf<CreatureDemobilised>(creature))
	{
		return;
	}
	if (mind.plannedTurn == mind.turn)
	{
		return;
	}
	mind.plannedTurn = mind.turn;
	// Before the creature decides for itself, it stops a reaction when no agenda runs
	const auto decisionState = [&registry, &mind, creature]() {
		const auto* reaction = registry.TryGet<const LivingReaction>(creature);
		return creature_turn_order::DecisionState {
		    .planRuns = mind.idle.step < mind.idle.agenda.size(),
		    .planForced = mind.planForced,
		    .reacting = reaction != nullptr && reaction->reaction != 0,
		};
	};
	if (!only.has_value() && creature_turn_order::StopsReactionBeforeDecision(decisionState()))
	{
		EndReaction(creature);
	}
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
	// A forced plan or a reaction keeps it from deciding. The thing that waits for the end of its work also waits.
	if (!only.has_value() && creature_turn_order::SkipsDecision(decisionState()))
	{
		return;
	}
	// When the thing that the cast on the leash waits for is gone, the cast does not wait
	if (mind.followUp == creature_follow_up::FollowUp::CastOnLeash &&
	    !(mind.leashCast.has_value() && registry.Valid(mind.leashCast->thing)))
	{
		mind.followUp.reset();
		mind.leashCast.reset();
	}
	// The feed of a worship site waits only for a flag of a site that is still there
	if (mind.followUp == creature_follow_up::FollowUp::FeedWorshipSite && FeedSiteAltar(mind.feedSiteFlag) == entt::null)
	{
		mind.followUp.reset();
		mind.feedSiteFlag = entt::null;
	}
	// When it completes its work, the thing that waits replaces its next decision: it becomes high after a magic
	// mushroom, or it reacts to a stroke or a slap
	if (adopt && mind.followUp.has_value() &&
	    creature_turn_order::DoesFollowUp(decisionState(), only.has_value(), registry.AllOf<ScriptControlled>(creature)))
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
	Situate(creature, situation);
	const auto random = [this](uint32_t range) { return Random(range); };
	bool fishingLooked = false;
	const auto prepare = [&](const creature_plan_actions::Executor& executor) {
		if (creature_plan_actions::FishesFirst(executor) && !fishingLooked)
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
		const auto strength = desires.IsActivated(d) && state.suppressedTurns == 0 ? state.value : 0.0f;
		strengths.at(d) = strength;
		eligible.at(d) = strength > 0.0f && std::ranges::any_of(tables->desireActions.at(d), [this](uint32_t action) {
			                 return ExecutorOf(action) != nullptr;
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

	// The lists of the actions of each desire keep their memory from one desire to the next
	std::vector<creature_planner::ActionCandidate> candidates;
	std::vector<const creature_plan_actions::Executor*> executors;
	BelievedFacts believed;
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
		if (aboutSomething)
		{
			// The thing shown on the leash makes every desire, compassion too, about the whole thing that the shown thing
			// is part of
			activity = leashTarget.has_value() ? ShownActivity(creature, mind, desire, *leashTarget)
			                                   : ChooseActivity(creature, mind, desire);
			// Compassion does what the town desire that it helps with asks for. Compassion for a creature has no action
			if (desire == Desire::Compassion && activity.has_value() &&
			    !creature_activity::CompassionHasActions(activity->kind))
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
			if (leashTarget.has_value())
			{
				// The plan acts on the shown thing when it suits the desire and is of use, else on the first part of the
				// whole thing that is
				parts = ShownParts(creature, mind, desire, *leashTarget, *activity);
			}
			else
			{
				parts = PartsToActOn(creature, mind, desire, *activity);
				// Anger chooses the part to act on before the action: the part that is most useful to act on
				if (desire == Desire::Anger)
				{
					std::vector<float> partWorth;
					std::ranges::transform(parts, std::back_inserter(partWorth),
					                       &creature_planner::ObjectCandidate::usefulness);
					const auto chosen = creature_activity::MostUseful(partWorth);
					parts = chosen.has_value() ? std::vector {parts.at(*chosen)}
					                           : std::vector<creature_planner::ObjectCandidate> {};
				}
			}
		}
		else if (leashTarget.has_value())
		{
			// A desire not about a whole thing acts on the shown thing when it suits the desire and is of use. If not, it
			// acts on the first part of the whole thing that suits the desire and is of use.
			parts = ShownThingsWithoutWhole(creature, mind, desire, *leashTarget);
		}
		// The actions of the desire, in the order of its list
		candidates.clear();
		executors.clear();
		for (const auto action : actions)
		{
			const auto* executor = ExecutorOf(action);
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
			    .leftArea = LeftAreaFor(registry, creature, mind, action),
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
		const auto heldByItself = [&](entt::entity thing) { return ecs::creature_carried::Carries(registry, creature, thing); };
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
		// The facts about the believed things that one action finds are for the next actions of this desire only
		believed.foundAt.reset();
		const auto findObject =
		    [&](const creature_planner::ActionCandidate& candidate) -> std::optional<creature_planner::ObjectCandidate> {
			const auto* executor = executorOf(candidate);
			const auto suits = [&](entt::entity thing) {
				return (executor->target == Target::None || Accepts(registry, executor->target, thing, creature)) &&
				       !(executor->notWhatItHolds && heldByItself(thing));
			};
			std::vector<creature_planner::ObjectCandidate> found;
			if (leashTarget.has_value())
			{
				// The first of the shown parts that the action can act on
				const auto first = std::ranges::find_if(parts, [&](const creature_planner::ObjectCandidate& part) {
					return suits(static_cast<entt::entity>(part.id));
				});
				return first != parts.end() ? std::optional(*first) : std::nullopt;
			}
			if (activity.has_value())
			{
				std::ranges::copy_if(parts, std::back_inserter(found), [&](const creature_planner::ObjectCandidate& part) {
					return suits(static_cast<entt::entity>(part.id));
				});
			}
			else
			{
				found = BelievedThingsToActOn(creature, mind, desire, suits, believed);
			}
			return mostUseful(found);
		};
		const auto distanceWeight = tables->rules.at(d).distanceWeight;
		const auto findUsed = [&](const creature_planner::ActionCandidate& candidate, std::optional<uint32_t> object) {
			return UsedFor(creature, executorOf(candidate)->use, position, distanceWeight, object);
		};
		// A desire about a whole thing finds the action before the part, but anger finds the part first
		auto order = creature_planner::PlanOrder::ActionFirst;
		if (leashTarget.has_value())
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
			Adopt(creature, mind, *plan, situation, false);
		}
		return;
	}
	// An agenda that it was made to do continues to its end
	if (!creature_planner::MayChoose(mind.planActive, mind.idle.step < mind.idle.agenda.size()))
	{
		return;
	}
	mind.planner.currentHasNoSteps = mind.planActive && mind.idle.stoppedByScript && mind.idle.serial == mind.planSerial &&
	                                 mind.idle.step >= mind.idle.agenda.size();
	const auto choice = creature_planner::Choose(mind.planner);
	if (choice.has_value() && !Adopt(creature, mind, *choice, situation, true))
	{
		mind.planner.best.at(static_cast<size_t>(choice->desire)).reset();
	}
}

void CreatureMindSystem::PlanTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> creatures;
	registry.Each<const Creature, CreatureMindState>(
	    [&creatures](entt::entity entity, const Creature&, CreatureMindState&) { creatures.push_back(entity); });
	// While the hand of a player holds a creature, it decides nothing: it keeps the plan that it had
	const auto held = [](entt::entity creature) {
		return Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().IsLocked(creature);
	};
	for (const auto entity : creatures)
	{
		if (auto* mind = registry.TryGet<CreatureMindState>(entity); mind != nullptr && !held(entity))
		{
			PlanCreature(entity, *mind);
		}
	}
}

void CreatureMindSystem::PlanCreatureTurn(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature>(creature))
	{
		return;
	}
	// While the hand of a player holds a creature, it decides nothing: it keeps the plan that it had
	const bool held = Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().IsLocked(creature);
	if (auto* mind = registry.TryGet<CreatureMindState>(creature); mind != nullptr && !held)
	{
		PlanCreature(creature, *mind);
	}
}
