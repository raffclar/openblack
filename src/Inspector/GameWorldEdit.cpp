/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameWorldEdit.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <utility>

#include "Audio/AudioManagerInterface.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "Editor/EditorEntities.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;

namespace
{

/// The kinds the editor's palette places, by the names the inspector gives them
constexpr std::array<std::pair<std::string_view, editor::PlaceKind>, 9> k_Kinds {{
    {"creature", editor::PlaceKind::Creature},
    {"villager", editor::PlaceKind::Villager},
    {"building", editor::PlaceKind::Building},
    {"tree", editor::PlaceKind::Tree},
    {"feature", editor::PlaceKind::Feature},
    {"mobile_object", editor::PlaceKind::MobileObject},
    {"mobile_static", editor::PlaceKind::MobileStatic},
    {"dispenser", editor::PlaceKind::Dispenser},
    {"miracle_bubble", editor::PlaceKind::MiracleBubble},
}};

std::optional<editor::PlaceKind> PlaceKindOf(std::string_view kind)
{
	const auto found = std::ranges::find(k_Kinds, kind, &std::pair<std::string_view, editor::PlaceKind>::first);
	return found == k_Kinds.end() ? std::nullopt : std::optional(found->second);
}

/// The map's cell a point is in, none off the map
std::optional<glm::ivec2> CellAt(glm::vec3 position)
{
	constexpr float k_CellSize = 10.0f;
	const auto size = glm::vec2(ecs::MapInterface::k_GridSize) * k_CellSize;
	if (position.x < 0.0f || position.z < 0.0f || position.x >= size.x || position.z >= size.y)
	{
		return std::nullopt;
	}
	const auto cell = ecs::MapInterface::GetGridCell(position);
	return glm::ivec2(cell.x, cell.y);
}

/// Why the inspector won't take a thing out, if it won't: the game keeps these, or something holds them
std::string RefusalToRemove(const ecs::Registry& registry, entt::entity entity, RemoveHow how)
{
	if (registry.AnyOf<Player, Hand>(entity))
	{
		return "a player or a hand is never taken out";
	}
	if (registry.AllOf<Creature>(entity))
	{
		return "a creature is never taken out of the world by the game's removal: its leash, mind and fights hold it";
	}
	if (registry.AllOf<Town>(entity))
	{
		return "a town is not taken out on its own: take out its buildings and people";
	}
	if (how == RemoveHow::Remove && registry.AllOf<Temple>(entity))
	{
		return "the temple goes through its destruction: destroy it with how = \"effect\"";
	}
	if (const auto* held = registry.TryGet<const InHand>(entity); held != nullptr)
	{
		return "a hand holds it: let it go first";
	}
	if (registry.AllOf<HeldByCreature>(entity))
	{
		return "a creature holds it: let it go first";
	}
	return {};
}

} // namespace

std::vector<std::string> GameWorldEdit::Kinds() const
{
	std::vector<std::string> kinds;
	for (const auto& [name, kind] : k_Kinds)
	{
		kinds.emplace_back(name);
	}
	return kinds;
}

std::vector<std::string> GameWorldEdit::TypeNames(std::string_view kind) const
{
	const auto place = PlaceKindOf(kind);
	if (!place.has_value())
	{
		return {};
	}
	const auto tables = editor::NameTables::Load();
	return tables.has_value() ? tables->Of(*place) : std::vector<std::string> {};
}

float GameWorldEdit::GroundHeight(glm::vec2 point) const
{
	return editor::LandHeight(point);
}

std::variant<entt::entity, std::string> GameWorldEdit::Create(std::string_view kind, int32_t type, glm::vec3 position,
                                                              float yaw)
{
	const auto place = PlaceKindOf(kind);
	if (!place.has_value())
	{
		return "no kind " + std::string(kind);
	}
	if (!Locator::entitiesRegistry::has_value() || !Locator::infoConstants::has_value())
	{
		return std::string("no land is loaded");
	}
	const auto entity = editor::Place({.kind = *place, .type = type}, position, yaw);
	if (entity == entt::null)
	{
		return "the game made no " + std::string(kind) + " of that type there";
	}
	return entity;
}

std::string GameWorldEdit::Move(entt::entity entity, glm::vec3 position)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.AllOf<Transform>(entity))
	{
		return "it has no place to move";
	}
	if (registry.AnyOf<InHand, HeldByCreature>(entity))
	{
		return "something holds it: let it go first";
	}
	if (registry.AllOf<InPhysics>(entity) && Locator::dynamicsSystem::has_value())
	{
		// Put where it is asked, it stops flying
		Locator::dynamicsSystem::value().RemoveObject(entity, false, false);
	}
	editor::MoveTo(entity, position);
	if (Locator::entitiesMap::has_value())
	{
		Locator::entitiesMap::value().Refile(entity);
	}
	return {};
}

std::string GameWorldEdit::Turn(entt::entity entity, float yaw)
{
	if (!Locator::entitiesRegistry::value().AllOf<Transform>(entity))
	{
		return "it has no place to turn";
	}
	editor::Turn(entity, yaw);
	return {};
}

std::string GameWorldEdit::Remove(entt::entity entity, RemoveHow how)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto refusal = RefusalToRemove(registry, entity, how); !refusal.empty())
	{
		return refusal;
	}
	if (how == RemoveHow::Effect)
	{
		ecs::world_objects::DestroyedByEffect(entity, {.killer = std::nullopt, .weight = 0.0f});
		return {};
	}
	// Out of the physics and silent first, then out of the world as the game takes things out
	if (Locator::dynamicsSystem::has_value())
	{
		Locator::dynamicsSystem::value().RemoveObject(entity, false, false);
	}
	if (Locator::audio::has_value())
	{
		Locator::audio::value().StopOwnedSounds(entity);
	}
	editor::Remove(entity);
	return {};
}

void GameWorldEdit::Changed(entt::entity entity, std::string_view component)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.SetLayoutDirty();
	// A thing moved by hand goes into its new cells, unless something holds it or it flies
	if (component == "Transform" && registry.Valid(entity) && !registry.AnyOf<InHand, HeldByCreature, InPhysics>(entity) &&
	    Locator::entitiesMap::has_value())
	{
		Locator::entitiesMap::value().Refile(entity);
	}
}

Presence GameWorldEdit::PresenceOf(entt::entity entity, std::optional<glm::vec3> lastPosition) const
{
	Presence presence;
	if (!Locator::entitiesRegistry::has_value())
	{
		return presence;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	presence.exists = registry.Valid(entity);
	if (presence.exists)
	{
		if (const auto* transform = registry.TryGet<const Transform>(entity); transform != nullptr)
		{
			lastPosition = transform->position;
		}
	}
	if (lastPosition.has_value())
	{
		presence.cell = CellAt(*lastPosition);
	}
	if (presence.cell.has_value() && Locator::entitiesMap::has_value())
	{
		const auto all = Locator::entitiesMap::value().GetAllInCell(*presence.cell);
		presence.inCell = std::ranges::find(all, entity) != all.end();
	}
	if (Locator::dynamicsSystem::has_value())
	{
		presence.inPhysics = Locator::dynamicsSystem::value().Find(entity) != nullptr;
	}
	return presence;
}
