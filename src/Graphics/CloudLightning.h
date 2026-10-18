/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <span>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/// EXPERIMENT (not for merge): the lightning inside the volumetric clouds. Each flash of a storm's lightning also
/// lights the cloud from inside, along a channel that goes across the cloud. The light flickers in a few quick pulses.
/// A near flash shows its bright channel; a far flash is sheet lightning, a wide soft glow that the cloud hides.
namespace openblack::graphics::cloud_lightning
{

/// The most flashes that light the clouds at one time
inline constexpr size_t k_MaxFlashes = 4;
/// The distance from the eye past which a flash is sheet lightning
inline constexpr float k_SheetDistance = 12000.0f;
/// The shortest and the longest time that the pulses of a flash take, in seconds
inline constexpr float k_ShortestFlicker = 0.2f;
inline constexpr float k_LongestFlicker = 1.0f;
/// The fewest and the most pulses of a flash
inline constexpr uint32_t k_FewestPulses = 2;
inline constexpr uint32_t k_MostPulses = 5;
/// The seconds for a pulse to rise to its peak, and for it to fall to a third
inline constexpr float k_PulseRise = 0.008f;
inline constexpr float k_PulseFade = 0.06f;
/// The seconds after its strike that a flash stays known, after its last pulse
inline constexpr float k_FlashMemory = k_LongestFlicker + 0.3f;
/// How far the light of a flash spreads through the cloud: a near flash, and sheet lightning. The light falls to a third
/// over this distance; the bright core round the channel is a quarter of it.
inline constexpr float k_BoltRadius = 2200.0f;
inline constexpr float k_SheetRadius = 3500.0f;
/// The light of sheet lightning, as a part of the light of a near flash
inline constexpr float k_SheetShare = 0.6f;
/// The shortest and the longest channel of a flash. The channel goes up through the storm's tower from its bottom.
inline constexpr float k_ShortestChannel = 2500.0f;
inline constexpr float k_LongestChannel = 6000.0f;
/// The channel goes up at most this far from the vertical, as the tangent of the angle
inline constexpr float k_MostLean = 0.6f;

/// The kind of flash that the eye sees
enum class FlashKind : uint8_t
{
	/// Near: the bright channel shows inside the cloud
	Bolt,
	/// Far: the cloud hides the channel, and its light shows as a wide glow
	Sheet,
};

/// A strike of a storm's lightning, as the storm has it this frame
struct Strike
{
	/// Where it struck, across the ground
	glm::vec2 position {0.0f};
	/// The seconds since the storm made it
	float time {0.0f};
	/// Its strength (a flash with thunder is stronger than one with a bolt)
	float strength {0.0f};
	/// The middle and the outer radius of the storm's own cloud
	glm::vec2 stormCentre {0.0f};
	float stormRadius {0.0f};
};

/// A flash that lights the clouds
struct FlashEvent
{
	/// Where the storm struck, across the ground
	glm::vec2 position {0.0f};
	/// Where the flash lights the cloud, across the ground (see FlashPlace)
	glm::vec2 place {0.0f};
	/// The seconds since it struck, which advance smoothly each frame
	float since {0.0f};
	float strength {0.0f};
	/// The number that gives the flash its own pulses and channel
	uint32_t seed {0};
	/// The time of the storm's strike that the last frame saw, to find a new strike at the same place
	float strikeTime {0.0f};
};

/// The flashes that light the clouds now
struct FlashEvents
{
	std::array<FlashEvent, k_MaxFlashes> events {};
	size_t count {0};

	[[nodiscard]] std::span<const FlashEvent> All() const { return {events.data(), count}; }
};

/// The light of a flash inside the cloud: a line from `start` to `end`
struct FlashLight
{
	glm::vec3 start {0.0f};
	glm::vec3 end {0.0f};
	/// The light now, 0 to the strength of the strike
	float brightness {0.0f};
	/// How far the light spreads through the cloud
	float radius {0.0f};
};

/// The number of a flash at a place
[[nodiscard]] uint32_t SeedAt(glm::vec2 position);
/// A number from 0 to 1 for a seed and an index, always the same for the same two
[[nodiscard]] float RandomOf(uint32_t seed, uint32_t index);
/// The seconds that the pulses of a flash take
[[nodiscard]] float FlickerSeconds(uint32_t seed);
/// The number of pulses of a flash
[[nodiscard]] uint32_t PulseCount(uint32_t seed);
/// How bright a flash is at a time since it struck, 0 to 1: a full first pulse, weaker pulses after it, and a dim glow
/// between them. It is 0 before the strike and after the last pulse fades.
[[nodiscard]] float FlickerEnvelope(float seconds, uint32_t seed);
/// Where a strike lights the cloud, across the ground. A strike inside the storm's reach lights the cloud over it. A
/// strike outside the reach (a storm can strike near the camera, away from its cloud) lights the storm's cloud on the
/// side towards the strike, at a place inside the cloud's edge.
[[nodiscard]] glm::vec2 FlashPlace(const Strike& strike, uint32_t seed);
/// The kind of flash at a distance from the eye
[[nodiscard]] FlashKind KindAt(float distance);
/// The light of a flash inside a storm's tower, from the tower's bottom up through its depth, as the eye sees it. The
/// channel starts at the bottom of the tower over the strike and goes up through the tower, at most 0.8 of its depth.
[[nodiscard]] FlashLight LightFor(const FlashEvent& flash, const glm::vec3& eye, float cloudBottom, float cloudDepth);
/// The flashes after `seconds` more, with the storms' strikes this frame. A strike at a new place, or a younger strike at
/// a known place, starts a new flash. When all the places are full, the new flash takes the place of the oldest.
[[nodiscard]] FlashEvents TrackFlashes(const FlashEvents& before, std::span<const Strike> strikes, float seconds);
/// The shaders' array of the lights: [i] xyz the start, w the brightness; [k_MaxFlashes + i] xyz the end, w the radius
[[nodiscard]] std::array<glm::vec4, k_MaxFlashes * 2> Pack(std::span<const FlashLight> lights);

} // namespace openblack::graphics::cloud_lightning
