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

#include <functional>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/vec2.hpp>

#include "ECS/Systems/LandShadeSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::systems
{

class LandShadeSystem final: public LandShadeSystemInterface
{
public:
	/// A land as it was laid: an identity that changes when another land loads, its blocks, the highest altitude of each
	/// block's cells, and the luminosity map's first block and size
	struct LaidLand
	{
		uint64_t identity {0};
		std::vector<land_shade::LaidBlock> blocks;
		std::vector<uint8_t> highest;
		glm::ivec2 firstBlock {0};
		glm::u16vec2 mapSize {0};
	};

	/// What the system reads from the game: the texels of a grey image, the identity of the land now loaded (none
	/// without a land), the land as it was laid, and the entities
	struct Sources
	{
		std::function<std::span<const uint8_t>(entt::id_type)> image;
		std::function<std::optional<uint64_t>()> landIdentity;
		std::function<LaidLand()> lay;
		std::function<Registry*()> registry;
	};

	/// The system with the game's textures, terrain and registry
	LandShadeSystem();
	explicit LandShadeSystem(Sources sources);

	void Shade(const LandShadeFrame& frame) override;
	void Clear() override;
	[[nodiscard]] const land_shade::ShadedLand& Land() const override { return _land; }
	[[nodiscard]] LandShadeMap Map() const override;
	[[nodiscard]] size_t ShadowCount() const override { return _shadows; }
	[[nodiscard]] size_t LightCount() const override { return _lights; }

private:
	void Lay();
	void CastShadows(const LandShadeFrame& frame, Registry& registry);
	void CastLights(const LandShadeFrame& frame, Registry& registry);
	/// The image of a light at its warm levels, made once
	[[nodiscard]] std::span<const uint8_t> WarmImage(entt::id_type id);
	/// Copies the blocks that changed this frame or the last into the map
	void UpdateMap(std::span<const glm::ivec2> changedBefore);

	Sources _sources;
	std::optional<uint64_t> _identity;
	land_shade::ShadedLand _land;
	std::vector<uint8_t> _highest;
	std::vector<glm::vec2> _corners;
	glm::ivec2 _firstBlock {0};
	glm::u16vec2 _mapSize {0};
	std::vector<uint8_t> _map;
	uint16_t _firstChangedRow {0};
	uint16_t _lastChangedRow {0};
	std::unordered_map<entt::id_type, std::vector<uint8_t>> _warmImages;
	size_t _shadows {0};
	size_t _lights {0};
};

} // namespace openblack::ecs::systems
