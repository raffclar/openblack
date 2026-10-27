/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "WorkshopSystem.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Audio/GameSoundEffects.h"
#include "Audio/Sound.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Common/PortableRotations.h"
#include "Common/X87Angle.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/ScaffoldArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HandStillness.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Workshop.h"
#include "ECS/DiscipleWorkRules.h"
#include "ECS/Map.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "ECS/ScaffoldGhostRules.h"
#include "ECS/ScaffoldImpressionRules.h"
#include "ECS/ScaffoldOfferRules.h"
#include "ECS/ScriptFind.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"
#include "ECS/Systems/Implementations/VillagerHome.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/TownHomes.h"
#include "ECS/TownPlanning.h"
#include "ECS/TownTurnOrder.h"
#include "ECS/WorkshopRules.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/ResourcePiles.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace workshop = openblack::ecs::workshop;
namespace scaffold_offer = openblack::ecs::scaffold_offer;

namespace
{

/// A town that is near enough to a scaffold builds with it, in metres
constexpr float k_TownReach = 50.0f;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

uint32_t Turn()
{
	return Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
}

float GroundAt(glm::vec2 point)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
}

/// A place on the map as the game keeps it: on the ground, at a position of the map's fixed grid
glm::vec3 OnMap(const map_coords::MapCoords& coords)
{
	const glm::vec2 metres {map_coords::ToMetres(coords.x), map_coords::ToMetres(coords.z)};
	return {metres.x, GroundAt(metres), metres.y};
}

map_coords::MapCoords CoordsOf(glm::vec3 position)
{
	return map_coords::FromMetres(glm::vec2(position.x, position.z));
}

/// The matrix of a special point of a building's model, and the building's matrix; none when the model has no such point
std::optional<std::pair<glm::mat4, glm::mat4>> PointAndBuilding(entt::entity building, size_t point)
{
	const auto& registry = Entities();
	const auto* transform = registry.TryGet<const Transform>(building);
	const auto* mesh = registry.TryGet<const Mesh>(building);
	if (transform == nullptr || mesh == nullptr || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return std::nullopt;
	}
	const auto& points = meshes.Handle(mesh->id)->GetExtraMetrics();
	if (point >= points.size())
	{
		return std::nullopt;
	}
	return std::pair {points.at(point), workshop::BuildingMatrix(ecs::placed_turn::AnglesOf(registry, building),
	                                                             ecs::placed_turn::CanLean(registry, building),
	                                                             transform->scale.x, transform->position)};
}

std::optional<workshop::SpecialPlace> SpecialPlaceOf(entt::entity building, size_t point)
{
	const auto matrices = PointAndBuilding(building, point);
	if (!matrices.has_value())
	{
		return std::nullopt;
	}
	return workshop::SpecialPlaceOf(matrices->first, matrices->second);
}

/// The town's workshops in the town's order, the newest first
std::vector<entt::entity> WorkshopsOf(entt::entity town)
{
	const auto& registry = Entities();
	std::vector<entt::entity> workshops;
	if (const auto* data = registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr)
	{
		for (const auto abode : data->abodes)
		{
			if (registry.Valid(abode) && registry.AllOf<Workshop>(abode))
			{
				workshops.push_back(abode);
			}
		}
	}
	std::ranges::reverse(workshops);
	return workshops;
}

void MarkDesiresChanged(entt::entity town)
{
	if (town != entt::null && Locator::townSystem::has_value())
	{
		Locator::townSystem::value().MarkDesiresChanged(town);
	}
}

/// A puff at a scaffold shows that it starts no building
void Puff(glm::vec3 position)
{
	if (Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().StartSpotVisual(SpotVisualType::SpellFailCast, position, std::nullopt, entt::null,
		                                                 1.0f);
	}
}

/// The tribe of a town, none for what is not a town
std::optional<Tribe> TribeOf(entt::entity town)
{
	const auto& registry = Entities();
	if (const auto* tribe = registry.Valid(town) ? registry.TryGet<const Tribe>(town) : nullptr)
	{
		return *tribe;
	}
	return std::nullopt;
}

} // namespace

WorkshopSystem::WorkshopSystem()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Entities();
	_connections.emplace_back(registry.OnDestroy<Scaffold>().connect<&WorkshopSystem::OnScaffoldGone>(*this));
	_connections.emplace_back(registry.OnDestroy<Workshop>().connect<&WorkshopSystem::OnWorkshopGone>(*this));
}

void WorkshopSystem::ProcessWorkshop(entt::entity workshopEntity)
{
	auto& registry = Entities();
	auto* data = registry.TryGet<Workshop>(workshopEntity);
	if (data == nullptr || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& info = Locator::infoConstants::value();
	const uint32_t wood = Locator::resourceStoreSystem::has_value()
	                          ? Locator::resourceStoreSystem::value().GetResource(workshopEntity, ResourceType::Wood)
	                          : 0;
	const workshop::TurnFacts facts {
	    .functional = ecs::villager_home::IsFunctional(workshopEntity),
	    .room = disciple_work::WorkshopRoom(data->makingTurns != 0, data->counted),
	    .wood = wood,
	    .scaffoldWood = info.scaffold.woodValue,
	};
	switch (workshop::NextEvent(data->makingTurns, facts))
	{
	case workshop::TurnEvent::Starts:
		StartScaffold(workshopEntity);
		break;
	case workshop::TurnEvent::Finishes:
		FinishScaffold(workshopEntity);
		break;
	case workshop::TurnEvent::None:
		break;
	}
	// The workshop looks only at the newest scaffold of its list: a scaffold names no next scaffold, thus the search
	// stops after the first one
	if (auto* after = registry.TryGet<Workshop>(workshopEntity); after != nullptr && !after->scaffolds.empty())
	{
		const auto newest = after->scaffolds.front();
		const auto* scaffold = registry.Valid(newest) ? registry.TryGet<const Scaffold>(newest) : nullptr;
		if (scaffold != nullptr &&
		    !workshop::KeepsScaffold(scaffold->putDownTurn, Turn(), info.scaffold.gameTurnsAfterPlacingCanStillPickUp,
		                             scaffold->scriptBuilding.has_value()))
		{
			ReleaseScaffold(newest);
		}
	}
	// TODO(scaffolds): the flag beside the workshop shows the wood that it still needs for a scaffold
}

void WorkshopSystem::StartScaffold(entt::entity workshopEntity)
{
	auto& registry = Entities();
	const auto& info = Locator::infoConstants::value();
	auto& stores = Locator::resourceStoreSystem::value();
	// The workshop asks again for the wood of a scaffold before it uses it
	if (stores.GetResource(workshopEntity, ResourceType::Wood) < info.scaffold.woodValue)
	{
		return;
	}
	stores.RemoveResource(workshopEntity, ResourceType::Wood, info.scaffold.woodValue);
	const auto* abodeInfo = ecs::world_objects::AbodeInfoOf(workshopEntity);
	registry.Get<Workshop>(workshopEntity).makingTurns =
	    abodeInfo != nullptr ? workshop::MakingTurns(abodeInfo->timeEachMobileObjectTakesToProduce) : 0;
	// The sound of the work loops at the workshop until the scaffold is finished
	if (Locator::soundTagSystem::has_value())
	{
		Locator::soundTagSystem::value().AttachObjectSound(workshopEntity,
		                                                   static_cast<entt::id_type>(audio::SoundId::G_Workshop), true);
	}
}

void WorkshopSystem::FinishScaffold(entt::entity workshopEntity)
{
	auto& registry = Entities();
	auto& data = registry.Get<Workshop>(workshopEntity);
	const auto place = workshop::FirstFreePlace(data.places);
	const auto special = SpecialPlaceOf(workshopEntity, workshop::k_PlacePoints.at(place));
	const auto coords =
	    special.has_value() ? special->coords : CoordsOf(registry.Get<const Transform>(workshopEntity).position);
	const float yAngle = special.has_value() ? special->angles.y : 0.0f;
	const auto town = ecs::town_homes::TownOfAbode(registry, workshopEntity);
	// A new scaffold is worth one scaffold
	const auto scaffold = CreateScaffold(OnMap(coords), town, workshopEntity, yAngle, workshop::ScaleOfValue(1));
	if (scaffold != entt::null)
	{
		auto& after = registry.Get<Workshop>(workshopEntity);
		workshop::AddScaffold(after, scaffold);
		registry.Get<Scaffold>(scaffold).place = place;
		after.places.at(place) = Workshop::Place::Holds;
		// A horn sounds at the workshop
		audio::StartGameSoundEffect(
		    static_cast<entt::id_type>(audio::SoundId::G_ScaffoldReady_01),
		    {.position = registry.Get<const Transform>(workshopEntity).position, .owner = workshopEntity});
	}
	registry.Get<Workshop>(workshopEntity).makingTurns = 0;
	if (Locator::soundTagSystem::has_value())
	{
		Locator::soundTagSystem::value().DetachObjectSound(workshopEntity,
		                                                   static_cast<entt::id_type>(audio::SoundId::G_Workshop));
	}
	registry.SetDirty();
}

entt::entity WorkshopSystem::WoodPileOf(entt::entity workshopEntity)
{
	auto& registry = Entities();
	auto* data = registry.Valid(workshopEntity) ? registry.TryGet<Workshop>(workshopEntity) : nullptr;
	if (data == nullptr)
	{
		return entt::null;
	}
	if (registry.Valid(data->woodPile) && registry.AllOf<Pot>(data->woodPile))
	{
		return data->woodPile;
	}
	if (!Locator::infoConstants::has_value())
	{
		return entt::null;
	}
	// The pile stands at the special point of the model with the height of the point, else at the workshop. It is not
	// turned.
	glm::vec3 point = registry.Get<const Transform>(workshopEntity).position;
	if (const auto matrices = PointAndBuilding(workshopEntity, workshop::k_WoodPilePoint))
	{
		point = workshop::SpecialPointOf(matrices->first, matrices->second);
	}
	if (Locator::terrainSystem::has_value())
	{
		const auto& island = Locator::terrainSystem::value();
		point = map_coords::ToWorld(island, map_coords::FromWorld(island, point));
	}
	const auto pile = ecs::archetypes::PotArchetype::CreateEmpty(point, 0.0f, PotInfo::MagicWood);
	if (pile == entt::null)
	{
		return entt::null;
	}
	registry.Get<Transform>(pile).scale = glm::vec3(magic::piles::k_MagicWoodScale);
	// The archetype can add entities, thus the workshop's data is found again
	registry.Get<Workshop>(workshopEntity).woodPile = pile;
	return pile;
}

int32_t WorkshopSystem::RoomOf(entt::entity workshopEntity) const
{
	const auto& registry = Entities();
	const auto* data = registry.Valid(workshopEntity) ? registry.TryGet<const Workshop>(workshopEntity) : nullptr;
	return data != nullptr ? disciple_work::WorkshopRoom(data->makingTurns != 0, data->counted) : 0;
}

entt::entity WorkshopSystem::CreateScaffold(glm::vec3 place, entt::entity town, entt::entity workshopEntity, float yAngle,
                                            float scale)
{
	if (!Locator::infoConstants::has_value())
	{
		return entt::null;
	}
	return ecs::archetypes::ScaffoldArchetype::Create(place, town, workshopEntity, yAngle, scale);
}

bool WorkshopSystem::CanTake(entt::entity scaffold) const
{
	const auto& registry = Entities();
	const auto* data = registry.TryGet<const Scaffold>(scaffold);
	if (data == nullptr || !registry.Valid(data->site))
	{
		return true;
	}
	const auto& info = Locator::infoConstants::value().scaffold;
	return workshop::KeepsScaffold(data->putDownTurn, Turn(), info.gameTurnsAfterPlacingCanStillPickUp,
	                               data->scriptBuilding.has_value()) &&
	       ecs::construction::BuiltOf(data->site) == 0.0f;
}

void WorkshopSystem::TakenByHand(entt::entity scaffold, PlayerNames player)
{
	auto& registry = Entities();
	auto* data = registry.TryGet<Scaffold>(scaffold);
	if (data == nullptr)
	{
		return;
	}
	data->handPlayer = player;
	data->putDownTurn = 0;
	LeaveSite(scaffold);
	data = registry.TryGet<Scaffold>(scaffold);
	// Its workshop keeps the place of the scaffold for it
	if (auto* owner = registry.Valid(data->workshop) ? registry.TryGet<Workshop>(data->workshop) : nullptr)
	{
		owner->places.at(data->place % owner->places.size()) = Workshop::Place::Away;
	}
}

void WorkshopSystem::LetGoByHand(entt::entity scaffold, bool forced)
{
	// The scaffold comes down gently when the hand puts it on the land and does not drop it by force
	if (auto* data = Entities().TryGet<Scaffold>(scaffold))
	{
		data->gentle = !forced;
	}
}

void WorkshopSystem::NotPutDownByHand(entt::entity scaffold)
{
	if (auto* data = Entities().TryGet<Scaffold>(scaffold))
	{
		data->gentle = false;
	}
}

void WorkshopSystem::CameToRest(entt::entity scaffold, bool fromHand, bool inMap)
{
	auto& registry = Entities();
	if (!registry.Valid(scaffold) || !registry.AllOf<Scaffold, Transform>(scaffold))
	{
		return;
	}
	if (inMap)
	{
		// A scaffold near a place of a workshop of its town goes into that place
		bool snapped = false;
		const auto town = TownForBuilding(registry.Get<const Transform>(scaffold).position);
		for (const auto workshopEntity : WorkshopsOf(town))
		{
			snapped = SnapToPlace(workshopEntity, scaffold) || snapped;
		}
		// A scaffold that the hand put down gently starts the building that it offers. Else a puff shows that it starts
		// no building.
		if (fromHand && !snapped)
		{
			const auto& data = registry.Get<const Scaffold>(scaffold);
			if (!data.gentle || !BuildBuilding(scaffold, data.handPlayer))
			{
				Puff(registry.Get<const Transform>(scaffold).position);
			}
		}
		auto& data = registry.Get<Scaffold>(scaffold);
		data.handPlayer.reset();
		data.putDownTurn = Turn();
	}
	registry.Get<Scaffold>(scaffold).gentle = false;
}

bool WorkshopSystem::CanMove(entt::entity scaffold) const
{
	const auto& registry = Entities();
	const auto* data = registry.TryGet<const Scaffold>(scaffold);
	return !workshop::StandsInYard(registry, scaffold) && (data == nullptr || !registry.Valid(data->site));
}

entt::entity WorkshopSystem::TownForBuilding(glm::vec3 place) const
{
	const auto& registry = Entities();
	const auto from = CoordsOf(place);
	// The town of the nearest building in the cells about the place
	if (Locator::entitiesMap::has_value())
	{
		const auto& map = Locator::entitiesMap::value();
		const auto nearest = ecs::script_find::FindNearest(from, k_TownReach, [&](glm::ivec2 cell) {
			std::vector<ecs::script_find::Candidate> found;
			for (const auto thing : map.GetFixedInGridCell(ecs::MapInterface::CellId(cell)))
			{
				if (registry.Valid(thing) && registry.AllOf<Abode>(thing) && registry.AllOf<Transform>(thing))
				{
					found.push_back({.entity = thing, .at = CoordsOf(registry.Get<const Transform>(thing).position)});
				}
			}
			return found;
		});
		if (nearest != entt::null)
		{
			return ecs::town_homes::TownOfAbode(registry, nearest);
		}
	}
	// Else the nearest town, or the town of the nearest planned building, in the order of the players
	entt::entity found = entt::null;
	float best = k_TownReach;
	for (uint8_t index = 0; index < static_cast<uint8_t>(PlayerNames::_COUNT); ++index)
	{
		for (const auto town : ecs::town_turns::TownsInGainedOrder(registry, static_cast<PlayerNames>(index)))
		{
			const auto* transform = registry.TryGet<const Transform>(town);
			if (transform == nullptr)
			{
				continue;
			}
			if (const float distance = gutils::GetDistanceInMetres(from, CoordsOf(transform->position)); distance < best)
			{
				best = distance;
				found = town;
			}
			for (const auto plan : registry.Get<const Town>(town).planned)
			{
				const auto* planned = registry.Valid(plan) ? registry.TryGet<const Transform>(plan) : nullptr;
				if (planned == nullptr)
				{
					continue;
				}
				if (const float distance = gutils::GetDistanceInMetres(from, CoordsOf(planned->position)); distance < best)
				{
					best = distance;
					found = town;
				}
			}
		}
	}
	return found;
}

void WorkshopSystem::ReleaseScaffold(entt::entity scaffold)
{
	auto& registry = Entities();
	auto* data = registry.TryGet<Scaffold>(scaffold);
	if (data == nullptr)
	{
		return;
	}
	const auto owner = std::exchange(data->workshop, entt::null);
	auto* workshopData = registry.Valid(owner) ? registry.TryGet<Workshop>(owner) : nullptr;
	if (workshopData == nullptr)
	{
		return;
	}
	// When the yard stops being full, the town wants again what it needs
	if (workshop::RemoveScaffold(*workshopData, scaffold, data->place))
	{
		MarkDesiresChanged(ecs::town_homes::TownOfAbode(registry, owner));
	}
}

bool WorkshopSystem::SnapToPlace(entt::entity workshopEntity, entt::entity scaffold)
{
	auto& registry = Entities();
	const auto at = CoordsOf(registry.Get<const Transform>(scaffold).position);
	bool near = false;
	for (const auto point : workshop::k_PlacePoints)
	{
		if (const auto special = SpecialPlaceOf(workshopEntity, point);
		    special.has_value() && gutils::GetDistanceInMetres(at, special->coords) < workshop::k_SnapReach)
		{
			near = true;
		}
	}
	if (!near)
	{
		return false;
	}
	if (registry.Get<const Scaffold>(scaffold).workshop != workshopEntity)
	{
		if (RoomOf(workshopEntity) == 0)
		{
			return false;
		}
		ReleaseScaffold(scaffold);
		auto& owner = registry.Get<Workshop>(workshopEntity);
		workshop::AddScaffold(owner, scaffold);
		auto& data = registry.Get<Scaffold>(scaffold);
		data.workshop = workshopEntity;
		data.place = workshop::FirstFreePlace(owner.places);
	}
	MoveToPlace(workshopEntity, scaffold);
	return true;
}

void WorkshopSystem::MoveToPlace(entt::entity workshopEntity, entt::entity scaffold)
{
	auto& registry = Entities();
	const auto place = registry.Get<const Scaffold>(scaffold).place;
	if (const auto special = SpecialPlaceOf(workshopEntity, workshop::k_PlacePoints.at(place % workshop::k_PlacePoints.size())))
	{
		auto& transform = registry.Get<Transform>(scaffold);
		transform.position = OnMap(special->coords);
		transform.rotation = x87::GameTurnMatrix(special->angles.x, special->angles.y, special->angles.z);
		ecs::placed_turn::KeepAngles(registry, scaffold, special->angles);
		if (Locator::entitiesMap::has_value())
		{
			Locator::entitiesMap::value().Refile(scaffold);
		}
	}
	auto& owner = registry.Get<Workshop>(workshopEntity);
	owner.places.at(place % owner.places.size()) = Workshop::Place::Holds;
	registry.SetDirty();
}

bool WorkshopSystem::NearWorkshops(entt::entity town, glm::vec3 place) const
{
	const auto& registry = Entities();
	const auto at = CoordsOf(place);
	// The distance to a special point that the model does not have counts as far
	const auto distanceTo = [&at](entt::entity workshopEntity, size_t point) {
		const auto special = SpecialPlaceOf(workshopEntity, point);
		return special.has_value() ? gutils::GetDistanceInMetres(at, special->coords) : std::numeric_limits<float>::max();
	};
	return std::ranges::any_of(WorkshopsOf(town), [&](entt::entity workshopEntity) {
		scaffold_offer::WorkshopDistances distances {
		    .workshop = gutils::GetDistanceInMetres(at, CoordsOf(registry.Get<const Transform>(workshopEntity).position)),
		    .flag = distanceTo(workshopEntity, scaffold_offer::k_FlagPoint),
		};
		for (size_t i = 0; i < workshop::k_PlacePoints.size(); ++i)
		{
			distances.places.at(i) = distanceTo(workshopEntity, workshop::k_PlacePoints.at(i));
		}
		return scaffold_offer::NearWorkshop(distances);
	});
}

std::optional<scaffold_offer::Found> WorkshopSystem::SearchOffer(entt::entity scaffold, entt::entity town)
{
	auto& registry = Entities();
	const auto& data = registry.Get<const Scaffold>(scaffold);
	const auto& transform = registry.Get<const Transform>(scaffold);
	const auto place = transform.position;
	// A town offers no building near its own workshops
	if (town != entt::null && NearWorkshops(town, place))
	{
		return std::nullopt;
	}
	// TODO(scaffolds): a scaffold held above another scaffold adds the value of that scaffold
	const auto value = workshop::ValueOfScale(transform.scale.x);
	const auto fits = [place](const ScaffoldOffer& offer, float yAngle) {
		return ecs::town_planning::FitsForScaffold(place, offer.mesh, yAngle, offer.scale);
	};
	const auto& rows = Locator::infoConstants::value().abode;
	if (town == entt::null)
	{
		// With no town near, the scaffold offers a village centre of the tribe of its own town, which starts a new town
		const auto tribe = TribeOf(data.town);
		if (!tribe.has_value())
		{
			return std::nullopt;
		}
		const auto centre = GAbodeInfo::FindFirst(rows, *tribe, AbodeNumber::TownCentre);
		if (!centre.has_value())
		{
			return std::nullopt;
		}
		const auto& info = rows.at(static_cast<size_t>(*centre));
		const scaffold_offer::Candidate candidate {
		    .offer = {.info = *centre, .number = AbodeNumber::TownCentre, .mesh = info.meshId},
		    .scaffoldsRequired = info.scaffoldsRequired,
		};
		const auto offer = scaffold_offer::SearchNewTown(candidate, value, fits);
		if (!offer.has_value())
		{
			return std::nullopt;
		}
		return scaffold_offer::Found {.offer = *offer, .foundsTown = true};
	}
	const auto tribe = TribeOf(town);
	if (!tribe.has_value())
	{
		return std::nullopt;
	}
	// A scaffold that a script gave a building offers only a building of that abode number
	std::optional<AbodeNumber> only;
	if (data.scriptBuilding.has_value())
	{
		only = rows.at(static_cast<size_t>(*data.scriptBuilding)).abodeNumber;
	}
	const auto candidates = ecs::town_planning::ScaffoldCandidates(town, *tribe, value, only);
	const float startYAngle = Locator::gameRandom::value().GameFloatRand(scaffold_offer::k_FullTurn);
	const auto offer = scaffold_offer::SearchTown(candidates, value, startYAngle, fits);
	if (!offer.has_value())
	{
		return std::nullopt;
	}
	return scaffold_offer::Found {.offer = *offer};
}

bool WorkshopSystem::SearchAndTakeOffer(entt::entity scaffold, entt::entity town)
{
	const auto found = SearchOffer(scaffold, town);
	auto& phantom = Entities().Get<Scaffold>(scaffold).phantom;
	if (found.has_value())
	{
		phantom.next = found->offer;
		if (found->foundsTown)
		{
			phantom.foundsTown = true;
		}
	}
	scaffold_offer::TakeNextOffer(phantom);
	return found.has_value();
}

void WorkshopSystem::ProcessHeld(entt::entity scaffold)
{
	auto& registry = Entities();
	if (!registry.Valid(scaffold) || !registry.AllOf<Scaffold, InHand, Transform>(scaffold) ||
	    !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto hand = registry.Get<const InHand>(scaffold).hand;
	const auto* stillness = registry.Valid(hand) ? registry.TryGet<const HandStillness>(hand) : nullptr;
	const scaffold_offer::HeldFacts facts {
	    .handStill = stillness != nullptr && stillness->stillTurns > 0,
	    .turnPerGameTurn = Locator::infoConstants::value().scaffold.phantomBuildingRotationPerGameTurn,
	};
	const auto place = registry.Get<const Transform>(scaffold).position;
	const auto town = TownForBuilding(place);
	const scaffold_offer::Search search = [this, scaffold, town]() { return SearchOffer(scaffold, town); };
	// The offer stays while the hand moves when it starts a new town away from every town. It also stays when it still
	// fits and the town is away from its workshops.
	const scaffold_offer::KeepsOffer keeps = [this, place, town](const ScaffoldPhantom& phantom) {
		if (!phantom.shown.has_value())
		{
			return false;
		}
		if (town == entt::null && phantom.foundsTown)
		{
			return true;
		}
		return ecs::town_planning::FitsForScaffold(place, phantom.shown->mesh, phantom.yAngle, phantom.scale) &&
		       town != entt::null && !NearWorkshops(town, place);
	};
	// The turn changes a copy of the phantom, which goes back to the scaffold at the end
	auto phantom = registry.Get<const Scaffold>(scaffold).phantom;
	const auto event = scaffold_offer::HeldTurn(phantom, facts, search, keeps);
	registry.Get<Scaffold>(scaffold).phantom = phantom;
	if (event == scaffold_offer::HeldEvent::ShowsNoOffer)
	{
		Puff(place);
	}
	if (phantom.shown.has_value())
	{
		MakeGhost(scaffold);
	}
}

bool WorkshopSystem::BuildBuilding(entt::entity scaffold, std::optional<PlayerNames> player)
{
	auto& registry = Entities();
	const auto place = registry.Get<const Transform>(scaffold).position;
	const auto town = TownForBuilding(place);
	// The scaffold looks again when it shows no offer, or an offer that needs more than its value
	{
		const auto& phantom = registry.Get<const Scaffold>(scaffold).phantom;
		const auto value = workshop::ValueOfScale(registry.Get<const Transform>(scaffold).scale.x);
		if (!phantom.shown.has_value() ||
		    Locator::infoConstants::value().abode.at(static_cast<size_t>(phantom.shown->info)).scaffoldsRequired > value)
		{
			SearchAndTakeOffer(scaffold, town);
		}
	}
	auto& phantom = registry.Get<Scaffold>(scaffold).phantom;
	if (!phantom.shown.has_value())
	{
		return false;
	}
	// TODO(scaffolds): a village centre offered away from every town starts a new town. openblack does not start the
	// town yet, thus such a scaffold starts no building.
	if (phantom.foundsTown && phantom.shown->number == AbodeNumber::TownCentre)
	{
		return false;
	}
	if (town == entt::null)
	{
		return false;
	}
	// The building takes the turn of the phantom first, then the turn of the offer
	const float offerYAngle = phantom.shown->yAngle;
	phantom.shown->yAngle = phantom.yAngle;
	if (!TryToBuild(scaffold, town))
	{
		if (auto& again = registry.Get<Scaffold>(scaffold).phantom; again.shown.has_value())
		{
			again.shown->yAngle = offerYAngle;
		}
		if (!TryToBuild(scaffold, town))
		{
			return false;
		}
	}
	MarkDesiresChanged(town);
	if (const auto logger = spdlog::get("game"))
	{
		SPDLOG_LOGGER_INFO(logger, "Scaffold: scaffold {} starts building {} at turn {}", entt::to_integral(scaffold),
		                   entt::to_integral(registry.Get<const Scaffold>(scaffold).site), Turn());
	}
	// The scaffold that a player put down impresses the people of the near towns
	if (player.has_value())
	{
		ImpressNearTowns(scaffold, *player);
	}
	const auto building = registry.Get<const Scaffold>(scaffold).site;
	ecs::town_planning::DropPlansUnder(place, ecs::systems::object_measures::TwoDRadius(registry, building));
	// The player who put the scaffold down hears the sound where the camera is. The other players hear it at the
	// scaffold.
	const auto sound = entt::hashed_string("InGame.sad/174").value();
	const bool local =
	    player.has_value() && Locator::playerSystem::has_value() && *player == Locator::playerSystem::value().GetLocalPlayer();
	if (local)
	{
		audio::PlayGameSoundEffect(sound, std::nullopt);
	}
	else
	{
		audio::StartGameSoundEffect(sound, {.position = place, .owner = scaffold});
	}
	return true;
}

void WorkshopSystem::ImpressNearTowns(entt::entity scaffold, PlayerNames player)
{
	if (!Locator::reactionSystem::has_value())
	{
		return;
	}
	auto& registry = Entities();
	auto& reactions = Locator::reactionSystem::value();
	const auto place = registry.Get<const Transform>(scaffold).position;
	// The time of the reaction starts at once
	const auto id = reactions.Create(
	    {.initiator = scaffold, .type = Reaction::ReactToScaffold, .player = player, .position = place, .onCast = true});
	std::vector<entt::entity> towns;
	std::vector<scaffold_impression::TownEntry> entries;
	registry.Each<const Town>([&](entt::entity town, const Town& data) {
		towns.push_back(town);
		entries.push_back({.made = data.made, .owner = data.owner, .gained = data.gained});
	});
	const auto& info = Locator::infoConstants::value().scaffold;
	for (const auto index : scaffold_impression::TownsToImpress(entries))
	{
		const auto town = towns.at(index);
		const auto* transform = registry.TryGet<const Transform>(town);
		// A town at the distance or farther is not impressed
		if (transform == nullptr ||
		    gutils::GetDistanceInMetres(place, transform->position) >= info.maxDistanceForImpressingTowns)
		{
			continue;
		}
		const auto* stats = registry.TryGet<const TownStats>(town);
		const uint32_t people = stats != nullptr ? stats->adults + stats->children : 0;
		const auto count = scaffold_impression::PeopleToImpress(people, info.proportionOfTownToImpress);
		// The people of the town's abodes, abode by abode in the town's order
		std::vector<std::vector<entt::entity>> abodes;
		for (const auto abode : registry.Get<const Town>(town).abodes)
		{
			if (const auto* data = registry.Valid(abode) ? registry.TryGet<const Abode>(abode) : nullptr)
			{
				abodes.push_back(data->inhabitants);
			}
		}
		const auto impressed = scaffold_impression::FirstPeople(abodes, count);
		for (const auto person : impressed)
		{
			reactions.Impress(id, person);
		}
		if (const auto logger = spdlog::get("game"))
		{
			SPDLOG_LOGGER_INFO(logger, "Scaffold: scaffold {} impresses {} people of town {} at turn {}",
			                   entt::to_integral(scaffold), impressed.size(), entt::to_integral(town), Turn());
		}
	}
}

bool WorkshopSystem::TryToBuild(entt::entity scaffold, entt::entity town)
{
	auto& registry = Entities();
	auto& phantom = registry.Get<Scaffold>(scaffold).phantom;
	// The next offer comes first when there is one
	if (phantom.next.has_value())
	{
		std::swap(phantom.shown, phantom.next);
	}
	if (!phantom.shown.has_value())
	{
		return false;
	}
	const auto place = registry.Get<const Transform>(scaffold).position;
	if (NearWorkshops(town, place))
	{
		return false;
	}
	const auto offer = *phantom.shown;
	// TODO(scaffolds): a script can let a scaffold build without the check of the land
	if (!ecs::town_planning::FitsForScaffold(place, offer.mesh, offer.yAngle, offer.scale))
	{
		return false;
	}
	const auto building =
	    ecs::town_planning::StartForScaffold(town, offer.info, OnMap(CoordsOf(place)), offer.yAngle, offer.scale);
	if (building == entt::null)
	{
		return false;
	}
	auto& data = registry.Get<Scaffold>(scaffold);
	// A building that no script gave the scaffold is not planned again when it goes
	if (!data.scriptBuilding.has_value())
	{
		registry.Get<Abode>(building).notPlannedAgain = true;
	}
	data.site = building;
	// The phantom on the site has the size of the scaffold
	data.phantom.scale = registry.Get<const Transform>(scaffold).scale.x;
	data.phantom.shown.reset();
	data.phantom.next.reset();
	// The phantom of the building shows on the site
	MakeGhost(scaffold);
	registry.SetDirty();
	return true;
}

void WorkshopSystem::LeaveSite(entt::entity scaffold)
{
	auto& registry = Entities();
	auto& data = registry.Get<Scaffold>(scaffold);
	const auto building = std::exchange(data.site, entt::null);
	if (registry.Valid(building))
	{
		ecs::world_objects::Remove(building);
	}
}

void WorkshopSystem::SiteClosed(entt::entity building)
{
	auto& registry = Entities();
	std::vector<entt::entity> onSite;
	registry.Each<const Scaffold>([&onSite, building](entt::entity scaffold, const Scaffold& data) {
		if (data.site == building)
		{
			onSite.push_back(scaffold);
		}
	});
	for (const auto scaffold : onSite)
	{
		registry.Get<Scaffold>(scaffold).site = entt::null;
		ecs::world_objects::Remove(scaffold);
	}
}

void WorkshopSystem::MakeGhost(entt::entity scaffold)
{
	auto& registry = Entities();
	if (registry.Valid(registry.Get<const Scaffold>(scaffold).ghost))
	{
		return;
	}
	// The copy shows nothing until a frame gives it a model
	const auto ghost = registry.Create();
	registry.Assign<Transform>(ghost, registry.Get<const Transform>(scaffold));
	registry.Assign<ScaffoldGhost>(ghost, ScaffoldGhost {.scaffold = scaffold});
	registry.Get<Scaffold>(scaffold).ghost = ghost;
}

void WorkshopSystem::PlaceGhostsForFrame(std::optional<glm::vec3> drawnHand)
{
	auto& registry = Entities();
	std::vector<std::pair<entt::entity, entt::entity>> ghosts;
	registry.Each<const Scaffold>([&registry, &ghosts](entt::entity scaffold, const Scaffold& data) {
		if (registry.Valid(data.ghost) && registry.AllOf<ScaffoldGhost, Transform>(data.ghost))
		{
			ghosts.emplace_back(scaffold, data.ghost);
		}
	});
	for (const auto& [scaffold, ghost] : ghosts)
	{
		const bool inHand = registry.AllOf<InHand>(scaffold);
		bool shown = false;
		if (inHand && drawnHand.has_value())
		{
			shown = PlaceHeldGhost(scaffold, ghost, *drawnHand);
		}
		else if (!inHand)
		{
			shown = PlaceSiteGhost(scaffold, ghost);
		}
		// A copy that shows nothing has no model
		if (!shown && registry.AllOf<Mesh>(ghost))
		{
			registry.Remove<Mesh>(ghost);
		}
		// The scaffold on a site becomes small as the building grows
		const auto site = registry.Get<const Scaffold>(scaffold).site;
		if (!inHand && registry.Valid(site))
		{
			const float scale = scaffold_ghost::ScaleOnSite(ecs::construction::BuiltOf(site),
			                                                registry.Get<const Scaffold>(scaffold).phantom.scale);
			registry.AssignOrReplace<ScaffoldDrawnScale>(scaffold, ScaffoldDrawnScale {.scale = scale});
		}
		else if (registry.AllOf<ScaffoldDrawnScale>(scaffold))
		{
			registry.Remove<ScaffoldDrawnScale>(scaffold);
		}
	}
}

bool WorkshopSystem::PlaceHeldGhost(entt::entity scaffold, entt::entity ghost, glm::vec3 drawnHand)
{
	auto& registry = Entities();
	// Only the local player sees the phantom of a scaffold in a hand, and only in the hand of that player
	if (!Locator::handSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto hand = registry.Get<const InHand>(scaffold).hand;
	const auto ownHands = Locator::handSystem::value().GetOwnHands();
	if (std::ranges::find(ownHands, hand) == ownHands.end())
	{
		return false;
	}
	const auto& phantom = registry.Get<const Scaffold>(scaffold).phantom;
	if (!phantom.shown.has_value())
	{
		return false;
	}
	const auto msIntoTurn = Locator::time::has_value() ? Locator::time::value().GetTurnRemainderMs() : 0;
	const auto look = scaffold_ghost::Held(
	    phantom, msIntoTurn, Locator::infoConstants::value().scaffold.phantomBuildingRotationPerGameTurn, _ghostSounds);
	if (look.sound == scaffold_ghost::GhostSound::Shrinks)
	{
		audio::PlayGameSoundEffect(entt::hashed_string("InGame.sad/50").value(), std::nullopt);
	}
	else if (look.sound == scaffold_ghost::GhostSound::Grows)
	{
		const auto pitch = scaffold_ghost::GrowPitchPercent(static_cast<int32_t>(phantom.shown->number));
		audio::StartGameSoundEffect(entt::hashed_string("InGame.sad/51").value(),
		                            {.pitchPercent = static_cast<uint32_t>(pitch)});
	}
	if (!look.pose.has_value())
	{
		return false;
	}
	// The phantom shows at the drawn hand, on the fixed grid of the map, as high above the land as the hand
	const auto coords = CoordsOf(drawnHand);
	const glm::vec2 metres {map_coords::ToMetres(coords.x), map_coords::ToMetres(coords.z)};
	const float aboveLand = drawnHand.y - GroundAt(glm::vec2(drawnHand.x, drawnHand.z));
	auto& transform = registry.Get<Transform>(ghost);
	transform.position = glm::vec3(metres.x, GroundAt(metres) + aboveLand, metres.y);
	transform.rotation = glm::mat3(portable_math::EulerAngleY(-look.pose->yAngle));
	transform.scale = glm::vec3(look.pose->scale);
	auto& data = registry.Get<ScaffoldGhost>(ghost);
	data.alpha = scaffold_ghost::k_HeldAlpha;
	const auto mesh = resources::HashIdentifier(phantom.shown->mesh);
	if (const auto* drawn = registry.TryGet<const Mesh>(ghost); drawn == nullptr || drawn->id != mesh)
	{
		registry.AssignOrReplace<Mesh>(ghost, mesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
	}
	return true;
}

bool WorkshopSystem::PlaceSiteGhost(entt::entity scaffold, entt::entity ghost)
{
	auto& registry = Entities();
	const auto site = registry.Get<const Scaffold>(scaffold).site;
	if (!registry.Valid(site))
	{
		return false;
	}
	const auto* building = registry.TryGet<const Transform>(site);
	const auto* model = registry.TryGet<const Mesh>(site);
	if (building == nullptr || model == nullptr)
	{
		return false;
	}
	// The phantom on the site is a copy of the building, which fades as the building grows
	registry.Get<Transform>(ghost) = *building;
	registry.Get<ScaffoldGhost>(ghost).alpha = scaffold_ghost::SiteAlpha(ecs::construction::BuiltOf(site));
	// Copied out first: adding to a pool may move what is in it
	const Mesh copy = *model;
	if (const auto* drawn = registry.TryGet<const Mesh>(ghost); drawn == nullptr || drawn->id != copy.id)
	{
		registry.AssignOrReplace<Mesh>(ghost, copy);
	}
	return true;
}

void WorkshopSystem::OnScaffoldGone(entt::registry& /*registry*/, entt::entity scaffold)
{
	// A scaffold that goes from the world stops to count for its workshop
	ReleaseScaffold(scaffold);
	// Its phantom goes with it
	auto& registry = Entities();
	if (const auto ghost = registry.Get<const Scaffold>(scaffold).ghost; registry.Valid(ghost))
	{
		registry.Destroy(ghost);
	}
}

void WorkshopSystem::OnWorkshopGone(entt::registry& /*registry*/, entt::entity workshopEntity)
{
	// A workshop that goes lets its scaffolds go: they stay where they are
	auto& registry = Entities();
	const auto& data = registry.Get<const Workshop>(workshopEntity);
	for (const auto scaffold : data.scaffolds)
	{
		if (auto* owned = registry.Valid(scaffold) ? registry.TryGet<Scaffold>(scaffold) : nullptr;
		    owned != nullptr && owned->workshop == workshopEntity)
		{
			owned->workshop = entt::null;
		}
	}
}
