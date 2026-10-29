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
#include <string>
#include <string_view>
#include <utility>

#include "Common/GameLanguage.h"
#include "Creature/BonusCreatures.h"
#include "Visual/DetailLevel.h"
#include "Visual/DisplayMode.h"
#include "Visual/LandDetail.h"
#include "Visual/UploadMode.h"

namespace openblack
{

enum class GraphicsBackend : uint8_t
{
	Noop,
	Direct3D12,
	Metal,
	Vulkan,
	/// OpenGL ES 3, which is WebGL 2 in a web browser. Only the web build has it.
	OpenGLES,
	/// WebGPU. In a web browser, the browser gives it. On the desktop, bgfx needs the Dawn library next to the game.
	WebGPU,
	/// Direct3D 11. Only the Windows build has it.
	Direct3D11,
	/// OpenGL 4.3 on the desktop. The web build does not have it.
	OpenGL,
};

/// The names a backend is given by on the command line, in any case: its own and the common short ones
static constexpr auto k_GraphicsBackendNames = std::to_array<std::pair<std::string_view, GraphicsBackend>>({
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
#if defined(_WIN32)
    {"Direct3D11", GraphicsBackend::Direct3D11},
    {"D3D11", GraphicsBackend::Direct3D11},
    {"DX11", GraphicsBackend::Direct3D11},
    {"DirectX11", GraphicsBackend::Direct3D11},
#endif
#if !defined(__EMSCRIPTEN__)
    {"OpenGL", GraphicsBackend::OpenGL},
    {"GL", GraphicsBackend::OpenGL},
#endif
#if defined(__EMSCRIPTEN__)
    {"OpenGLES", GraphicsBackend::OpenGLES},
    {"GLES", GraphicsBackend::OpenGLES},
    {"WebGL", GraphicsBackend::OpenGLES},
#endif
    {"WebGPU", GraphicsBackend::WebGPU},
    {"WGPU", GraphicsBackend::WebGPU},
});

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
	/// The language of the install, from Country.txt in the game folder. The game reads it one time as it starts.
	GameLanguage language {GameLanguage::UkEnglish};
	bool wireframe {false};
	/// The debug overlay labelling the villagers with their numbers (the game's own labels come with its keys)
	bool showVillagerNames {false};
	/// The debug overlay's details of what the villagers are doing, their age, health and hunger
	bool showVillagerDetails {false};
	bool debugVillagerNames {false};
	bool debugVillagerStates {false};
	/// Each miracle dispenser is labelled with its miracle
	bool showDispenserNames {true};
	/// The Creature Cave's ImGui screen of the creature's facts and lessons, a debugging aid shown only when asked for
	bool showCreatureCaveScreen {false};

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
	/// Pushing the mouse on past the screen's sides moves the world camera, in full screen (the player's profile)
	bool pushScrolling {false};
	/// For the testbed only: a game in a window pushes the camera as one in full screen does, so that the push can be
	/// watched without the game taking the whole screen
	bool testbedPushScrollInWindow {false};
	/// Creatures' vertices at the seams between their bones are blended towards their partners, as the game draws them
	bool blendCreatureSeams {true};
	/// Creatures cast their shadows onto the land and what stands on it, and villagers a script draws in high detail
	/// onto the land
	bool drawCreatureShadows {true};
	/// Trees cast their shadows as they are drawn each frame, swaying with them and bent away from the hand, rather than
	/// baked into the land once as the game does (off: as the game does)
	bool dynamicTreeShadows {false};
	/// Debug: the Creature Breeder's bonus creatures taken as unlocked. Vanilla only had them once an unlock from outside
	/// the game was installed; none is unlocked by default, as on a normal install
	creature_swap::Unlocked unlockedBonusCreatures;

	bool vsync {false};
	/// Tools: the window is open but the renderer draws nothing (the no-renderer backend)
	bool noDraw {false};
	/// Tools: the window never takes the focus, the front or the mouse, and stays in a window
	bool background {false};
	bool running {false};
	/// Tools: the sounds make no noise and last their length by the game's machine clock, so that a run with a fixed
	/// frame time goes the same way at any speed
	bool clockedAudio {false};
	/// The dedicated server: it has no audio device, and all of its sounds go to the audio manager that plays nothing. It
	/// runs its frames with no window.
	bool dedicatedServer {false};
	/// What the resource loaders make: the data and the objects on the graphics device, or the data only for a world with
	/// no device
	graphics::UploadMode resourceUploads {graphics::UploadMode::Device};

	float timeOfDay {12.0f};
	float smallBumpMapStrength {1.0f};

	float cameraXFov {70.0f};
	float cameraNearClip {1.0f};
	/// The game's graphics detail level, 0 to 6, read once at start-up (see graphics::detail_level)
	uint8_t detailLevel {4};
	/// What the detail level turns on and off: the level's table, with this computer's own values for the custom level
	graphics::detail_level::Settings detail {graphics::detail_level::For(4)};
	/// How many detail levels the options' selector steps through, from this computer's settings
	uint8_t maxDetailLevel {graphics::detail_level::k_MenuLevels};
	/// The volumes of the sound effects and the music that this computer keeps, if it keeps them (0 to 1)
	std::optional<float> keptEffectsVolume;
	std::optional<float> keptMusicVolume;
	/// Debug: the land's detail lines in place of the detail level's (see graphics::land_detail)
	std::optional<graphics::land_detail::Lines> landDetailLines;

	float guiScale {1.0f};

	GraphicsBackend graphicsBackend {GraphicsBackend::Noop};
	glm::u16vec2 resolution {256, 256};
	windowing::DisplayMode displayMode {windowing::DisplayMode::Windowed};

	uint32_t numFramesToSimulate {0};
	/// Log frame time statistics every so many frames, never when 0
	uint32_t frameStatsInterval {0};
	/// With the frame statistics, the GPU time of each render view
	bool frameStatsViews {false};
	/// Tools: log the times of the computer gods each time this many turns go by, never when 0
	uint32_t aiTimesEvery {0};
	/// With the frame statistics, the file that gets each frame of the run when the game ends; none when empty
	std::string frameStatsOut;
	/// Draw the first frame of a land once behind the loading screen, so that the renderer makes its pipelines and
	/// render targets there and not in the first frames of the game
	bool warmUpFirstFrame {true};

	/// The https:// origin of the lobby (bwgame-service). With none, the game has no online services. The web build
	/// uses the origin of the page for any value.
	std::optional<std::string> lobbyOrigin;
	/// Tests on this computer only: a PEM file of more certificate authorities for a lobby on a loopback address. The
	/// lobby's web client, its push channel and the game sockets then also trust it.
	std::optional<std::string> lobbyTestAuthorities;
	/// The real-world weather of the player's place changes the weather of a single-player game. It is on unless the
	/// player turns it off.
	bool useInternetWeather {true};
	/// A network game shows the round trip of this computer's game socket (deviation 143). It is off unless the player
	/// turns it on.
	bool showPing {false};
};
} // namespace openblack
