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

#include <glm/vec3.hpp>

#include "WeatherInfo.h"

// The registered weather volumes: a storm descriptor and the storm object built from it (a list, newest first). Each
// one adds its WeatherInfo to the atmosphere grid inside its radius (CalcAtmos). The storm miracle, the climates'
// natural storms, the map command CREATE_WEATHER_STORM and the CHL weather things create them.

namespace openblack::weather::storms
{
/// A handle: the original keeps the storm pointer and asks whether it is still in the list
using StormId = uint32_t;
constexpr StormId k_NoStorm = 0;

/// A storm's descriptor, with the original's defaults. Names from the CHL setters (CHANGE_*_PROPERTIES).
struct StormDescriptor
{
	glm::vec3 position {0.0f};  ///< The current centre (x, height, z)
	float innerRadius {100.0f}; ///< Full strength inside
	float outerRadius {300.0f}; ///< Nothing outside
	float fadeInTime {10.0f};   ///< Seconds (also the fade-out)
	float lifeTime {100.0f};    ///< Seconds
	float strength {1.0f};      ///< The fade at full strength
	int32_t numClouds {8};      ///< CHANGE_CLOUD_PROPERTIES
	float blackness {0.5f};
	float elevation {160.0f}; ///< The clouds' height above the land
	float fallSpeed {1.0f};   ///< CHANGE_WEATHER_PROPERTIES (the rain drawing)
	float sheetMin {0.0f};    ///< Sheet lightning (flash and thunder) every sheetMin..sheetMax s; 0 = none
	float sheetMax {0.0f};
	float forkMin {0.0f}; ///< Fork lightning (a PSys strike) every forkMin..forkMax s
	float forkMax {0.0f};
	float snowCoverRate {1.2f}; ///< x snow byte x fade: snow laid on the land (the snow cover, not ported)
	uint32_t unknown0x44 {0};   ///< Never set
	/// What the storm adds (by default 10 degrees, rain 100, overcast 100, wind (10, 0))
	WeatherInfo weather {10, 100, 0, 100, 10, 0, 0, 0};
};

/// A storm: its descriptor and its running state
struct Storm
{
	StormId id {k_NoStorm};
	StormDescriptor descriptor;
	float age {0.0f};        ///< Seconds
	glm::vec3 target {0.0f}; ///< Where it moves to (its creation point unless someone moves it)
	float speed {1.0f};      ///< Metres per second towards the target (0 when constructed)
	bool arrived {true};
	int32_t deleteCounter {0}; ///< 0 = alive; set by MarkForDeletion, deleted when it passes 2
	float sheetTimer {0.0f};
	float forkTimer {0.0f};
	glm::vec3 drawPosition {0.0f}; ///< The position used by CalcAtmos this turn
	float innerRadius {0.0f};      ///< The inner radius, faded in and out with the storm
	float outerRadius {0.0f};
	float fade {0.0f};       ///< 0 .. strength
	bool drawClouds {false}; ///< Only for the weather things' storms (bit 10 of the thing's flags)
	/// The lightning's light flash: started by the fork (0.5) and sheet (1.0) lightning of the storm's update, aged
	/// each turn, its light worked out each frame (ECS/Weather/LightningFlash.h)
	struct Flash
	{
		glm::vec3 position {0.0f}; ///< The lightning's point
		float radius {0.0f};       ///< The storm's outer radius
		bool active {false};
		float age {0.0f};       ///< Seconds
		float f1 {0.0f};        ///< The flash at the camera ((1 - age) x intensity)
		float f3 {0.0f};        ///< The land's light ((1 - age)^3 x intensity)
		float intensity {1.0f}; ///< 1.0 when constructed
	} flash;
};

/// A new storm at the head of the list (speed 1, target = its position)
StormId Create(const StormDescriptor& descriptor);
/// The storm while it is in the list and not marked for deletion, else nullptr
[[nodiscard]] Storm* Find(StormId id);
/// Marks it; it still updates for two turns and is deleted on the third (UpdateAll)
void MarkForDeletion(StormId id);
/// Gone at once. A climate being deleted does this with its storms.
void Destroy(StormId id);
/// KILL_STORMS_IN_AREA: marks every storm whose 2D distance to `position` is below radius + its outer
/// radius
void KillStormsInArea(const glm::vec3& position, float radius);
/// From the atmosphere's game update: the update of each storm, then the deletion count
void UpdateAll(float seconds);
/// CalcAtmos of every storm not marked for deletion, newest first
void CalcAtmosAll(const glm::vec3& point, WeatherInfo& weather);
/// One storm's weather added at a point
void CalcAtmos(const Storm& storm, const glm::vec3& point, WeatherInfo& weather);
/// Every storm, newest first (the debug hooks and the drawing)
void ForEach(const std::function<void(const Storm&)>& function);
/// The same with write access (the per-frame flash, LightningFlash.cpp, and the cloud puffs, StormClouds.cpp)
void ForEachMutable(const std::function<void(Storm&)>& function);
/// Deletes every storm (a new land)
void Clear();

/// The engine's callbacks for the storms' lightning: the fork one creates a PSys strike, the sheet one plays the
/// thunder; both get the storm, the point (storm x, z, land height + elevation) and the outer radius. Unset = nothing
/// (the lightning PSys and the thunder are other lanes'); the light flash is Storm::flash (LightningFlash.h).
using LightningCallback = std::function<void(const Storm&, const glm::vec3&, float)>;
void SetForkCallback(LightningCallback callback);
void SetSheetCallback(LightningCallback callback);
} // namespace openblack::weather::storms
