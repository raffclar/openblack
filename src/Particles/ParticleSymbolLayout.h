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
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>

#include "ParticleDrawFrame.h"
#include "ParticleDrawPath.h"
#include "ParticleEffect.h"

/// The effects that draw only players' symbols, as the symbols of belief of the village centres, in one layout for all
/// the effects. A symbol keeps only the values of its atom. Its three sprites are made at the first draw that needs
/// them, thus a pass makes no sprite for an effect that it does not see. The order of the draws and each sprite are the
/// same as Frame and Order give for the same effect.
namespace openblack::particles::draw
{

/// One symbol: the values of its atom, and its player and that player's colour
struct LaidSymbol
{
	SymbolAtom atom;
	/// 0xRRGGBB
	uint32_t playerRgb;
	int player;
};

/// The symbols of one effect, in the order that its walk gives
struct SymbolRun
{
	DrawPath path;
	/// Where a queued effect takes its place
	glm::vec3 origin;
	uint32_t first;
	uint32_t count;
	int symbolsInARow;
	/// The material of the glows (0) and of the symbols (1). Each is none when its sheet is not known.
	std::array<std::optional<Material>, 2> materials;
	/// The lowest and the highest corners of a box that holds each corner of each sprite that the run draws
	std::optional<std::pair<glm::vec3, glm::vec3>> spriteBox;
	/// True when the sprites of the run are made
	bool made {false};
};

/// The lists that OrderSymbols works in. A caller that keeps them from one frame to the next keeps their memory.
struct SymbolOrderLists
{
	std::vector<uint64_t> keys;
	std::vector<uint64_t> scratch;
	std::vector<uint32_t> order;
};

class SymbolLayout
{
public:
	/// The sprite of a symbol in an order: three for each symbol, the glows first
	static constexpr uint32_t k_SpritesPerSymbol = 3;

	void Clear();
	/// Adds the walk of an effect that OnlySymbols accepts, and gives the index of its run
	uint32_t Add(const Effect::DrawWalk& walk, DrawPath path, const glm::vec3& origin, int player, const Sources& sources);
	/// True when the run puts at least one sprite in the frame
	[[nodiscard]] bool Draws(uint32_t run) const;
	[[nodiscard]] const SymbolRun& Run(uint32_t run) const { return _runs.at(run); }
	/// The values of the atom of a symbol, by its index in the layout
	[[nodiscard]] const SymbolAtom& Atom(uint32_t symbol) const { return _symbols.at(symbol).atom; }
	/// The sprite with this place in an order (see OrderSymbols). The sprites of its run must be made.
	[[nodiscard]] const sprites::SpriteInstance& Sprite(uint32_t place) const;
	/// Makes the sprites of a run, when they are not made yet
	void MakeSprites(uint32_t run);

private:
	std::vector<LaidSymbol> _symbols;
	std::vector<SymbolSprites> _sprites;
	std::vector<SymbolRun> _runs;
};

/// Appends the draws of a run, as seen from the camera, to the commands. The places of their sprites go to the end of
/// the sprite order, and each command counts from the start of that order. The material of a command is the index of
/// the material in the run. The commands and the sprites are the same as Order gives for the frame of the effect.
void OrderSymbols(const SymbolLayout& layout, uint32_t run, const glm::vec3& camera, std::optional<glm::vec3> hand,
                  std::vector<Command>& commands, std::vector<uint32_t>& spriteOrder, SymbolOrderLists& lists);

} // namespace openblack::particles::draw
