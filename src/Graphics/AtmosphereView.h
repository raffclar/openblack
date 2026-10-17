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

#include <optional>

#include <bgfx/bgfx.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Graphics/Atmosphere.h"
#include "Graphics/GlobalIllumination.h"
#include "Graphics/GraphicsHandle.h"
#include "Graphics/Underwater.h"

namespace openblack::graphics
{

/// EXPERIMENT (not for merge): a texture that a pass draws into, with its target. It makes them when it is first asked
/// for its target and releases them when it goes.
class OwnedTarget
{
public:
	OwnedTarget(const char* name, uint16_t width, uint16_t height, bgfx::TextureFormat::Enum format, bool filtered = true);
	~OwnedTarget();
	OwnedTarget(const OwnedTarget&) = delete;
	OwnedTarget& operator=(const OwnedTarget&) = delete;

	[[nodiscard]] bgfx::FrameBufferHandle Target();
	[[nodiscard]] TextureHandle Texture() const;
	[[nodiscard]] uint16_t Width() const { return _width; }
	[[nodiscard]] uint16_t Height() const { return _height; }
	/// Whether a pass has drawn into it
	bool drawn {false};

private:
	const char* _name;
	uint16_t _width;
	uint16_t _height;
	bgfx::TextureFormat::Enum _format;
	bool _filtered;
	bgfx::TextureHandle _texture = BGFX_INVALID_HANDLE;
	bgfx::FrameBufferHandle _target = BGFX_INVALID_HANDLE;
};

/// EXPERIMENT (not for merge): the air's light of one frame, for the haze, the sky, the clouds, the fog and the light in
/// the shade
struct AtmosphereFrame
{
	/// x the dust in the air, y how much denser the haze is than the real air, z the eye's height over the sea in
	/// kilometres, w the sea's level
	glm::vec4 params {1.0f, atmosphere::k_HazeScale, 0.0f, 0.0f};
	/// xyz the direction towards the sun or the moon, w 1 while the haze shows
	glm::vec4 light {0.0f, 1.0f, 0.0f, 1.0f};
	/// rgb what the air's light is scaled by on the screen
	glm::vec4 sun {1.0f};
	/// The sine of the height of the air's sun over the horizon, from the hour (under 0 after sunset)
	float sunHeight {1.0f};
	/// rgb the horizon's colour on the screen
	glm::vec4 horizon {0.0f};
	/// The light of the sun or the moon at the ground, a part of its light at noon (for the clouds and the fog)
	glm::vec3 lightTint {1.0f};
	/// lightTint in the colours of the screen, with the same strongest colour (for the clouds)
	glm::vec3 lightTintOnScreen {1.0f};
	/// The light of the sun or the moon at the ground and the sky's light, for a light of strength 1 at the top of the
	/// air (for the light in the shade)
	glm::vec3 lightAtGround {1.0f};
	/// The light of the air's sky from above on the screen, as the dome shows the sky (for the shade of the clouds)
	glm::vec3 skyFromAbove {0.0f};
	global_illumination::ShL1 sky;
	/// Where the sky's dome is: xy the middle across the land, z how far across and w how high a point must be to be on
	/// the dome. The dome writes its depth, so the haze and the fog know the sky by this.
	glm::vec4 skyDome {2560.0f, 2560.0f, 1e9f, 1e9f};
	/// The tint of the haze and the fog at the place the camera looks at (see atmosphere::PlaceTint)
	glm::vec3 placeTint {1.0f};
	/// The game's own sun or moon as it shows in the sky: xyz the direction towards it, w how strongly its glare shows;
	/// and the colour of its light on the screen
	glm::vec4 sunDisc {0.0f, 1.0f, 0.0f, 0.0f};
	glm::vec4 sunColour {0.0f};
	/// The low tier's haze for each world unit
	float lowTierDensity {0.0f};
	underwater::LightDetail hazeDetail {underwater::LightDetail::Full};
	underwater::LightDetail lightDetail {underwater::LightDetail::Full};
	/// Whether the haze and the light in the shade show
	bool haze {true};
	bool lighting {true};
};

/// EXPERIMENT (not for merge): what the renderer keeps for the air's light: the texture of the light that gets through
/// the air, the texture of the sky round the eye, the haze's grid, and the sky's light worked out on the processor,
/// which it works out again only when the light, the air or the eye's height has changed enough.
class AtmosphereView
{
public:
	AtmosphereView();

	OwnedTarget transmittance;
	OwnedTarget skyView;
	OwnedTarget aerial;
	/// The dust in the air that the texture of the light that gets through was made for
	std::optional<float> transmittanceFor;

	/// The sky's light as the processor last worked it out, and what for
	struct SkyCache
	{
		glm::vec3 towardsLight {0.0f};
		float turbidity {0.0f};
		float altitude {0.0f};
		float seconds {0.0f};
		glm::vec3 horizon {0.0f};
		global_illumination::ShL1 sky;
	};
	std::optional<SkyCache> cache;
	/// How wet the land is after rain, and the seconds of the last frame
	float wetness {0.0f};
	std::optional<float> lastSeconds;
};

} // namespace openblack::graphics
