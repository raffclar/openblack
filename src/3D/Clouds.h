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

#include <vector>

#include <glm/ext/vector_uint2_sized.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/FrameAnim.h"

namespace openblack
{

/// The sky's alignment (0 good .. 1 neutral .. 2 evil in the original, kept here in openblack's units:
/// alignment = 1 - X, so -1 evil .. 1 good). The sky moves it every frame towards the target by the game time step
/// * 0.01 * 0.1 (0.001 per ms, good to evil in 2 s) and snaps it when it passes; the clouds, the land light table and
/// the sky all read the moved value. It starts neutral and a new land does not reset it (opening a land only reads it).
class SkyAlignment
{
public:
	/// @param target -1 evil .. 1 good, @param milliseconds the game time step (0 while paused)
	void Update(float target, float milliseconds) noexcept;
	[[nodiscard]] float Get() const noexcept { return _value; }

private:
	float _value {0.0f};
};

/// The sky's clouds ("Clouds" detail key): 70 mist puffs, plus
/// two huge ones pinned at the horizon, drifting with the wind and fading at the ends of their track.
class Clouds
{
public:
	struct Cloud
	{
		glm::vec3 local; ///< position along the wind track (x in -8000..8000), before the rotation to the world
		float size;
		float k; ///< Edge-on shrink factor
		bool pinned;
		int counter {0}; ///< The mist's own animation counter 0..900
		float counterRemainder {0.0f};
	};

	/// Run every time a land opens: a new layout for every land
	Clouds();

	/// A land opened: the next cloud update builds a new layout
	static void OnLandscapeOpened() noexcept;
	/// Bumped by OnLandscapeOpened
	[[nodiscard]] static uint32_t GetLandscapeGeneration() noexcept;

	/// The target of the sky's alignment (the original stores (1 - clamp((v + 1) / 2, 0, 1)) * 2): v = the alignment of
	/// the most influential player at the interface position, the value kept for every player; -1 evil .. 1
	/// good: ecs::effects::alignment::GetInterfaceAlignment() x 2 - 1 (unless the test hook or the debug "Sky
	/// alignment" slider moved off 0 say otherwise).
	[[nodiscard]] static float InfluentialPlayerAlignment() noexcept;
	/// The overcast amount at the camera, 0..1 (the light table caps its base colour at 255 - 96 * overcast, and so
	/// the clouds through table[255]): the overcast byte of
	/// weather::atmos::GetWeatherSmooth(camera) x 0.01 (it can pass 1: a byte of up to 127); 0 with a clear sky (a storm
	/// of the weather miracle gives 0.8).
	[[nodiscard]] static float WeatherOvercastAtCamera() noexcept;

	/// The clouds' 0xAARRGGBB. Integer lerp of good 0x00FFFFFF / neutral 0xC8FFFFFF / evil
	/// 0xFFAAA066 by the sky alignment X (i = trunc(X), f = trunc((X - i) * 256)), each channel times light
	/// table[255] (c * t >> 8), then c + floor((8960 - 70 c) / 256) (255 -> 220); the alpha byte is the lerped one.
	/// @param alignment -1 evil .. 1 good (X = 1 - alignment), @param table255 the light table's last entry as 0xAARRGGBB
	[[nodiscard]] static uint32_t Colour(float alignment, uint32_t table255) noexcept;

	/// Moves the clouds by the game time step (ms); nothing moves while the game is paused
	void Update(float milliseconds);
	/// The mist's draw: the cloud's own animation counter += trunc(game time step * 0.255), modulo 900.
	/// Only a cloud that the mist sends to the Z-sorter (its sphere touches the screen) is drawn,
	/// so only its counter advances, and the clouds drift out of step with each other.
	void AdvanceAnimation(size_t index, float milliseconds);

	[[nodiscard]] const std::vector<Cloud>& GetClouds() const noexcept { return _clouds; }
	/// World position of a cloud: its track rotated by the wind angle (3 pi / 4) about the island centre (1280, 1280)
	[[nodiscard]] static glm::vec3 WorldPosition(const Cloud& cloud);
	/// Edge alpha 0..255 (rounded): fades in over the first 2000 units of the track and out over the last 2000
	[[nodiscard]] static int EdgeAlpha(const Cloud& cloud);
	/// Animation frame 0..15 of the mist texture atlas: counter * 45 / 900 in integers, & 15. A whole cell each time:
	/// the original sets one UV offset and draws once, so frames switch without a blend
	[[nodiscard]] static int GetFrame(const Cloud& cloud) noexcept { return graphics::frame_anim::MistCell(cloud.counter); }

	/// Cloud shadows ("CloudShadows" key): every cloud with an alpha stamps
	/// Data\Textures\sclouds.raw (40 x 40, one texel per 10-unit cell) at its world (x, 0, z), not centred, with alpha
	/// / 255, mode 2 (the shadow: luminosity = min(luminosity, max(0x30, 255 - (255 - s) alpha / 255)),
	/// land_light::AddStamp / ApplyStamp). `shadowImage` must stay alive until the stamps are applied.
	/// @param alpha per cloud 0..255
	void StampShadows(const std::vector<uint8_t>& shadowImage, const std::vector<float>& alpha) const;

private:
	std::vector<Cloud> _clouds;
};

} // namespace openblack
