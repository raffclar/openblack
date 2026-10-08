/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

// The fire graphic (a particle system): the burning object's flames (S_Fire.raw, mode 13), the steam of a hot object
// being cooled and the smoke of a fire that went out (S_SpriteSheet3.raw, modes 13 and 6). It is updated with the frame
// time when drawn and Z-sorted as one object. Also the tint the fire gives the burning object (tree colour, charring,
// glow).

namespace openblack::ecs::fire
{
struct FireEffect;

namespace graphic
{
/// FireGraphic's draw flags for a new fire on `object`: bit 0 morphed with the land, 1 flames, 2 smoke, 3 steam, 4 the
/// light map
[[nodiscard]] uint8_t InitialFlags(entt::entity object);
/// Only for an object with a 3D object
void Create(FireEffect& fire);
void Destroy(FireEffect& fire);
/// Updates every fire, `seconds` of frame time
void Update(float seconds);
/// The turn hook of the fire graphics: OPENBLACK_FIRE_TRACE (the bursts read game_clock::Turn())
void SetTurn(uint32_t turn);
void Clear();

/// The colour a burning tree is drawn with (x/256 per channel of its colour):
/// min(life > 0.9 ? max(50, 255 - (1 - life) 2550) : 50, ecs::TreeBrightness()). nullopt when the object has no fire.
[[nodiscard]] std::optional<glm::u8vec3> TreeDrawColour(entt::entity object);
/// The charring grey 255 - ceil(int(charring x 255) x 175 / 256) (80 when fully charred)
[[nodiscard]] uint8_t CharringGrey(const FireEffect& fire);
/// The glow, (1 - charring) x clamp(0.001 T (1 + 0.2 noise)) x 255 as RGB(c 180 /
/// 256, c 60 / 256, 0)
[[nodiscard]] glm::u8vec3 CharringGlow(const FireEffect& fire, float turnTime);
} // namespace graphic
} // namespace openblack::ecs::fire
