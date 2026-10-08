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
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/Billboard.h"

namespace openblack
{
class L3DAnim;
}

namespace openblack::graphics
{
class L3DMesh;
struct IntroLightOverlay;
} // namespace openblack::graphics

/// The intro's JC specials (docs/bw1-notes/intro.md): PLAY_JC_SPECIAL 0, 1, 2, 4, 5, their per-frame update, the
/// grip of the intro hand (read by the landscape draw) and ReleaseAll. Special 6 (MissionaryBoat) is ecs/MissionaryBoat.h.
///
/// In FollowUs (Land 1): 0 makes the light that falls from the sky onto the Son ("the light", 20 sprites of
/// misc0.raw in mode 13), 1 puts the drawn camera behind it (the engine's debug camera mode 2), 2 gives the
/// camera back 4.5 s later. When the light lands the intro hand appears at the Son's spot and plays
/// Data\MISC\hand_intro2.anm ("Hand_Pick_Up_Swimmer", 766 ms, one shot); then 4 puts it at the boat with
/// Data\MISC\hand_intro.anm ("Hand_Put_Down_Swimmer", 3066 ms, looping), 5 plays it with the Son in its grip (the
/// SuperVillager's feature 7) and the second 5 jumps to 2379 ms and lets the Son go; at the end of the clip the hand goes.
namespace openblack::ecs::intro_special
{

/// The Son's spot of FollowUs, where the light lands and the pick-up hand stands
inline constexpr glm::vec3 k_SonSpot {std::bit_cast<float>(0x44B10AC9u), 0.0f, std::bit_cast<float>(0x4500580Cu)};
/// The put-down hand (special 4)
inline constexpr glm::vec3 k_HandSpot {std::bit_cast<float>(0x44BACC21u), std::bit_cast<float>(0xC0200000u),
                                       std::bit_cast<float>(0x4502B795u)};
/// The pick-up hand in state 12 (set once), 1.5 below the Son's spot
inline constexpr glm::vec3 k_PickUpSpot {std::bit_cast<float>(0x44B10AC9u), std::bit_cast<float>(0xBFC00000u),
                                         std::bit_cast<float>(0x4500580Cu)};
/// The light's direction before it is normalised
inline constexpr glm::vec3 k_LightDirection {std::bit_cast<float>(0xBF800000u), std::bit_cast<float>(0xBE75C28Fu),
                                             std::bit_cast<float>(0xBF800000u)};
inline constexpr float k_HandScale = std::bit_cast<float>(0x3C03126Fu); ///< 0.008
inline constexpr float k_PickUpYaw = std::bit_cast<float>(0x40490FDBu); ///< pi
inline constexpr int32_t k_SeekMs = 0x94B;                              ///< 2379, the second special 5
inline constexpr float k_GripLastMs = 1817.0f;                          ///< No grip past this clip time
inline constexpr float k_GripDrop = std::bit_cast<float>(0x3F266666u);  ///< 0.65 below the grip bone
inline constexpr int32_t k_PickUpGo = 0x3E8;                            ///< 1000: the pick-up clip may run

/// The put-down hand's yaw, -pi/4 - 15 x pi/180, each step a float
[[nodiscard]] float PutDownYaw();

/// The object animation time rule as the intro update writes it: n = time + ms; a looping clip n % duration
/// (signed), else min(n, duration - 1). `wrapped` when the result < time
[[nodiscard]] int32_t AdvanceTime(int32_t time, uint32_t milliseconds, int32_t duration, bool looping, bool& wrapped);

/// The light of special 0: 20 sprites (zeroed, each with its own material) of misc0.raw in render mode 13
/// (render_modes::materials::k_Misc0Additive)
namespace light
{
inline constexpr int k_Sprites = 20;
inline constexpr float k_Far = std::bit_cast<float>(0xC57A0000u);        ///< -4000: the start, along -direction
inline constexpr float k_Speed = std::bit_cast<float>(0x3EE66666u);      ///< 0.45 units a millisecond
inline constexpr float k_Travel = std::bit_cast<float>(0x45796000u);     ///< 3990: then it has arrived
inline constexpr float k_Spacing = std::bit_cast<float>(0x40C00000u);    ///< 6: sprite i trails 6 i behind
inline constexpr float k_Spin = std::bit_cast<float>(0x3924B5BEu);       ///< Sprite 19 turns 0.00015708 rad/ms
inline constexpr int32_t k_ZAlwaysAfterMs = 0x202D;                      ///< 8237: ZFUNC ALWAYS after it
inline constexpr float k_CameraBack = std::bit_cast<float>(0x43160000u); ///< 150 behind the head
inline constexpr float k_CameraUp = std::bit_cast<float>(0x41F00000u);   ///< 30 above
inline constexpr int32_t k_HoldMs = 500;
inline constexpr int32_t k_FadeMs = 1000;
inline constexpr int32_t k_FadeFloor = 50; ///< The last alpha

enum class State : int32_t
{
	Falling = 0, ///< 20 sprites along the beam
	Hold = 1,    ///< 500 ms, sprite 0 three times
	Fade = 2,    ///< 1000 ms, sprite 0's alpha 255 -> 50
	Done = 4,    ///< nothing drawn; the intro update deletes it
};

struct Light
{
	std::array<graphics::billboard::Sprite, k_Sprites> sprites;
	glm::vec3 head {0.0f};      ///< Where the beam's front is
	glm::vec3 start {0.0f};     ///< 4000 up the beam from the target
	glm::vec3 direction {0.0f}; ///< Normalised (with the engine's inverse square root)
	State state {State::Falling};
	int32_t elapsed {0}; ///< ms of the fall (the draw's frame game time)
	bool arrived {false};
};

/// A random float in a range (game_random::crt::Random): replaceable for the tests
using RandomFn = std::function<float(float, float)>;

/// The light falling onto `target` along `direction`
[[nodiscard]] Light Create(const glm::vec3& target, const glm::vec3& direction, const RandomFn& random);
/// d = (float)elapsed x 0.45, past 3990 it is 3990 and it has arrived; start + direction d
glm::vec3 HeadPosition(Light& light);
/// (special 1) HeadPosition into `head`, returned (the debug camera's position)
glm::vec3 Start(Light& light);

/// What one call of the light's Z-object callback draws
struct Frame
{
	/// The sprite draws in order (screen sprites, mode A, each with its near test)
	std::vector<graphics::billboard::Sprite> drawn;
	/// The depth test ALWAYS around the draws, LESSEQUAL back after
	bool depthAlways {false};
	/// the falling state's write of the debug camera's position
	std::optional<glm::vec3> cameraPosition;
};
/// The Z-object callback (queued with key |head - camera|^2, SumOrder::XYZ). `timer` is the shared timer of the hold
/// and the fade; `milliseconds` the frame's game time (0 while paused: then the falling state draws no Random, the
/// other two still draw two)
void Draw(Light& light, uint32_t milliseconds, int32_t& timer, const RandomFn& random, Frame& out);
} // namespace light

/// The grip as a pure function: nothing past 1817 ms; else the hand posed at `time` with its own matrix (`model`),
/// bone `bone` (the EBone block's bone) times the EBone's matrix 0 (the EBone point taken through the bone), its y
/// less 0.65. (pending) a bone outside the pose (the original reads past the buffer) gives nothing
[[nodiscard]] std::optional<glm::vec3> GripPoint(const graphics::L3DMesh& mesh, const L3DAnim& clip, int32_t time,
                                                 const glm::mat4& model, const std::array<float, 12>& eBoneMatrix,
                                                 int32_t bone);

/// PLAY_JC_SPECIAL: 0, 1, 2, 4, 5. 6 is MissionaryBoat (CHLApi.cpp), 3 a script graphics effect (pending: not in the
/// game's scripts), others nothing
void Play(int32_t special);
/// The per-frame update then, when it queued the light, the light's callback (one Z object a frame; drawn in the main
/// view's queue). `milliseconds` = the frame's game time (game_clock::FrameGameMs)
void Update(uint32_t milliseconds);
/// The point the SuperVillagers with feature 7 are drawn at while the Son is followed (the first special 5 to the
/// second, or special 0); called before Update (the original reads the hand as the last frame left it).
/// (approximate) when the grip writes nothing the original reads a stale value: the last point written is kept
[[nodiscard]] std::optional<glm::vec3> Grip();
/// The light's Z object of this frame for the draw (Graphics/OverlayFrame.h), before DrawScene
void FillFrame(graphics::IntroLightOverlay& out);
/// IS_PLAYING_JC_SPECIAL(13): set when the pick-up clip wraps, never cleared
[[nodiscard]] bool HasPickUpClipFinished();
/// (THING_JC_SPECIAL 18, a script reboot) the light, the hand (object, mesh, anim), the material, not playing, the
/// debug camera off, state -1
void ReleaseAll();

} // namespace openblack::ecs::intro_special
