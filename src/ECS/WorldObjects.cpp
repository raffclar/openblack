/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorldObjects.h"

#include <algorithm>
#include <optional>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "Animals/FishFarmRules.h"
#include "ECS/AbodeKnock.h"
#include "ECS/AbodeSurroundings.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/DestructionGhost.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Firefly.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicForest.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/ResourcePile.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownAggression.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Components/Weather.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Creches.h"
#include "ECS/GameEndConditions.h"
#include "ECS/Graveyards.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerFire.h"
#include "ECS/Systems/Implementations/VillagerHome.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/PuzzleGameSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Systems/WonderSystemInterface.h"
#include "ECS/Systems/WorkshopSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/ThingModel.h"
#include "ECS/TownAggression.h"
#include "ECS/TownAreaRules.h"
#include "ECS/TownPlanning.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/ResourcePiles.h"
#include "Magic/SpellSparing.h"
#include "Physics/DamageMesh.h"
#include "Physics/LivingRules.h"
#include "Resources/ResourcesInterface.h"
#include "ScriptHeaders/ScriptPropertyRules.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// A building that is built and whose town stands: one whose repair the town would see to
bool IsAbodeOfATown(const Registry& registry, entt::entity object)
{
	// A miracle dispenser is a building of its town as a house is
	if (const auto* dispenser = registry.TryGet<const SpellDispenser>(object))
	{
		return registry.Valid(dispenser->town) && registry.AllOf<Town>(dispenser->town);
	}
	const auto* abode = registry.TryGet<const Abode>(object);
	if (abode == nullptr)
	{
		return false;
	}
	const auto& towns = registry.Context().towns;
	const auto town = towns.find(abode->townId);
	return town != towns.end() && registry.Valid(town->second) && registry.AllOf<Town>(town->second);
}

bool IsBuiltBuildingOfATown(const Registry& registry, entt::entity object)
{
	if (const auto* progress = registry.TryGet<const BuildProgress>(object); progress != nullptr && progress->built < 1.0f)
	{
		return false;
	}
	return IsAbodeOfATown(registry, object);
}

/// An object with no model stands this big
constexpr float k_DefaultRadius = 0.5f;
constexpr float k_DefaultHeight = 1.0f;

template <typename Array, typename Index>
const GObjectInfo* Row(const Array& rows, Index index)
{
	const auto i = static_cast<size_t>(index);
	return i < rows.size() ? &rows[i] : nullptr;
}

/// A building hurt below its full life brings everyone inside out, as a knock does. One that stops working through it
/// calls an emergency in its town when it is the town's storage pit or village centre.
void BuildingHurt(entt::entity abode, float before, float after)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto inhabitants = registry.Get<const Abode>(abode).inhabitants;
	for (const auto villager : inhabitants)
	{
		villager_home::SetStateWhenTappedOnAbode(villager);
	}
	const auto* info = world_objects::AbodeInfoOf(abode);
	if (info == nullptr)
	{
		return;
	}
	const float stops = info->thresholdForStopBeingFunctional;
	// A storage pit that stops working lets the people near it take from its piles
	if (before > stops && after <= stops && info->abodeType == AbodeType::StoragePit &&
	    Locator::resourceStoreSystem::has_value())
	{
		Locator::resourceStoreSystem::value().StoreStopsWorking(abode);
	}
	if (abode_knock::HurtCallsEmergency(info->abodeType, before, after, info->thresholdForStopBeingFunctional) &&
	    Locator::townSystem::has_value())
	{
		const auto& towns = registry.Context().towns;
		if (const auto town = towns.find(registry.Get<const Abode>(abode).townId);
		    town != towns.end() && registry.Valid(town->second) && registry.AllOf<Town>(town->second))
		{
			Locator::townSystem::value().SetInStateOfEmergency(town->second);
		}
	}
}

/// In a network game a harm to a building may count as a building smashed by the player who did it
void CountSmashed(std::optional<PlayerNames> by, const game_end::BuildingHarm& harm)
{
	if (const auto condition = game_end::SmashedCondition(harm); condition.has_value() && Locator::gameEndSystem::has_value())
	{
		Locator::gameEndSystem::value().AddToCondition(by, *condition, 1);
	}
}
} // namespace

const GAbodeInfo* world_objects::AbodeInfoOf(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	// A miracle dispenser is a building of the buildings' table, by its own row
	if (const auto* dispenser = registry.TryGet<const SpellDispenser>(object);
	    dispenser != nullptr && Locator::infoConstants::has_value())
	{
		const auto& rows = Locator::infoConstants::value().abode;
		const auto row = static_cast<size_t>(dispenser->building);
		return dispenser->building != AbodeInfo::None && row < rows.size() ? &rows[row] : nullptr;
	}
	const auto* abode = registry.TryGet<const Abode>(object);
	if (abode == nullptr || !Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto* mesh = registry.TryGet<const Mesh>(object);
	const GAbodeInfo* first = nullptr;
	for (const auto& info : Locator::infoConstants::value().abode)
	{
		if (info.abodeNumber != abode->type)
		{
			continue;
		}
		if (mesh != nullptr && resources::HashIdentifier(info.meshId) == mesh->id)
		{
			return &info;
		}
		if (first == nullptr)
		{
			first = &info;
		}
	}
	return first;
}

const GObjectInfo* world_objects::InfoOf(entt::entity object)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return nullptr;
	}
	const auto& info = Locator::infoConstants::value();
	if (const auto* villager = registry.TryGet<const Villager>(object))
	{
		return Row(info.villager, RowOf(*villager, GVillagerInfo::Find));
	}
	if (const auto* creature = registry.TryGet<const Creature>(object))
	{
		return Row(info.creature, creature::InfoRow(creature->species));
	}
	if (const auto* tree = registry.TryGet<const Tree>(object))
	{
		return Row(info.tree, tree->type);
	}
	if (registry.AllOf<DeadTree>(object))
	{
		return Row(info.mobileStatic, MobileStaticInfo::DeadTree);
	}
	if (const auto* field = registry.TryGet<const Field>(object))
	{
		return Row(info.fieldType, field->type);
	}
	if (registry.AnyOf<Abode, SpellDispenser>(object))
	{
		return AbodeInfoOf(object);
	}
	if (const auto* forest = registry.TryGet<const BigForest>(object))
	{
		return Row(info.bigForest, forest->type);
	}
	if (const auto* animated = registry.TryGet<const AnimatedStatic>(object))
	{
		return Row(info.animatedStatic, animated->type);
	}
	if (const auto* feature = registry.TryGet<const Feature>(object))
	{
		return Row(info.feature, feature->type);
	}
	if (const auto* pot = registry.TryGet<const Pot>(object))
	{
		return Row(info.pot, pot->type);
	}
	if (const auto* still = registry.TryGet<const MobileStatic>(object))
	{
		return Row(info.mobileStatic, still->type);
	}
	if (const auto* mobile = registry.TryGet<const MobileObject>(object))
	{
		return Row(info.mobileObject, mobile->type);
	}
	// The football has a row of its own
	if (registry.AllOf<Ball>(object))
	{
		return &info.ball;
	}
	// A scaffold has the one row of the scaffolds
	if (registry.AllOf<Scaffold>(object))
	{
		return &info.scaffold;
	}
	return nullptr;
}

const GObjectInfo* world_objects::HelpInfoOf(entt::entity object)
{
	if (const auto* kind = InfoOf(object))
	{
		return kind;
	}
	if (!Locator::infoConstants::has_value() || !Locator::entitiesRegistry::value().Valid(object))
	{
		return nullptr;
	}
	if (Locator::entitiesRegistry::value().AllOf<Vortex>(object))
	{
		return Row(Locator::infoConstants::value().mobileStatic, MobileStaticInfo::Vortex);
	}
	return nullptr;
}

const GObjectInfo* world_objects::EffectInfoOf(entt::entity object)
{
	if (const auto* kind = InfoOf(object))
	{
		return kind;
	}
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return nullptr;
	}
	// A town's desire flag takes no effect, also no heat from a fire
	if (registry.AllOf<TownDesireFlag>(object))
	{
		return nullptr;
	}
	const auto& info = Locator::infoConstants::value();
	// A firefly takes heat and catches fire by its row of the fixed things' table
	if (registry.AllOf<Firefly>(object))
	{
		return &info.singleMapFixed.at(Firefly::k_InfoRow);
	}
	if (const auto* flowers = registry.TryGet<const Flowers>(object))
	{
		return Row(info.flowers, flowers->type);
	}
	// An animal takes heat and catches fire by the row of its kind
	if (const auto* animal = registry.TryGet<const Animal>(object))
	{
		return Row(info.animal, animal->type);
	}
	// A worship site has the row of its tribe
	if (const auto* site = registry.TryGet<const WorshipSite>(object))
	{
		return Row(info.worshipSite, site->tribe);
	}
	if (registry.AllOf<Temple>(object))
	{
		return &info.citadelHeart;
	}
	if (registry.AllOf<FishFarm>(object))
	{
		return &info.fishFarm;
	}
	// A street lantern takes effects and fire by its row of the statics' table
	if (const auto* lantern = registry.TryGet<const StreetLantern>(object))
	{
		return Row(info.mobileStatic, lantern->info);
	}
	return nullptr;
}

std::vector<model_surface::Triangle> world_objects::DrawnTrianglesOf(entt::entity object)
{
	const auto* mesh = Locator::entitiesRegistry::value().TryGet<const Mesh>(object);
	if (mesh == nullptr || !Locator::resources::has_value())
	{
		return {};
	}
	const auto& files = Locator::resources::value().GetL3DFiles();
	if (!files.Contains(mesh->id))
	{
		return {};
	}
	return model_surface::DrawnTriangles(*files.Handle(mesh->id), {.detailLevels = 1, .latestState = 0});
}

glm::mat4 world_objects::PlacementOf(entt::entity object)
{
	const auto& transform = Locator::entitiesRegistry::value().Get<const Transform>(object);
	return glm::translate(glm::mat4(1.0f), transform.position) * glm::mat4(transform.rotation) *
	       glm::scale(glm::mat4(1.0f), transform.scale);
}

world_objects::Size world_objects::SizeOf(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	// A fish farm has no model: it reaches its own radius across the land, and has no height
	if (registry.AllOf<FishFarm>(object))
	{
		return {.radius = fish_farm::k_Radius, .height = 0.0f};
	}
	// A town reaches half the longer side of the ground it keeps as covered
	if (const auto* town = registry.TryGet<const Town>(object); town != nullptr)
	{
		return {.radius = town_area::Radius(town->area), .height = 0.0f};
	}
	Size size {.radius = k_DefaultRadius, .height = k_DefaultHeight};
	const auto* transform = registry.TryGet<const Transform>(object);
	// A one-shot miracle's globe is drawn by the miracles' own pass, with its dome's model. An altar has its model
	// before the model shows.
	auto model = thing_model::ModelOf(registry, object);
	if (!model.has_value() && registry.AllOf<OneOffSpellSeed>(object))
	{
		model = OneOffSpellSeed::k_MeshId.value();
	}
	if (transform == nullptr || !model.has_value() || !Locator::resources::has_value())
	{
		return size;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(*model))
	{
		return size;
	}
	const auto box = meshes.Handle(*model)->GetBoundingBox().Size() * transform->scale;
	size.radius = std::max(box.x, box.z) * 0.5f;
	size.height = box.y;
	return size;
}

std::optional<world_objects::Size> world_objects::SizeOfModel(entt::id_type model, float scale)
{
	if (!Locator::resources::has_value())
	{
		return std::nullopt;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(model))
	{
		return std::nullopt;
	}
	const auto box = meshes.Handle(model)->GetBoundingBox().Size() * scale;
	return Size {.radius = std::max(box.x, box.z) * 0.5f, .height = box.y};
}

namespace
{
/// The pot of a pile of food or wood, whose life is what it holds; none for anything else
const Pot* PileOf(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.AllOf<ResourcePile>(object) ? registry.TryGet<const Pot>(object) : nullptr;
}

/// What a pile of its kind is drawn full at
uint32_t FullAmountOf(const Pot& pile)
{
	return Locator::infoConstants::value().pot.at(static_cast<size_t>(pile.type)).maxAmountInPot;
}

/// The town an abode belongs to, none for none
entt::entity TownOfAbode(const Abode& abode)
{
	entt::entity town = entt::null;
	Locator::entitiesRegistry::value().Each<const Town>([&](entt::entity entity, const Town& candidate) {
		if (candidate.id == abode.townId)
		{
			town = entity;
		}
	});
	return town;
}

/// The town that a building belongs to: the town of an abode or of a miracle dispenser, none for none
entt::entity TownOfBuilding(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto* abode = registry.TryGet<const Abode>(object))
	{
		return TownOfAbode(*abode);
	}
	if (const auto* dispenser = registry.TryGet<const SpellDispenser>(object);
	    dispenser != nullptr && registry.Valid(dispenser->town) && registry.AllOf<Town>(dispenser->town))
	{
		return dispenser->town;
	}
	return entt::null;
}

/// The town of the storage pit a pile is part of, or of the building whose site keeps its wood in it; none for a pile
/// of neither. (A worship site's food pile is left out: which town the game gives it was not traced.)
std::optional<entt::entity> TownOfPile(entt::entity object)
{
	if (PileOf(object) == nullptr)
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	auto structure =
	    Locator::resourceStoreSystem::has_value() ? Locator::resourceStoreSystem::value().StoreOf(object) : std::nullopt;
	if (structure.has_value() && !registry.AllOf<StoragePit>(*structure))
	{
		return std::nullopt;
	}
	if (!structure.has_value())
	{
		registry.Each<const BuildingSites>([&](entt::entity building, const BuildingSites& sites) {
			for (const auto& site : sites.sites)
			{
				if (std::ranges::find(site.piles, object) != site.piles.end())
				{
					structure = building;
				}
			}
		});
	}
	const auto* abode = structure.has_value() ? registry.TryGet<const Abode>(*structure) : nullptr;
	if (abode == nullptr)
	{
		return std::nullopt;
	}
	return TownOfAbode(*abode);
}

/// A pile gives up what it loses as anything does, from its store when it is part of one, and takes what it gains
/// into itself or its store, keeping the poison it has
void ChangeHeld(entt::entity pile, magic::piles::HeldChange change)
{
	if (!Locator::resourceStoreSystem::has_value() || (change.removed == 0 && change.added == 0))
	{
		return;
	}
	auto& stores = Locator::resourceStoreSystem::value();
	const auto& pot = Locator::entitiesRegistry::value().Get<const Pot>(pile);
	const auto type = Locator::infoConstants::value().pot.at(static_cast<size_t>(pot.type)).resourceType;
	if (change.removed != 0)
	{
		static_cast<void>(stores.RemoveResource(pile, type, change.removed));
		return;
	}
	const bool poisoned = pot.poisoned;
	if (const auto store = stores.StoreOf(pile))
	{
		static_cast<void>(stores.AddToStore(*store, type, change.added, std::nullopt, poisoned, std::nullopt));
		return;
	}
	static_cast<void>(stores.AddToPile(pile, type, change.added, poisoned));
}
} // namespace

float world_objects::LifeOf(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return 0.0f;
	}
	if (const auto* needs = registry.TryGet<const CreatureNeeds>(object))
	{
		return needs->needs.life;
	}
	// A pile's life is what it holds of its fill
	if (const auto* pile = PileOf(object))
	{
		return magic::piles::Life(pile->amount, FullAmountOf(*pile));
	}
	if (const auto* animal = registry.TryGet<const Animal>(object))
	{
		return animal->life;
	}
	if (const auto* life = registry.TryGet<const ObjectLife>(object))
	{
		return life->life;
	}
	if (const auto* villager = registry.TryGet<const Villager>(object))
	{
		return villager->life;
	}
	return 1.0f;
}

void world_objects::SetLife(entt::entity object, float life, bool allowed)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return;
	}
	if (auto* needs = registry.TryGet<CreatureNeeds>(object))
	{
		if (allowed)
		{
			needs->needs.life = life;
		}
		return;
	}
	if (auto* villager = registry.TryGet<Villager>(object))
	{
		// Counted as injured or better before the life is taken, whether it is or not
		CountInjury(object, villager->life, life);
		if (allowed)
		{
			villager->life = life;
		}
		return;
	}
	if (auto* animal = registry.TryGet<Animal>(object))
	{
		if (allowed)
		{
			animal->life = life;
		}
		return;
	}
	if (registry.AllOf<Tree>(object) && life <= 0.0f)
	{
		Remove(object);
		return;
	}
	// A pile set to a life gains or loses what that life holds
	if (const auto* pile = PileOf(object))
	{
		if (allowed)
		{
			ChangeHeld(object, magic::piles::ChangeForLife(pile->amount, FullAmountOf(*pile), life));
		}
		return;
	}
	if (allowed)
	{
		registry.AssignOrReplace<ObjectLife>(object, ObjectLife {.life = life});
	}
}

float world_objects::ReduceLife(entt::entity object, float damage, std::optional<PlayerNames> by, bool byCreature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return 0.0f;
	}
	// A village totem gives the harm to its town centre, and keeps its own life
	if (const auto* totem = registry.TryGet<const VillageTotem>(object); totem != nullptr && registry.Valid(totem->townCentre))
	{
		return ReduceLife(totem->townCentre, damage, by, byCreature);
	}
	// A field burns its food and keeps its life
	if (auto* field = registry.TryGet<Field>(object))
	{
		if (const auto* info = Row(Locator::infoConstants::value().fieldType, field->type))
		{
			const auto& type = static_cast<const GFieldTypeInfo&>(*info);
			field->crop.food = std::max(field->crop.food - damage * type.totalFoodInField, 0.0f);
		}
		return 1.0f;
	}
	// A pile loses what it holds rather than life: burnt or blasted away, and gone once a pile on its own has nothing left
	if (const auto* pile = PileOf(object))
	{
		ChangeHeld(object, magic::piles::ChangeForDamage(pile->amount, FullAmountOf(*pile), damage));
		return LifeOf(object);
	}
	if (auto* needs = registry.TryGet<CreatureNeeds>(object))
	{
		// A creature in a duel also loses the damage from its fight health, and is knocked out when none is left. A
		// creature with no life left passes out on its next turn, as its body finds it has no life left.
		if (Locator::creatureFightSystem::has_value())
		{
			Locator::creatureFightSystem::value().LoseFightHealth(object, damage);
		}
		needs = registry.TryGet<CreatureNeeds>(object);
		if (needs == nullptr)
		{
			return 0.0f;
		}
		// The harm sets the life as a script sets it: a creature that a script holds in a cutscene, or an indestructible
		// creature, keeps its life when the harm leaves nearly no life
		const float after = needs->needs.life < damage ? 0.0f : needs->needs.life - damage;
		if (script::property_rules::CanSetLife(after, HeldDuringCutscene(object), registry.AllOf<Indestructible>(object)))
		{
			needs->needs.life = after;
		}
		return needs->needs.life;
	}
	// A building, a worship site or a temple not yet built loses what is built of it rather than life, and all its life
	// once nothing is
	if (auto* progress = registry.TryGet<BuildProgress>(object);
	    progress != nullptr && progress->built < 1.0f && registry.AnyOf<Abode, SpellDispenser, WorshipSite, Temple>(object))
	{
		progress->built = progress->built - damage <= 0.0f ? 0.0f : progress->built - damage;
		if (progress->built != 0.0f)
		{
			return LifeOf(object);
		}
		damage = LifeOf(object);
	}
	const float before = LifeOf(object);
	if (auto* villager = registry.TryGet<Villager>(object))
	{
		villager->life = std::max(villager->life - damage, 0.0f);
		CountInjury(object, before, villager->life);
		return villager->life;
	}
	auto& life = registry.AllOf<ObjectLife>(object) ? registry.Get<ObjectLife>(object)
	                                                : registry.Assign<ObjectLife>(object, ObjectLife {.life = before});
	life.life = std::max(life.life - damage, 0.0f);
	if (registry.AllOf<Abode>(object) && abode_knock::HurtBringsPeopleOut(life.life))
	{
		// Its people come out of a building left under its full life, however little it lost
		BuildingHurt(object, before, life.life);
	}
	// A settled building a player puts out of action counts as destroyed by them
	if (const auto* abode = registry.TryGet<const Abode>(object); abode != nullptr && abode->info != AbodeInfo::None)
	{
		const auto& info = Locator::infoConstants::value().abode.at(static_cast<size_t>(abode->info));
		if (statistics::StopsWorking(before, life.life, info.thresholdForStopBeingFunctional) &&
		    statistics::CountsAsDestroyed(by.has_value(), abode->age))
		{
			player_statistics::CountDestroyed(registry, *by);
		}
		CountSmashed(by, {.lifeBefore = before,
		                  .lifeAfter = life.life,
		                  .stopsWorkingAt = info.thresholdForStopBeingFunctional,
		                  .byPlayer = by.has_value(),
		                  .age = abode->age,
		                  .byCreature = byCreature});
	}
	// A miracle dispenser is a building of its town and counts by the same rule
	if (const auto* dispenser = registry.TryGet<const SpellDispenser>(object); dispenser != nullptr)
	{
		if (const auto* info = AbodeInfoOf(object); info != nullptr)
		{
			if (statistics::StopsWorking(before, life.life, info->thresholdForStopBeingFunctional) &&
			    statistics::CountsAsDestroyed(by.has_value(), dispenser->age))
			{
				player_statistics::CountDestroyed(registry, *by);
			}
			CountSmashed(by, {.lifeBefore = before,
			                  .lifeAfter = life.life,
			                  .stopsWorkingAt = info->thresholdForStopBeingFunctional,
			                  .byPlayer = by.has_value(),
			                  .age = dispenser->age,
			                  .byCreature = byCreature});
		}
	}
	// A built building of a town left under its full life gets a site for its repair, which starts from a little less
	// than the life it is left with, whatever took it there
	if (life.life < 1.0f && IsBuiltBuildingOfATown(registry, object))
	{
		registry.AssignOrReplace<RepairSite>(object, RepairSite {.startLife = physics::damage::RepairStartLife(life.life)});
	}
	// A town's abode left under its full life is given a site by its town, for its builders to mend it
	if (life.life < 1.0f && IsAbodeOfATown(registry, object) && construction::BroughtWoodSite(object) == nullptr)
	{
		construction::OpenSite(object, 0.0f);
	}
	return life.life;
}

bool world_objects::IsBuilding(entt::entity object)
{
	return Locator::entitiesRegistry::value().AnyOf<Abode, SpellDispenser, Field>(object);
}

namespace
{
/// Whether a script owns the widescreen bars of a cutscene
[[nodiscard]] bool ScriptOwnsWideScreen()
{
	if (!Locator::cinematicDirectorSystem::has_value())
	{
		return false;
	}
	const auto& director = Locator::cinematicDirectorSystem::value();
	return director.IsWideScreenOn() && director.GetWideScreenOwner() != 0;
}
} // namespace

bool world_objects::HeldDuringCutscene(entt::entity object)
{
	return Locator::entitiesRegistry::value().AllOf<InScript>(object) && ScriptOwnsWideScreen();
}

bool world_objects::CanBeDestroyedBySpell(entt::entity object, entt::entity spell)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto can = [&registry, object, spell] {
		// Nor anything a script made indestructible
		if (!registry.Valid(object) ||
		    registry.AnyOf<Creature, Field, Temple, TeleportStone, OneOffSpellSeed, Indestructible>(object))
		{
			return false;
		}
		if (const auto* abode = registry.TryGet<const Abode>(object); abode != nullptr && abode->type == AbodeNumber::Totem)
		{
			return false;
		}
		if (const auto* pot = registry.TryGet<const Pot>(object); pot != nullptr && pot->amount == 0)
		{
			return false;
		}
		// A thing that a script holds while its cutscene runs is spared, unless a script controls the miracle
		const bool spellScriptControlled =
		    spell != entt::null && registry.Valid(spell) && registry.AllOf<ScriptControlled>(spell);
		return !magic::spell_sparing::CutsceneSpares(registry.AllOf<InScript>(object), ScriptOwnsWideScreen(),
		                                             spellScriptControlled);
	}();
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "World: #{} {} be destroyed by a miracle", entt::to_integral(object),
	                    can ? "may" : "may not");
	return can;
}

bool world_objects::IsVillager(entt::entity object)
{
	return Locator::entitiesRegistry::value().AllOf<Villager>(object);
}

bool world_objects::IsCreature(entt::entity object)
{
	return Locator::entitiesRegistry::value().AllOf<Creature>(object);
}

void world_objects::Destroy(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object) || IsCreature(object))
	{
		return;
	}
	if (IsBuilding(object))
	{
		// It is left standing with no life, its people out
		ReduceLife(object, LifeOf(object));
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "World: building #{} left standing with no life", entt::to_integral(object));
		return;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "World: #{} destroyed", entt::to_integral(object));
	Remove(object);
}

void world_objects::LeaveGhost(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(object);
	if (transform == nullptr || !Locator::explosionSystem::has_value())
	{
		return;
	}
	auto model = glm::translate(glm::mat4(1.0f), transform->position) * glm::mat4(transform->rotation);
	model = glm::scale(model, transform->scale);
	if (const auto* globe = registry.TryGet<const OneOffSpellSeed>(object))
	{
		// The ghost of a one-shot globe is its dome, turned as the last frame drew it
		Locator::explosionSystem::value().AddGhost(
		    DestructionGhost {.mesh = OneOffSpellSeed::k_MeshId, .model = globe->drawnModel.value_or(model)});
		return;
	}
	if (const auto* mesh = registry.TryGet<const Mesh>(object))
	{
		Locator::explosionSystem::value().AddGhost(DestructionGhost {.mesh = mesh->id, .model = model});
	}
}

void world_objects::KeepForScript(entt::entity building)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (construction::BroughtWoodSite(building) == nullptr)
	{
		// A building of no town stays as it is, its fire too
		if (!IsAbodeOfATown(registry, building))
		{
			return;
		}
		construction::OpenSite(building, 0.0f);
	}
	// A building that is not finished gets all of its life again
	if (!construction::IsBuilt(building))
	{
		SetLife(building, 1.0f);
	}
	if (Locator::fireSystem::has_value())
	{
		Locator::fireSystem::value().Forget(building);
	}
}

void world_objects::DestroyedByEffect(entt::entity object, const EffectDeath& death)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object) || IsCreature(object))
	{
		return;
	}
	if (registry.AllOf<Temple>(object))
	{
		// A temple whose heart loses the last of its life is not removed: its destruction starts, and it goes at its end
		if (Locator::templeDestructionSystem::has_value())
		{
			Locator::templeDestructionSystem::value().Start(object);
		}
		return;
	}
	// A worship site with no life left stays as it is
	if (registry.AllOf<WorshipSite>(object))
	{
		return;
	}
	if (registry.AnyOf<Abode, SpellDispenser>(object))
	{
		// A ghost of it flickers out where it stood, also when it stays
		LeaveGhost(object);
		if (registry.AllOf<InScript>(object))
		{
			KeepForScript(object);
			return;
		}
		// The scaffold of an unfinished building starts a new site there at once, after the building goes
		const auto scaffold =
		    Locator::workshopSystem::has_value() ? Locator::workshopSystem::value().LeaveDestroyedSite(object) : entt::null;
		const auto player = PlayerOf(object).value_or(PlayerNames::NEUTRAL);
		Remove(object);
		if (scaffold != entt::null)
		{
			Locator::workshopSystem::value().ForceBuild(scaffold, player);
		}
		return;
	}
	if (registry.AllOf<Villager>(object))
	{
		// Any effect's death counts as killed by a spell, put down to the effect's player
		villager_fire::DieByEffect(object, villager_fire::SpellDeath(death.killer, death.weight));
		return;
	}
	// An animal falls dead rather than vanishing; a miracle's fades out
	if (registry.AllOf<Animal>(object) && Locator::animalSystem::has_value())
	{
		if (registry.AllOf<SpellAnimal>(object))
		{
			Locator::animalSystem::value().StartFading(object);
		}
		else
		{
			Locator::animalSystem::value().SetDying(object);
		}
		return;
	}
	// A fish farm goes from the land, its fishermen deciding afresh
	if (registry.AllOf<FishFarm>(object) && Locator::fishFarmSystem::has_value())
	{
		Locator::fishFarmSystem::value().RemoveFarm(object);
		return;
	}
	if (auto* field = registry.TryGet<Field>(object))
	{
		// Its crop is gone, and its fire with it
		field->crop.timesSown = 0;
		field->crop.age = 0.0f;
		field->crop.food = 0.0f;
		if (Locator::fireSystem::has_value())
		{
			Locator::fireSystem::value().Forget(object);
		}
		return;
	}
	Destroy(object);
}

void world_objects::Remove(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return;
	}
	if (Locator::fireSystem::has_value())
	{
		Locator::fireSystem::value().Forget(object);
	}
	// A puzzle game that goes leaves its pieces with no puzzle
	if (registry.AllOf<PuzzleGame>(object) && Locator::puzzleGameSystem::has_value())
	{
		Locator::puzzleGameSystem::value().Forget(object);
	}
	if (const auto* villager = registry.TryGet<const Villager>(object))
	{
		villager_home::LeaveHome(object);
		if (villager->abode != entt::null && registry.Valid(villager->abode))
		{
			if (auto* abode = registry.TryGet<Abode>(villager->abode))
			{
				std::erase(abode->inhabitants, object);
			}
		}
	}
	// A storage pit that goes removes its piles. When it is the storage pit of its town, the town looks for another storage
	// pit and lets go of its temporary piles.
	if (const auto* pit = registry.TryGet<const StoragePit>(object))
	{
		std::vector<entt::entity> piles(pit->woodPiles.begin(), pit->woodPiles.end());
		piles.insert(piles.begin(), pit->foodPile);
		const auto& towns = registry.Context().towns;
		const auto* abode = registry.TryGet<const Abode>(object);
		if (const auto town = abode != nullptr ? towns.find(abode->townId) : towns.end();
		    town != towns.end() && construction::StoragePitOf(town->second) == object && Locator::townSystem::has_value())
		{
			Locator::townSystem::value().SetStoragePit(town->second);
		}
		for (const auto pile : piles)
		{
			world_objects::Remove(pile);
		}
	}
	// A workshop that goes removes its wood pile
	if (const auto* workshop = registry.TryGet<const Workshop>(object);
	    workshop != nullptr && registry.Valid(workshop->woodPile))
	{
		world_objects::Remove(workshop->woodPile);
	}
	// A weather thing that goes ends its storm at once
	if (registry.AllOf<WeatherThing>(object) && Locator::weatherSystem::has_value())
	{
		Locator::weatherSystem::value().EndWeatherThing(object);
	}
	// A graveyard going hands its town's dead over to another of the town's graveyards
	graveyards::Lost(object);
	// A crèche going hands its town's children over to another of the town's buildings
	creches::Lost(object);
	// A finished wonder that goes removes its power from its player
	if (Locator::wonderSystem::has_value())
	{
		Locator::wonderSystem::value().Lost(object);
	}
	// A town's building going is planned again by its town, where it stood, and the things it put round itself go. A
	// miracle dispenser is one of the buildings of its town.
	if (registry.AnyOf<Abode, SpellDispenser>(object))
	{
		town_planning::PlanAgain(object);
		abode_surroundings::Delete(object);
	}
	// A building's people are made homeless as it goes, the newest first
	if (const auto* abode = registry.TryGet<const Abode>(object); abode != nullptr && Locator::townSystem::has_value())
	{
		const auto inhabitants = abode->inhabitants;
		for (const auto villager : inhabitants)
		{
			villager_home::HomeDeleted(villager);
		}
	}
	// A building that goes closes each of its sites first, so that the builders stop their work and step away from it
	construction::CloseSite(object);
	const auto leftTown = TownOfBuilding(object);
	registry.Destroy(object);
	registry.SetDirty();
	// The town that the building left goes from the game when it has nothing left
	if (leftTown != entt::null && Locator::townSystem::has_value())
	{
		Locator::townSystem::value().BuildingLeft(leftTown);
	}
}

void world_objects::AttackTown(entt::entity object, float damage, PlayerNames aggressor, const town_aggression::Harm& harm,
                               bool byCreature, std::optional<guidance::TownAttackVoice> cry)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& info = Locator::infoConstants::value();
	entt::entity town = entt::null;
	const GObjectInfo* kind = nullptr;
	if (const auto* villager = registry.TryGet<const Villager>(object))
	{
		town = villager->town;
		kind = InfoOf(object);
	}
	else if (const auto pileTown = TownOfPile(object); pileTown.has_value())
	{
		// A pile is its store's or its building's, and burning it attacks that town as much as its own kind says
		town = *pileTown;
		kind = InfoOf(object);
	}
	else if (const auto* dispenser = registry.TryGet<const SpellDispenser>(object))
	{
		// A miracle dispenser is a building of its town, by its own row
		town = dispenser->town;
		kind = AbodeInfoOf(object);
	}
	else if (const auto* farm = registry.TryGet<const FishFarm>(object))
	{
		// A fish farm is its town's, by its own row
		town = farm->town;
		kind = &info.fishFarm;
	}
	else if (const auto* abode = registry.TryGet<const Abode>(object))
	{
		town = TownOfAbode(*abode);
		if (town != entt::null)
		{
			if (const auto* tribe = registry.TryGet<const Tribe>(town))
			{
				const auto row = static_cast<size_t>(GAbodeInfo::Find(*tribe, abode->type));
				kind = row < info.abode.size() ? &info.abode.at(row) : nullptr;
			}
		}
	}
	if (town == entt::null || !registry.Valid(town) || kind == nullptr)
	{
		return;
	}
	if (!ecs::town_aggression::CountsAsAttack(damage))
	{
		return;
	}
	// The people cry out before the town counts the attack
	if (cry.has_value() && Locator::guidanceSystem::has_value())
	{
		Locator::guidanceSystem::value().TownAttackCry(town, *cry);
	}
	const float amount = ecs::town_aggression::AggressionFromDamage(damage, kind->aggressorValue);
	auto* aggression = registry.TryGet<TownAggression>(town);
	if (aggression == nullptr)
	{
		aggression = &registry.Assign<TownAggression>(town);
	}
	const bool owner = registry.Get<const Town>(town).owner == aggressor;
	const uint32_t turn = Locator::time::has_value() ? static_cast<uint32_t>(Locator::time::value().GetTurn()) : 0;
	ecs::town_aggression::Attacked(aggression->record, aggressor, owner, amount, info.town.firstTimeDamageDoneAddition, turn,
	                               harm);
	// The advisors of the player at this machine may remark on the player attacking a town
	if (Locator::guidanceSystem::has_value())
	{
		Locator::guidanceSystem::value().TownAttacked(town, aggressor, byCreature);
	}
}

void world_objects::CountInjury(entt::entity villager, float before, float after)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* person = registry.TryGet<const Villager>(villager);
	if (person == nullptr || !registry.Valid(person->town))
	{
		return;
	}
	auto* town = registry.TryGet<Town>(person->town);
	if (town == nullptr)
	{
		return;
	}
	const int change = physics::living::InjuredChange(before, after);
	if (change > 0)
	{
		++town->injured;
	}
	else if (change < 0 && town->injured > 0)
	{
		--town->injured;
	}
}

float world_objects::IncreaseLife(entt::entity object, float amount)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return 0.0f;
	}
	// A village totem gives the life to its town centre, and keeps its own life
	if (const auto* totem = registry.TryGet<const VillageTotem>(object); totem != nullptr && registry.Valid(totem->townCentre))
	{
		return IncreaseLife(totem->townCentre, amount);
	}
	const float before = LifeOf(object);
	if (auto* villager = registry.TryGet<Villager>(object))
	{
		villager->life = std::min(villager->life + amount, 1.0f);
		CountInjury(object, before, villager->life);
		return villager->life;
	}
	auto& life = registry.AllOf<ObjectLife>(object) ? registry.Get<ObjectLife>(object)
	                                                : registry.Assign<ObjectLife>(object, ObjectLife {.life = before});
	life.life = std::min(life.life + amount, 1.0f);
	// A storage pit that starts to work again stops its piles calling people
	if (const auto* info = registry.AllOf<Abode>(object) ? world_objects::AbodeInfoOf(object) : nullptr;
	    info != nullptr && info->abodeType == AbodeType::StoragePit && before <= info->thresholdForStopBeingFunctional &&
	    life.life > info->thresholdForStopBeingFunctional && Locator::resourceStoreSystem::has_value())
	{
		Locator::resourceStoreSystem::value().StoreWorksAgain(object);
	}
	return life.life;
}

bool world_objects::CanBeCrushed(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return false;
	}
	return registry.AnyOf<Villager, Creature, Animal, Abode, Tree, DeadTree, Feature, Field, AnimatedStatic, BigForest,
	                      MobileStatic, Flowers, SpellDispenser, WorshipSite, Temple, FishFarm>(object);
}

PlayerNames world_objects::IconPlayer(const Registry& registry, entt::entity icon)
{
	if (const auto* hand = registry.TryGet<const TownCentreIcon>(icon))
	{
		const auto* town = registry.Valid(hand->town) ? registry.TryGet<const Town>(hand->town) : nullptr;
		return town != nullptr ? town->owner : PlayerNames::NEUTRAL;
	}
	if (const auto* site = registry.TryGet<const WorshipIcon>(icon))
	{
		const auto* component = registry.Valid(site->site) ? registry.TryGet<const WorshipSite>(site->site) : nullptr;
		return component != nullptr ? component->player : PlayerNames::NEUTRAL;
	}
	return PlayerNames::NEUTRAL;
}

std::optional<PlayerNames> world_objects::PlayerOf(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return std::nullopt;
	}
	const auto townOwner = [&registry](auto matches) -> std::optional<PlayerNames> {
		std::optional<PlayerNames> owner;
		registry.Each<const Town>([&](entt::entity entity, const Town& town) {
			if (matches(entity, town))
			{
				owner = town.owner;
			}
		});
		return owner;
	};
	if (const auto* creature = registry.TryGet<const Creature>(object))
	{
		return creature->owner;
	}
	if (const auto* town = registry.TryGet<const Town>(object))
	{
		return town->owner;
	}
	if (const auto* ball = registry.TryGet<const MagicFireBall>(object))
	{
		return ball->hasPlayer ? ball->player : PlayerNames::NEUTRAL;
	}
	if (const auto* villager = registry.TryGet<const Villager>(object))
	{
		const auto* town = registry.Valid(villager->town) ? registry.TryGet<const Town>(villager->town) : nullptr;
		return town != nullptr ? std::optional(town->owner) : std::nullopt;
	}
	if (const auto* field = registry.TryGet<const Field>(object))
	{
		return townOwner([field](entt::entity, const Town& town) { return static_cast<int>(town.id) == field->town; });
	}
	if (const auto* abode = registry.TryGet<const Abode>(object))
	{
		return townOwner([abode](entt::entity, const Town& town) { return town.id == abode->townId; })
		    .value_or(PlayerNames::NEUTRAL);
	}
	// A miracle dispenser is its town's player's, and a dispenser of no town is the neutral player's
	if (const auto* dispenser = registry.TryGet<const SpellDispenser>(object))
	{
		const auto* town = registry.Valid(dispenser->town) ? registry.TryGet<const Town>(dispenser->town) : nullptr;
		return town != nullptr ? town->owner : PlayerNames::NEUTRAL;
	}
	// A fish farm is its town's player's, and a farm of no town is no player's
	if (const auto* farm = registry.TryGet<const FishFarm>(object))
	{
		const auto* town = registry.Valid(farm->town) ? registry.TryGet<const Town>(farm->town) : nullptr;
		return town != nullptr ? std::optional(town->owner) : std::nullopt;
	}
	// A temple is its player's
	if (const auto* temple = registry.TryGet<const Temple>(object))
	{
		return temple->owner;
	}
	// A worship site is its temple's player's
	if (const auto* site = registry.TryGet<const WorshipSite>(object))
	{
		return site->player;
	}
	if (const auto* magic = registry.TryGet<const MagicTree>(object))
	{
		const auto* forest = registry.Valid(magic->forest) ? registry.TryGet<const MagicForest>(magic->forest) : nullptr;
		return forest != nullptr ? std::optional(forest->player) : std::nullopt;
	}
	if (registry.AnyOf<Tree, DeadTree>(object))
	{
		return std::nullopt;
	}
	return PlayerNames::NEUTRAL;
}
