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

#include <array>
#include <bit>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

/// The rules of the opening's arrival of the god and the boy's rescue: the light that falls from the sky onto the boy in
/// the sea, the camera that chases it, and the god's hand that lifts him out and sets him down beside his parents
namespace openblack::ecs::intro_rules
{

/// Where the boy swims when the light falls onto him, and where it aims
inline constexpr glm::vec3 k_BoySpot {std::bit_cast<float>(0x44B10AC9u), 0.0f, std::bit_cast<float>(0x4500580Cu)};
/// The hand that lifts the boy stands a metre and a half under the sea at his spot
inline constexpr glm::vec3 k_PickUpSpot {std::bit_cast<float>(0x44B10AC9u), std::bit_cast<float>(0xBFC00000u),
                                         std::bit_cast<float>(0x4500580Cu)};
/// The hand that sets him down stands at the parents' beach, under the sand
inline constexpr glm::vec3 k_PutDownSpot {std::bit_cast<float>(0x44BACC21u), std::bit_cast<float>(0xC0200000u),
                                          std::bit_cast<float>(0x4502B795u)};
/// The way the light comes before it is made unit length: down at a shallow slope, from beyond the boy
inline constexpr glm::vec3 k_LightWay {-1.0f, std::bit_cast<float>(0xBE75C28Fu), -1.0f};
/// The hands' size, and the way the lifting one faces
inline constexpr float k_HandScale = std::bit_cast<float>(0x3C03126Fu);
inline constexpr float k_PickUpYaw = std::bit_cast<float>(0x40490FDBu);
/// The setting-down clip jumps here when the boy is let go
inline constexpr int32_t k_LetGoMs = 2379;
/// Past this time in the setting-down clip the hand holds no one
inline constexpr float k_HoldsUntilMs = 1817.0f;
/// The boy hangs this far under the hand's grip
inline constexpr float k_HeldBelowGrip = std::bit_cast<float>(0x3F266666u);
/// What the lifting clip waits for once the light has gone
inline constexpr int32_t k_LiftGo = 1000;

/// The way the setting-down hand faces: an eighth of a turn and fifteen degrees, each step in single precision
[[nodiscard]] float PutDownYaw();

/// A clip's time moved on by a frame's milliseconds: a looping clip comes round, any other holds its last millisecond.
/// `cameRound` when the time went back.
[[nodiscard]] int32_t AdvanceClip(int32_t time, uint32_t milliseconds, int32_t playTime, bool looping, bool& cameRound);

/// The light's sprites, how it falls and how it fades once landed
namespace light
{
inline constexpr size_t k_Sprites = 20;
/// It starts this far back along its way from the boy (a negative distance), comes at this many metres a millisecond,
/// and has arrived this far along, short of him
inline constexpr float k_Start = std::bit_cast<float>(0xC57A0000u);
inline constexpr float k_Speed = std::bit_cast<float>(0x3EE66666u);
inline constexpr float k_Arrives = std::bit_cast<float>(0x45796000u);
/// The trail's sprites are this far apart
inline constexpr float k_Spacing = 6.0f;
/// The thin streak turns this many radians each millisecond
inline constexpr float k_Spin = std::bit_cast<float>(0x3924B5BEu);
/// From this far into the fall the light shows through the land and sea
inline constexpr int32_t k_ThroughEverythingAfterMs = 8237;
/// The chasing camera stays this far behind the light on its way, and this far above
inline constexpr float k_CameraBehind = 150.0f;
inline constexpr float k_CameraAbove = 30.0f;
/// Landed, the flash holds, then fades over this long, to this alpha
inline constexpr int32_t k_HoldMs = 500;
inline constexpr int32_t k_FadeMs = 1000;
inline constexpr int32_t k_FadeTo = 50;

enum class State : int32_t
{
	Falling = 0,
	Holding = 1,
	Fading = 2,
	Gone = 4,
};

/// A sprite facing the screen, as the light draws it
struct Sprite
{
	glm::vec3 position {0.0f};
	float halfWidth {0.0f};
	/// Its half height is its half width times this
	float heightFactor {1.0f};
	/// Turned in the plane of the screen
	float angle {0.0f};
	/// Its cell of the 8 by 8 sheet
	uint8_t cell {0};
	/// 0xAARRGGBB
	uint32_t argb {0};
};

struct Light
{
	std::array<Sprite, k_Sprites> sprites;
	glm::vec3 head {0.0f};
	glm::vec3 start {0.0f};
	/// Unit length, by the engine's inverse square root
	glm::vec3 way {0.0f};
	State state {State::Falling};
	/// Milliseconds of its fall
	int32_t elapsed {0};
	bool arrived {false};
	/// The milliseconds left of its hold or its fade
	int32_t timer {0};
};

/// A random number in a range, drawn as the game's C runtime draws them
using Random = std::function<float(float, float)>;

/// The light coming along `way` (not yet unit length) onto `target`: six round sprites in front, twelve larger ones
/// trailing, a soft glow and a thin streak at its head
[[nodiscard]] Light Make(const glm::vec3& target, const glm::vec3& way, const Random& random);
/// How far it has come: steadily, until it arrives just short of its target
glm::vec3 HeadPosition(Light& light);

/// What a frame of the light draws
struct Frame
{
	std::vector<Sprite> drawn;
	/// Drawn whatever is in front of it
	bool throughEverything {false};
	/// While falling, where the chasing camera is
	std::optional<glm::vec3> cameraAt;
};
/// The light moves on by a frame's game milliseconds (0 while paused, when its sprites keep their turn and size): falling,
/// it flickers along its trail; landed, its front sprite flashes three times over for the hold and the fade
void Step(Light& light, uint32_t milliseconds, const Random& random, Frame& out);
} // namespace light

/// Where the setting-down hand holds the boy: the grip point taken through the hand's bone posed at the clip's time, in
/// the world, and the boy hangs a little below it. None once the clip is past the time the hand lets go, or for a bone
/// the pose doesn't have.
[[nodiscard]] std::optional<glm::vec3> GripPoint(int32_t clipTime, std::span<const glm::mat4> bones, uint32_t bone,
                                                 const glm::vec3& point, const glm::mat4& model);

} // namespace openblack::ecs::intro_rules
