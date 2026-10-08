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
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

/// Backend names accepted on the command line; each name appears once
inline constexpr std::array<std::pair<std::string_view, GraphicsBackend>, 4> k_GraphicsBackendStringLookup {{
    {"Noop", GraphicsBackend::Noop},
    {"Direct3D12", GraphicsBackend::Direct3D12},
    {"Metal", GraphicsBackend::Metal},
    {"Vulkan", GraphicsBackend::Vulkan},
}};

/// The mind file LOAD_MY_CREATURE reads in Scripts/CreatureMind when nothing else is given: the creature of the
/// game folder's first profile, so that every run loads the same one
inline constexpr std::string_view k_DefaultProfileCreatureFile = "C4ba71b36.erc";

struct EngineConfig
{
	bool wireframe {false};
	bool showVillagerNames {false};
	bool debugVillagerNames {false};
	bool debugVillagerStates {false};

	bool viewDetailOverlay {false};
	bool drawSky {true};
	bool drawWater {true};
	bool drawIsland {true};
	bool drawEntities {true};
	bool drawSprites {true};
	bool drawBoundingBoxes {false};
	bool drawFootpaths {false};
	bool drawStreams {false};

	bool vsync {false};
	/// The original's graphics detail level 0..6 (Graphics/DetailLevel.h); 4 is the original's default
	uint8_t detailLevel {4};
	/// The profile's creature: a mind file in Scripts/CreatureMind, which the original names in the profile's
	/// registry entry. Empty for a profile with no creature
	std::string profileCreatureFile {k_DefaultProfileCreatureFile};
	bool running {false};

	float timeOfDay {12.0f};
	float skyAlignment {0.0f};
	float bumpMapStrength {1.0f};
	float smallBumpMapStrength {1.0f};

	float cameraXFov {70.0f};
	float cameraNearClip {1.0f};
	float cameraFarClip {static_cast<float>(0x10000)};

	float guiScale {1.0f};

	/// Music main volume 0..127: the AudioMusicMasterVolume value of the BWSetup registry key, applied as the music
	/// main volume; 127 without the key. Not saved yet (openblack has no settings file for it).
	uint32_t audioMusicMainVolume {0x7F};
	/// Sample main volume 0..127 (every effect and voice): the AudioSampleMasterVolume value of BWSetup, applied as the
	/// sample main volume; 127 without the key. Not saved yet (openblack has no settings file for it).
	uint32_t audioSampleMainVolume {0x7F};

	GraphicsBackend graphicsBackend {GraphicsBackend::Noop};
	glm::u16vec2 resolution {256, 256};
	windowing::DisplayMode displayMode {windowing::DisplayMode::Windowed};

	uint32_t numFramesToSimulate {0};
};
} // namespace openblack
