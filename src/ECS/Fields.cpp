/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Fields.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <iterator>
#include <vector>

#include <glm/vec3.hpp>

#include "Common/GameRandom.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/FishFarms.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WorldEffectsInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Weather/Weather.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack::ecs::components;

namespace
{
/// The trees' wind sway: 16 phases advancing at 1.06-2.12 rad/s, the speeds drawn again (Random(1, 2)) every 2 s
struct WindSwaySlots
{
	// 0 until the first draw: no sway for the first 2 s
	std::array<float, 16> speed {};
	std::array<float, 16> phase {};
	std::array<float, 16> lean {};
	float sinceSpeeds {0.0f}; ///< += the frame's game ms, back to 0 once over 2000

	void Update(float milliseconds)
	{
		sinceSpeeds += milliseconds;
		if (sinceSpeeds > 2000.0f)
		{
			for (auto& s : speed)
			{
				s = openblack::game_random::crt::Random(1.0f, 2.0f); // slot by slot
			}
			sinceSpeeds = 0.0f;
		}
		for (size_t i = 0; i < 16; ++i)
		{
			phase[i] += milliseconds * speed[i] * 0.00106061f;
			lean[i] = -0.03f * std::cos(phase[i]);
		}
	}
};

/// The WindSway state (Locator::worldEffects)
WindSwaySlots& WindSwayData()
{
	return openblack::Locator::worldEffects::value().Get<WindSwaySlots>();
}

/// k = 0 gives a, 255 gives b, per channel (a (255 - k) + b k) / 255 truncated
glm::u8vec3 BlendColour(int k, glm::ivec3 a, glm::ivec3 b)
{
	k = std::clamp(k, 0, 255);
	return glm::u8vec3((a * (255 - k) + b * k) / 255);
}
} // namespace

glm::u8vec3 openblack::ecs::FieldDrawColour(const Field& field)
{
	constexpr glm::ivec3 k_Olive(121, 145, 25);
	constexpr glm::ivec3 k_LightGreen(170, 212, 67);
	constexpr glm::ivec3 k_White(255, 255, 255);
	if (field.growth < Field::k_AgeGrowth)
	{
		return BlendColour(static_cast<int>(255.0f * (1.0f - field.food / Field::k_TotalFood)), k_Olive, k_LightGreen);
	}
	if (field.growth < Field::k_AgeRecolt)
	{
		return BlendColour(
		    static_cast<int>(255.0f * (field.growth - Field::k_AgeGrowth) / (Field::k_AgeRecolt - Field::k_AgeGrowth)), k_Olive,
		    k_White);
	}
	return glm::u8vec3(k_White);
}

float openblack::ecs::WindSway(uint32_t slot)
{
	return WindSwayData().lean.at(slot & 15u);
}

void openblack::ecs::ProcessField(entt::entity entity, uint32_t turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	// (fields::FieldOf is defined further down, in fields::)
	const auto fieldOf = [&registry, entity]() { return registry.Valid(entity) ? registry.TryGet<Field>(entity) : nullptr; };
	auto* fieldComponent = fieldOf();
	if (fieldComponent == nullptr)
	{
		return;
	}
	// a field marked for deletion was taken out of the list the global lists' pass walks
	if (!ecs::IsAvailable(entity))
	{
		return;
	}
	// The abode's process first: the building site's Process and the empty-abode counters (a field is always empty of
	// villagers; its life reduction changes nothing). The second counter has a reader: StopBeingFunctional's game
	// stats test (>= 200)
	if (const auto site = abodes::GetBuildingSite(entity); site != entt::null)
	{
		building_sites::Process(site);
	}
	abode_villagers::ProcessAbode(entity);
	// (openblack) the site's Process may change the registry: the field again (IsAvailable is not asked again on
	// purpose: the original goes on with the growth whatever the Abode part did)
	fieldComponent = fieldOf();
	if (fieldComponent == nullptr)
	{
		return;
	}
	auto& field = *fieldComponent;
	// (turn + the field's offset) % 10, unsigned
	if ((turn + field.turnOffset) % 10 != 0)
	{
		return;
	}
	// IsOnFire: a fire effect that is on fire
	if (fire::IsOnFire(entity))
	{
		return;
	}
	const auto& info = fields::InfoOf(field);
	// (float)crops < timesToSow
	if (static_cast<float>(field.crops) < info.timesToSow)
	{
		return;
	}
	// past ripe: growth > ageRecolt
	if (!(field.growth <= info.ageRecolt))
	{
		return;
	}
	// The land alignment at the field's MapCoords and whether it rains there (only x and z are read; UpdateFields
	// moves the Transform's y for the sinking)
	const auto at = map_coords::ToWorld(object::MapCoordsOf(entity));
	const float alignment = effects::alignment::LandAlignmentAt(at);
	const bool raining = weather::IsRainingAt(at);
	const float d = fields::GrowthStep(field.growth, info, alignment, raining);
	// growth += d; food += d x totalFoodInField / ageRecolt
	field.growth += d;
	field.food += d * info.totalFoodInField / info.ageRecolt;
	// The original tests ripeness twice on the same growth, the first must be true and the second false for the town
	// pulse: unreachable, not ported
}

void openblack::ecs::ProcessFieldsTurn(uint32_t turn)
{
	// The global lists' first loop: every field's Process, every turn. (openblack) a snapshot of the fields first, in
	// the registry's order: the original reads each one's next after its Process and walks the list from its head,
	// the newest field first
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> fields;
	registry.Each<const Field>([&fields](entt::entity entity, const Field&) { fields.push_back(entity); });
	for (const auto entity : fields)
	{
		ProcessField(entity, turn);
	}
}

bool openblack::ecs::ApplyWaterSpellToField(entt::entity entity)
{
	auto* field = Locator::entitiesRegistry::value().TryGet<Field>(entity);
	if (field == nullptr)
	{
		return false;
	}
	// The info is the field's GFieldTypeInfo; the 6 info.dat rows are the same, so the first row's effectOfWaterSpell
	// stands for the field's own
	fields::ApplyWaterSpell(*field, [] { return Locator::infoConstants::value().fieldType.at(0).effectOfWaterSpell; });
	return true;
}

bool openblack::ecs::IsFieldRipe(entt::entity entity)
{
	const auto* field = Locator::entitiesRegistry::value().TryGet<const Field>(entity);
	return field != nullptr && field->growth >= Field::k_AgeRecolt;
}

int32_t openblack::ecs::RemoveFieldFood(entt::entity entity, float amount)
{
	return fields::RemoveFood(entity, amount);
}

void openblack::ecs::UpdateFields(float seconds)
{
	// the wind the ripe fields (and trees) sway in, once per frame like the trees' pre-draw
	WindSwayData().Update(seconds * 1000.0f);
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<Field, Transform, const Mesh>([&](entt::entity entity, Field& field, Transform& transform, const Mesh& mesh) {
		// (openblack) a field marked for deletion is not drawn (it is out of the map): nothing to sink or fade
		if (!ecs::IsAvailable(entity))
		{
			return;
		}
		// v = food / 350 - 1, eased over 1 s; y += 2 v scale half height (the mesh's half height read inline, so the mesh
		// level)
		auto* sink = registry.TryGet<PileSink>(entity);
		if (sink == nullptr)
		{
			const float height = ecs::object::MeshHalfHeight(mesh.id);
			sink = &registry.Assign<PileSink>(entity, transform.position.y, height);
		}
		const float v = field.food / Field::k_TotalFood - 1.0f;
		if (std::abs(v - field.sinkTarget) > 1e-4f || !field.sinkStarted)
		{
			field.sinkTarget = v;
			field.sinkStarted = true;
			field.sink.SetDestinationWithSpeedAndTime(v, 0.0f, 1.0f);
		}
		field.sink.Update(seconds);
		float shown = field.sink.value;
		float alpha = 1.0f;
		if (shown < -0.8f)
		{
			// the alpha byte = ((v + 0.8) 2 + 1) 255, v held at -0.8
			alpha = std::clamp((shown + 0.8f) * 2.0f + 1.0f, 0.0f, 1.0f);
			shown = -0.8f;
		}
		// not drawn at all below a quarter of the growing age or 25 food
		if (field.growth < 0.25f * Field::k_AgeGrowth || field.food < 25.0f)
		{
			alpha = 0.0f;
		}
		// scale x half height x v, doubled
		const float sunk = transform.scale.y * sink->height * shown;
		sink->offset.SetPosition(sunk + sunk);
		transform.position.y = sink->baseY + sink->offset.value;
		auto* fade = registry.TryGet<Alpha>(entity);
		if (alpha < 1.0f && fade == nullptr)
		{
			registry.Assign<Alpha>(entity, alpha);
		}
		else if (alpha < 1.0f)
		{
			fade->value = alpha;
		}
		else if (fade != nullptr)
		{
			registry.Remove<Alpha>(entity);
		}
	});
}

// ---- The farmers and the field functions the villager side calls (Fields.h) ----------------------------------------

namespace openblack::ecs::fields
{
namespace
{
/// RandomFarmPoint's 5 - GameFloatRand(10), IsTouching's +- 5
constexpr float k_FarmPointHalf = 5.0f;
/// The range of RandomFarmPoint's two GameFloatRand
constexpr float k_FarmPointRange = 10.0f;

// The FPU runs at 24-bit precision: every add / subtract / multiply / divide below is rounded to float, so they are
// float operations here, one per statement (no fused multiply-add); integer and float loads and compares are
// exact

Field* FieldOf(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(entity) ? registry.TryGet<Field>(entity) : nullptr;
}

/// The field's MapCoords. UpdateFields moves the Transform's y with the food (the draw's sink), which the original's
/// altitude never sees: the altitude is taken from PileSink::baseY (the y before the sink) when the field has one
map_coords::MapCoords PositionOf(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(entity);
	const auto* sink = registry.TryGet<const PileSink>(entity);
	if (transform == nullptr || sink == nullptr)
	{
		return object::MapCoordsOf(entity);
	}
	return map_coords::FromWorld(glm::vec3(transform->position.x, sink->baseY, transform->position.z));
}

/// entt's on_destroy<Field> (Registry::Destroy from ecs::ToBeDeleted, Remove, Reset): the field's deletion runs
/// DeleteDependants. The component is still there during the signal
void OnFieldDestroyed(entt::registry& registry, entt::entity entity)
{
	// (openblack) once: a field deleted through ecs::ToBeDeleted has run it at the mark already
	if (registry.get<Field>(entity).dependantsDeleted)
	{
		return;
	}
	DeleteDependants(entity);
}

/// Connected before the first farmer joins (a field without farmers has nothing to release); entt's sink::connect
/// disconnects the same listener first, so it is connected once per registry
void ConnectFieldDeletion()
{
	Locator::entitiesRegistry::value().OnDestroy<Field>().connect<&OnFieldDestroyed>();
}
} // namespace

void DisconnectDeletionListeners()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	Locator::entitiesRegistry::value().OnDestroy<Field>().disconnect<&OnFieldDestroyed>();
	fish_farms::DisconnectDeletionListener();
}

void ReleaseWorker(entt::entity villager)
{
	// SetTopState(163). (openblack guard) a villager already gone (a registry Reset clears the pools one after another)
	// is not called
	if (Locator::entitiesRegistry::value().Valid(villager))
	{
		villager::SetTopState(villager, VillagerStates::DecideWhatToDo);
	}
}

const GFieldTypeInfo& InfoOf(const Field& field)
{
	return Locator::infoConstants::value().fieldType.at(static_cast<size_t>(field.type));
}

float GrowthStep(float growth, const GFieldTypeInfo& info, float alignment, bool raining)
{
	// a = (alignment x 0.5 + 1) doubled
	const float a = (alignment * 0.5f + 1.0f) * 2.0f;
	// below ageGrowth: growing
	const bool growing = growth < info.ageGrowth;
	if (raining)
	{
		// the rain multipliers (growing / ripening)
		return a * (growing ? info.effectRainWhenGrowing : info.effectRainWhenRipening);
	}
	// the sun multipliers (growing / ripening)
	return a * (growing ? info.effectSunWhenGrowing : info.effectSunWhenRipening);
}

void ApplyWaterSpell(Field& field, float (*effectOfWaterSpell)())
{
	if (!(static_cast<float>(field.crops) > static_cast<float>(Field::k_TimesToSow)))
	{
		// crops <= timesToSow (as floats): crops = timesToSow + 1, truncated toward zero
		field.crops = static_cast<uint8_t>(Field::k_TimesToSow + 1);
		return;
	}
	if (field.growth > Field::k_AgeRecolt)
	{
		return; // past ripe, nothing
	}
	const float water = effectOfWaterSpell();
	field.growth += water;
	// the original's ripeness test is called here and its result dropped
	field.food += water * Field::k_TotalFood / Field::k_AgeRecolt;
}

float GetPercentFull(entt::entity entity)
{
	const auto* field = FieldOf(entity);
	if (field == nullptr)
	{
		return 0.0f;
	}
	// the crops (u8) as a float over timesToSow
	return static_cast<float>(field->crops) / InfoOf(*field).timesToSow;
}

int GetFieldActivity(entt::entity entity)
{
	const auto* field = FieldOf(entity);
	if (field == nullptr)
	{
		return 0;
	}
	// GetPercentFull < 1 -> 1
	if (GetPercentFull(entity) < 1.0f)
	{
		return 1;
	}
	// growth >= ageGrowth -> 2
	return field->growth < InfoOf(*field).ageGrowth ? 0 : 2;
}

float GetDesireToBeFarmed(entt::entity entity)
{
	const auto* field = FieldOf(entity);
	if (field == nullptr)
	{
		return 0.0f;
	}
	// a fire effect (the pointer, not IsOnFire)
	if (fire::Find(entity) != nullptr)
	{
		return 0.0f;
	}
	// IsFunctional != 1
	if (!abode_queries::IsFunctional(entity))
	{
		return 0.0f;
	}
	const auto& info = InfoOf(*field);
	// farmers / maxFarmerInFarm (read signed), rounded to float; below 1 (or unordered) kept, else 1; a = 1 - that
	const auto maximum = static_cast<float>(static_cast<int32_t>(info.maxFarmerInFarm));
	float share = static_cast<float>(field->farmers.size()) / maximum;
	if (!(share < 1.0f) && !std::isnan(share))
	{
		share = 1.0f;
	}
	const float a = 1.0f - share;
	// p = GetPercentFull below 1, else 1
	const float full = GetPercentFull(entity);
	const float p = full < 1.0f ? full : 1.0f;
	const int activity = GetFieldActivity(entity);
	if (activity == 2)
	{
		// growth below ageRecolt -> 0, else a
		return field->growth < info.ageRecolt ? 0.0f : a;
	}
	if (activity == 1)
	{
		// (1 - p) a a a, each step rounded to float
		float d = 1.0f - p;
		d = d * a;
		d = d * a;
		d = d * a;
		return d;
	}
	return 0.0f;
}

bool PlantCrop(entt::entity entity)
{
	auto* field = FieldOf(entity);
	// (float)crops < timesToSow -> ++crops, 1
	if (field == nullptr || !(static_cast<float>(field->crops) < InfoOf(*field).timesToSow))
	{
		return false;
	}
	++field->crops;
	return true;
}

bool IsStillSowing(entt::entity entity)
{
	const auto* field = FieldOf(entity);
	// (float)crops < timesToSow
	return field != nullptr && static_cast<float>(field->crops) < InfoOf(*field).timesToSow;
}

map_coords::MapCoords RandomFarmPoint(entt::entity entity)
{
	auto pos = PositionOf(entity);
	// x first, then z: r = 5 - the draw
	const float r1 = k_FarmPointHalf - game_random::GameFloatRand(k_FarmPointRange);
	const float r2 = k_FarmPointHalf - game_random::GameFloatRand(k_FarmPointRange);
	// (pos x 10 x 2^-16 + r) x 65536 / 10, truncated toward zero, on each axis; the altitude copied
	pos.x = map_coords::ToFixedGUtils(map_coords::ToMetres(pos.x) + r1);
	pos.z = map_coords::ToFixedGUtils(map_coords::ToMetres(pos.z) + r2);
	return pos;
}

bool RipeFarmPoint(entt::entity entity, map_coords::MapCoords& out)
{
	const auto* field = FieldOf(entity);
	// growth < ageRecolt -> 0, no draw
	if (field == nullptr || field->growth < InfoOf(*field).ageRecolt)
	{
		return false;
	}
	out = RandomFarmPoint(entity);
	return true;
}

map_coords::MapCoords GetArrivePos(entt::entity entity)
{
	return PositionOf(entity);
}

bool IsTouching(entt::entity entity, const map_coords::MapCoords& pos)
{
	if (FieldOf(entity) == nullptr)
	{
		return false;
	}
	const auto f = map_coords::ToMetres(PositionOf(entity));
	const auto p = map_coords::ToMetres(pos);
	// on x: below fx - 5 -> 0; not below fx + 5 -> 0
	if (p.x < f.x - k_FarmPointHalf || !(p.x < f.x + k_FarmPointHalf))
	{
		return false;
	}
	// the same on z
	return !(p.y < f.y - k_FarmPointHalf) && p.y < f.y + k_FarmPointHalf;
}

int32_t RemoveFood(entt::entity entity, float amount)
{
	auto* field = FieldOf(entity);
	if (field == nullptr)
	{
		return 0;
	}
	const auto& info = InfoOf(*field);
	// no food (equal to 0, or unordered) -> 0
	if (!(field->food < 0.0f || field->food > 0.0f))
	{
		return 0;
	}
	// (float)crops < timesToSow -> 0
	if (static_cast<float>(field->crops) < info.timesToSow)
	{
		return 0;
	}
	// k = amount truncated toward zero
	const int32_t k = map_coords::FtoL(amount);
	// unripe (growth < ageRecolt): cost = amount x ratioBeforeRipe + k, truncated toward zero (each step rounded to float;
	// (approximate) a k above 2^24 is rounded to float before the add); ripe: k
	int32_t cost = k;
	if (field->growth < info.ageRecolt)
	{
		const float product = amount * info.ratioBeforeRipe;
		const float sum = product + static_cast<float>(k);
		cost = map_coords::FtoL(sum);
	}
	// the cost read as unsigned, compared with the food
	const auto unsignedCost = static_cast<double>(static_cast<uint32_t>(cost));
	if (unsignedCost < static_cast<double>(field->food))
	{
		// food -= cost; returns k
		field->food = static_cast<float>(static_cast<double>(field->food) - unsignedCost);
		return k;
	}
	// it runs out: cooled to 0
	fire::SetTemperature(entity, 0.0f, entt::null);
	field = FieldOf(entity);
	// with a town: its pulse
	if (const auto town = TownOf(entity); town != entt::null)
	{
		town_queries::Pulse(town);
	}
	// unripe (read again): food = 0, amount x ratioBeforeRipe truncated toward zero
	if (field->growth < info.ageRecolt)
	{
		field->food = 0.0f;
		const float product = amount * info.ratioBeforeRipe; // rounded to float
		return map_coords::FtoL(product);
	}
	// food truncated toward zero, then food = crops = growth = 0
	const int32_t left = map_coords::FtoL(field->food);
	field->food = 0.0f;
	field->crops = 0;
	field->growth = 0.0f;
	return left;
}

float GetFoodValue(entt::entity entity)
{
	const auto* field = FieldOf(entity);
	// growth < ageRecolt ? 0 : food
	if (field == nullptr || field->growth < InfoOf(*field).ageRecolt)
	{
		return 0.0f;
	}
	return field->food;
}

void AddFarmer(entt::entity entity, entt::entity villager)
{
	auto* field = FieldOf(entity);
	if (field == nullptr)
	{
		return;
	}
	// already in the list -> nothing; a null villager -> nothing
	if (std::find(field->farmers.begin(), field->farmers.end(), villager) != field->farmers.end() || villager == entt::null)
	{
		return;
	}
	// a new node at the head (the deletion listener connected first)
	ConnectFieldDeletion();
	field->farmers.insert(field->farmers.begin(), villager);
}

void RemoveFarmer(entt::entity entity, entt::entity villager)
{
	if (auto* field = FieldOf(entity); field != nullptr)
	{
		// every node of the villager unlinked and freed
		std::erase(field->farmers, villager);
	}
	// the villager's target cleared, always (also when it was not in the list or the list is empty)
	villager::SetTargetThing(villager, entt::null);
}

bool HasFarmer(entt::entity entity, entt::entity villager)
{
	const auto* field = FieldOf(entity);
	return field != nullptr && std::find(field->farmers.begin(), field->farmers.end(), villager) != field->farmers.end();
}

uint32_t FarmerCount(entt::entity entity)
{
	const auto* field = FieldOf(entity);
	return field != nullptr ? static_cast<uint32_t>(field->farmers.size()) : 0u;
}

entt::entity TownOf(entt::entity entity)
{
	const auto* field = FieldOf(entity);
	if (field == nullptr || field->town < 0)
	{
		return entt::null;
	}
	// openblack keeps the Town::id (FieldArchetype, CREATE_TOWN_FIELD)
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = town_queries::TownByKey(static_cast<uint32_t>(field->town));
	const bool found = town != entt::null && registry.Valid(town) && registry.AllOf<Town>(town);
	return found ? town : entt::null;
}

std::vector<entt::entity> TownFields(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* t = town != entt::null && registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr;
	if (t == nullptr)
	{
		return {};
	}
	std::vector<entt::entity> list;
	registry.Each<const Field>([&](entt::entity entity, const Field& field) {
		// a field marked for deletion is out of the town's field list
		if (field.town >= 0 && static_cast<uint32_t>(field.town) == t->id && ecs::IsAvailable(entity))
		{
			list.push_back(entity);
		}
	});
	// the head insertion at creation: newest first
	std::sort(list.begin(), list.end(),
	          [](entt::entity a, entt::entity b) { return object_index::Of(a) > object_index::Of(b); });
	return list;
}

bool IsField(entt::entity thing)
{
	return FieldOf(thing) != nullptr;
}

void DeleteDependants(entt::entity entity)
{
	const auto* field = FieldOf(entity);
	if (field == nullptr)
	{
		return;
	}
	FieldOf(entity)->dependantsDeleted = true; // (openblack) for the on_destroy<Field> listener
	// v = the head's villager (none: nothing)
	auto villager = field->farmers.empty() ? entt::entity {entt::null} : field->farmers.front();
	while (villager != entt::null)
	{
		// before the call, the villager after v's first node in the list as it is now (none: 0)
		auto next = entt::entity {entt::null};
		if (const auto* f = FieldOf(entity); f != nullptr)
		{
			const auto it = std::find(f->farmers.begin(), f->farmers.end(), villager);
			if (it != f->farmers.end() && std::next(it) != f->farmers.end())
			{
				next = *std::next(it);
			}
		}
		// SetTopState(163) (its exit ExitFarming -> RemoveFarmer unlinks it)
		ReleaseWorker(villager);
		// the villager's target cleared
		villager::SetTargetThing(villager, entt::null);
		villager = next;
	}
	// Out of the town's field list (SetTownArea) and the global list: nothing to do in openblack (TownFields / the
	// components are the lists; that SetTownArea, the field still in the town's structures, is followed by
	// RemoveStructureFromTown's in abodes::ToBeDeleted, which is the one that shows). RemoveMapObject:
	// ecs::ToBeDeleted's generic part. The fence: not ported (no fences, its builder has no caller)
}
} // namespace openblack::ecs::fields
