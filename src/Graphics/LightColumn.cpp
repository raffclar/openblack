/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LightColumn.h"

#include <cmath>

#include <algorithm>
#include <numbers>

namespace openblack::graphics::light_column
{

bool Shown(PlayerNames owner, bool borderShown)
{
	return owner != PlayerNames::NEUTRAL && borderShown;
}

bool TempleShown(bool destroying, float destructionClock)
{
	return !destroying || destructionClock < k_DestructionSeconds;
}

uint8_t TempleAlpha(float pulse)
{
	// Rounded down, as the game turns it to a whole number
	return static_cast<uint8_t>((std::clamp(pulse, 0.0f, 1.0f) + 1.0f) * k_TempleAlphaLow);
}

glm::vec4 Tint(uint32_t colour, uint8_t alpha)
{
	constexpr float k_ByteMax = 255.0f;
	return {static_cast<float>((colour >> 16u) & 0xFFu) / k_ByteMax, static_cast<float>((colour >> 8u) & 0xFFu) / k_ByteMax,
	        static_cast<float>(colour & 0xFFu) / k_ByteMax, static_cast<float>(alpha) / k_ByteMax};
}

glm::mat4 Model(const glm::vec3& position, const glm::vec3& eye)
{
	// The model's sideways axis is turned a quarter round from the way to the eye, so its depth points away from it
	const float angle = std::atan2(eye.z - position.z, eye.x - position.x) + std::numbers::pi_v<float> * 0.5f;
	const float cosine = std::cos(angle);
	const float sine = std::sin(angle);
	glm::mat4 model(1.0f);
	model[0] = glm::vec4(cosine, 0.0f, sine, 0.0f);
	model[1] = glm::vec4(0.0f, k_UprightStretch, 0.0f, 0.0f);
	model[2] = glm::vec4(-sine, 0.0f, cosine, 0.0f);
	model[3] = glm::vec4(position, 1.0f);
	return model;
}

} // namespace openblack::graphics::light_column
