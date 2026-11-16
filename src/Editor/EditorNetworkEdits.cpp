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
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugCheatSystemInterface.h"
#include "ECS/Systems/EditorSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/VillagerAge.h"
#include "EditorEntities.h"
#include "EditorHistory.h"
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

void LocalWrites::ScaleTo(entt::entity entity, float firstAxis)
{
	editor::ScaleTo(entity, firstAxis);
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

void CommandWrites::ScaleTo(entt::entity entity, float firstAxis)
{
	FlushNow();
	_submit(EditorEdit {.kind = EditorEdit::Kind::Scale, .object = entity, .scale = firstAxis});
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
	                    std::isfinite(edit.radians) && std::isfinite(edit.scale);
	if (!finite)
	{
		return "a value is not a finite number";
	}
	switch (edit.kind)
	{
	case EditorEdit::Kind::Place:
		if (edit.placeKind > static_cast<uint8_t>(k_LastPlaceKind))
		{
			return "the palette has no such kind of item";
		}
		// A creature of the palette goes as a new creature of the creature tools, with the settings of the tools
		if (edit.placeKind == static_cast<uint8_t>(PlaceKind::Creature))
		{
			return "a creature goes as a new creature of the creature tools";
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
	case EditorEdit::Kind::Scale:
		if (edit.object == entt::null)
		{
			return "the edit names no thing";
		}
		if (edit.scale < k_SmallestEditorScale || edit.scale > k_LargestEditorScale)
		{
			return "the scale is not in the range of the editor";
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
		const auto made = editor::Place(item, edit.position, edit.radians);
		// The thing goes in the list of the things that the editor made, which a save of the land files writes
		if (made != entt::null && Locator::editorSystem::has_value())
		{
			Locator::editorSystem::value().GetHistory().AddMade(made);
		}
		return made != entt::null;
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
	case EditorEdit::Kind::Scale:
		editor::ScaleTo(edit.object, edit.scale);
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

std::optional<std::string_view> VillagerEditRefusal(const player_commands::EditorVillagerEdit& edit) noexcept
{
	using Field = player_commands::EditorVillagerEdit::Field;
	using ecs::components::LivingAction;
	using ecs::components::Villager;
	if (edit.villager == entt::null)
	{
		return "the edit names no villager";
	}
	if (!std::isfinite(edit.amount))
	{
		return "a value is not a finite number";
	}
	const auto below = [&edit](auto count) { return edit.number < static_cast<uint32_t>(count); };
	switch (edit.field)
	{
	case Field::Life:
		return edit.amount >= 0.0f && edit.amount <= 1.0f ? std::nullopt
		                                                  : std::optional<std::string_view>("the life is not from 0 to 1");
	case Field::Food:
		return edit.amount >= 0.0f ? std::nullopt : std::optional<std::string_view>("the food is below zero");
	case Field::Age:
		return std::nullopt;
	case Field::LifeStage:
		return below(Villager::LifeStage::_COUNT) ? std::nullopt : std::optional<std::string_view>("no such life stage");
	case Field::Sex:
		return below(Villager::Sex::_COUNT) ? std::nullopt : std::optional<std::string_view>("no such sex");
	case Field::Tribe:
		return below(Tribe::_COUNT) ? std::nullopt : std::optional<std::string_view>("no such tribe");
	case Field::Role:
		return below(VillagerNumber::_COUNT) ? std::nullopt : std::optional<std::string_view>("no such role");
	case Field::State:
		if (edit.index >= static_cast<uint8_t>(LivingAction::Index::_Count))
		{
			return "no such slot of states";
		}
		return below(VillagerStates::_COUNT) ? std::nullopt : std::optional<std::string_view>("no such state");
	}
	return "the edit has no such field";
}

bool ApplyVillagerEdit(ecs::Registry& registry, const ecs::systems::LivingActionSystemInterface* actions,
                       const player_commands::EditorVillagerEdit& edit)
{
	using Field = player_commands::EditorVillagerEdit::Field;
	using ecs::components::LivingAction;
	using ecs::components::Villager;
	if (VillagerEditRefusal(edit).has_value() || !registry.Valid(edit.villager) ||
	    !registry.AllOf<Villager, LivingAction>(edit.villager))
	{
		return false;
	}
	auto& villager = registry.Get<Villager>(edit.villager);
	switch (edit.field)
	{
	case Field::Life:
		villager.life = edit.amount;
		break;
	case Field::Age:
		ecs::villager_age::SetBirthTurnForAge(villager, edit.number);
		break;
	case Field::Food:
		villager.food = edit.amount;
		break;
	case Field::LifeStage:
		villager.lifeStage = static_cast<Villager::LifeStage>(edit.number);
		break;
	case Field::Sex:
		villager.sex = static_cast<Villager::Sex>(edit.number);
		break;
	case Field::Tribe:
		villager.tribe = static_cast<Tribe>(edit.number);
		break;
	case Field::Role:
		villager.number = static_cast<VillagerNumber>(edit.number);
		break;
	case Field::State:
		if (actions == nullptr)
		{
			return false;
		}
		actions->VillagerSetState(registry.Get<LivingAction>(edit.villager), static_cast<LivingAction::Index>(edit.index),
		                          static_cast<VillagerStates>(edit.number), true);
		break;
	}
	registry.SetDirty();
	return true;
}

void SendVillagerEdit(const player_commands::EditorVillagerEdit& edit)
{
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(edit);
	}
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

story_land_skip::GameFacts OwnStoryGame()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return {.networkGame = true};
	}
	const auto& context = Locator::entitiesRegistry::value().Context();
	return {.networkGame = Locator::time::has_value() && Locator::time::value().IsNetworkGame(),
	        .skirmish = context.skirmish,
	        .landNumber = context.mapScriptGlobals.landNumber};
}

bool SendStoryLandSkip(int32_t land)
{
	if (!story_land_skip::Shown(OwnStoryGame()) || !story_land_skip::GoesAsCommand(land) ||
	    !Locator::playerCommandSystem::has_value())
	{
		return false;
	}
	Locator::playerCommandSystem::value().Submit(player_commands::DebugLandSkip {.land = static_cast<uint8_t>(land)});
	return true;
}

} // namespace openblack::editor
