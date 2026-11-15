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

#include "Editor/EditorActions.h"

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
};

} // namespace openblack::editor
