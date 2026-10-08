/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Workshops.h"

#include <algorithm>
#include <optional>

#include <glm/mat3x3.hpp>

#include "3D/ObjectMatrix.h"
#include "Audio/Audio.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Workshop.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "ECS/Scaffolds.h"
#include "ECS/ShowNeeds.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownQueries.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Objects/MagicPiles.h"
#include "Worship/SpecialPoints.h"

// The workshop and the town's workshop list (Workshops.h)

namespace openblack::ecs
{
using namespace components;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Workshop* WorkshopComponent(entt::entity workshop)
{
	auto& registry = Entities();
	return workshop != entt::null && registry.Valid(workshop) ? registry.TryGet<Workshop>(workshop) : nullptr;
}

Town* TownComponent(entt::entity town)
{
	auto& registry = Entities();
	return town != entt::null && registry.Valid(town) ? registry.TryGet<Town>(town) : nullptr;
}

// ---- constants ----------------------------------------------------------------------------------------------------

/// The mesh's special points: 4 the wood pile, 6 + slot the slots, 10 the drop area centre. 9 is the ShowNeeds icon
/// ((not ported) the icon)
constexpr int32_t k_PilePoint = 4;
constexpr int32_t k_FirstSlotPoint = 6;
constexpr int32_t k_DropAreaPoint = 10;
/// The scaffold areas' reach from the workshop itself
constexpr float k_ScaffoldAreaReach = 50.0f;
/// The drop area's radius and each slot's
constexpr float k_DropAreaRadius = 10.0f;
constexpr float k_SlotAreaRadius = 5.0f;
/// The snap distance to a slot
constexpr float k_SnapRadius = 2.0f;
/// GetDistanceModifier's maximum when choosing the best workshop
constexpr float k_BestWorkshopDistance = 500.0f;
/// Added to the wood and to the wood value in GetVisualWoodDesire
constexpr float k_DesireEpsilon = 0.0001f;
/// The working loop of the InGame bank. (pending) the sample's name in the .sad
constexpr int k_WorkingLoopSample = 0x4A;
/// G_ScaffoldReady 150, InGame bank
constexpr int k_ScaffoldReadySample = 0x96;

/// The scaffold info's WoodValue (2500 in info.dat)
uint32_t ScaffoldWoodValue()
{
	return Locator::infoConstants::value().scaffold.woodValue;
}

/// The workshop's resource mirror (the abode's), what GetResource reads (not overridden by the workshop)
uint32_t MirrorOf(entt::entity workshop, ResourceType type)
{
	const auto* a = Entities().TryGet<const Abode>(workshop);
	if (a == nullptr)
	{
		return 0;
	}
	return type == ResourceType::Food ? a->foodAmount : type == ResourceType::Wood ? a->woodAmount : 0;
}

/// The abode's DoResourceRemoving (not overridden) around its JustRemoveResource: min(n, mirror[type]) off the mirror
uint32_t DoMirrorRemoving(entt::entity workshop, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper)
{
	return object_resources::DoResourceRemoving(workshop, type, amount, dropper, [workshop, type, amount]() {
		auto* a = Entities().TryGet<Abode>(workshop);
		if (a == nullptr || (type != ResourceType::Food && type != ResourceType::Wood))
		{
			return 0u;
		}
		auto& held = type == ResourceType::Food ? a->foodAmount : a->woodAmount;
		const uint32_t removed = std::min(amount, held);
		held -= removed;
		return removed;
	});
}

/// The Y angle and the position of a special point of the workshop's mesh (the position, the Y angle, or the three
/// DecomposeYXZ angles) through worship::GetSpecialPoint (the metric through the Transform). (not verified) that
/// SpecialPoint::yAngle is the Y angle FinishScaffold gives the new scaffold
std::optional<worship::SpecialPoint> SpecialPointOf(entt::entity workshop, int32_t point)
{
	return worship::GetSpecialPoint(workshop, point);
}

/// The slot's special point: 0 -> 6, 1 -> 7, 2 -> 8; GetSlotPos passes any other value on as the point number (the
/// snap takes 8 for 2 and 3)
int32_t SlotPoint(int32_t slot)
{
	return slot >= 0 && slot < static_cast<int32_t>(Workshop::k_Slots) ? k_FirstSlotPoint + slot : slot;
}

/// slot[i] = value for i in the array; a slot value 3 from the scaffold's 2 bits writes into the original's padding
/// after the array: nothing kept
void SetSlotByte(Workshop& w, int32_t slot, uint8_t value)
{
	if (slot >= 0 && slot < static_cast<int32_t>(Workshop::k_Slots))
	{
		w.slots.at(static_cast<size_t>(slot)) = value;
	}
}

/// GetResource(WOOD) >= WoodValue (unsigned) -> RemoveResource(WOOD, WoodValue) (the workshop's: from the pile, then
/// the mirror); the countdown = TimeEachMobileObjectTakesToProduce (200.0) truncated toward zero; the working loop sound tag
/// (track 0, mode 2, loops -1, is3D 1, InGame bank, no delay)
void StartScaffold(entt::entity workshop)
{
	const uint32_t value = ScaffoldWoodValue();
	if (MirrorOf(workshop, ResourceType::Wood) < value)
	{
		return;
	}
	workshops::RemoveResource(workshop, ResourceType::Wood, value, {});
	auto* w = WorkshopComponent(workshop);
	if (w == nullptr)
	{
		return;
	}
	// (openblack guard) an abode without its info record: 0 turns (AbodeArchetype always keeps one)
	const auto* info = abodes::InfoOf(workshop);
	w->countdown = info != nullptr ? map_coords::FtoL(info->timeEachMobileObjectTakesToProduce) : 0;
	audio::tags::Create(workshop, k_WorkingLoopSample, false, 2, -1, false, true, audio::SfxBank::InGame, 0);
}

/// slot = the first free one; the slot's point; a scaffold made there (the town, this workshop, the point's Y angle,
/// scale 1.0) with value 1, AddScaffold, its slot bits, slot occupied and G_ScaffoldReady at the workshop (owner this,
/// is3D 1, track 0, at its position). Always the countdown = 0 and the working loop removed
void FinishScaffold(entt::entity workshop)
{
	auto& registry = Entities();
	const int32_t slot = workshops::GetFirstFreeSlot(workshop);
	// (pending) the angle when the point is missing: the original leaves it uninitialised; 0 here
	float yAngle = 0.0f;
	const auto pos = workshops::GetSlotPos(workshop, slot, &yAngle);
	const auto town = abode_villagers::TownOf(workshop);
	const auto scaffold = scaffolds::Create(pos, town, workshop, yAngle, 1.0f);
	if (scaffold != entt::null)
	{
		scaffolds::SetValue(scaffold, 1);
		workshops::AddScaffold(workshop, scaffold);
		// the scaffold's slot bits: ecs::scaffolds writes its component
		scaffolds::SetSlot(scaffold, slot);
		if (auto* w = WorkshopComponent(workshop); w != nullptr)
		{
			SetSlotByte(*w, slot, Workshop::k_SlotOccupied);
		}
		audio::PlayOptions options;
		options.sample = {audio::Bank(audio::SfxBank::InGame), k_ScaffoldReadySample};
		options.owner = audio::Owner::Thing(workshop);
		options.is3D = true;
		options.track = false;
		// the land under it plus its altitude, as ToWorld
		options.position = map_coords::ToWorld(object::MapCoordsOf(workshop));
		audio::PlaySoundEffect(options);
	}
	if (auto* w = WorkshopComponent(workshop); w != nullptr)
	{
		w->countdown = 0;
	}
	if (registry.Valid(workshop))
	{
		audio::tags::Remove(workshop, k_WorkingLoopSample, audio::SfxBank::InGame);
	}
}

/// The snap: the scaffold's slot point (0 -> 6, 1 -> 7, 2 and 3 -> 8); out of the map cells; moved to the point at
/// altitude 0; SetXYZAngles from the point's DecomposeYXZ angles; back into the map cells; slot occupied. SetXYZAngles
/// builds RotationYXZ(y, x, z) times the object's scale, so the rotation is RotationYXZ(DecomposeYXZ(point)) and the scale
/// stays the Transform's own. No point: (0, 0) with the angles 0
void Snap(entt::entity workshop, entt::entity scaffold)
{
	auto& registry = Entities();
	const int32_t slot = scaffolds::GetSlot(scaffold);
	const int32_t point = slot == 0 ? k_FirstSlotPoint : slot == 1 ? k_FirstSlotPoint + 1 : k_FirstSlotPoint + 2;
	const auto special = SpecialPointOf(workshop, point);
	map_coords::MapCoords at {};
	if (special.has_value())
	{
		at = map_coords::FromWorld(special->position);
	}
	at.altitude = 0.0f;
	map_cells::RemoveMapObject(scaffold);
	if (auto* transform = registry.TryGet<Transform>(scaffold); transform != nullptr)
	{
		transform->position = map_coords::ToWorld(at);
		transform->rotation =
		    workshops::SnappedRotation(special.has_value() ? std::optional<glm::mat3>(special->rotation) : std::nullopt);
	}
	map_cells::InsertMapObject(scaffold);
	if (auto* w = WorkshopComponent(workshop); w != nullptr)
	{
		SetSlotByte(*w, scaffolds::GetSlot(scaffold), Workshop::k_SlotOccupied);
	}
	registry.SetDirty();
}
} // namespace

// =====================================================================================================================
// lifetime
// =====================================================================================================================

bool workshops::IsWorkshop(entt::entity entity)
{
	return WorkshopComponent(entity) != nullptr;
}

void workshops::Create(entt::entity workshop)
{
	auto& registry = Entities();
	if (workshop == entt::null || !registry.Valid(workshop))
	{
		return;
	}
	// all the workshop's fields start at 0: the component's defaults
	auto& component = registry.AssignOrReplace<Workshop>(workshop);
	// the ghost's 3D object (no object, no creation index; (not ported) its draw); no sign yet and not being
	// deleted -> a ShowNeeds sign (an object: one creation index) at the workshop's position. Then the abode's part
	// (the grey chimney smoke: AbodeArchetype's chimney_smoke::Attach, already made) and one more call on the
	// workshop's 3D object ((pending) what it does)
	if (component.showNeeds == entt::null)
	{
		// the workshop's own position: its Transform position as it is.
		// (openblack, guard) no Transform: the origin
		const auto* transform = registry.TryGet<const Transform>(workshop);
		component.showNeeds =
		    show_needs::Create(transform != nullptr ? transform->position : glm::vec3 {}, workshop, show_needs::k_WorkshopInfo);
	}
	// CreatePileWood (also for a workshop made from a plan: the pile exists during its construction). Its
	// place in the map cells: OnInsertedInMap
	CreatePileWood(workshop);
}

void workshops::OnInsertedInMap(entt::entity workshop)
{
	// the workshop's InsertMapObject came before CreatePileWood: the pile's RemoveMapObject and InsertMapObject again,
	// now after the workshop's
	const auto pile = GetPileWood(workshop);
	if (pile == entt::null || !Entities().Valid(pile))
	{
		return;
	}
	map_cells::RemoveMapObject(pile);
	map_cells::InsertMapObject(pile);
}

void workshops::CreatePileWood(entt::entity workshop)
{
	auto* w = WorkshopComponent(workshop);
	// a pile already -> nothing
	if (w == nullptr || w->woodPile != entt::null)
	{
		return;
	}
	// the wood's position and angle, angle 0 first
	float angle = 0.0f;
	const auto pos = GetResourcePosAndYAngle(workshop, ResourceType::Wood, &angle);
	// a "Magic Wood" pot (pot 9) with 0 wood, no player, owned by this workshop: a pile with angle 0 and scale 1 (the
	// angle is dropped, as the site's pile), and no reaction set up at its creation (the Standard site's pile has one).
	// allowEmpty is openblack's rule for an amount of 0 (the original makes the pile whatever the amount)
	const auto pile = magic::objects::CreateMagicWood(map_coords::ToWorld(pos), std::nullopt, 0, true);
	if ((w = WorkshopComponent(workshop)) != nullptr)
	{
		w->woodPile = pile;
	}
}

map_coords::MapCoords workshops::GetResourcePosAndYAngle(entt::entity workshop, ResourceType type, float* angle)
{
	// WOOD and a 3D object -> the special point 4 and its Y angle; when it exists, that point (the MapCoords of the
	// world point: its altitude above the land kept)
	if (type == ResourceType::Wood)
	{
		if (const auto point = SpecialPointOf(workshop, k_PilePoint); point.has_value())
		{
			if (angle != nullptr)
			{
				*angle = point->yAngle;
			}
			return map_coords::FromWorld(point->position);
		}
	}
	// *angle = 0, the workshop's own position
	if (angle != nullptr)
	{
		*angle = 0.0f;
	}
	return object::MapCoordsOf(workshop);
}

entt::entity workshops::GetPileWood(entt::entity workshop)
{
	const auto* w = WorkshopComponent(workshop);
	return w != nullptr ? w->woodPile : entt::null;
}

entt::entity workshops::WorkshopOfPile(entt::entity pile)
{
	if (pile == entt::null)
	{
		return entt::null;
	}
	entt::entity found = entt::null;
	Entities().Each<const Workshop>([pile, &found](entt::entity workshop, const Workshop& w) {
		if (w.woodPile == pile)
		{
			found = workshop;
		}
	});
	return found;
}

void workshops::MakeFunctional(entt::entity workshop)
{
	if (!IsWorkshop(workshop))
	{
		return;
	}
	// the abode's part is abodes::MakeFunctional, which calls this after it. With a town, the pulse and AddWorkshop
	const auto town = abode_villagers::TownOf(workshop);
	if (town == entt::null)
	{
		return;
	}
	town_queries::Pulse(town);
	AddWorkshop(town, workshop);
}

void workshops::DeleteDependants(entt::entity workshop)
{
	// with a town, RemoveWorkshop
	if (const auto town = abode_villagers::TownOf(workshop); town != entt::null)
	{
		RemoveWorkshop(town, workshop);
	}
	auto* w = WorkshopComponent(workshop);
	if (w == nullptr)
	{
		return;
	}
	// while the list has a head with a scaffold: available -> its owner cleared; then every node of it out. The owned
	// count and the slots are left as they are (literal)
	while (!w->scaffolds.empty())
	{
		const auto scaffold = w->scaffolds.front();
		if (scaffold == entt::null)
		{
			break;
		}
		scaffolds::SetOwner(scaffold, entt::null);
		w->scaffolds.erase(std::remove(w->scaffolds.begin(), w->scaffolds.end(), scaffold), w->scaffolds.end());
	}
	// the abode's own part is the caller's (abodes::DestroyedByEffect)
}

void workshops::ToBeDeleted(entt::entity workshop, bool now)
{
	// DeleteDependants first
	DeleteDependants(workshop);
	auto* w = WorkshopComponent(workshop);
	if (w == nullptr)
	{
		return;
	}
	// the pile: unlinked from the workshop (openblack's pile keeps no link), its ToBeDeleted(now) (ecs::ToBeDeleted);
	// the pile link cleared. Its wood is lost (no carry-away reaction, unlike a site's pile)
	if (const auto pile = w->woodPile; pile != entt::null)
	{
		w->woodPile = entt::null;
		if (Entities().Valid(pile))
		{
			ecs::ToBeDeleted(pile, now);
		}
	}
	// the ShowNeeds sign's ToBeDeleted(now), the link cleared
	if (auto* again = WorkshopComponent(workshop); again != nullptr && again->showNeeds != entt::null)
	{
		const auto sign = again->showNeeds;
		again->showNeeds = entt::null;
		show_needs::Delete(sign, now);
	}
	// the abode's own deletion is the caller's
}

void workshops::RemovePotFromStructure(entt::entity workshop, entt::entity pot)
{
	auto* w = WorkshopComponent(workshop);
	// only the workshop's own pile
	if (w == nullptr || pot == entt::null || w->woodPile != pot)
	{
		return;
	}
	// DoResourceRemoving(the pot's resource type, its amount) off the mirror, the pile link cleared
	auto& registry = Entities();
	if (const auto* p = registry.Valid(pot) ? registry.TryGet<const Pot>(pot) : nullptr; p != nullptr)
	{
		const auto& info = Locator::infoConstants::value().pot.at(static_cast<size_t>(p->type));
		DoMirrorRemoving(workshop, info.resourceType, p->amount, {});
	}
	if ((w = WorkshopComponent(workshop)) != nullptr)
	{
		w->woodPile = entt::null;
	}
	// then the structure's own part. (pending) not read
}

// =====================================================================================================================
// the town list
// =====================================================================================================================

void workshops::AddWorkshop(entt::entity town, entt::entity workshop)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return;
	}
	// already in the list -> nothing; else at the head
	if (std::find(t->workshops.begin(), t->workshops.end(), workshop) != t->workshops.end())
	{
		return;
	}
	t->workshops.insert(t->workshops.begin(), workshop);
}

void workshops::RemoveWorkshop(entt::entity town, entt::entity workshop)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return;
	}
	// found -> unlinked; else nothing
	if (const auto it = std::find(t->workshops.begin(), t->workshops.end(), workshop); it != t->workshops.end())
	{
		t->workshops.erase(it);
	}
}

entt::entity workshops::GetBestWorkshop(entt::entity town, const map_coords::MapCoords& pos, bool useDesire,
                                        bool onlyFunctional)
{
	const auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return entt::null;
	}
	// best = 0, none kept
	float best = 0.0f;
	entt::entity found = entt::null;
	const auto list = t->workshops; // a copy: GetDesireToBeSupplied changes nothing, kept safe
	for (const auto workshop : list)
	{
		// d = useDesire ? GetDesireToBeSupplied : 1.0
		const float d = useDesire ? GetDesireToBeSupplied(workshop) : 1.0f;
		// GetDistanceModifier(GetDistanceInMetres(pos, its pos), 500.0) x d
		const float distance = gutils::GetDistanceInMetres(pos, object::MapCoordsOf(workshop));
		const float modifier = gutils::GetDistanceModifier(distance, k_BestWorkshopDistance);
		const float v = modifier * d;
		// v > best (<= and NaN skipped) and (!onlyFunctional || IsFunctional)
		if (v > best && (!onlyFunctional || abode_queries::IsFunctional(workshop)))
		{
			best = v;
			found = workshop;
		}
	}
	return found;
}

bool workshops::IsScaffoldAwayFromWorkshops(entt::entity town, entt::entity scaffold)
{
	const auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return true; // (openblack guard) the original is called on a town
	}
	// the first workshop with IsPosWithinScaffoldAreas(scaffold pos) -> false; none -> true
	const auto pos = object::MapCoordsOf(scaffold);
	return std::ranges::none_of(t->workshops, [&pos](const auto workshop) { return IsPosWithinScaffoldAreas(workshop, pos); });
}

bool workshops::CheckScaffoldSnapToPoint(entt::entity town, entt::entity scaffold)
{
	const auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return false;
	}
	// r |= CheckSnapToPoint(s) for every workshop, head first (none skipped)
	const auto list = t->workshops; // a copy: a snap moves the scaffold between the workshops' lists, not this one
	bool snapped = false;
	for (const auto workshop : list)
	{
		snapped = CheckSnapToPoint(workshop, scaffold) || snapped;
	}
	return snapped;
}

// =====================================================================================================================
// the turn
// =====================================================================================================================

void workshops::Process(entt::entity workshop)
{
	auto& registry = Entities();
	if (WorkshopComponent(workshop) == nullptr)
	{
		return;
	}
	if (abode_queries::IsFunctional(workshop))
	{
		auto* w = WorkshopComponent(workshop);
		if (w->countdown != 0)
		{
			// count down; at 0 FinishScaffold
			w->countdown = w->countdown - 1;
			if (w->countdown == 0)
			{
				FinishScaffold(workshop);
			}
		}
		// GetSpaceInStore != 0 and GetResource(WOOD) (the mirror) >= WoodValue (unsigned) -> StartScaffold
		else if (GetSpaceInStore(workshop) != 0 && MirrorOf(workshop, ResourceType::Wood) >= ScaffoldWoodValue())
		{
			StartScaffold(workshop);
		}
	}
	// the head scaffold: !CanStillBeAdjusted -> it leaves the workshop. The original walks a next link that is always
	// null, so only the head is ever seen (literal)
	if (const auto* w = WorkshopComponent(workshop); w != nullptr && !w->scaffolds.empty())
	{
		const auto scaffold = w->scaffolds.front();
		// (openblack guard) a scaffold entity already gone
		if (scaffolds::IsScaffold(scaffold) && !scaffolds::CanStillBeAdjusted(scaffold))
		{
			scaffolds::RemoveFromWorkshop(scaffold);
		}
	}
	// the ShowNeeds sign's desire = GetVisualWoodDesire (no null test, literal)
	if (auto* w = WorkshopComponent(workshop); w != nullptr)
	{
		show_needs::SetDesire(w->showNeeds, GetVisualWoodDesire(workshop));
	}
	// the abode's turn: the site's Process (the workshop site's does nothing), then abode_villagers::ProcessAbode
	if (!registry.Valid(workshop))
	{
		return;
	}
	if (const auto site = abodes::GetBuildingSite(workshop); site != entt::null)
	{
		building_sites::Process(site);
	}
	abode_villagers::ProcessAbode(workshop);
}

int32_t workshops::GetSpaceInStore(entt::entity workshop)
{
	const auto* w = WorkshopComponent(workshop);
	if (w == nullptr)
	{
		return 0;
	}
	// 3 - (a scaffold in production) - the owned count
	return k_MaxWaiting - (w->countdown != 0 ? 1 : 0) - w->owned;
}

float workshops::GetVisualWoodDesire(entt::entity workshop)
{
	// wood = GetResource(WOOD), rounded to a float
	const auto wood = static_cast<float>(MirrorOf(workshop, ResourceType::Wood));
	// the wood value (exact)
	const auto value = static_cast<float>(ScaffoldWoodValue());
	// (wood + 0.0001) / (value + 0.0001), one rounding per step (single precision)
	const float a = wood + k_DesireEpsilon;
	const float b = value + k_DesireEpsilon;
	float r = a / b;
	// r < 1 (and NaN) kept; else 1
	if (r >= 1.0f)
	{
		r = 1.0f;
	}
	// 1 - r
	return 1.0f - r;
}

float workshops::GetDesireToBeSupplied(entt::entity workshop)
{
	// GetSpaceInStore != 0 -> 1.0
	if (GetSpaceInStore(workshop) != 0)
	{
		return 1.0f;
	}
	// GetVisualWoodDesire above 0 -> 1.0; else 0.0
	return GetVisualWoodDesire(workshop) > 0.0f ? 1.0f : 0.0f;
}

uint32_t workshops::WoodWanted(entt::entity workshop)
{
	// space = GetSpaceInStore; <= 0 -> 0
	const int32_t space = GetSpaceInStore(workshop);
	if (space <= 0)
	{
		return 0;
	}
	// w = WoodValue x space - GetResource(WOOD) (wrapping uint32); (0 < w) ? w : 0, which is w itself: a wood above
	// the space's value returns the wrapped value (literal)
	const uint32_t w = ScaffoldWoodValue() * static_cast<uint32_t>(space) - MirrorOf(workshop, ResourceType::Wood);
	return 0u < w ? w : 0u;
}

// =====================================================================================================================
// resources
// =====================================================================================================================

uint32_t workshops::AddResource(entt::entity workshop, ResourceType type, uint32_t amount, bool poisoned)
{
	auto& registry = Entities();
	// n == 0 -> 0
	if (amount == 0 || WorkshopComponent(workshop) == nullptr)
	{
		return 0;
	}
	// GetResource(type) (the mirror) == 0 and a town -> the pulse
	if (MirrorOf(workshop, type) == 0)
	{
		if (const auto town = abode_villagers::TownOf(workshop); town != entt::null)
		{
			town_queries::Pulse(town);
		}
	}
	uint32_t added = 0;
	// WOOD: no pile -> CreatePileWood; an available pile -> added = its AddToPotDirect(WOOD, n, poisoned)
	if (type == ResourceType::Wood)
	{
		if (GetPileWood(workshop) == entt::null)
		{
			CreatePileWood(workshop);
		}
		if (const auto pile = GetPileWood(workshop); pile != entt::null && registry.Valid(pile))
		{
			added = pot_resource::AddToPotDirect(pile, ResourceType::Wood, amount, poisoned);
		}
	}
	// DoResourceAdding(type, added) with no status, so only the abode's JustAddResource (mirror[type] += added); no
	// desire, alignment, belief nor creature mimic
	object_resources::DoResourceAdding(workshop, type, added, {}, [workshop, type, added]() {
		if (auto* a = Entities().TryGet<Abode>(workshop);
		    a != nullptr && (type == ResourceType::Food || type == ResourceType::Wood))
		{
			auto& held = type == ResourceType::Food ? a->foodAmount : a->woodAmount;
			held += added;
		}
		return added;
	});
	return added;
}

uint32_t workshops::RemoveResource(entt::entity workshop, ResourceType type, uint32_t amount,
                                   const pot_resource::Dropper& dropper)
{
	auto& registry = Entities();
	auto* w = WorkshopComponent(workshop);
	if (w == nullptr)
	{
		return 0;
	}
	// WOOD and a pile -> r = the pile's JustRemoveResource(WOOD, n); return DoResourceRemoving(WOOD, r, status) (the
	// mirror)
	if (type == ResourceType::Wood && w->woodPile != entt::null && registry.Valid(w->woodPile))
	{
		const auto pile = w->woodPile;
		const uint32_t r = object_resources::RemoveFromPotDirect(pile, amount);
		// (openblack guard) the original keeps an emptied pile that is part of a structure, and so does
		// RemoveFromPotDirect for a workshop's pile (WorkshopOfPile); should it be gone all the same, the pile link is
		// cleared and the next AddResource makes a new one
		if (!registry.Valid(pile))
		{
			if ((w = WorkshopComponent(workshop)) != nullptr)
			{
				w->woodPile = entt::null;
			}
		}
		return DoMirrorRemoving(workshop, ResourceType::Wood, r, dropper);
	}
	// DoResourceRemoving(type, n, status)
	return DoMirrorRemoving(workshop, type, amount, dropper);
}

uint32_t workshops::RemoveResourceFromPile(entt::entity pile, ResourceType type, uint32_t amount,
                                           const pot_resource::Dropper& dropper)
{
	const auto workshop = WorkshopOfPile(pile);
	if (workshop == entt::null)
	{
		return 0;
	}
	// the pile is part of the workshop; the workshop's site is never linked to it. (pending) the poison out flag, as
	// ObjectResources. The amount over the maximum is 0 for a workshop: the pile is asked for all n
	uint32_t r = 0;
	if (amount != 0)
	{
		// JustRemoveResource(type, n) (the type is not tested)
		r = object_resources::RemoveFromPotDirect(pile, amount);
	}
	// r != 0 -> the workshop's DoResourceRemoving(type, r, status) (its value not kept)
	if (r != 0)
	{
		DoMirrorRemoving(workshop, type, r, dropper);
	}
	// r < n -> r += the workshop's RemoveResource(type, n - r, status)
	if (r < amount)
	{
		r += RemoveResource(workshop, type, amount - r, dropper);
	}
	return r;
}

bool workshops::IsResourceStore(entt::entity workshop, ResourceType type)
{
	// the structure's own answer (WOOD with a site) || WOOD || ANY (-2)
	const bool site = abodes::GetBuildingSite(workshop) != entt::null;
	return (type == ResourceType::Wood && site) || type == ResourceType::Wood || type == ResourceType::Any;
}

// =====================================================================================================================
// slots and scaffolds
// =====================================================================================================================

map_coords::MapCoords workshops::GetSlotPos(entt::entity workshop, int32_t slot, float* yAngle)
{
	// the special point SlotPoint(slot), zeroed first (no point leaves (0, 0)); out = (x, z, 0) (the altitude 0, on
	// the land)
	map_coords::MapCoords out {};
	if (const auto point = SpecialPointOf(workshop, SlotPoint(slot)); point.has_value())
	{
		out = map_coords::FromWorld(point->position);
		if (yAngle != nullptr)
		{
			*yAngle = point->yAngle;
		}
	}
	out.altitude = 0.0f;
	return out;
}

int32_t workshops::GetFirstFreeSlot(entt::entity workshop)
{
	const auto* w = WorkshopComponent(workshop);
	if (w == nullptr)
	{
		return 0;
	}
	// the first free slot i < 3; none -> 0
	for (size_t i = 0; i < Workshop::k_Slots; ++i)
	{
		if (w->slots.at(i) == Workshop::k_SlotFree)
		{
			return static_cast<int32_t>(i);
		}
	}
	return 0;
}

bool workshops::IsNearSpecialPoint(entt::entity workshop, int32_t point, const map_coords::MapCoords& pos, float radius)
{
	// the special point n, (0, 0, 0) when missing; GetDistanceInMetres(p, it) < r
	map_coords::MapCoords at {};
	if (const auto special = SpecialPointOf(workshop, point); special.has_value())
	{
		at = map_coords::FromWorld(special->position);
	}
	return gutils::GetDistanceInMetres(pos, at) < radius;
}

bool workshops::IsPosWithinScaffoldAreas(entt::entity workshop, const map_coords::MapCoords& pos)
{
	// GetDistanceInMetres(own pos, p) above 50 -> false (<= and NaN go on)
	const float distance = gutils::GetDistanceInMetres(object::MapCoordsOf(workshop), pos);
	if (distance > k_ScaffoldAreaReach)
	{
		return false;
	}
	// the four tests, all asked, ORed
	bool inside = IsNearSpecialPoint(workshop, k_DropAreaPoint, pos, k_DropAreaRadius);
	inside = IsNearSpecialPoint(workshop, k_FirstSlotPoint, pos, k_SlotAreaRadius) || inside;
	inside = IsNearSpecialPoint(workshop, k_FirstSlotPoint + 1, pos, k_SlotAreaRadius) || inside;
	inside = IsNearSpecialPoint(workshop, k_FirstSlotPoint + 2, pos, k_SlotAreaRadius) || inside;
	return inside;
}

bool workshops::CheckSnapToPoint(entt::entity workshop, entt::entity scaffold)
{
	if (WorkshopComponent(workshop) == nullptr || !scaffolds::IsScaffold(scaffold))
	{
		return false;
	}
	// IsNearSpecialPoint(6 / 7 / 8, scaffold pos, 2.0), all three asked; none -> false
	const auto pos = object::MapCoordsOf(scaffold);
	bool near = IsNearSpecialPoint(workshop, k_FirstSlotPoint, pos, k_SnapRadius);
	near = IsNearSpecialPoint(workshop, k_FirstSlotPoint + 1, pos, k_SnapRadius) || near;
	near = IsNearSpecialPoint(workshop, k_FirstSlotPoint + 2, pos, k_SnapRadius) || near;
	if (!near)
	{
		return false;
	}
	// another owner's: GetSpaceInStore == 0 -> false; else it leaves its workshop, AddScaffold(s), owner = this, its
	// slot = the first free one
	if (scaffolds::GetOwner(scaffold) != workshop)
	{
		if (GetSpaceInStore(workshop) == 0)
		{
			return false;
		}
		scaffolds::RemoveFromWorkshop(scaffold);
		AddScaffold(workshop, scaffold);
		scaffolds::SetOwner(scaffold, workshop);
		scaffolds::SetSlot(scaffold, GetFirstFreeSlot(workshop));
	}
	Snap(workshop, scaffold);
	return true;
}

glm::mat3 workshops::SnappedRotation(const std::optional<glm::mat3>& pointRotation)
{
	// DecomposeYXZ then SetXYZAngles: only the point's angles are kept
	float y = 0.0f;
	float x = 0.0f;
	float z = 0.0f;
	if (pointRotation.has_value())
	{
		affine::DecomposeYXZ(*pointRotation, y, x, z);
	}
	return affine::RotationYXZ(y, x, z);
}

void workshops::AddScaffold(entt::entity workshop, entt::entity scaffold)
{
	auto* w = WorkshopComponent(workshop);
	if (w == nullptr)
	{
		return;
	}
	// a scaffold -> a node at the head (no duplicate test)
	if (scaffold != entt::null)
	{
		w->scaffolds.insert(w->scaffolds.begin(), scaffold);
	}
	// the owned count goes up in any case
	w->owned = w->owned + 1;
}

void workshops::RemoveScaffold(entt::entity workshop, entt::entity scaffold)
{
	auto* w = WorkshopComponent(workshop);
	// not in the list -> nothing
	if (w == nullptr || !OwnsScaffold(workshop, scaffold))
	{
		return;
	}
	// every node of it out
	w->scaffolds.erase(std::remove(w->scaffolds.begin(), w->scaffolds.end(), scaffold), w->scaffolds.end());
	// owned count down; its slot freed
	w->owned = w->owned - 1;
	SetSlotByte(*w, scaffolds::GetSlot(scaffold), Workshop::k_SlotFree);
	// 2 owned now and a town -> the pulse
	if (w->owned == 2)
	{
		if (const auto town = abode_villagers::TownOf(workshop); town != entt::null)
		{
			town_queries::Pulse(town);
		}
	}
}

bool workshops::OwnsScaffold(entt::entity workshop, entt::entity scaffold)
{
	const auto* w = WorkshopComponent(workshop);
	// walk the list for a node of s
	return w != nullptr && std::find(w->scaffolds.begin(), w->scaffolds.end(), scaffold) != w->scaffolds.end();
}

void workshops::FreeSlot(entt::entity workshop, int32_t slot)
{
	// i < 3 (unsigned) -> slot[i] free
	if (auto* w = WorkshopComponent(workshop); w != nullptr)
	{
		SetSlotByte(*w, slot, Workshop::k_SlotFree);
	}
}

void workshops::ScaffoldMoved(entt::entity workshop, entt::entity scaffold)
{
	// its slot taken away (the ghost is drawn at the slot, the place stays reserved)
	if (auto* w = WorkshopComponent(workshop); w != nullptr)
	{
		SetSlotByte(*w, scaffolds::GetSlot(scaffold), Workshop::k_SlotTakenAway);
	}
}

uint8_t workshops::GetSlotState(entt::entity workshop, int32_t slot)
{
	// read when asking whether a scaffold can become a physics object
	const auto* w = WorkshopComponent(workshop);
	if (w == nullptr || slot < 0 || slot >= static_cast<int32_t>(Workshop::k_Slots))
	{
		return Workshop::k_SlotFree;
	}
	return w->slots.at(static_cast<size_t>(slot));
}
} // namespace openblack::ecs
