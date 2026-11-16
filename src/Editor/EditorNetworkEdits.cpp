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

#include "Creature/CreatureMorph.h"
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
#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/VillagerAge.h"
#include "EditorEntities.h"
#include "EditorHistory.h"
#include "EditorMath.h"
#include "Input/LocalPlayer.h"
#include "Locator.h"
#include "Online/NetworkSession.h"

namespace openblack::editor
{

using player_commands::EditorEdit;
using player_commands::EditorRemake;

namespace
{
[[nodiscard]] bool Finite(const glm::vec3& value) noexcept
{
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] bool Finite(const glm::mat3& value) noexcept
{
	return Finite(value[0]) && Finite(value[1]) && Finite(value[2]);
}
} // namespace

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

void LocalWrites::SetPose(entt::entity entity, const Pose& pose)
{
	editor::SetPose(entity, pose);
}

MakeResult LocalWrites::Make(const Recipe& recipe)
{
	return {.entity = editor::Make(recipe)};
}

CommandWrites::CommandWrites(Submit submit, TurnNow turnNow, CopyOf copyOf, NewRequest newRequest)
    : _submit(std::move(submit))
    , _turnNow(std::move(turnNow))
    , _copyOf(std::move(copyOf))
    , _newRequest(std::move(newRequest))
{
}

void CommandWrites::SubmitEdit(EditorEdit edit)
{
	edit.request = _newRequest ? _newRequest() : Request {0};
	if (edit.object != entt::null)
	{
		_requestOf.insert_or_assign(edit.object, edit.request);
	}
	if (edit.kind == EditorEdit::Kind::Place)
	{
		_lastMakeRequest = edit.request;
	}
	_submit(edit);
}

void CommandWrites::SubmitMake(player_commands::Action action)
{
	if (auto* edit = std::get_if<EditorEdit>(&action))
	{
		SubmitEdit(*edit);
		return;
	}
	const auto request = _newRequest ? _newRequest() : Request {0};
	if (auto* remake = std::get_if<EditorRemake>(&action))
	{
		remake->request = request;
	}
	_lastMakeRequest = request;
	_submit(std::move(action));
}

std::optional<entt::entity> CommandWrites::Place(const PlaceItem& item, glm::vec3 position, float yawRadians)
{
	FlushNow();
	SubmitEdit(EditorEdit {.kind = EditorEdit::Kind::Place,
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
	SubmitEdit(EditorEdit {.kind = EditorEdit::Kind::Scale, .object = entity, .scale = firstAxis});
}

void CommandWrites::Remove(entt::entity entity)
{
	FlushNow();
	SubmitEdit(EditorEdit {.kind = EditorEdit::Kind::Remove, .object = entity});
}

std::optional<entt::entity> CommandWrites::Duplicate(entt::entity entity, glm::vec2 offset)
{
	if (auto copy = _copyOf ? _copyOf(entity, offset) : std::nullopt; copy.has_value())
	{
		FlushNow();
		SubmitMake(std::move(*copy));
	}
	return std::nullopt;
}

void CommandWrites::SetPose(entt::entity entity, const Pose& pose)
{
	FlushNow();
	// The editor does not send a pose that the world refuses, so that the history does not wait for it
	const auto edit = PoseEditOf(entity, pose);
	if (!EditRefusal(edit).has_value())
	{
		SubmitEdit(edit);
	}
}

MakeResult CommandWrites::Make(const Recipe& recipe)
{
	FlushNow();
	const auto remake = RemakeOf(recipe);
	if (RemakeRefusal(remake).has_value())
	{
		return {};
	}
	SubmitMake(remake);
	return {.request = _lastMakeRequest};
}

Request CommandWrites::TakeRequestOf(entt::entity entity)
{
	const auto found = _requestOf.find(entity);
	if (found == _requestOf.end())
	{
		return 0;
	}
	const auto request = found->second;
	_requestOf.erase(found);
	return request;
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
			SubmitEdit(EditorEdit {.kind = EditorEdit::Kind::Turn, .object = entity, .radians = pending.radians});
		}
		if (pending.position.has_value())
		{
			SubmitEdit(EditorEdit {.kind = EditorEdit::Kind::Move, .object = entity, .position = *pending.position});
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
	case EditorEdit::Kind::Pose:
		if (edit.object == entt::null)
		{
			return "the edit names no thing";
		}
		if (!Finite(edit.rotation) || !Finite(edit.keptAngles) || !Finite(edit.poseScale))
		{
			return "a value is not a finite number";
		}
		if (edit.poseScale.x < k_SmallestEditorScale || edit.poseScale.x > k_LargestEditorScale)
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

std::optional<entt::entity> ApplyEdit(const EditorEdit& edit)
{
	if (EditRefusal(edit).has_value() || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
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
			return std::nullopt;
		}
		const auto made = editor::Place(item, edit.position, edit.radians);
		// The thing goes in the list of the things that the editor made, which a save of the land files writes
		if (made != entt::null && Locator::editorSystem::has_value())
		{
			Locator::editorSystem::value().GetHistory().AddMade(made);
		}
		return made != entt::null ? std::optional(made) : std::nullopt;
	}
	if (!EditableInNetworkGame(registry, edit.object))
	{
		return std::nullopt;
	}
	switch (edit.kind)
	{
	case EditorEdit::Kind::Move:
		editor::MoveTo(edit.object, edit.position);
		return edit.object;
	case EditorEdit::Kind::Turn:
		editor::Turn(edit.object, edit.radians);
		return edit.object;
	case EditorEdit::Kind::Remove:
		editor::Remove(edit.object);
		return edit.object;
	case EditorEdit::Kind::Scale:
		editor::ScaleTo(edit.object, edit.scale);
		return edit.object;
	case EditorEdit::Kind::Pose:
		editor::SetPose(edit.object, PoseOf(edit));
		return edit.object;
	case EditorEdit::Kind::Place:
		break;
	}
	return std::nullopt;
}

EditorEdit PoseEditOf(entt::entity entity, const Pose& pose) noexcept
{
	return {.kind = EditorEdit::Kind::Pose,
	        .object = entity,
	        .position = pose.position,
	        .rotation = pose.rotation,
	        .keepsAngles = pose.keptAngles.has_value(),
	        .keptAngles = pose.keptAngles.value_or(glm::vec3 {0.0f}),
	        .poseScale = pose.scale};
}

Pose PoseOf(const EditorEdit& edit) noexcept
{
	return {.position = edit.position,
	        .rotation = edit.rotation,
	        .keptAngles = edit.keepsAngles ? std::optional(edit.keptAngles) : std::nullopt,
	        .scale = edit.poseScale};
}

EditorRemake RemakeOf(const Recipe& recipe) noexcept
{
	EditorRemake remake {.placeKind = static_cast<uint8_t>(recipe.item.kind),
	                     .placeType = recipe.item.type,
	                     .challenge = recipe.item.challenge,
	                     .position = recipe.pose.position,
	                     .rotation = recipe.pose.rotation,
	                     .keepsAngles = recipe.pose.keptAngles.has_value(),
	                     .keptAngles = recipe.pose.keptAngles.value_or(glm::vec3 {0.0f}),
	                     .scale = recipe.pose.scale};
	if (const auto& body = recipe.creature; body.has_value())
	{
		remake.creature = true;
		remake.owner = static_cast<uint8_t>(body->owner);
		remake.mind = body->mind;
		remake.size = body->size;
		remake.alignment = body->alignment;
		remake.fatness = body->fatness;
		remake.strength = body->strength;
	}
	return remake;
}

Recipe RecipeOf(const EditorRemake& remake) noexcept
{
	Recipe recipe {
	    .item = {.kind = static_cast<PlaceKind>(remake.placeKind), .type = remake.placeType, .challenge = remake.challenge},
	    .pose = {.position = remake.position,
	             .rotation = remake.rotation,
	             .keptAngles = remake.keepsAngles ? std::optional(remake.keptAngles) : std::nullopt,
	             .scale = remake.scale}};
	if (remake.creature)
	{
		recipe.creature = CreatureCopy {.owner = static_cast<PlayerNames>(remake.owner),
		                                .mind = remake.mind,
		                                .size = remake.size,
		                                .alignment = remake.alignment,
		                                .fatness = remake.fatness,
		                                .strength = remake.strength};
	}
	return recipe;
}

std::optional<std::string_view> RemakeRefusal(const EditorRemake& remake) noexcept
{
	const bool finite = Finite(remake.position) && Finite(remake.rotation) && Finite(remake.keptAngles) &&
	                    Finite(remake.scale) && std::isfinite(remake.size) && std::isfinite(remake.alignment) &&
	                    std::isfinite(remake.fatness) && std::isfinite(remake.strength);
	if (!finite)
	{
		return "a value is not a finite number";
	}
	if (remake.placeKind > static_cast<uint8_t>(k_LastPlaceKind))
	{
		return "the palette has no such kind of item";
	}
	if (remake.placeType < 0)
	{
		return "the item has no such type";
	}
	if (remake.creature != (remake.placeKind == static_cast<uint8_t>(PlaceKind::Creature)))
	{
		return "only a creature has the body of a creature";
	}
	if (!remake.creature)
	{
		if (remake.scale.x < k_SmallestEditorScale || remake.scale.x > k_LargestEditorScale)
		{
			return "the scale is not in the range of the editor";
		}
		return std::nullopt;
	}
	if (remake.placeType <= static_cast<int32_t>(CreatureType::Unknown) ||
	    remake.placeType >= static_cast<int32_t>(CreatureType::_COUNT))
	{
		return "the game has no such creature";
	}
	if (remake.owner >= static_cast<uint8_t>(PlayerNames::_COUNT))
	{
		return "the game has no such player";
	}
	if (remake.size < creature_morph::k_MinScale || remake.size > creature_morph::k_MaxScale)
	{
		return "the size is not in the range of the creature tools";
	}
	if (remake.alignment < -1.0f || remake.alignment > 1.0f || remake.fatness < 0.0f || remake.fatness > 1.0f ||
	    remake.strength < 0.0f || remake.strength > 1.0f)
	{
		return "a value of the body is not in its range";
	}
	return std::nullopt;
}

entt::entity ApplyRemake(const EditorRemake& remake)
{
	if (RemakeRefusal(remake).has_value() || !Locator::entitiesRegistry::has_value())
	{
		return entt::null;
	}
	// The palette's names give the types of each kind. A type out of its table makes nothing.
	if (!remake.creature)
	{
		const auto names = NameTables::Load();
		const auto kind = static_cast<PlaceKind>(remake.placeKind);
		if (!names.has_value() || static_cast<size_t>(remake.placeType) >= names->Of(kind).size())
		{
			return entt::null;
		}
	}
	return editor::Make(RecipeOf(remake));
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

std::optional<player_commands::Action> CopyCommandOf(const ecs::Registry& registry, entt::entity entity, glm::vec2 offset)
{
	const auto item = ItemOf(registry, entity);
	if (!item.has_value())
	{
		return std::nullopt;
	}
	if (item->kind != PlaceKind::Creature)
	{
		const auto edit = CopyEditOf(registry, entity, offset);
		return edit.has_value() ? std::optional<player_commands::Action>(*edit) : std::nullopt;
	}
	auto recipe = RecipeOf(registry, entity);
	if (!recipe.has_value())
	{
		return std::nullopt;
	}
	// The copy of a creature has the body of the creature, and stands as high above the land as the creature
	auto& position = recipe->pose.position;
	const auto aboveLand = position.y - LandHeight({position.x, position.z});
	const glm::vec2 point {position.x + offset.x, position.z + offset.y};
	position = {point.x, LandHeight(point) + aboveLand, point.y};
	return player_commands::Action {RemakeOf(*recipe)};
}

bool SentByThisComputer(PlayerNames player, uint8_t observer)
{
	const auto* hands = Locator::handSystem::has_value() ? &Locator::handSystem::value() : nullptr;
	if (!input::IsLocalPlayer(hands, player))
	{
		return false;
	}
	const auto* session = Locator::networkGame::has_value() ? Locator::networkGame::value().Session() : nullptr;
	const auto own = session != nullptr ? session->OwnObserver() : std::nullopt;
	return observer == own.value_or(0);
}

Request NewEditorRequest()
{
	if (!Locator::editorSystem::has_value())
	{
		return 0;
	}
	const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : uint32_t {0};
	return Locator::editorSystem::value().GetHistory().NewRequest(turn);
}

RequestAnswer AnswerOf(const ecs::Registry& registry, entt::entity entity, Awaited awaited)
{
	if (entity == entt::null || !registry.Valid(entity))
	{
		return {};
	}
	RequestAnswer answer {.entity = entity};
	if (awaited == Awaited::Thing)
	{
		answer.recipe = RecipeOf(registry, entity);
	}
	else
	{
		answer.pose = PoseOf(registry, entity);
	}
	return answer;
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
	    [](entt::entity entity, glm::vec2 offset) -> std::optional<player_commands::Action> {
		    if (!Locator::entitiesRegistry::has_value())
		    {
			    return std::nullopt;
		    }
		    return CopyCommandOf(Locator::entitiesRegistry::value(), entity, offset);
	    },
	    [] { return NewEditorRequest(); });
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
