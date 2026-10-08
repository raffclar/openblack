/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MagicTeleport.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <string>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Audio/Audio.h"
#include "Audio/Game/BankTables.h"
#include "Common/GUtilsDistance.h"
#include "Debug/DebugEnv.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/MagicTeleport.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerTeleport.h"
#include "ECS/ToBeDeleted.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Chants.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/Spell.h"
#include "Magic/MagicTables.h"
#include "Particles/PSysManager.h"
#include "Particles/ParticleTypes.h"

using namespace openblack;
using namespace openblack::magic;
using ecs::components::MagicTeleport;
using ecs::components::Transform;

namespace
{
auto& Reg()
{
	return Locator::entitiesRegistry::value();
}

MagicTeleport* StoneOf(entt::entity stone)
{
	auto& registry = Reg();
	return registry.Valid(stone) ? registry.TryGet<MagicTeleport>(stone) : nullptr;
}

std::vector<entt::entity>& ListOf(PlayerNames player)
{
	return players::MagicOf(player).teleportStones;
}

/// The flat distance in metres of two map positions, each made a MapCoords (x and z truncated to 16.16)
float Distance2D(const glm::vec3& a, const glm::vec3& b)
{
	return gutils::GetDistanceInMetres(a, b);
}

/// A 3D IN_GAME sound tag (mode 2, no loop, no delay) at the point (x, the land + the height, z) that plays once.
/// `mapPosition` is a map position (x, height above the land, z), as magic::ToMap gives.
void PlayInGameSample(int sample, const glm::vec3& mapPosition)
{
	audio::tags::CreateAtMapCoords(mapPosition.x, mapPosition.z, mapPosition.y, sample, false, 2, 0, false, true,
	                               audio::SfxBank::InGame, 0);
}

/// For one stone: the travellers that are gone, not available or not reacting to the stone's reaction lose every
/// entry
void ProcessTravellers(entt::entity stone)
{
	auto* component = StoneOf(stone);
	if (component == nullptr)
	{
		return;
	}
	auto& list = component->travellers;
	for (size_t i = 0; i < list.size();)
	{
		const auto living = list[i].living;
		const bool keep = living != entt::null && ecs::IsAvailable(living) &&
		                  ecs::villager_teleport::CurrentReaction(living) == component->reaction;
		if (keep)
		{
			++i;
			continue;
		}
		std::erase_if(list, [living](const MagicTeleport::Traveller& entry) { return entry.living == living; });
		i = 0; // the original restarts from the node after the one it looked at; with the entries gone it is the same
	}
}
} // namespace

// ---- pure rules ----

int32_t teleport::FastDistance(const glm::vec3& a, const glm::vec3& b)
{
	// the two MapCoords (x and z in fixed point)
	return gutils::FastDistance(map_coords::FromMetres(glm::vec2(a.x, a.z)), map_coords::FromMetres(glm::vec2(b.x, b.z)));
}

bool teleport::IsWorthTheDetour(const glm::vec3& living, const glm::vec3& destination, const glm::vec3& stone,
                                const glm::vec3& other)
{
	// the integer distances compared as floats: 1.2 x (d(l, this) + d(T, dest)) < d(l, dest)
	const auto direct = static_cast<float>(FastDistance(living, destination));
	const auto detour = static_cast<float>(FastDistance(living, stone) + FastDistance(other, destination));
	return detour * k_DetourFactor < direct;
}

int teleport::ChooseTarget(const glm::vec3& living, const glm::vec3& destination, const std::vector<glm::vec3>& others,
                           bool force, float* saving)
{
	float best = force ? -1000000.0f : 0.0f;
	int target = -1;
	for (size_t i = 0; i < others.size(); ++i)
	{
		const float s = Distance2D(destination, living) - Distance2D(destination, others[i]);
		if (s > best) // strictly greater
		{
			best = s;
			target = static_cast<int>(i);
		}
	}
	if (saving != nullptr)
	{
		*saving = best;
	}
	return target;
}

float teleport::JumpCost(float saving, float costPerKilometer)
{
	return -saving * costPerKilometer * 0.001f;
}

int teleport::FindRouteStone(const std::vector<glm::vec3>& stones, const glm::vec3& from, const glm::vec3& to,
                             float maxDistance)
{
	float nearFrom = maxDistance;
	float nearTo = maxDistance;
	int best = -1;
	for (size_t i = 0; i < stones.size(); ++i)
	{
		const float d1 = Distance2D(stones[i], from);
		if (d1 < nearFrom)
		{
			nearFrom = d1;
			best = static_cast<int>(i);
		}
		const float d2 = Distance2D(stones[i], to);
		if (d2 < nearTo)
		{
			nearTo = d2;
		}
	}
	return nearFrom + nearTo < maxDistance ? best : -1;
}

// ---- the stones ----

bool teleport::TraceEnabled()
{
	static const bool trace = debug_env::TeleportTrace() || debug_env::SpellTrace();
	return trace;
}

glm::vec3 teleport::MapPositionOf(entt::entity object)
{
	const auto* transform = Reg().TryGet<const Transform>(object);
	return transform != nullptr ? ToMap(transform->position) : glm::vec3(0.0f);
}

std::optional<PlayerNames> teleport::PlayerOf(entt::entity stone)
{
	const auto* component = StoneOf(stone);
	if (component == nullptr)
	{
		return std::nullopt;
	}
	return component->player;
}

const std::vector<entt::entity>& teleport::StonesOf(PlayerNames player)
{
	return ListOf(player);
}

entt::entity teleport::Create(const glm::vec3& mapPosition, entt::entity spell)
{
	auto& registry = Reg();
	// a mobile static at the position, with the teleport's info and scale 1.0
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	registry.Assign<Transform>(entity, ToWorld(mapPosition), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& stone = registry.Assign<MagicTeleport>(entity);
	stone.spell = spell;
	// the stone's player is the spell's
	if (registry.Valid(spell))
	{
		if (const auto* component = registry.TryGet<const ecs::components::Spell>(spell);
		    component != nullptr && component->hasPlayer)
		{
			stone.player = component->player;
		}
	}
	if (stone.player.has_value())
	{
		// the head of the player's list, one more
		auto& list = ListOf(*stone.player);
		list.insert(list.begin(), entity);
		// a REACT_TO_TELEPORT reaction for the player: spread at once over the cells in its radius
		stone.reaction =
		    ecs::effects::reactions::CreateReaction(entity, openblack::Reaction::ReactToTeleport, *stone.player, false);
	}
	// the stone goes into its cells, then, unless the object is already being deleted, the vortex PSys (no spell,
	// PT 73, the world position, no direction, magnitude 1.0) with the stone's player
	ecs::map_cells::InsertMapObject(entity);
	const auto file = psys::ParticleTypeFile(static_cast<ParticleType>(k_VortexParticleType));
	if (!file.empty())
	{
		auto& component = registry.Get<MagicTeleport>(entity);
		component.psys = psys::manager::StartForSpell(std::string(file), ToWorld(mapPosition), glm::vec3(0.0f), 1.0f, nullptr);
		if (component.psys != 0)
		{
			psys::manager::SetPerFrame(component.psys); // the stone's draw steps it with the frame time
			if (auto* effect = psys::manager::Find(component.psys); effect != nullptr)
			{
				effect->SetPlayer(component.player.has_value() ? static_cast<int>(*component.player) : -1);
			}
		}
	}
	// the scale goes down to a hundredth: the stone has no mesh, so nothing shows it
	registry.Get<MagicTeleport>(entity).scale *= 0.01f;
	registry.SetDirty();
	if (TraceEnabled())
	{
		const auto& component = registry.Get<const MagicTeleport>(entity);
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Teleport: stone {} at ({:.1f}, {:.1f}) for spell {} player {} (stones {}) reaction {} psys {}",
		                   static_cast<uint32_t>(entity), mapPosition.x, mapPosition.z, static_cast<uint32_t>(spell),
		                   component.player.has_value() ? static_cast<int>(*component.player) : -1,
		                   component.player.has_value() ? ListOf(*component.player).size() : 0, component.reaction,
		                   component.psys);
	}
	return entity;
}

void teleport::ToBeDeleted(entt::entity stone)
{
	auto& registry = Reg();
	auto* component = StoneOf(stone);
	if (component == nullptr)
	{
		return;
	}
	// its reactions, the vortex, out of the player's list, the travellers' list; then the mobile static's own deletion
	ecs::effects::reactions::RemoveAllReactionsInitiatedByObject(stone);
	if (component->psys != 0)
	{
		psys::manager::Delete(component->psys);
		component->psys = 0;
	}
	if (component->player.has_value())
	{
		std::erase(ListOf(*component->player), stone);
	}
	component->travellers.clear();
	if (TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Teleport: stone {} deleted", static_cast<uint32_t>(stone));
	}
	ecs::map_cells::RemoveMapObject(stone); // out of its cells
	registry.Destroy(stone);
	registry.SetDirty();
}

bool teleport::ShouldLivingThingReact(entt::entity stone, entt::entity living)
{
	const auto* component = StoneOf(stone);
	if (component == nullptr || !component->player.has_value() || !ecs::villager_teleport::IsMoving(living))
	{
		return false;
	}
	const auto destination = ecs::villager_teleport::FinalDestination(living);
	const auto at = MapPositionOf(living);
	const auto here = MapPositionOf(stone);
	for (const auto other : ListOf(*component->player))
	{
		if (other == stone)
		{
			continue;
		}
		if (IsWorthTheDetour(at, destination, here, MapPositionOf(other)))
		{
			return true;
		}
	}
	return false;
}

void teleport::RegisterDestination(entt::entity stone, entt::entity living, const glm::vec3& destination)
{
	auto* component = StoneOf(stone);
	if (component == nullptr)
	{
		return;
	}
	auto& list = component->travellers;
	std::erase_if(list, [living](const MagicTeleport::Traveller& entry) { return entry.living == living; });
	if (living != entt::null)
	{
		list.insert(list.begin(), {living, destination});
	}
}

int teleport::DoTeleport(entt::entity stone, entt::entity living, bool force)
{
	auto* component = StoneOf(stone);
	if (component == nullptr || !component->player.has_value())
	{
		return 0;
	}
	const auto entry = std::find_if(component->travellers.begin(), component->travellers.end(),
	                                [living](const MagicTeleport::Traveller& t) { return t.living == living; });
	if (entry == component->travellers.end())
	{
		return 0;
	}
	const auto destination = entry->destination;
	const auto at = MapPositionOf(living);
	std::vector<entt::entity> others;
	std::vector<glm::vec3> positions;
	for (const auto other : ListOf(*component->player))
	{
		if (other != stone)
		{
			others.push_back(other);
			positions.push_back(MapPositionOf(other));
		}
	}
	float saving = 0.0f;
	const int index = ChooseTarget(at, destination, positions, force, &saving);
	if (index < 0)
	{
		return 0;
	}
	const auto target = others[static_cast<size_t>(index)];
	const auto spell = component->spell;
	auto& registry = Reg();
	if (spell != entt::null && registry.Valid(spell) && registry.AllOf<ecs::components::Spell>(spell))
	{
		// a point spell event at this stone (velocity 0, strength 1, no shields, no target)
		const auto here = MapPositionOf(stone);
		const psys::SpellEventInfo event {.type = psys::SpellEventInfo::Type::Point,
		                                  .position = glm::vec3(here.x, here.y, here.z)};
		OpsOf(registry.Get<const ecs::components::Spell>(spell).spellClass).spellEvent(spell, event);
		// the teleport info's costPerKilometer; the jump is paid for, forced
		if (registry.Valid(spell) && registry.AllOf<ecs::components::Spell>(spell))
		{
			const auto* info = GetMagicInfoAs<GMagicTeleportInfo>(Locator::infoConstants::value(),
			                                                      registry.Get<const ecs::components::Spell>(spell).magicType);
			const float perKilometre = info != nullptr ? info->costPerKilometer : 0.0f;
			auto& spellComponent = registry.Get<ecs::components::Spell>(spell);
			const float before = spellComponent.chants;
			chants::PayFor(spellComponent, ChantContextOf(spell), JumpCost(saving, perKilometre), true);
			if (TraceEnabled())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Teleport: spell {} PayFor({:.2f}, forced): chants {:.2f} -> {:.2f}",
				                   static_cast<uint32_t>(spell), JumpCost(saving, perKilometre), before, spellComponent.chants);
			}
		}
	}
	// SPOT_VISUAL 14 where it is and where it goes. The original's 1.0 is not a duration (the spot visual keeps its
	// entry's own duration): here 0 = the entry's own life
	const auto targetPosition = MapPositionOf(target);
	psys::manager::CreateSpotVisual(k_SpotVisualVillagerTeleport, ToWorld(at), 0.0f, entt::null);
	psys::manager::CreateSpotVisual(k_SpotVisualVillagerTeleport, ToWorld(targetPosition), 0.0f, entt::null);
	if (TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Teleport: {} {} from stone {} ({:.1f}, {:.1f}) to stone {} ({:.1f}, {:.1f}) for ({:.1f}, {:.1f}): "
		                   "saving {:.2f} m{}",
		                   registry.AllOf<ecs::components::Villager>(living) ? "villager" : "living",
		                   static_cast<uint32_t>(living), static_cast<uint32_t>(stone), at.x, at.z,
		                   static_cast<uint32_t>(target), targetPosition.x, targetPosition.z, destination.x, destination.z,
		                   saving, force ? " (forced)" : "");
	}
	MoveByTeleport(living, targetPosition);
	return 1;
}

bool teleport::ValidToApplyVillagerDirectly(entt::entity stone, entt::entity villager)
{
	const auto* component = StoneOf(stone);
	if (component == nullptr)
	{
		return false;
	}
	const auto villagerPlayer = ecs::villager_teleport::PlayerOf(villager);
	if (villagerPlayer != component->player)
	{
		return false;
	}
	// the player's stone count != 1 (a stone without a player would match a villager without one: never here)
	return component->player.has_value() && ListOf(*component->player).size() != 1;
}

int teleport::ApplyVillagerDirectly(entt::entity stone, entt::entity villager)
{
	if (StoneOf(stone) == nullptr)
	{
		return 0x17;
	}
	// FLYING, the interface puts it down at the stone, LANDED, DecideWhatToDo
	ecs::villager_teleport::LandAt(villager, MapPositionOf(stone));
	// the final destination is read after DecideWhatToDo. (approximate) the original's DecideWhatToDo may have
	// chosen a new walk by then; openblack's only sets the state, so the goal read here is still the one it had
	RegisterDestination(stone, villager, ecs::villager_teleport::FinalDestination(villager));
	if (DoTeleport(stone, villager, true) == 1)
	{
		ecs::villager_teleport::DecideWhatToDo(villager);
		return 1;
	}
	return 0x17;
}

void teleport::MoveByTeleport(entt::entity living, const glm::vec3& mapPosition)
{
	auto& registry = Reg();
	auto* transform = registry.TryGet<Transform>(living);
	if (transform == nullptr)
	{
		return;
	}
	// G_SpellTeleportEnergiseGo where the living is and G_SpellTeleportEnergiseArrive where it goes, bank IN_GAME
	PlayInGameSample(39, ToMap(transform->position));
	PlayInGameSample(38, mapPosition);
	const auto world = ToWorld(glm::vec3(mapPosition.x, 0.0f, mapPosition.z));
	// the new position, at the land; at the head of the new cell's list when
	// the cell changes
	ecs::map_cells::MoveMapObject(living, world);
	ecs::villager_teleport::OnMoved(living);
	registry.SetDirty();
}

bool teleport::AnyMultiCellStaticNear(const glm::vec3& mapPosition, float radius)
{
	// the teleport's cast check: a spiral of max(3, ceil(2r / 10))^2 cells, every object in each, d < r, stopped at
	// best x 1.5 + 10 (ecs::map_cells::FindNearestInSpiral). The stones are in the cells too (MultiMapFixed)
	const auto coords = map_coords::FromMetres(glm::vec2(mapPosition.x, mapPosition.z));
	return ecs::map_cells::FindNearestInSpiral(coords, ecs::map_cells::IsMultiCellStaticClass, radius) != entt::null;
}

void teleport::ProcessPlayers()
{
	// in each player's turn, every stone that is available
	for (size_t p = 0; p < static_cast<size_t>(PlayerNames::_COUNT); ++p)
	{
		for (const auto stone : std::vector<entt::entity>(ListOf(static_cast<PlayerNames>(p))))
		{
			ProcessTravellers(stone);
		}
	}
}

void teleport::UpdateFrame(float seconds)
{
	auto& registry = Reg();
	std::vector<entt::entity> stones;
	registry.Each<MagicTeleport>([&stones](entt::entity entity, const MagicTeleport&) { stones.push_back(entity); });
	for (const auto entity : stones)
	{
		auto& component = registry.Get<MagicTeleport>(entity);
		if (component.psys == 0)
		{
			continue;
		}
		// the vortex moves to the stone's world position, is processed (power 1, enabled) with the frame time and drawn
		const auto& transform = registry.Get<const Transform>(entity);
		psys::manager::SetOrigin(component.psys, transform.position);
		psys::ProcessInfo info {
		    .power = 1.0f,
		    .enabled = true,
		};
		if (!psys::manager::ProcessForSpell(component.psys, info, seconds))
		{
			component.psys = 0;
		}
	}
}

std::vector<entt::entity> teleport::HandCollisionStones()
{
	std::vector<entt::entity> result;
	auto& registry = Reg();
	registry.Each<MagicTeleport>([&](entt::entity entity, const MagicTeleport&) {
		if (SeedOf(entity) != entt::null)
		{
			result.push_back(entity);
		}
	});
	return result;
}

uint32_t teleport::ReactionOf(entt::entity stone)
{
	const auto* component = StoneOf(stone);
	return component != nullptr ? component->reaction : 0;
}

entt::entity teleport::SeedOf(entt::entity stone)
{
	const auto* component = StoneOf(stone);
	auto& registry = Reg();
	if (component == nullptr || component->spell == entt::null || !registry.Valid(component->spell))
	{
		return entt::null;
	}
	const auto* spell = registry.TryGet<const ecs::components::Spell>(component->spell);
	if (spell == nullptr || spell->seed == entt::null || !registry.Valid(spell->seed) ||
	    !registry.AllOf<ecs::components::SpellSeed>(spell->seed))
	{
		return entt::null;
	}
	return spell->seed;
}

void teleport::Clear()
{
	for (size_t p = 0; p < static_cast<size_t>(PlayerNames::_COUNT); ++p)
	{
		ListOf(static_cast<PlayerNames>(p)).clear();
	}
	ecs::villager_teleport::Clear();
}
