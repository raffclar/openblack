/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EditorNetworkEdits.h"

#include <cmath>

#include <utility>

#include "ECS/Components/Hand.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugCheatSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "EditorEntities.h"
#include "EditorMath.h"
#include "Input/LocalPlayer.h"
#include "Locator.h"

namespace openblack::editor
{

using player_commands::EditorEdit;

std::optional<entt::entity> LocalWrites::Place(const PlaceItem& item, glm::vec3 position, float yawRadians)
{
	const auto entity = editor::Place(item, position, yawRadians);
	return entity != entt::null ? std::optional(entity) : std::nullopt;
}

void LocalWrites::MoveTo(entt::entity entity, glm::vec3 position)
{
	editor::MoveTo(entity, position);
}

void LocalWrites::Turn(entt::entity entity, float radians)
{
	editor::Turn(entity, radians);
}

void LocalWrites::Remove(entt::entity entity)
{
	editor::Remove(entity);
}

std::optional<entt::entity> LocalWrites::Duplicate(entt::entity entity, glm::vec2 offset)
{
	return editor::Duplicate(entity, offset);
}

CommandWrites::CommandWrites(Submit submit, TurnNow turnNow, CopyOf copyOf)
    : _submit(std::move(submit))
    , _turnNow(std::move(turnNow))
    , _copyOf(std::move(copyOf))
{
}

std::optional<entt::entity> CommandWrites::Place(const PlaceItem& item, glm::vec3 position, float yawRadians)
{
	FlushNow();
	_submit(EditorEdit {.kind = EditorEdit::Kind::Place,
	                    .object = entt::null,
	                    .placeKind = static_cast<uint8_t>(item.kind),
	                    .placeType = item.type,
	                    .challenge = item.challenge,
	                    .position = position,
	                    .radians = yawRadians});
	return std::nullopt;
}

void CommandWrites::MoveTo(entt::entity entity, glm::vec3 position)
{
	_waiting[entity].position = position;
}

void CommandWrites::Turn(entt::entity entity, float radians)
{
	_waiting[entity].radians += radians;
}

void CommandWrites::Remove(entt::entity entity)
{
	FlushNow();
	_submit(EditorEdit {.kind = EditorEdit::Kind::Remove, .object = entity});
}

std::optional<entt::entity> CommandWrites::Duplicate(entt::entity entity, glm::vec2 offset)
{
	if (const auto copy = _copyOf ? _copyOf(entity, offset) : std::nullopt; copy.has_value())
	{
		FlushNow();
		_submit(*copy);
	}
	return std::nullopt;
}

void CommandWrites::Flush()
{
	const auto turn = _turnNow ? _turnNow() : uint32_t {0};
	if (_sentAt.has_value() && *_sentAt == turn)
	{
		return;
	}
	if (!_waiting.empty())
	{
		FlushNow();
		_sentAt = turn;
	}
}

void CommandWrites::FlushNow()
{
	// The turn goes first, so that a thing that turns about the middle of a group turns where it stood, then moves
	for (const auto& [entity, pending] : _waiting)
	{
		if (pending.radians != 0.0f)
		{
			_submit(EditorEdit {.kind = EditorEdit::Kind::Turn, .object = entity, .radians = pending.radians});
		}
		if (pending.position.has_value())
		{
			_submit(EditorEdit {.kind = EditorEdit::Kind::Move, .object = entity, .position = *pending.position});
		}
	}
	_waiting.clear();
}

std::optional<std::string_view> EditRefusal(const EditorEdit& edit) noexcept
{
	const auto finite = std::isfinite(edit.position.x) && std::isfinite(edit.position.y) && std::isfinite(edit.position.z) &&
	                    std::isfinite(edit.radians);
	if (!finite)
	{
		return "a value is not a finite number";
	}
	switch (edit.kind)
	{
	case EditorEdit::Kind::Place:
		if (edit.placeKind > static_cast<uint8_t>(PlaceKind::Highlight))
		{
			return "the palette has no such kind of item";
		}
		// A creature of the palette needs the creature tools of this computer, which a network game does not share
		if (edit.placeKind == static_cast<uint8_t>(PlaceKind::Creature))
		{
			return "the editor does not place a creature in a network game";
		}
		if (edit.placeType < 0)
		{
			return "the item has no such type";
		}
		return std::nullopt;
	case EditorEdit::Kind::Move:
	case EditorEdit::Kind::Turn:
	case EditorEdit::Kind::Remove:
		if (edit.object == entt::null)
		{
			return "the edit names no thing";
		}
		return std::nullopt;
	}
	return "the edit has no such kind";
}

bool EditableInNetworkGame(const ecs::Registry& registry, entt::entity entity)
{
	using namespace ecs::components;
	if (!registry.Valid(entity) || !registry.AllOf<Transform>(entity) || registry.AllOf<Hand>(entity))
	{
		return false;
	}
	return registry.AllOf<Mesh>(entity) || registry.AllOf<ScriptMarker>(entity);
}

bool ApplyEdit(const EditorEdit& edit)
{
	if (EditRefusal(edit).has_value() || !Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (edit.kind == EditorEdit::Kind::Place)
	{
		const PlaceItem item {
		    .kind = static_cast<PlaceKind>(edit.placeKind), .type = edit.placeType, .challenge = edit.challenge};
		// The palette's names give the types of each kind. A type out of its table makes nothing.
		const auto names = NameTables::Load();
		if (!names.has_value() || static_cast<size_t>(edit.placeType) >= names->Of(item.kind).size())
		{
			return false;
		}
		return editor::Place(item, edit.position, edit.radians) != entt::null;
	}
	if (!EditableInNetworkGame(registry, edit.object))
	{
		return false;
	}
	switch (edit.kind)
	{
	case EditorEdit::Kind::Move:
		editor::MoveTo(edit.object, edit.position);
		return true;
	case EditorEdit::Kind::Turn:
		editor::Turn(edit.object, edit.radians);
		return true;
	case EditorEdit::Kind::Remove:
		editor::Remove(edit.object);
		return true;
	case EditorEdit::Kind::Place:
		break;
	}
	return false;
}

std::optional<EditorEdit> CopyEditOf(const ecs::Registry& registry, entt::entity entity, glm::vec2 offset)
{
	const auto item = ItemOf(registry, entity);
	const auto pose = PoseOf(registry, entity);
	if (!item.has_value() || !pose.has_value() || item->kind == PlaceKind::Creature)
	{
		return std::nullopt;
	}
	// The copy stands as high above the land as the original, such as a bubble that floats
	const auto& position = pose->position;
	const auto aboveLand = position.y - LandHeight({position.x, position.z});
	const glm::vec2 point {position.x + offset.x, position.z + offset.y};
	// The palette turns a thing that is not a creature the other way to the angle that it faces
	return EditorEdit {.kind = EditorEdit::Kind::Place,
	                   .object = entt::null,
	                   .placeKind = static_cast<uint8_t>(item->kind),
	                   .placeType = item->type,
	                   .challenge = item->challenge,
	                   .position = {point.x, LandHeight(point) + aboveLand, point.y},
	                   .radians = -YawOf(pose->rotation)};
}

std::unique_ptr<EditorWrites> MakeNetworkWrites()
{
	return std::make_unique<CommandWrites>(
	    [](player_commands::Action action) {
		    if (Locator::playerCommandSystem::has_value())
		    {
			    Locator::playerCommandSystem::value().Submit(std::move(action));
		    }
	    },
	    []() { return Locator::time::has_value() ? Locator::time::value().GetTurn() : uint32_t {0}; },
	    [](entt::entity entity, glm::vec2 offset) -> std::optional<EditorEdit> {
		    if (!Locator::entitiesRegistry::has_value())
		    {
			    return std::nullopt;
		    }
		    return CopyEditOf(Locator::entitiesRegistry::value(), entity, offset);
	    });
}

void SendDebugCheat(debug_cheats::Cheat cheat, bool on)
{
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(
		    player_commands::DebugCheat {.cheat = static_cast<uint8_t>(cheat), .on = on});
	}
}

uint32_t OwnDebugCheats()
{
	if (!Locator::debugCheatSystem::has_value())
	{
		return 0;
	}
	const auto* hands = Locator::handSystem::has_value() ? &Locator::handSystem::value() : nullptr;
	return Locator::debugCheatSystem::value().CheatsOf(input::LocalPlayer(hands));
}

} // namespace openblack::editor
