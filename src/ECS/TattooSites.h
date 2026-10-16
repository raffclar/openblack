/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include "3D/ScreenPick.h"
#include "Creature/CreatureTattoo.h"
#include "Creature/TattooEditor.h"
#include "ECS/PosedModel.h"
#include "Enums.h"

namespace openblack::graphics
{
class L3DMesh;
}

namespace openblack::ecs
{
class Registry;
}

/// The tattoo places of a creature's body as the tattoo editor shows them: where each place is on the skin, and where
/// it is on the screen as the body is posed
namespace openblack::ecs::tattoo_sites
{

/// Where each tattoo place of a body is on its skin. A place that the species does not have, or that no triangle
/// holds, has none.
using Found = std::array<std::optional<posed_model::SkinPoint>, creature_tattoo::k_SlotCount>;

/// The places of a species on its body's mesh, found one time when the editor first shows the body
[[nodiscard]] Found Find(const graphics::L3DMesh& mesh, CreatureType species);

/// Each place as the posed body shows it, in the pixels of a view whose top left corner is at an origin on the window.
/// Only a place in front of the camera has a pixel. The creature gives the body its shape.
[[nodiscard]] std::vector<creature_tattoo_editor::SiteOnScreen> OnScreen(const Registry& registry, entt::entity creature,
                                                                         const graphics::L3DMesh& mesh, const glm::mat4& model,
                                                                         std::span<const glm::mat4> bones, const Found& found,
                                                                         const screen_pick::View& view, glm::ivec2 origin);

} // namespace openblack::ecs::tattoo_sites
