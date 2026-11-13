/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ParticleSymbolLayout.h"

#include <cmath>

#include <algorithm>
#include <iterator>
#include <limits>

#include <glm/common.hpp>

#include "Common/RadixSort.h"
#include "Graphics/ZSort.h"
#include "Visual/RenderModes.h"

using namespace openblack;
using namespace openblack::particles;
using namespace openblack::particles::draw;

namespace
{
/// The low bytes of a sort key, which hold the place of the symbol in its run
constexpr size_t k_KeyPlaceBytes = sizeof(uint32_t);
constexpr uint32_t k_LowHalf = 0xFFFFFFFFu;

/// The material of each sprite of a symbol: the two glows take the first and the symbol takes the second
constexpr std::array<size_t, SymbolLayout::k_SpritesPerSymbol> k_SpriteMaterial {0, 0, 1};

/// The sprites of a symbol that the run draws: a sprite whose sheet is not known is left out
struct DrawnSprites
{
	std::array<uint32_t, SymbolLayout::k_SpritesPerSymbol> sprites {};
	/// The material of each, as the index of the first material in the run that is equal to it
	std::array<uint32_t, SymbolLayout::k_SpritesPerSymbol> materials {};
	size_t count {0};
};

DrawnSprites DrawnSpritesOf(const SymbolRun& run)
{
	// Two equal materials share the index of the first, as one material of a frame does
	const bool same = run.materials[0].has_value() && run.materials[1].has_value() && *run.materials[0] == *run.materials[1];
	DrawnSprites drawn;
	for (uint32_t sprite = 0; sprite < SymbolLayout::k_SpritesPerSymbol; ++sprite)
	{
		const auto material = k_SpriteMaterial.at(sprite);
		if (!run.materials.at(material).has_value())
		{
			continue;
		}
		drawn.sprites.at(drawn.count) = sprite;
		drawn.materials.at(drawn.count) = same ? 0 : static_cast<uint32_t>(material);
		++drawn.count;
	}
	return drawn;
}
} // namespace

void SymbolLayout::Clear()
{
	// The sprites keep their room: the next frame writes each sprite before it reads it
	_symbols.clear();
	_runs.clear();
}

uint32_t SymbolLayout::Add(const Effect::DrawWalk& walk, DrawPath path, const glm::vec3& origin, int player,
                           const Sources& sources)
{
	const int shown = player >= 0 ? player : k_NeutralPlayer;
	const auto [glow, symbol] = SymbolMaterials(sources);
	SymbolRun run {
	    .path = path,
	    .origin = origin,
	    .first = static_cast<uint32_t>(_symbols.size()),
	    .count = 0,
	    .symbolsInARow = sources.symbolsInARow,
	    .materials = {glow, symbol},
	};
	for (const auto& step : walk.steps)
	{
		const auto& atom = walk.atoms.at(step.index);
		// An atom that draws nothing puts nothing in a frame
		if (!IsLaidSymbol(atom))
		{
			continue;
		}
		const int symbolPlayer = SymbolPlayer(atom, shown);
		_symbols.push_back({
		    .atom = SymbolAtomOf(atom),
		    .playerRgb = sources.playerColour ? sources.playerColour(symbolPlayer) : 0xFFFFFFu,
		    .player = symbolPlayer,
		});
		++run.count;
	}
	if (_sprites.size() < _symbols.size())
	{
		_sprites.resize(_symbols.size());
	}

	// The box of the sprites, in the order of the sprites of the frame: the farthest corner of a sprite that faces the
	// screen is half its width across and half its height up, and its height is its width
	const auto drawn = DrawnSpritesOf(run);
	if (run.count > 0 && drawn.count > 0)
	{
		glm::vec3 lowest(std::numeric_limits<float>::max());
		glm::vec3 highest(std::numeric_limits<float>::lowest());
		for (uint32_t i = run.first; i < run.first + run.count; ++i)
		{
			const auto& atom = _symbols[i].atom;
			const auto [glowHalfWidth, symbolHalfWidth] = SymbolHalfWidths(atom.scale);
			for (size_t k = 0; k < drawn.count; ++k)
			{
				const float halfWidth = drawn.sprites.at(k) == 2 ? symbolHalfWidth : glowHalfWidth;
				const float reach = std::abs(halfWidth) + std::abs(halfWidth);
				lowest = glm::min(lowest, atom.position - reach);
				highest = glm::max(highest, atom.position + reach);
			}
		}
		run.spriteBox = std::pair {lowest, highest};
	}
	_runs.push_back(run);
	return static_cast<uint32_t>(_runs.size() - 1);
}

bool SymbolLayout::Draws(uint32_t run) const
{
	const auto& found = _runs.at(run);
	return found.count > 0 && DrawnSpritesOf(found).count > 0;
}

const sprites::SpriteInstance& SymbolLayout::Sprite(uint32_t place) const
{
	return _sprites[place / k_SpritesPerSymbol].instances.at(place % k_SpritesPerSymbol);
}

void SymbolLayout::MakeSprites(uint32_t run)
{
	auto& found = _runs.at(run);
	if (found.made)
	{
		return;
	}
	for (uint32_t i = found.first; i < found.first + found.count; ++i)
	{
		const auto& symbol = _symbols[i];
		_sprites[i] = SymbolOf(symbol.atom, symbol.playerRgb, symbol.player, found.symbolsInARow);
	}
	found.made = true;
}

void draw::OrderSymbols(const SymbolLayout& layout, uint32_t runIndex, const glm::vec3& camera, std::optional<glm::vec3> hand,
                        std::vector<Command>& commands, std::vector<uint32_t>& spriteOrder, SymbolOrderLists& lists)
{
	const auto& run = layout.Run(runIndex);
	// Order leaves out what is drawn before the land and with the learning words
	if (run.path == DrawPath::BeforeLand || run.path == DrawPath::WithLearningWords || !layout.Draws(runIndex))
	{
		return;
	}
	const auto drawn = DrawnSpritesOf(run);

	// The symbols in the order of their sprites. Each sprite of a symbol takes its place at the symbol, thus a stable
	// sort of the symbols by their distance, the farthest first, gives the order of a stable sort of the sprites.
	auto& order = lists.order;
	order.clear();
	const bool sorted = run.path == DrawPath::Sorted;
	if (sorted)
	{
		auto& keys = lists.keys;
		keys.clear();
		for (uint32_t i = 0; i < run.count; ++i)
		{
			const auto inverted = ~graphics::zsort::Depth(layout.Atom(run.first + i).position, camera);
			keys.push_back((static_cast<uint64_t>(inverted) << 32u) | i);
		}
		openblack::radix_sort::Sort(keys, lists.scratch, k_KeyPlaceBytes);
		std::ranges::transform(keys, std::back_inserter(order),
		                       [](uint64_t key) { return static_cast<uint32_t>(key & k_LowHalf); });
	}
	else
	{
		for (uint32_t i = 0; i < run.count; ++i)
		{
			order.push_back(i);
		}
	}

	// The place of an effect that is not sorted, as Order gives it
	uint32_t placeDepth = 0;
	if (!sorted)
	{
		const bool afterHand = run.path == DrawPath::Immediate && hand.has_value();
		const auto base = graphics::zsort::Depth(afterHand ? *hand : run.origin, camera);
		placeDepth = run.path == DrawPath::AtOnceAfterLand ? graphics::zsort::k_AtOnceAfterLand
		                                                   : (afterHand && base > 0 ? base - 1 : base);
	}

	// Runs of sprites that share a material join one command, and each command takes a place a little nearer than the
	// one before
	const auto firstCommand = commands.size();
	const auto emit = [&](uint32_t symbol, size_t k) {
		const auto material = drawn.materials.at(k);
		const auto place = ((run.first + symbol) * SymbolLayout::k_SpritesPerSymbol) + drawn.sprites.at(k);
		if (commands.size() > firstCommand && commands.back().material == material)
		{
			++commands.back().count;
			spriteOrder.push_back(place);
			return;
		}
		uint32_t depth = sorted ? graphics::zsort::Depth(layout.Atom(run.first + symbol).position, camera) : placeDepth;
		if (commands.size() > firstCommand)
		{
			depth = std::min(depth, commands.back().depth > 0 ? commands.back().depth - 1 : 0);
		}
		commands.push_back({.kind = ItemKind::Sprite,
		                    .first = static_cast<uint32_t>(spriteOrder.size()),
		                    .count = 1,
		                    .material = material,
		                    .depth = depth});
		spriteOrder.push_back(place);
	};

	// Sprites that are added to what is behind and write no depth give the same picture in any order. Thus they are
	// drawn a material at a time, in the order in which each material comes first, and in their order otherwise.
	const auto& mode = graphics::render_modes::Desc(graphics::render_modes::Mode::AlphaTexturedAlphaAdditiveNz);
	const bool commutes = mode.blend == graphics::render_modes::Blend::Additive && !mode.zWrite;
	if (!commutes)
	{
		for (const auto symbol : order)
		{
			for (size_t k = 0; k < drawn.count; ++k)
			{
				emit(symbol, k);
			}
		}
		return;
	}
	std::array<uint32_t, SymbolLayout::k_SpritesPerSymbol> materialOrder {};
	size_t materialCount = 0;
	for (size_t k = 0; k < drawn.count; ++k)
	{
		const auto material = drawn.materials.at(k);
		if (std::find(materialOrder.begin(), materialOrder.begin() + static_cast<std::ptrdiff_t>(materialCount), material) ==
		    materialOrder.begin() + static_cast<std::ptrdiff_t>(materialCount))
		{
			materialOrder.at(materialCount++) = material;
		}
	}
	for (size_t m = 0; m < materialCount; ++m)
	{
		for (const auto symbol : order)
		{
			for (size_t k = 0; k < drawn.count; ++k)
			{
				if (drawn.materials.at(k) == materialOrder.at(m))
				{
					emit(symbol, k);
				}
			}
		}
	}
}
