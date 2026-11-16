/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

#include "Commands/PlayerCommands.h"
#include "Editor/EditorActions.h"

namespace openblack::ecs::components
{
struct LivingAction;
struct Villager;
} // namespace openblack::ecs::components

namespace openblack::ecs::systems
{
class LivingActionSystemInterface;
}

namespace openblack::editor
{
struct EditorContext;

/// The picked thing's sections: its place, turn and size to change, what it is made of, and, for a creature, every
/// section of the creature tools (its looks, body, mind and learning, movement and commands, hands, leash, fight and
/// sounds), and for a villager, its life and states
class InspectorPanel
{
public:
	void Draw(EditorContext& context) noexcept;

private:
	void DrawHeader(EditorContext& context, entt::entity entity) noexcept;
	void DrawTransform(EditorContext& context, entt::entity entity) noexcept;
	void DrawVillager(EditorContext& context, entt::entity entity) noexcept;
	/// The fields of a villager change the world of this computer at once
	static void DrawLocalVillager(ecs::components::Villager& villager, ecs::components::LivingAction& action,
	                              const ecs::systems::LivingActionSystemInterface* actions) noexcept;
	/// The fields of a villager send each change as a command of the turns
	void DrawNetworkVillager(entt::entity entity, const ecs::components::Villager& villager,
	                         const ecs::components::LivingAction& action,
	                         const ecs::systems::LivingActionSystemInterface* actions) noexcept;
	void DrawComponents(EditorContext& context, entt::entity entity) noexcept;

	/// The pose of the main pick as a field of the transform starts to change it
	PoseEdit _edit;
	/// While the turn slider of a network game moves: the angle of the thing when the slider started, and the angle of the
	/// slider. The turn goes as one command when the slider lets go, because the thing turns only at a later turn.
	struct HeldTurn
	{
		entt::entity entity {entt::null};
		float from {0.0f};
		float degrees {0.0f};
	};
	std::optional<HeldTurn> _heldTurn;
	/// While the scale field of a network game moves: the thing and the scale of the field. The scale goes as one command
	/// when the field lets go, because the size of the thing changes only at a later turn.
	struct HeldScale
	{
		entt::entity entity {entt::null};
		float scale {1.0f};
	};
	std::optional<HeldScale> _heldScale;
	/// While a field of a villager that the player types or drags changes in a network game: the villager, the field and
	/// its amount. The amount goes as one command when the field lets go.
	struct HeldVillager
	{
		entt::entity entity {entt::null};
		player_commands::EditorVillagerEdit::Field field {player_commands::EditorVillagerEdit::Field::Life};
		float amount {0.0f};
	};
	std::optional<HeldVillager> _heldVillager;
};

} // namespace openblack::editor
