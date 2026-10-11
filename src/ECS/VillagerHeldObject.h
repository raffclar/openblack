/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>

#include <glm/mat4x4.hpp>

#include "3D/AllMeshes.h"
#include "Enums.h"

/// What a villager holds in its hand: the model of each thing it can hold, where that model is drawn as the villager's
/// arm moves, and the log its wood becomes when it lets it fall. Free of the game's state so they can be tested alone.
namespace openblack::ecs::villager_held
{

/// The bone of the villager's skeleton at the end of its arm, which holds what it carries
inline constexpr size_t k_GripBone = 15;

/// The model a held thing is drawn with, the same for every tribe; none for nothing at all
[[nodiscard]] std::optional<MeshId> MeshOf(CarriedObject carried);

/// Where a held thing is drawn: on the grip bone of the villager as it is drawn, its own axes turned to the grip's (its
/// sideways and up axes reversed and its up and forward axes swapped) and keeping the grip's place
[[nodiscard]] glm::mat4 HeldMatrix(const glm::mat4& villagerModel, const glm::mat4& gripBone);

/// How opaque a held thing is drawn, of 255, or none when it is opaque. A held thing fades out in the distance as any
/// small thing does; nearer than that it takes the villager's own fading
[[nodiscard]] std::optional<uint8_t> HeldAlpha(std::optional<uint8_t> ownFade, std::optional<uint8_t> villagerFade);

/// Whether a villager let go by a hand, launched or killed lets its wood fall as a log: only when it shows something in
/// its hand and carries more wood than shows
[[nodiscard]] bool DropsLog(CarriedObject shown, int16_t wood, uint32_t minWoodToShowGraphic);

/// What the log's wood is worth against a whole pine's, so that it holds just the wood the villager carried
[[nodiscard]] float LogWoodMultiplier(int16_t wood, float pineWoodValue);

/// The kind of log a fallen tree gives a villager who takes its wood: a log drawn as plain wood or as one of the three
/// held logs gives that kind; any other gives its tree's own kind
[[nodiscard]] uint8_t WoodKindOfFallenTree(MeshId mesh, CarriedTreeType treeKind);

} // namespace openblack::ecs::villager_held
