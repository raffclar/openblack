/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <string_view>
#include <utility>

#include "Windowing/WindowingInterface.h"

namespace openblack
{

enum class GraphicsBackend : uint8_t
{
	Noop,
	Direct3D12,
	Metal,
	Vulkan,
};

/// The names a backend is given by on the command line, in any case: its own and the common short ones
static constexpr std::array<std::pair<std::string_view, GraphicsBackend>, 10> k_GraphicsBackendNames {{
    {"Noop", GraphicsBackend::Noop},
    {"Null", GraphicsBackend::Noop},
    {"Direct3D12", GraphicsBackend::Direct3D12},
    {"D3D12", GraphicsBackend::Direct3D12},
    {"DX12", GraphicsBackend::Direct3D12},
    {"DirectX12", GraphicsBackend::Direct3D12},
    {"Metal", GraphicsBackend::Metal},
    {"MTL", GraphicsBackend::Metal},
    {"Vulkan", GraphicsBackend::Vulkan},
    {"VK", GraphicsBackend::Vulkan},
}};

/// The backend a command line names, by any of its names in any case
[[nodiscard]] constexpr std::optional<GraphicsBackend> ParseGraphicsBackend(std::string_view name) noexcept
{
	const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
	const auto found = std::ranges::find_if(
	    k_GraphicsBackendNames, [&](const auto& entry) { return std::ranges::equal(entry.first, name, {}, lower, lower); });
	return found != k_GraphicsBackendNames.end() ? std::optional {found->second} : std::nullopt;
}

struct EngineConfig
{
	bool wireframe {false};
	/// The debug overlay labelling the villagers with their numbers (the game's own labels come with its keys)
	bool showVillagerNames {false};
	/// The debug overlay's details of what the villagers are doing, their age, health and hunger
	bool showVillagerDetails {false};
	bool debugVillagerNames {false};
	bool debugVillagerStates {false};
	/// Each miracle dispenser is labelled with its miracle
	bool showDispenserNames {true};

	bool viewDetailOverlay {false};
	bool drawSky {true};
	bool drawWater {true};
	bool drawIsland {true};
	bool drawEntities {true};
	bool drawVegetation {true};
	bool drawSprites {true};
	bool drawBoundingBoxes {false};
	bool drawFootpaths {false};
	bool drawStreams {false};
	/// The hand mesh is a left hand, which Black & White mirrors into a right hand unless the player profile asks for
	/// a left hand
	bool rightHandedHand {true};
	/// Creatures' vertices at the seams between their bones are blended towards their partners, as the game draws them
	bool blendCreatureSeams {true};
	/// Creatures cast their shadows onto the land and what stands on it, and villagers a script draws in high detail
	/// onto the land
	bool drawCreatureShadows {true};
	/// Trees cast their shadows as they are drawn each frame, swaying with them and bent away from the hand, rather than
	/// baked into the land once as the game does (off: as the game does)
	bool dynamicTreeShadows {false};

	bool vsync {false};
	bool running {false};

	float timeOfDay {12.0f};
	float smallBumpMapStrength {1.0f};

	float cameraXFov {70.0f};
	float cameraNearClip {1.0f};
	/// The game's graphics detail level, 0 to 6, read once at start-up (see graphics::detail_level)
	uint8_t detailLevel {4};

	float guiScale {1.0f};

	GraphicsBackend graphicsBackend {GraphicsBackend::Noop};
	glm::u16vec2 resolution {256, 256};
	windowing::DisplayMode displayMode {windowing::DisplayMode::Windowed};

	uint32_t numFramesToSimulate {0};
	/// Log frame time statistics every so many frames, never when 0
	uint32_t frameStatsInterval {0};
	/// With the frame statistics, the GPU time of each render view
	bool frameStatsViews {false};
};
} // namespace openblack
