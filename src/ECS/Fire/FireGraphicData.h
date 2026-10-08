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

#include <list>
#include <memory>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::graphics::frame_anim
{
struct StackedFrames;
} // namespace openblack::graphics::frame_anim

// What one fire draws: its flames, steam and smoke puffs and its light map (ECS/Fire/FireGraphic.cpp updates them)

namespace openblack::ecs::fire::graphic
{
/// A flame or puff of the fire graphic
struct SpritePos
{
	glm::vec3 position {0.0f}; ///< Local (flames) or world (puffs)
	float scale {1.0f};
	float age {0.0f};
	uint8_t alpha {255};
	int index {0}; ///< The mesh triangle
	glm::vec3 velocity {0.0f};
	float baseScale {1.0f};
};

/// One fire's graphic
struct Graphic
{
	entt::entity object {entt::null};
	/// Bit 0 the 3D object morphs with the land, bit 1 flames, bit 2 smoke when it goes out, bit 3 steam while cooled,
	/// bit 4 light map
	uint8_t flags {0x1E};
	float flameAccumulator {0.0f};
	int flameCount {0};
	bool veryHotDone {false};
	float steamAccumulator {0.0f};
	int steamCount {0};
	uint32_t steamStart {0};
	float steamTemperature {0.0f};
	float smokeAccumulator {0.0f};
	int smokeCount {0};
	uint32_t smokeStart {0};
	glm::vec3 smokeLocal {0.0f};
	int smokeIndex {0};
	int maxFlames {0};       ///< The most flames at once
	float localScale {1.0f}; ///< The flames' local scale
	float scaleMultiplier {1.0f};
	std::list<SpritePos> flames; ///< Newest first
	std::list<SpritePos> steam;
	std::list<SpritePos> smoke;
	/// The fire graphic as a sound channel owner, given at its first sizzle
	uint32_t soundOwner {0};
	/// The fireball's light map; null: none
	std::shared_ptr<const graphics::frame_anim::StackedFrames> lightMap;
};
} // namespace openblack::ecs::fire::graphic
