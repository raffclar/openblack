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

#include <functional>
#include <optional>

#include <glm/vec3.hpp>

#include "Creature/CreatureLayers.h"

/// The player's creature as the Creature Cave shows it. The cave shows a copy of the creature's body, its shape, skin
/// and tattoos, by the fire at the cave's heart, while the creature itself stays where it is in the world. The copy has
/// a pose of its own: it sits by the fire looking into it, or, when it is small, sometimes lies asleep further back.
/// Whenever it isn't facing the fire it gets up and turns on the spot to face it again. Clicking it zooms the camera
/// onto it and opens the tattoo editor: it gets up and turns to face the camera, and while a mouse button is held it
/// looks at the pointer for a second and a half. While the editor is open it fidgets now and then.
namespace openblack::cave_creature
{

/// The three places the temple's walk mesh marks in the cave: the seat by the fire, the fire, and a place further back
struct Places
{
	glm::vec3 seat;
	glm::vec3 fire;
	glm::vec3 back;
};

/// How it rests by the fire
enum class Rest : uint8_t
{
	Sit,
	Sleep,
};

/// Only a creature smaller than this ever sleeps there, on the toss of a coin as the cave is made
constexpr float k_SleepsBelowSize = 1.25f;
/// The sizes the copy is drawn at, as the creature's own grown size is held between them
constexpr float k_MinSize = 0.05f;
constexpr float k_MaxSize = 4.0f;
/// Facing this close to where it should face, in radians, it rests; any further off it turns
constexpr float k_FacingWithin = 0.01f;
/// A sitting creature yawns when a draw of 0 to 999 comes up 23, once a frame
constexpr uint32_t k_YawnRange = 1000;
constexpr uint32_t k_YawnDraw = 23;
/// Zoomed onto it, a mouse button held for less than this long has it look at the pointer
constexpr float k_PointerLookMs = 1500.0f;
/// The point under the pointer it looks at lies this far into the screen
constexpr float k_PointerDepth = 3.0f;
/// The pose moves on by no more than this much of a frame's time
constexpr float k_MaxFrameMs = 200.0f;
/// The editor's fidgets: one every 20 to 40 seconds, the first nine a random choice of three, then it sits
constexpr float k_IdleBaseMs = 20000.0f;
constexpr uint32_t k_IdleRandomMs = 20000;
constexpr uint32_t k_IdleFidgets = 10;

/// The size the copy is drawn at: the creature's grown size, within the sizes a creature can be drawn at
[[nodiscard]] float SizeOf(float grownSize);
/// How it rests: asleep only when smaller than k_SleepsBelowSize and the coin says so
[[nodiscard]] Rest RestFor(float size, bool coin);
/// Where it is placed as the cave is made: from the seat towards the place further back, the further the smaller it is
/// (none of the way from size 2 up, all of it at size 0), and a sleeper twice as far less half the way; always at the
/// seat's height
[[nodiscard]] glm::vec3 StandsAt(const Places& places, float size, Rest rest);
/// The way it faces to face a point, as a heading about the vertical
[[nodiscard]] float HeadingTowards(const glm::vec3& from, const glm::vec3& to);
/// Whether a heading is far enough off the one wanted to turn
[[nodiscard]] bool MustTurn(float heading, float wanted);

/// What the copy is doing, kept between frames
struct State
{
	Rest rest {Rest::Sit};
	/// The way it faces
	float heading {0.0f};
	/// A turn waiting for the body to be free
	std::optional<float> wantedHeading;
	/// The turn it is making, from one heading to another, timed in milliseconds of its spin
	struct Turn
	{
		float from;
		float to;
		float timeMs;
		float durationMs;
		/// The two spins it blends, and how far from the first to the second
		size_t spin;
		size_t blendedWith;
		float blendWeight;
	};
	std::optional<Turn> turn;
	/// The camera was zoomed onto it last frame
	bool zoomed {false};
	/// How long a mouse button has been held while zoomed onto it, none while none is
	std::optional<float> heldMs;
	/// The editor's fidgets: the time to the next and how many so far
	float idleMs {0.0f};
	uint32_t fidgets {0};
};

/// The body it plays, as CreatureAnimation keeps it
struct Body
{
	creature_layers::BodyAction body {};
	creature_layers::GestureLayer gesture {};
	std::optional<glm::vec3> lookAt;
};

/// What it sees this frame
struct Frame
{
	/// The camera is zoomed onto it, as while the tattoo editor is open
	bool zoomed {false};
	/// The fire, and where the camera looks at it from and looks at
	glm::vec3 fire {0.0f};
	glm::vec3 eye {0.0f};
	glm::vec3 focus {0.0f};
	/// Where it stands
	glm::vec3 position {0.0f};
	/// A mouse button is held, and the left one, and the point under the pointer k_PointerDepth into the screen
	bool buttonHeld {false};
	bool leftHeld {false};
	glm::vec3 pointer {0.0f};
	/// The frame's milliseconds, at most k_MaxFrameMs
	float milliseconds {0.0f};
};

/// A random whole number below a range
using Random = std::function<uint32_t(uint32_t range)>;

/// The editor's fidgets start again from the beginning as it opens, the next 20 to 40 seconds away
void StartIdle(State& state, const Random& random);

/// One frame of the copy's life. Not zoomed onto, it faces the fire: when it does it rests (sitting looks into the
/// fire), and when it doesn't its rest ends and it turns to face it. Zoomed onto, it gets up and turns to face the
/// camera as the zoom starts, looks at the pointer while a button has been held for under a second and a half, and
/// fidgets now and then. A turn waits for the body to be free. Returns the heading of a turn to start this frame.
[[nodiscard]] std::optional<float> Step(State& state, Body& body, const Frame& frame, const Random& random);

/// The turn some milliseconds of its spin on: finished, it faces the way it turned to
void AdvanceTurn(State& state, float milliseconds);

/// Whether the body is busy: playing an action or turning
[[nodiscard]] bool Busy(const State& state, const Body& body);

} // namespace openblack::cave_creature
