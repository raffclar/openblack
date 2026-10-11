/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerDisciples.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/HelpSpeech.h"
#include "Camera/Camera.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Disciple.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HiddenByState.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/DiscipleRules.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/VillagerAge.h"
#include "ECS/WorldObjects.h"
#include "Graphics/Texture2D.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MapSpiral.h"
#include "ObjectMeasures.h"
#include "Resources/ResourcesInterface.h"
#include "VillagerAnimate.h"
#include "VillagerBreeding.h"
#include "VillagerBuild.h"
#include "VillagerFisherman.h"
#include "VillagerForester.h"
#include "VillagerHome.h"
#include "VillagerPhysics.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace villager_disciple = openblack::ecs::villager_disciple;

namespace
{
using enum VillagerDisciple;

/// The guidance's line for a new disciple is played at this volume, at its own pitch
constexpr uint32_t k_MadeVolume = 85;
constexpr uint32_t k_MadePitch = 100;
/// The glow is drawn from the smoke sheet, its puffs eight to a row, the first fifteen played through in half a second
constexpr auto k_GlowSheet = entt::hashed_string("raw/smoke");
constexpr auto k_GlowSheetAlpha = entt::hashed_string("raw/smokea");
constexpr uint32_t k_GlowFrames = 15;
constexpr float k_GlowCycleMs = 500.0f;
/// Half-bright orange, added to what is behind
constexpr glm::vec4 k_GlowTint {193.0f / 255.0f, 129.0f / 255.0f, 25.0f / 255.0f, 128.0f / 255.0f};
/// The symbols are drawn from the leash sheet, eight to a row, as they are
constexpr auto k_SymbolSheet = entt::hashed_string("raw/leash");
constexpr auto k_SymbolSheetAlpha = entt::hashed_string("raw/leasha");
constexpr float k_SheetCells = 8.0f;
/// The symbol stands two and a half of the villager's sizes over its glow, as wide on the screen wherever it is
constexpr float k_SymbolRise = 2.5f;
constexpr float k_SymbolPixels = 16.0f;
/// No sprite is drawn smaller than this
constexpr float k_LeastSpriteSize = 0.0001f;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

entt::entity EntityOf(LivingAction& action)
{
	return Entities().ToEntity(action);
}

void SetTopState(LivingAction& action, VillagerStates state)
{
	Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top, state, false);
}

std::optional<entt::entity> ValidTown(entt::entity town)
{
	const auto& registry = Entities();
	if (town != entt::null && registry.Valid(town) && registry.AllOf<Town>(town))
	{
		return town;
	}
	return std::nullopt;
}

std::optional<entt::entity> TownById(uint32_t id)
{
	std::optional<entt::entity> found;
	Entities().Each<const Town>([&found, id](entt::entity entity, const Town& town) {
		if (town.id == id)
		{
			found = entity;
		}
	});
	return found;
}

std::optional<entt::entity> TownOfVillager(entt::entity villager)
{
	const auto* person = Entities().TryGet<const Villager>(villager);
	return person != nullptr ? ValidTown(person->town) : std::nullopt;
}

/// The town a thing belongs to, if it belongs to one
std::optional<entt::entity> TownOfThing(entt::entity thing)
{
	const auto& registry = Entities();
	if (const auto* field = registry.TryGet<const Field>(thing))
	{
		return field->town >= 0 ? TownById(static_cast<uint32_t>(field->town)) : std::nullopt;
	}
	if (const auto* abode = registry.TryGet<const Abode>(thing))
	{
		return TownById(abode->townId);
	}
	if (const auto* farm = registry.TryGet<const FishFarm>(thing))
	{
		return ValidTown(farm->town);
	}
	if (const auto* site = registry.TryGet<const WorshipSite>(thing); site != nullptr && !site->towns.empty())
	{
		return ValidTown(site->towns.back());
	}
	if (registry.AllOf<Villager>(thing))
	{
		return TownOfVillager(thing);
	}
	return std::nullopt;
}

glm::vec3 PositionOf(entt::entity entity)
{
	return Entities().Get<const Transform>(entity).position;
}

const GVillagerInfo* VillagerInfoOf(const Villager& villager)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& infos = Locator::infoConstants::value().villager;
	const auto row = static_cast<size_t>(GVillagerInfo::Find(villager.tribe, villager.number));
	return row < infos.size() ? &infos.at(row) : nullptr;
}

bool IsSexuallyActive(const Villager& villager)
{
	const auto* info = VillagerInfoOf(villager);
	if (info == nullptr)
	{
		return false;
	}
	const auto age = villager_age::AgeNow(villager);
	return info->startHavingSexAge <= age && age < info->stopHavingSexAge;
}

/// About its life: neither dead nor dying
bool IsFunctionalVillager(entt::entity villager)
{
	const auto& registry = Entities();
	const auto* person = registry.TryGet<const Villager>(villager);
	return person != nullptr && person->life > 0.0f && !registry.AllOf<VillagerDeath>(villager);
}

bool IsBuilt(entt::entity thing)
{
	const auto* progress = Entities().TryGet<const BuildProgress>(thing);
	return progress == nullptr || progress->built >= 1.0f;
}

bool IsRepaired(entt::entity thing)
{
	return world_objects::LifeOf(thing) >= 1.0f;
}

/// How much a kind of thing draws villagers, none for a kind that offers no work or isn't a thing at all
std::optional<float> KindDesire(entt::entity thing)
{
	const auto& registry = Entities();
	if (!Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& info = Locator::infoConstants::value();
	if (registry.AllOf<FishFarm>(thing))
	{
		return info.fishFarm.villagerInteractDesire;
	}
	if (const auto* site = registry.TryGet<const WorshipSite>(thing))
	{
		const auto row = static_cast<size_t>(site->tribe);
		return row < info.worshipSite.size() ? std::optional(info.worshipSite.at(row).villagerInteractDesire) : std::nullopt;
	}
	if (!registry.AnyOf<Abode, Field, Tree, BigForest, Villager>(thing))
	{
		return std::nullopt;
	}
	const auto* row = world_objects::InfoOf(thing);
	return row != nullptr ? std::optional(row->villagerInteractDesire) : std::nullopt;
}

/// The adults living in a home
int64_t AdultsIn(entt::entity abode)
{
	int64_t adults = 0;
	for (const auto person : Entities().Get<const Abode>(abode).inhabitants)
	{
		const auto* villager = Entities().TryGet<const Villager>(person);
		if (villager != nullptr && villager->lifeStage == Villager::LifeStage::Adult)
		{
			++adults;
		}
	}
	return adults;
}

/// A house's room for another adult, as its people over its places for adults
bool HasRoomForAdults(entt::entity abode)
{
	const auto* info = world_objects::AbodeInfoOf(abode);
	if (info == nullptr || info->maxVillagersInAbode == 0)
	{
		return false;
	}
	return static_cast<float>(AdultsIn(abode)) / static_cast<float>(info->maxVillagersInAbode) < 1.0f;
}

/// Room in a town for another villager: room for adults left in its built homes, or below the size a town without
/// homes is allowed when it has none
bool TownHasRoom(entt::entity town)
{
	return Locator::townSystem::has_value() && Locator::townSystem::value().HasRoomForNewcomer(town);
}

/// What the choice reads of a thing held near
disciple::NearFacts NearFactsOf(entt::entity thing, entt::entity held)
{
	const auto& registry = Entities();
	disciple::NearFacts facts;
	const auto town = TownOfThing(thing);
	const auto heldTown = TownOfVillager(held);
	facts.hasTown = town.has_value();
	facts.townHabitable = !town.has_value() || !registry.Get<const Town>(*town).uninhabitable;
	facts.heldVillagersTown = town == heldTown;
	facts.owner = town.has_value() ? std::optional(registry.Get<const Town>(*town).owner) : std::nullopt;
	// A field is a building of its town too, but farmed rather than lived in
	if (registry.AllOf<Field>(thing))
	{
		facts.kind = disciple::NearKind::Field;
		facts.built = IsBuilt(thing);
		return facts;
	}
	if (const auto* abode = registry.TryGet<const Abode>(thing))
	{
		facts.built = IsBuilt(thing);
		facts.repaired = IsRepaired(thing);
		facts.functional = villager_home::IsFunctional(thing);
		switch (abode->type)
		{
		case AbodeNumber::StoragePit:
			facts.kind = disciple::NearKind::StoragePit;
			break;
		case AbodeNumber::TownCentre:
			facts.kind = disciple::NearKind::TownCentre;
			facts.townHasRoom = town.has_value() && TownHasRoom(*town);
			break;
		case AbodeNumber::Workshop:
			facts.kind = disciple::NearKind::Workshop;
			break;
		default:
		{
			const auto* info = world_objects::AbodeInfoOf(thing);
			const bool house = info != nullptr && info->maxVillagersInAbode > 0;
			facts.kind = house ? disciple::NearKind::House : disciple::NearKind::Building;
			facts.roomForAdults = house && HasRoomForAdults(thing);
			const auto* person = registry.TryGet<const Villager>(held);
			facts.heldVillagersHome = person != nullptr && person->abode == thing;
			break;
		}
		}
		return facts;
	}
	if (const auto* site = registry.TryGet<const WorshipSite>(thing))
	{
		facts.kind = disciple::NearKind::WorshipSite;
		facts.owner = site->player;
		facts.built = IsBuilt(thing);
		facts.repaired = IsRepaired(thing);
		facts.heldVillagersWorshipSite = heldTown.has_value() && registry.Get<const Town>(*heldTown).worshipSite == thing;
		return facts;
	}
	if (registry.AllOf<FishFarm>(thing))
	{
		facts.kind = disciple::NearKind::FishFarm;
		return facts;
	}
	if (registry.AllOf<BigForest>(thing))
	{
		facts.kind = disciple::NearKind::Forest;
		return facts;
	}
	if (registry.AllOf<Tree>(thing))
	{
		facts.kind = disciple::NearKind::Tree;
		return facts;
	}
	if (const auto* other = registry.TryGet<const Villager>(thing))
	{
		facts.kind = disciple::NearKind::Villager;
		facts.villagerFunctional = IsFunctionalVillager(thing);
		facts.villagerSexuallyActive = IsSexuallyActive(*other);
		facts.villagerMale = other->sex == Villager::Sex::MALE;
	}
	return facts;
}

/// What the choice reads of the held villager and the hand
disciple::HeldFacts HeldFactsOf(entt::entity villager, std::optional<PlayerNames> hand)
{
	const auto& registry = Entities();
	const auto& person = registry.Get<const Villager>(villager);
	const auto town = TownOfVillager(villager);
	return {
	    .hand = hand,
	    .villager = town.has_value() ? std::optional(registry.Get<const Town>(*town).owner) : std::nullopt,
	    .sexuallyActive = IsSexuallyActive(person),
	    .male = person.sex == Villager::Sex::MALE,
	    .child = person.lifeStage == Villager::LifeStage::Child,
	    .otherwiseRefused = registry.AllOf<ScriptControlled>(villager) || !IsFunctionalVillager(villager),
	};
}

/// The things about a villager worth looking at, the most interesting first: those of its kinds that offer work, in the
/// nine map cells round it
std::vector<entt::entity> ThingsNear(entt::entity villager)
{
	std::vector<entt::entity> found;
	std::vector<float> interests;
	if (!Locator::entitiesMap::has_value())
	{
		return found;
	}
	const auto& registry = Entities();
	const auto position = PositionOf(villager);
	const float ownRadius = systems::object_measures::TwoDRadius(registry, villager);
	// The map's cells don't keep the fish farms, which are few: each is looked at with the things of its cell
	std::vector<std::pair<glm::ivec2, entt::entity>> farms;
	registry.Each<const FishFarm, const Transform>([&farms](entt::entity farm, const FishFarm&, const Transform& transform) {
		farms.emplace_back(map_coords::CellOf(transform.position), farm);
	});
	for (const auto& cell : magic::SpiralCells(map_coords::CellOf(position), disciple::k_CellsSearched))
	{
		auto inCell = Locator::entitiesMap::value().GetAllInCell(cell);
		for (const auto& [farmCell, farm] : farms)
		{
			if (farmCell == cell)
			{
				inCell.insert(inCell.begin(), farm);
			}
		}
		for (const auto thing : inCell)
		{
			if (thing == villager || !registry.Valid(thing) || std::ranges::find(found, thing) != found.end())
			{
				continue;
			}
			// Only things that are about count
			if (registry.AllOf<Villager>(thing) && !IsFunctionalVillager(thing))
			{
				continue;
			}
			const auto desire = KindDesire(thing);
			const auto* transform = registry.TryGet<const Transform>(thing);
			if (!desire.has_value() || transform == nullptr)
			{
				continue;
			}
			const float gap = gutils::GetDistanceInMetres(glm::xz(position), glm::xz(transform->position)) -
			                  (ownRadius + systems::object_measures::TwoDRadius(registry, thing));
			found.push_back(thing);
			interests.push_back(disciple::Interest(gap, *desire));
		}
	}
	std::vector<entt::entity> ordered;
	for (const auto index : disciple::OrderByInterest(interests))
	{
		ordered.push_back(found.at(index));
	}
	return ordered;
}

std::optional<PlayerNames> LocalPlayer()
{
	return Locator::playerSystem::has_value() ? std::optional(Locator::playerSystem::value().GetLocalPlayer()) : std::nullopt;
}

/// Making a disciple moves the player's alignment by what its town wants of the work
void ConsiderAlignment(entt::entity villager, PlayerNames player, VillagerDisciple type)
{
	const auto town = TownOfVillager(villager);
	const auto kind = disciple::KindOf(type);
	if (!town.has_value() || !Locator::infoConstants::has_value() || !Locator::alignmentSystem::has_value())
	{
		return;
	}
	std::optional<float> desire;
	if (kind.desire.has_value() && Locator::townDesireSystem::has_value())
	{
		desire = Locator::townDesireSystem::value().GetDesire(*town, *kind.desire);
	}
	const auto& info = Locator::infoConstants::value().town;
	const float change = disciple::AlignmentForMaking(desire, info.consideredEvilIfMakingDisciplesAndDesireIsLessThan,
	                                                  info.consideredGoodIfMakingDisciplesAndDesireIsGreaterThan,
	                                                  info.alignmentChangePerDisciple);
	auto& alignment = Locator::alignmentSystem::value();
	alignment.AddPendingAlignment(player, disciple::DampedAlignment(alignment.GetPlayerAlignment(player), change));
}

/// Walks up to a thing, unless already near its edge, and looks at it
void SetupInspectObject(LivingAction& action, entt::entity thing)
{
	const auto villager = EntityOf(action);
	auto& registry = Entities();
	registry.AssignOrReplace<DiscipleInspect>(villager, DiscipleInspect {.thing = thing});
	const auto there = glm::xz(PositionOf(thing));
	const auto here = glm::xz(PositionOf(villager));
	const float radius = systems::object_measures::TwoDRadius(registry, thing);
	if (gutils::GetDistanceInMetres(here, there) - radius > disciple::k_InspectReach)
	{
		const float angle = gutils::Get3DAngleFromXZ(there, here);
		const auto step = gutils::GetPosFromAngle(angle, radius + disciple::k_InspectReach);
		villager_home::SetupMoveTo(action, there + glm::vec2(map_coords::ToMetres(step.x), map_coords::ToMetres(step.z)),
		                           VillagerStates::InspectObject);
		return;
	}
	SetTopState(action, VillagerStates::InspectObject);
}

/// Where an idle disciple of a town waits, none for one of no town
std::optional<glm::vec2> WaitPosition(entt::entity villager)
{
	const auto town = TownOfVillager(villager);
	if (!town.has_value())
	{
		return std::nullopt;
	}
	auto& registry = Entities();
	for (const auto abode : registry.Get<const Town>(*town).abodes)
	{
		const auto* building = registry.Valid(abode) ? registry.TryGet<const Abode>(abode) : nullptr;
		if (building == nullptr || building->type != AbodeNumber::TownCentre)
		{
			continue;
		}
		const auto centre = glm::xz(PositionOf(abode));
		const auto step = disciple::WaitStepFromCentre(gutils::Get3DAngleFromXZ(centre, glm::xz(PositionOf(villager))),
		                                               systems::object_measures::TwoDRadius(registry, abode), [](float limit) {
			                                               return Locator::gameRandom::value().GameFloatRand(limit);
		                                               });
		const auto offset = gutils::GetPosFromAngle(step.radians, step.metres);
		return centre + glm::vec2(map_coords::ToMetres(offset.x), map_coords::ToMetres(offset.z));
	}
	return glm::xz(PositionOf(*town));
}

/// With no work to be had, it walks to where idle disciples wait
bool SetDiscipleNothingToDo(LivingAction& action)
{
	const auto place = WaitPosition(EntityOf(action));
	if (!place.has_value())
	{
		return false;
	}
	action.turnsUntilStateChange = 0;
	villager_home::SetupMoveTo(action, *place, VillagerStates::DiscipleNothingToDo);
	return true;
}

/// The disciple's own work, when there is some to be had now
bool SetAboutWork(LivingAction& action, VillagerDisciple type)
{
	switch (type)
	{
	case Builder:
		return villager_build::CheckNeededForBuilding(action) == 1;
	case ChangeHouse:
	{
		// It stops being a disciple as it sets about moving into the building nearest it, or failing that the
		// building's town, then goes to its new home
		const auto villager = EntityOf(action);
		villager_disciple::SetType(villager, None);
		auto& towns = Locator::townSystem::value();
		if (!towns.MoveInByBuilding(towns.NearestBuilding(glm::xz(PositionOf(villager))), villager))
		{
			return false;
		}
		// TODO(disciples): moved to another player's town, the new player counts a villager converted (a multiplayer
		// win condition)
		SetTopState(action, VillagerStates::GoHomeAndChange);
		return true;
	}
	case FromVortex:
		SetTopState(action, VillagerStates::ScriptInCrowd);
		villager_disciple::SetType(EntityOf(action), None);
		return true;
	case Forester:
		return villager_forester::CheckSatisfyWoodDesire(action) == 1;
	case Fisherman:
		return villager_fisherman::FishermanLookForWater(action) == 1;
	case Breeder:
		return villager_breeding::SetupBreederDisciple(action);
	case Worship:
		SetTopState(action, VillagerStates::GotoWorshipSiteForWorship);
		return true;
	// TODO(disciples): the other kinds' work isn't in openblack yet (see the handover): farming for farmers, supplying
	// workshops and trading. Until it is they find none and wait by the town centre; a missionary lives as an ordinary
	// villager of its town.
	default:
		return false;
	}
}

/// Whether the living action system has a state's work: the states a disciple lands into that aren't yet there are
/// left for deciding afresh
bool LandingStateIsThere(VillagerStates state)
{
	switch (state)
	{
	case VillagerStates::DecideWhatToDo:
	case VillagerStates::ScriptInCrowd:
	case VillagerStates::ForesterArrivesAtForest:
	case VillagerStates::BreederJustLanded:
	case VillagerStates::GotoWorshipSiteForWorship:
		return true;
	default:
		return false;
	}
}

/// The first person in the player's hand, or the one under the hand when it holds no one
std::optional<entt::entity> PersonShown()
{
	auto& registry = Entities();
	std::optional<entt::entity> shown;
	registry.Each<const HandGrab>([&shown, &registry](const HandGrab& grab) {
		if (grab.object != entt::null && registry.Valid(grab.object) && registry.AllOf<Villager, Disciple>(grab.object))
		{
			shown = grab.object;
		}
	});
	if (!shown.has_value() && Locator::pickingSystem::has_value())
	{
		const auto hovered = Locator::pickingSystem::value().GetPick().object;
		if (hovered.has_value() && registry.Valid(*hovered) && registry.AllOf<Villager, Disciple>(*hovered))
		{
			shown = hovered;
		}
	}
	return shown;
}

entt::entity MakeMark(entt::entity villager, bool symbol)
{
	auto& registry = Entities();
	const auto& textures = Locator::resources::value().GetTextures();
	const auto sheet = symbol ? k_SymbolSheet : k_GlowSheet;
	const auto alpha = symbol ? k_SymbolSheetAlpha : k_GlowSheetAlpha;
	const auto entity = registry.Create();
	registry.Assign<Sprite>(entity, Sprite {.texture = textures.Handle(sheet)->GetNativeHandle(),
	                                        .uvMin = glm::vec2(0.0f),
	                                        .uvExtent = glm::vec2(1.0f / k_SheetCells),
	                                        .tint = symbol ? glm::vec4(1.0f) : k_GlowTint,
	                                        .additive = !symbol,
	                                        .facesCamera = true,
	                                        .alpha = textures.Handle(alpha)->GetNativeHandle()});
	registry.Assign<Transform>(entity, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(0.0f));
	registry.Assign<DiscipleMark>(entity, DiscipleMark {.villager = villager, .symbol = symbol});
	return entity;
}

glm::vec2 CellUv(uint32_t cell)
{
	return {static_cast<float>(cell % 8) / k_SheetCells, static_cast<float>(cell / 8) / k_SheetCells};
}
} // namespace

VillagerDisciple villager_disciple::TypeOf(entt::entity villager)
{
	const auto* disciple = Entities().TryGet<const Disciple>(villager);
	return disciple != nullptr ? disciple->type : None;
}

void villager_disciple::SetType(entt::entity villager, VillagerDisciple type, entt::entity tradeTown)
{
	auto& registry = Entities();
	if (type == None)
	{
		registry.Remove<Disciple>(villager);
		return;
	}
	registry.AssignOrReplace<Disciple>(villager, Disciple {.type = type, .tradeTown = tradeTown});
}

void villager_disciple::PickedUp(entt::entity villager)
{
	Entities().AssignOrReplace<DiscipleHeld>(villager, DiscipleHeld {.atPickUp = disciple::AtPickUp(TypeOf(villager))});
}

void villager_disciple::LetGo(entt::entity villager, std::optional<PlayerNames> player, bool forced)
{
	if (!forced || !Entities().AllOf<Villager>(villager))
	{
		return;
	}
	const auto town = TownOfVillager(villager);
	const auto owner = town.has_value() ? std::optional(Entities().Get<const Town>(*town).owner) : std::nullopt;
	if (!owner.has_value() || owner == player)
	{
		SetType(villager, None);
	}
}

void villager_disciple::PutDown(entt::entity villager, std::optional<PlayerNames> player)
{
	const auto* held = Entities().TryGet<const DiscipleHeld>(villager);
	const auto local = LocalPlayer();
	if (held == nullptr || !player.has_value() || player != local || held->atPickUp == TypeOf(villager))
	{
		return;
	}
	if (!Locator::helpSpeechSystem::has_value() || !Locator::audio::has_value() || !Locator::alignmentSystem::has_value())
	{
		return;
	}
	const float alignment = Locator::alignmentSystem::value().GetPlayerAlignment(*player);
	const auto sample = disciple::MadeSample(TypeOf(villager), alignment, Locator::gameRandom::value().LocalFloatRand(1.0f));
	if (!sample.has_value())
	{
		return;
	}
	const auto sound = Locator::helpSpeechSystem::value().GetTable().FindSound(audio::SpeechBank::Guidance, *sample);
	SPDLOG_LOGGER_DEBUG(spdlog::get("ai"), "Villager #{}: made a disciple, guidance line {} ({})", entt::to_integral(villager),
	                    *sample, sound.has_value() ? "found" : "missing");
	if (sound.has_value() && Locator::resources::value().GetSounds().Contains(*sound))
	{
		Locator::audio::value().StartSoundEffect(*sound, {.pitchPercent = k_MadePitch, .volume = k_MadeVolume});
	}
}

uint32_t villager_disciple::InHand(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& registry = Entities();
	const auto* held = registry.TryGet<const DiscipleHeld>(villager);
	if (held == nullptr)
	{
		return 1;
	}
	const auto things = ThingsNear(villager);
	std::vector<disciple::NearFacts> facts;
	facts.reserve(things.size());
	for (const auto thing : things)
	{
		facts.push_back(NearFactsOf(thing, villager));
	}
	const auto choice = disciple::ChooseWhileHeld(facts, HeldFactsOf(villager, LocalPlayer()), held->atPickUp);
	entt::entity tradeTown = entt::null;
	if (choice.type == Trader && choice.from.has_value())
	{
		tradeTown = TownOfThing(things.at(*choice.from)).value_or(entt::null);
	}
	else if (const auto* disciple = registry.TryGet<const Disciple>(villager); disciple != nullptr && !choice.from)
	{
		tradeTown = disciple->tradeTown;
	}
	SetType(villager, choice.type, tradeTown);
	return 1;
}

void villager_disciple::TouchDown(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& registry = Entities();
	auto* held = registry.TryGet<DiscipleHeld>(villager);
	if (held == nullptr)
	{
		return;
	}
	const auto type = TypeOf(villager);
	const auto atPickUp = held->atPickUp;
	// Its first turn down: the player who put it down is judged for a new disciple
	if (!held->touchedDown)
	{
		held->touchedDown = true;
		if (const auto player = villager_physics::DropperOf(villager); player.has_value() && type != None && type != atPickUp)
		{
			ConsiderAlignment(villager, *player, type);
		}
	}
}

bool villager_disciple::SetAboutWorkOnLanding(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& registry = Entities();
	if (!registry.AllOf<DiscipleHeld>(villager))
	{
		return false;
	}
	registry.Remove<DiscipleHeld>(villager);
	const auto type = TypeOf(villager);
	if (type == None)
	{
		return false;
	}
	const auto things = ThingsNear(villager);
	if (things.empty())
	{
		return false;
	}
	// TODO(disciples): a farmer, builder, breeder or craftsman put by a thing of another town moves into that town;
	// villagers don't change towns yet
	const auto kind = disciple::KindOf(type);
	if (kind.landingState == VillagerStates::InvalidState)
	{
		SetupInspectObject(action, things.front());
	}
	else if (LandingStateIsThere(kind.landingState))
	{
		SetTopState(action, kind.landingState);
	}
	else
	{
		SetTopState(action, VillagerStates::DecideWhatToDo);
	}
	return true;
}

uint32_t villager_disciple::DecideAsDisciple(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto type = TypeOf(villager);
	if (type == None)
	{
		return 0;
	}
	if (SetAboutWork(action, type))
	{
		// TODO(disciples): others of its town about it may follow it to the same work once (the dropped-by-hand
		// reaction), not yet in openblack
		return 1;
	}
	if (type == Missionary)
	{
		return 0;
	}
	const auto final = Locator::livingActionSystem::value().VillagerGetFinalState(action);
	if (final != VillagerStates::DiscipleNothingToDo && TypeOf(villager) != None && disciple::KindOf(type).waitsWhenIdle &&
	    SetDiscipleNothingToDo(action))
	{
		return 1;
	}
	return 0;
}

uint32_t villager_disciple::InspectObject(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& registry = Entities();
	const auto* inspect = registry.TryGet<const DiscipleInspect>(villager);
	const auto thing = inspect != nullptr && registry.Valid(inspect->thing) ? inspect->thing : entt::null;
	if (thing != entt::null && !villager_build::FaceTowards(villager, glm::xz(PositionOf(thing)), 0x80))
	{
		return 1;
	}
	if (villager_animate::IsReadyForNewAnimation(action))
	{
		registry.Remove<DiscipleInspect>(villager);
		if (TypeOf(villager) == Builder && thing != entt::null && world_objects::IsBuilding(thing) &&
		    villager_build::TakeUpSite(action, thing) == 1)
		{
			return 1;
		}
		SetTopState(action, VillagerStates::DecideWhatToDo);
	}
	return 1;
}

uint32_t villager_disciple::DiscipleNothingToDo(LivingAction& action)
{
	// TODO(disciples): when its town's wants change it looks again within ten turns; openblack keeps no such change
	if (action.turnsUntilStateChange > 0)
	{
		--action.turnsUntilStateChange;
	}
	if (action.turnsUntilStateChange > 0)
	{
		return 1;
	}
	if (!villager_animate::IsReadyForNewAnimation(action))
	{
		action.turnsUntilStateChange = 1;
		return 1;
	}
	if (DecideAsDisciple(action) == 0)
	{
		action.turnsUntilStateChange = static_cast<uint16_t>(disciple::k_IdleRetryTurns);
	}
	return 1;
}

bool villager_disciple::EnterDiscipleNothingToDo(LivingAction& action, VillagerStates, VillagerStates)
{
	// It faces the town centre
	const auto villager = EntityOf(action);
	const auto town = TownOfVillager(villager);
	if (!town.has_value())
	{
		return true;
	}
	auto& registry = Entities();
	for (const auto abode : registry.Get<const Town>(*town).abodes)
	{
		const auto* building = registry.Valid(abode) ? registry.TryGet<const Abode>(abode) : nullptr;
		if (building != nullptr && building->type == AbodeNumber::TownCentre)
		{
			villager_build::FaceTowards(villager, glm::xz(PositionOf(abode)), 0x100);
			break;
		}
	}
	return true;
}

void villager_disciple::TownClaimed(entt::entity town)
{
	auto& registry = Entities();
	if (!registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return;
	}
	for (const auto abode : registry.Get<const Town>(town).abodes)
	{
		const auto* building = registry.Valid(abode) ? registry.TryGet<const Abode>(abode) : nullptr;
		if (building == nullptr)
		{
			continue;
		}
		for (const auto person : building->inhabitants)
		{
			if (registry.Valid(person) && disciple::KindOf(TypeOf(person)).waitsWhenIdle)
			{
				SetType(person, None);
			}
		}
	}
}

void villager_disciple::UpdateMarks()
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	auto& registry = Entities();
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_GlowSheet.value()) || !textures.Contains(k_GlowSheetAlpha.value()) ||
	    !textures.Contains(k_SymbolSheet.value()) || !textures.Contains(k_SymbolSheetAlpha.value()))
	{
		return;
	}
	// The puffs turn with the game's time
	float clock = 0.0f;
	if (Locator::time::has_value())
	{
		const auto& time = Locator::time::value();
		constexpr auto k_TurnMs = static_cast<float>(systems::TimeSystemInterface::k_TurnDuration.count());
		const double gameMs = (static_cast<double>(time.GetTurn()) + static_cast<double>(time.GetTurnFraction())) * k_TurnMs;
		clock = static_cast<float>(std::fmod(gameMs, static_cast<double>(k_GlowCycleMs)));
	}
	const auto frame = static_cast<uint32_t>(static_cast<float>(k_GlowFrames) / k_GlowCycleMs * clock);

	// Marks of villagers gone or no longer glowing go
	std::vector<entt::entity> gone;
	std::optional<entt::entity> symbolMark;
	registry.Each<const DiscipleMark>([&](entt::entity mark, const DiscipleMark& of) {
		if (of.symbol)
		{
			symbolMark = mark;
			return;
		}
		if (!registry.Valid(of.villager) || !disciple::KindOf(TypeOf(of.villager)).glows)
		{
			gone.push_back(mark);
		}
	});
	for (const auto mark : gone)
	{
		registry.Destroy(mark);
	}
	std::vector<entt::entity> glowing;
	registry.Each<const Disciple>([&glowing](entt::entity villager, const Disciple& disciple) {
		if (disciple::KindOf(disciple.type).glows)
		{
			glowing.push_back(villager);
		}
	});
	std::vector<entt::entity> marked;
	registry.Each<const DiscipleMark>([&marked](const DiscipleMark& of) {
		if (!of.symbol)
		{
			marked.push_back(of.villager);
		}
	});
	for (const auto villager : glowing)
	{
		if (std::ranges::find(marked, villager) == marked.end())
		{
			MakeMark(villager, false);
		}
	}

	// Each glow over its villager's head, the size of the villager, its puffs turning; none while the villager is out of
	// sight
	registry.Each<const DiscipleMark, Sprite, Transform>(
	    [&registry, frame](const DiscipleMark& of, Sprite& sprite, Transform& transform) {
		    if (of.symbol)
		    {
			    return;
		    }
		    const auto& body = registry.Get<const Transform>(of.villager);
		    const float size = std::max(body.scale.x, k_LeastSpriteSize);
		    const bool hidden = registry.AnyOf<HiddenByState>(of.villager);
		    transform.position = body.position + glm::vec3(0.0f, body.scale.x, 0.0f);
		    transform.scale = hidden ? glm::vec3(0.0f) : glm::vec3(size, size, 1.0f);
		    sprite.uvMin = CellUv(frame);
	    });

	// The symbol of the disciple in the hand or under it, over its glow
	const auto shown = PersonShown();
	const auto cell =
	    shown.has_value() && disciple::KindOf(TypeOf(*shown)).glows ? disciple::SymbolCell(TypeOf(*shown)) : std::nullopt;
	if (!cell.has_value())
	{
		if (symbolMark.has_value())
		{
			registry.Get<Transform>(*symbolMark).scale = glm::vec3(0.0f);
		}
		return;
	}
	const auto mark = symbolMark.value_or(entt::null) != entt::null ? *symbolMark : MakeMark(*shown, true);
	auto& transform = registry.Get<Transform>(mark);
	const auto& body = registry.Get<const Transform>(*shown);
	transform.position = body.position + glm::vec3(0.0f, body.scale.x + (body.scale.x * k_SymbolRise), 0.0f);
	float size = k_LeastSpriteSize;
	if (Locator::camera::has_value() && Locator::windowing::has_value())
	{
		const float halfWidth = static_cast<float>(Locator::windowing::value().GetSize().x) * 0.5f;
		const float distance = glm::distance(Locator::camera::value().GetOrigin(), transform.position);
		size = std::max(distance * (k_SymbolPixels / std::max(halfWidth, 1.0f)) * 0.5f, k_LeastSpriteSize);
	}
	transform.scale = glm::vec3(size, size, 1.0f);
	registry.Get<Sprite>(mark).uvMin = CellUv(*cell);
	registry.Get<DiscipleMark>(mark).villager = *shown;
}
