/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FireEffect.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Life.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/FireEffectSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerFire.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Weather/Weather.h"
#include "FireGraphic.h"
#include "FireObjectTraits.h"
#include "FireSound.h"
#include "Locator.h"
#include "Magic/Spells/SpellStormAndTornado.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::fire;

namespace
{
/// The game's fires; stops with a message when there are none (before the game or after it has gone)
systems::FireEffectSystemInterface& Fires()
{
	if (!Locator::fireEffectSystem::has_value())
	{
		std::fputs("ecs::fire: no fires in the locator (Locator::fireEffectSystem)\n", stderr);
		std::abort();
	}
	return Locator::fireEffectSystem::value();
}

bool Trace()
{
	return TraceEnabled();
}

float LandAt(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// GetDistanceInMetres: x, z only, through the table hypotenuse
float Distance2D(const glm::vec3& a, const glm::vec3& b)
{
	return gutils::GetDistanceInMetres(a, b);
}

/// InBounds: the 10 m cell inside the map
bool InBounds(const glm::vec3& position)
{
	if (!Locator::terrainSystem::has_value())
	{
		std::fputs("ecs::fire: no terrain in the locator (Locator::terrainSystem)\n", stderr);
		std::abort();
	}
	const uint16_t side = Locator::terrainSystem::value().GetCellsPerSide();
	return map_coords::InBounds(position, side); // the position is truncated to fixed point, then the cells compared unsigned
}

/// The cell's hasWater bit; 1 outside the map or without a block (ecs::sea_cells)
bool IsWater(const glm::vec3& position)
{
	return sea_cells::IsWater(position);
}

/// No water in the cell but the coast line bit (ecs::sea_cells)
bool IsCoastal(const glm::vec3& position)
{
	return sea_cells::IsCoastal(position);
}

/// The rain or snow at the fire centre (the land plus its height):
/// max(rain, snow) of the weather there (0..127, ECS/Weather)
float MaxRainingOrSnowing(const glm::vec3& centre)
{
	return weather::GetMaxRainingOrSnowingAt(glm::vec3(centre.x, LandAt(centre.x, centre.z) + centre.y, centre.z));
}

/// The player of an object: a villager's town's owner; the others none (inferred: per class)
std::optional<PlayerNames> PlayerOfObject(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* villager = registry.TryGet<const components::Villager>(object);
	    villager != nullptr && registry.Valid(villager->town))
	{
		if (const auto* town = registry.TryGet<const components::Town>(villager->town); town != nullptr)
		{
			return town->owner;
		}
	}
	return std::nullopt;
}

/// Removes every reaction of a type the object started
void RemoveReactions(entt::entity object, Reaction type)
{
	// each reaction is shut down, which first takes every follower out (they stop reacting): the villagers in 215 and
	// the fire fighters of this fire
	std::vector<uint32_t> ids;
	for (const auto& reaction : effects::reactions::All())
	{
		if (reaction.initiator == object && reaction.type == type)
		{
			ids.push_back(reaction.id);
		}
	}
	for (const auto id : ids)
	{
		villager_fire::ShutDownReaction(id);
	}
	effects::reactions::RemoveAllReactionsOfTypeInitiatedBy(object, type);
}

/// The objects of the 10 m map cell in the order FindType(ANY) walks them (then with the previous one): the fixed
/// list, then the mobile one only when the last fixed one's type counts as fixed (ecs::map_cells::FindType), as a
/// snapshot. The held object is out of the
/// map (the module skips it, with the flying ones)
void CellObjects(const glm::ivec2& cell, std::vector<entt::entity>& out)
{
	// nothing off the map (InBounds: the cell unsigned against 512)
	out.clear();
	const map_cells::ReadBatch batch;
	for (auto object = map_cells::FindType(cell, ObjectType::Any); object != entt::null;
	     object = map_cells::FindType(cell, ObjectType::Any, object))
	{
		out.push_back(object);
	}
}

// ---- fire groups (the root / next chain and the root's firemen list) ----

/// Whether both fires are in the same group
bool IsInSameFireGroupAs(const FireEffect& self, const FireEffect* other)
{
	if (other == nullptr)
	{
		return false;
	}
	if (other->root == self.root)
	{
		return true;
	}
	for (const auto* member = self.root; member != nullptr; member = member->next)
	{
		if (member == other)
		{
			return true;
		}
	}
	return false;
}

/// The firemen list of `from`'s root appended to `to`'s
void MoveFiremen(FireEffect& from, FireEffect& to)
{
	to.firemen.insert(to.firemen.end(), from.firemen.begin(), from.firemen.end());
	from.firemen.clear();
}

/// The other fire (with the rest of its group) goes right after this one
void AddToMyFireGroup(FireEffect& self, FireEffect& other)
{
	if (IsInSameFireGroupAs(self, &other))
	{
		return;
	}
	FireEffect* otherRoot = other.root;
	FireEffect* myRoot = self.root;
	if (otherRoot != nullptr && !(otherRoot == &other && other.next == nullptr))
	{
		MoveFiremen(*otherRoot, *myRoot);
		FireEffect* member = otherRoot;
		while (member->next != nullptr)
		{
			member->root = myRoot;
			member = member->next;
		}
		member->root = myRoot;
		member->next = self.next;
		self.next = otherRoot;
		return;
	}
	MoveFiremen(other, *myRoot);
	other.next = self.next;
	self.next = &other;
	other.root = myRoot;
}

/// This root hands the group over to `newRoot` (a member), which goes first
void HandRootTo(FireEffect& self, FireEffect& newRoot)
{
	if (self.root != &self)
	{
		return;
	}
	MoveFiremen(self, newRoot);
	FireEffect* oldRoot = self.root;
	for (FireEffect* member = oldRoot; member != nullptr; member = member->next)
	{
		if (member->next == &newRoot)
		{
			member->next = newRoot.next;
			newRoot.root = &newRoot;
			newRoot.next = oldRoot;
		}
		member->root = &newRoot;
	}
}

/// The group's firemen stop (all of them, or those fighting this fire)
void StopFiremen(FireEffect& self, bool onlyThisFire)
{
	auto* root = self.root;
	if (root == nullptr)
	{
		return;
	}
	const auto firemen = root->firemen; // StopFireFighting takes them off the list
	for (const auto villager : firemen)
	{
		if (!onlyThisFire || villager_fire::FireOf(villager) == self.id)
		{
			villager_fire::StopFireFighting(villager);
		}
	}
}

/// The fire leaves its group
void RemoveFromFireGroup(FireEffect& self)
{
	FireEffect* root = self.root;
	if (root == &self)
	{
		if (self.next == nullptr)
		{
			StopFiremen(self, false);
			return;
		}
		StopFiremen(self, true);
		HandRootTo(self, *self.next);
		self.root->next = self.next;
		self.root = &self;
		self.next = nullptr;
		return;
	}
	if (root == nullptr)
	{
		return;
	}
	StopFiremen(self, true);
	for (FireEffect* member = root; member != nullptr && member->next != nullptr; member = member->next)
	{
		if (member->next == &self)
		{
			member->next = self.next;
			self.root = &self;
			self.next = nullptr;
			if (member->next == nullptr)
			{
				break;
			}
		}
	}
}

// ---- heat ----

/// 0.1 x 100 x dT
float HeatFromDifference(float difference)
{
	return 0.1f * 100.0f * difference;
}

/// This fire heats the object `target`
void HeatTransfer(FireEffect& source, entt::entity target)
{
	// villagers fighting a fire (on the ground) are immune to it
	if (traits::IsObjectInMap(source.object) && villager_fire::IsFireMan(target))
	{
		return;
	}
	if (source.source != entt::null && source.source == target)
	{
		return;
	}
	const float radius = source.FireRadius();
	if (radius == 0.0f)
	{
		return;
	}
	// a hot object that doesn't burn only heats what catches fire below its temperature
	if (!source.IsOnFire() && !(traits::CombustionTemperature(target) <= source.temperature))
	{
		return;
	}
	const auto targetCentre = traits::FireCentre(target);
	const auto sourceCentre = traits::FireCentre(source.object);
	const float distance = Distance2D(targetCentre, sourceCentre);
	if (!(traits::DefaultFireRadius(target) + radius > distance))
	{
		return;
	}
	// heights above the land: the source far above the target (a fireball, a held tree) misses it
	if (!(sourceCentre.y < 3.0f && targetCentre.y < 3.0f))
	{
		const float above = sourceCentre.y - targetCentre.y;
		if (!(traits::Height(target) + source.FlameHeight() > above))
		{
			return;
		}
	}
	// the object to affect is the object itself (citadel parts and the heart redirect it, not ported)
	const entt::entity actual = target;
	if (!ecs::IsAvailable(actual))
	{
		return;
	}
	const float difference = source.temperature - GetTemperature(actual);
	if (!(difference > 0.0f))
	{
		return;
	}
	FireEffect* fire = Find(actual);
	if (fire == nullptr)
	{
		fire = Create(actual, source.player, entt::null);
		if (fire == nullptr)
		{
			return;
		}
	}
	const float heat = std::min(HeatFromDifference(difference), source.HeatContent() * 0.5f);
	fire->AddHeat(heat, difference);
	if (!source.IsOnFire())
	{
		source.AddHeat(-heat, -difference);
	}
	if (!traits::InHand(actual) && fire->root != source.root)
	{
		AddToMyFireGroup(source, *fire);
	}
	if (traits::IsVillager(actual) && !villager_fire::IsInOnFireState(actual))
	{
		villager_fire::SetupOnFire(actual, source.id);
	}
}

/// One fire's Process, once per turn
void Process(FireEffect& fire)
{
	auto& registry = Locator::entitiesRegistry::value();
	// (the port) the object was deleted by something else: its deletion takes its fire with it
	if (!ecs::IsAvailable(fire.object))
	{
		ToBeDeleted(fire);
		return;
	}
	// fully cooled with no charring left
	if (fire.temperature - fire.Ambient() < 0.1f && fire.charring == 0.0f)
	{
		ToBeDeleted(fire);
		return;
	}
	if (fire.source != entt::null && !registry.Valid(fire.source))
	{
		fire.source = entt::null;
	}
	fire.flags &= 0xF0;
	const float tc = fire.CombustionThreshold();
	if (fire.temperature > 3.0f * tc)
	{
		fire.flags |= FireEffect::k_VeryHot;
	}
	if (fire.previous < tc && fire.temperature >= tc)
	{
		fire.flags |= FireEffect::k_JustIgnited;
		// TODO: a creature gets 3 flames (random points in its box)
	}
	const float defenceBurn = traits::DefenceMultiplierBurn(fire.object);
	const bool cools = fire.temperature < tc || defenceBurn == 0.0f; // not burning (or not hurt by it)
	float multiplier = 1.0f;
	const auto centre = traits::FireCentre(fire.object);
	bool heat = false;
	if ((IsCoastal(centre) || IsWater(centre)) && centre.y < 2.0f)
	{
		// in the water: cools 50 times faster
		fire.flags |= FireEffect::k_Cooling;
		multiplier = 50.0f;
	}
	else if (const float rain = MaxRainingOrSnowing(centre); rain > 0.0f && traits::RainCoolingMultiplier(fire.object) > 0.0f)
	{
		multiplier = traits::RainCoolingMultiplier(fire.object) * rain + 1.0f;
		fire.flags |= FireEffect::k_Cooling;
		// the storm spell whose radius covers the object's position gets
		// REACT_TO_MAGIC_WATER_PUTTING_OUT_FIRE (Magic/Spells/SpellStormAndTornado)
		if (const auto* transform = Locator::entitiesRegistry::value().TryGet<const components::Transform>(fire.object))
		{
			magic::spell_storm::ReactToRainOnFire(transform->position);
		}
	}
	else
	{
		if (fire.previous > fire.temperature)
		{
			fire.flags |= FireEffect::k_Cooling;
		}
		heat = !cools;
	}
	if (heat)
	{
		// burning: T += 0.1 T / 2 Tc, up to 2 Tc
		fire.temperature += 0.1f * fire.temperature / fire.MaxBurnTemperature();
		if (fire.temperature > fire.MaxBurnTemperature())
		{
			fire.temperature = fire.MaxBurnTemperature();
		}
	}
	else
	{
		const float ambient = fire.Ambient();
		fire.temperature = ambient < fire.temperature ? fire.temperature : ambient;
		if (!(fire.temperature > fire.previous))
		{
			// nothing heated it this turn: T -= (T + 10 - Tamb) x 4 H r x 0.1 x m / cap
			const float area = 4.0f * traits::Height(fire.object) * traits::Radius(fire.object);
			fire.temperature -= (fire.temperature + 10.0f - fire.Ambient()) * area * 0.1f * multiplier / fire.Capacity();
			if (fire.previous >= tc && fire.temperature < tc)
			{
				fire.flags |= FireEffect::k_JustExtinguished;
			}
		}
	}
	if (fire.temperature >= tc)
	{
		// burning: damage and charring
		// damage x 0.1 per turn; charring grows by 0.04 below 0.6 life, up to (0.6 - life) / 0.6
		const float damage = (fire.temperature - tc) / (fire.MaxBurnTemperature() - tc) * defenceBurn * 0.1f;
		const float life = life::LifeOf(fire.object);
		if (life < 0.6f)
		{
			float charring = std::min(fire.charring + 0.04f, 1.0f);
			const float limit = std::max((0.6f - life) * (1.0f / 0.6f), 0.0f);
			fire.charring = limit < charring ? limit : charring;
		}
		if (life > 0.0f)
		{
			traits::ReduceLifeDueToBurning(fire.object, damage, fire.player);
			if (!ecs::IsAvailable(fire.object))
			{
				return;
			}
			if (!traits::IsAvailable(fire.object) || life::LifeOf(fire.object) <= 0.0f)
			{
				if (!traits::IsCreature(fire.object))
				{
					if (Trace())
					{
						SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fire: object {} burnt down (T {:.0f})",
						                   static_cast<int>(fire.object), fire.temperature);
					}
					// destroyed by the effect with the fire's player and 0: not the burn damage
					traits::DestroyedByEffect(fire.object, fire.player, 0.0f);
					if (!ecs::IsAvailable(fire.object))
					{
						ToBeDeleted(fire); // the object's deletion took the fire with it
					}
				}
				if ((fire.flags & FireEffect::k_Deleted) != 0)
				{
					return;
				}
			}
		}
	}
	else
	{
		const float life = life::LifeOf(fire.object);
		if (fire.charring != 0.0f && life != 0.0f)
		{
			const float charring = std::max(fire.charring - 0.02f, 0.0f); // fades by 0.02 per turn
			const float limit = std::max((0.6f - life) * (1.0f / 0.6f), 0.0f);
			fire.charring = charring < limit ? charring : limit;
		}
	}
	// spread: every object in the spiral of cells within R + 10 m of the fire centre. (The wind is
	// computed there but its result is overwritten: it doesn't move the search.)
	const float radius = fire.FireRadius();
	if (InBounds(centre))
	{
		bool search = true;
		if (traits::InHand(fire.object))
		{
			// held: only inside the holder's influence. (inferred: one local hand, PLAYER_ONE; openblack has no
			// holder-player accessor)
			search = influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, glm::vec3(centre.x, 0.0f, centre.z)) > 0.0f;
		}
		if (search)
		{
			const float reach = radius + 10.0f;
			// the spiral walks a copy of the fire's own MapCoords: the distance is GetDistanceInMetres between it and
			// the centre, and each step only adds whole cells: the fraction is the same in both, so the difference is a
			// whole number of 10 m cells, and the 16-bit add wraps at the edge
			const auto start = map_coords::FromMetres(glm::vec2(centre.x, centre.z));
			auto coords = start;
			map_coords::Spiral spiral; // from dir = count = 1
			std::vector<entt::entity> objects;
			for (int steps = 99999; steps != 0; --steps)
			{
				const auto cell = map_coords::Cell(coords);
				// GetDistanceInMetres (the table hypotenuse)
				if (!(gutils::GetDistanceInMetres(coords, start) <= reach))
				{
					break;
				}
				CellObjects(glm::ivec2(cell), objects);
				// FindType(ANY): every object of the cell but the fire's own;
				// no "done" set, so an object in several cells of the spiral is heated once per cell (as in the original)
				for (const auto object : objects)
				{
					if (object != fire.object)
					{
						HeatTransfer(fire, object);
						if ((fire.flags & FireEffect::k_Deleted) != 0)
						{
							return;
						}
					}
				}
				map_coords::AddCells(coords, spiral.Next());
			}
		}
	}
	// the group root that no longer burns hands the group to the first member that does
	if (fire.root == &fire && !fire.IsAboveReactionTemperature() && fire.next != nullptr)
	{
		for (FireEffect* member = &fire; member != nullptr; member = member->next)
		{
			if (member->IsAboveReactionTemperature())
			{
				HandRootTo(fire, *member);
				break;
			}
		}
	}
	// the fire's reaction (not for objects in the hand or flying. inferred: PhysicsObjects::IsFlying stands for the
	// flying flag)
	if (!traits::InHand(fire.object) && !physics::PhysicsObjects::IsFlying(fire.object))
	{
		// a reaction removed with the rest of its object's (a pot's removal takes them all): the id is
		// forgotten, and a new one made while it burns (inferred: the original keeps a pointer there)
		if (fire.reaction != 0 && effects::reactions::Find(fire.reaction) == nullptr)
		{
			fire.reaction = 0;
		}
		if (fire.reaction == 0)
		{
			if (fire.IsAboveReactionTemperature() && !traits::IsVillager(fire.object))
			{
				fire.reaction = effects::reactions::CreateReaction(fire.object, Reaction::ReactToFire,
				                                                   fire.player.value_or(PlayerNames::NEUTRAL), true);
			}
		}
		else if (!fire.IsAboveReactionTemperature())
		{
			fire.reaction = 0;
			RemoveReactions(fire.object, Reaction::ReactToFire);
			RemoveReactions(fire.object, Reaction::ReactToBurningObjectInHand);
		}
	}
	// the fire sound: the 2 fires nearest the camera with a fraction above 0.1
	sound::Consider(fire, fire.FireFraction() > 0.1f);
	fire.previous = fire.temperature;
}
} // namespace

bool fire::TraceEnabled()
{
	static const bool enabled = [] {
		const char* value = std::getenv("OPENBLACK_FIRE_TRACE");
		return value != nullptr && value[0] != '\0' && value[0] != '0';
	}();
	return enabled;
}

float FireEffect::CombustionThreshold() const
{
	if (object == entt::null)
	{
		return 0.0f;
	}
	return std::max(traits::CombustionTemperature(object), 40.0f);
}

float FireEffect::Ambient() const
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const components::Transform>(object);
	return AmbientTemperature(transform != nullptr ? transform->position : glm::vec3(0.0f));
}

float FireEffect::Capacity() const
{
	return std::max(traits::HeatCapacity(object), 1.0f);
}

bool FireEffect::IsOnFire() const
{
	return !(CombustionThreshold() > temperature);
}

bool FireEffect::IsAboveReactionTemperature() const
{
	return temperature >= 100.0f || !(CombustionThreshold() > temperature);
}

float FireEffect::FireFraction() const
{
	const float tc = CombustionThreshold();
	float fraction = (temperature - 0.8f * tc) / (MaxBurnTemperature() - 0.8f * tc);
	const float twiceLife = 2.0f * life::LifeOf(object);
	if (!(twiceLife > fraction))
	{
		fraction = twiceLife;
	}
	if (!(fraction > 0.0f))
	{
		return 0.0f;
	}
	return fraction < 1.0f ? fraction : 1.0f;
}

float FireEffect::FireRadius() const
{
	const float fraction = std::clamp(FireFraction(), 0.0f, 1.0f);
	return traits::DefaultFireRadius(object) * 1.25f * fraction;
}

float FireEffect::MaxFireRadius() const
{
	return object != entt::null ? traits::DefaultFireRadius(object) * 1.25f : 0.0f;
}

float FireEffect::SafeFireRadius() const
{
	if (object == entt::null)
	{
		return 0.0f;
	}
	const float maximum = MaxFireRadius();
	const float radius = FireRadius();
	return (radius > maximum ? maximum : radius) + 1.0f;
}

float FireEffect::FlameHeight() const
{
	float range = MaxBurnTemperature() - Ambient();
	if (range < 0.0001f)
	{
		range = 0.0001f;
	}
	float fraction = (temperature - Ambient()) / range;
	fraction = fraction > 0.0f ? (fraction < 1.0f ? fraction : 1.0f) : 0.0f;
	return traits::Height(object) * 1.25f * fraction;
}

float FireEffect::HeatContent() const
{
	return (temperature - Ambient()) * Capacity();
}

void FireEffect::AddHeat(float heat, float maxChange)
{
	float change = heat / Capacity();
	if (std::abs(maxChange) < std::abs(change))
	{
		change = maxChange;
	}
	temperature += change;
}

float FireEffect::GroupBurningRadius() const
{
	float sum = 0.0f;
	for (const auto* member = root; member != nullptr; member = member->next)
	{
		if (member->IsOnFire())
		{
			sum += traits::Radius(member->object);
		}
	}
	return sum;
}

float FireEffect::GroupBurningPriority() const
{
	float highest = 0.0f;
	for (const auto* member = root; member != nullptr; member = member->next)
	{
		if (member->object != entt::null)
		{
			const float priority = traits::BurningPriority(member->object);
			if (priority > highest)
			{
				highest = priority;
			}
		}
	}
	return highest;
}

FireEffect* FireEffect::NearestFireToFight(const glm::vec3& position) const
{
	if (root == nullptr)
	{
		return nullptr;
	}
	FireEffect* best = nullptr;
	float bestDistance = 1e7f;
	for (FireEffect* member = root; member != nullptr; member = member->next)
	{
		if (member->object == entt::null)
		{
			continue;
		}
		const auto centre = traits::FireCentre(member->object);
		const float objectRadius = traits::DefaultFireRadius(member->object);
		const float safe = member->SafeFireRadius();
		const float keep = safe < objectRadius ? objectRadius : safe;
		// the distance from the position to the fire centre, the 16.16 x/z delta converted to metres
		const float distance = Distance2D(position, centre) - keep;
		if (distance < bestDistance && member->IsAboveReactionTemperature())
		{
			bestDistance = distance;
			best = member;
		}
	}
	return best;
}

void fire::AddToFireGroup(FireEffect& self, FireEffect& other)
{
	AddToMyFireGroup(self, other);
}

FireEffect* fire::Find(entt::entity object)
{
	return Fires().FindByObject(object);
}

FireEffect* fire::Get(uint32_t id)
{
	return Fires().FindById(id);
}

float fire::AmbientTemperature([[maybe_unused]] const glm::vec3& position)
{
	return 24.7f;
}

float fire::GetTemperature(entt::entity object)
{
	if (const auto* fire = Find(object))
	{
		return fire->temperature;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const components::Transform>(object);
	return AmbientTemperature(transform != nullptr ? transform->position : glm::vec3(0.0f));
}

bool fire::IsOnFire(entt::entity object)
{
	const auto* fire = Find(object);
	return fire != nullptr && fire->IsOnFire();
}

FireEffect* fire::Create(entt::entity object, std::optional<PlayerNames> player, entt::entity source)
{
	// the original's "being deleted" test: ecs::IsAvailable
	if (!ecs::IsAvailable(object) || !traits::IsBurnReceiver(object, 100.0f) || traits::CannotBeSetOnFire(object) ||
	    traits::CombustionTemperature(object) == 0.0f)
	{
		return nullptr;
	}
	if (auto* existing = Find(object))
	{
		return existing; // (the port) one fire per object, as in the original
	}
	// a new fire
	auto owned = std::make_unique<FireEffect>();
	auto* fire = owned.get();
	fire->id = Fires().TakeId();
	fire->tag = Fires().TakeCreateTag();
	fire->object = object;
	fire->source = source;
	fire->player = player;
	fire->temperature = GetTemperature(object);
	fire->previous = fire->temperature;
	Fires().Insert(std::move(owned));
	graphic::Create(*fire); // its sprites
	fire->root = fire;
	fire->next = nullptr;
	traits::StartOnFire(object);
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fire: new fire {} on object {} (T {:.1f}, Tc {:.0f}, cap {:.1f})", fire->id,
		                   static_cast<int>(object), fire->temperature, fire->CombustionThreshold(), fire->Capacity());
	}
	return fire;
}

void fire::ToBeDeleted(FireEffect& fire)
{
	if ((fire.flags & FireEffect::k_Deleted) != 0)
	{
		return;
	}
	Fires().Unlist(fire);
	RemoveFromFireGroup(fire);
	fire.flags |= FireEffect::k_Deleted;
	if (fire.object != entt::null)
	{
		auto& registry = Locator::entitiesRegistry::value();
		if (registry.Valid(fire.object))
		{
			traits::EndOnFire(fire.object);
			if (fire.reaction != 0) // only with a reaction
			{
				fire.reaction = 0;
				RemoveReactions(fire.object, Reaction::ReactToFire);
				RemoveReactions(fire.object, Reaction::ReactToBurningObjectInHand);
			}
		}
		Fires().ForgetObject(fire.object);
		fire.object = entt::null;
	}
	graphic::Destroy(fire);
	sound::Free(fire);
	Fires().ForgetId(fire.id);
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fire: fire {} deleted", fire.id);
	}
}

void fire::SetTemperature(entt::entity object, float temperature, entt::entity source)
{
	FireEffect* fire = Find(object);
	if (fire == nullptr)
	{
		if (!(GetTemperature(object) < temperature))
		{
			return;
		}
		fire = Create(object, PlayerOfObject(object), source);
		if (fire == nullptr)
		{
			return;
		}
	}
	fire->temperature = temperature;
}

void fire::SetOnFire(entt::entity object, float speed)
{
	FireEffect* fire = Find(object);
	if (fire == nullptr)
	{
		fire = Create(object, PlayerOfObject(object), entt::null);
		if (fire == nullptr)
		{
			return;
		}
	}
	fire->temperature = fire->MaxBurnTemperature() * speed + fire->CombustionThreshold();
}

void fire::ApplyEffectToFireEffectIfNecessary(entt::entity object, const effects::EffectValues& values)
{
	// the object to affect is the object itself
	const entt::entity target = object;
	if (!ecs::IsAvailable(target))
	{
		return;
	}
	const float burn = values.numbers[static_cast<size_t>(effects::EffectValues::Number::Burn)];
	if (burn == 0.0f)
	{
		return;
	}
	FireEffect* fire = Find(target);
	if (fire == nullptr)
	{
		if (!(burn > 0.0f))
		{
			return;
		}
		fire = Create(target, values.player, values.appliedBy);
		if (fire == nullptr)
		{
			return;
		}
	}
	const float difference = fire->Ambient() + burn - fire->temperature;
	fire->AddHeat(HeatFromDifference(difference), difference);
	if (traits::IsVillager(target) && !villager_fire::IsInOnFireState(target))
	{
		villager_fire::SetupOnFire(target, 0);
	}
}

void fire::CheckToSeeIfObjectIsNearOnFireObject(entt::entity object)
{
	if (!traits::IsBurnReceiver(object, 100.0f))
	{
		return;
	}
	const auto centre = traits::FireCentre(object);
	if (!InBounds(centre))
	{
		return;
	}
	std::vector<entt::entity> objects;
	CellObjects(map_coords::CellOf(glm::vec2(centre.x, centre.z)), objects); // FindType(ANY)
	for (const auto other : objects)
	{
		if (other == object)
		{
			continue;
		}
		if (auto* fire = Find(other); fire != nullptr)
		{
			HeatTransfer(*fire, object);
		}
	}
}

void fire::CopyFire(entt::entity from, entt::entity to)
{
	auto* source = Find(from);
	if (source == nullptr || to == entt::null)
	{
		return;
	}
	FireEffect* fire = Find(to);
	if (fire == nullptr)
	{
		fire = Create(to, source->player, source->source);
		if (fire == nullptr)
		{
			return;
		}
	}
	AddToMyFireGroup(*source, *fire);
	const float temperature = fire->temperature > source->temperature ? fire->temperature : source->temperature;
	fire->previous = temperature;
	fire->temperature = temperature;
}

void fire::MoveFire(entt::entity from, entt::entity to)
{
	auto* fire = Find(from);
	if (fire == nullptr || to == entt::null)
	{
		return;
	}
	Fires().MoveObject(from, to, *fire);
	if (fire->reaction != 0)
	{
		// the reaction's initiator becomes the new object
		effects::reactions::SetInitiator(fire->reaction, to);
	}
	fire->object = to;
}

void fire::StartedMoving(entt::entity object, bool inHand)
{
	auto* fire = Find(object);
	if (fire == nullptr)
	{
		return;
	}
	RemoveFromFireGroup(*fire);
	if (fire->reaction != 0)
	{
		fire->reaction = 0;
		RemoveReactions(object, Reaction::ReactToFire);
	}
	if (inHand)
	{
		fire->reaction = effects::reactions::CreateReaction(object, Reaction::ReactToBurningObjectInHand,
		                                                    fire->player.value_or(PlayerNames::NEUTRAL), true);
	}
}

void fire::SetOutMagicHand(entt::entity object)
{
	auto* fire = Find(object);
	if (fire == nullptr)
	{
		return;
	}
	fire->reaction = 0;
	RemoveReactions(object, Reaction::ReactToBurningObjectInHand);
}

void fire::ProcessList()
{
	Fires().SetProcessTag(0);
	sound::RefreshDistances();
	// the list as it is now: the fires this turn creates join the head and wait for the next turn
	const std::vector<FireEffect*> list = Fires().List();
	for (auto* fire : list)
	{
		if ((fire->flags & FireEffect::k_Deleted) != 0 || fire->tag != Fires().ProcessTag())
		{
			continue;
		}
		Process(*fire);
	}
	sound::StartSlots();
	// the deleted ones are freed (deletion is deferred in the original too)
	Fires().FreeDeleted();
}

const std::vector<FireEffect*>& fire::All()
{
	return Fires().List();
}

void fire::Clear()
{
	for (auto* fire : Fires().List())
	{
		graphic::Destroy(*fire);
	}
	sound::Clear();
	Fires().Clear();
	traits::Clear();
}
