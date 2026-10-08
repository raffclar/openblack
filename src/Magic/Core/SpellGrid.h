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

#include <array>

#include <glm/vec3.hpp>

// The spell presence grid (64 x 64 bytes, 80 m cells): where spells were this turn. Its readers are
// UNVERIFIED (the minimap or the computer player, inferred).

namespace openblack::magic
{
/// The grid's cells and decay. The game's grid belongs to the spell system (Locator::spellSystem); the spell_grid
/// functions below reach it
class SpellGrid
{
public:
	static constexpr int k_Side = 64;
	static constexpr uint32_t k_StartDecay = 0x20;

	/// The cell of a map position (x, z metres) takes that value
	void Mark(const glm::vec3& position, uint8_t value);
	/// Every cell loses the decay (to 0 when it is the decay or less), and the decay goes back to its start value
	void Decay();
	[[nodiscard]] uint8_t At(const glm::vec3& position) const;
	void Clear();

private:
	std::array<std::array<uint8_t, k_Side>, k_Side> _cells {};
	uint32_t _decay {k_StartDecay}; ///< What each cell loses per turn
};
} // namespace openblack::magic

namespace openblack::magic::spell_grid
{
/// The cell of a map position (x, z metres) takes that value
void Mark(const glm::vec3& position, uint8_t value);
/// Every turn: every cell loses k (to 0 when it is k or less) and k goes back to 0x20. The original does
/// it only when a game flag is 1, else k grows by 0x20 up to 0x100 (UNVERIFIED flag; taken as 1).
void Decay();
[[nodiscard]] uint8_t At(const glm::vec3& position);
void Clear();
} // namespace openblack::magic::spell_grid
