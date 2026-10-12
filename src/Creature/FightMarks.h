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

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/// The sparkles that mark the player's queued fight moves. Each click on the opponent, on the player's own creature
/// or on the arena's ground (and each special move or miracle drawn as a gesture) leaves a sparkle where it landed,
/// one for each move waiting in the queue. They follow the body or the creature as it moves, and are drawn half way
/// towards the camera so the body doesn't hide them. Their colour is their place in the queue: the next move's is
/// yellow, the following ones paler yellow, then orange, red and dark red towards the twelfth. A blow's sparkle
/// flickers through its sheet faster the longer its click was held.
namespace openblack::creature_fight::marks
{
/// Where a sparkle sits
enum class Anchor : uint8_t
{
	/// On the opponent's body, moving with it
	Opponent,
	/// On the player's own creature, moving with it
	Self,
	/// On the ground, in the creature's own frame, so that it moves and turns with the creature
	Ground,
};

struct Mark
{
	Anchor anchor {Anchor::Ground};
	/// The triangle of the body's skin it sits on (Opponent, Self): its submesh, its first index, and how far along its
	/// first and third sides from its first corner, so that it moves with the skin as the body is posed
	uint32_t subMesh {0};
	uint32_t firstIndex {0};
	float s {0.0f};
	float t {0.0f};
	/// Where it sits in the creature's frame (Ground)
	glm::vec3 local {0.0f};
	/// How long its click has been held, up to the full charge
	float chargeMs {0.0f};
	/// Its frame in the sparkle sheet
	uint32_t frame {0};
};

/// The sparkles, one for each move the player has queued, in queue order
class MarkList
{
public:
	static constexpr size_t k_Capacity = 12;

	/// A sparkle added at the back. False when twelve are already shown.
	bool Add(const Mark& mark);
	/// A move made in place of the queue keeps only the sparkle added last, at the front. The front's frame is kept.
	void KeepNewest();
	/// The front move was made
	void PopFront();
	/// The move added last was taken away
	void DropNewest();
	void Clear() { _count = 0; }
	/// The sparkle added last charges while its click is held, up to the full charge
	void Charge(float elapsedMs);
	/// One drawing: every blow's sparkle on the opponent moves on through the sheet by its charge
	void Animate();
	[[nodiscard]] size_t Size() const { return _count; }
	[[nodiscard]] bool Full() const { return _count >= k_Capacity; }
	[[nodiscard]] std::span<const Mark> Marks() const { return {_marks.data(), _count}; }

private:
	std::array<Mark, k_Capacity> _marks {};
	size_t _count {0};
};

/// A held click charges its sparkle up to this
constexpr float k_FullChargeMs = 1200.0f;
/// The sparkles' sheet: eight frames a row, sixteen used
constexpr uint32_t k_FramesPerRow = 8;
constexpr uint32_t k_FrameCount = 16;
/// They are drawn this share of the way from where they sit to the camera
constexpr float k_TowardsCamera = 0.5f;

/// The frame a blow's sparkle moves on to at a drawing: one frame for each whole eighth of the full charge held
[[nodiscard]] uint32_t NextFrame(uint32_t frame, float chargeMs);
/// A sparkle's colour by its place in the queue, 0xAARRGGBB, always somewhat see-through. The front one's blue spills
/// over into its green, so it shows yellow rather than white.
[[nodiscard]] uint32_t ColourOf(size_t place, size_t capacity = MarkList::k_Capacity);
/// The colour as the renderer takes it: red, green, blue and alpha from 0 to 1
[[nodiscard]] glm::vec4 Rgba(uint32_t argb);
/// Where a sparkle is drawn, seen from a camera
[[nodiscard]] glm::vec3 DrawnAt(const glm::vec3& at, const glm::vec3& eye);
/// The top left corner of a frame in the sheet and the frame's size, as texture coordinates
[[nodiscard]] glm::vec4 FrameUv(uint32_t frame);

/// A point in a frame, and back out of it
[[nodiscard]] glm::vec3 ToLocal(const glm::mat4& frame, const glm::vec3& point);
[[nodiscard]] glm::vec3 ToWorld(const glm::mat4& frame, const glm::vec3& local);
/// The ground's frame of a creature: at its feet, turned by its heading
[[nodiscard]] glm::mat4 GroundFrame(const glm::vec3& position, float heading);

/// Steps the sparkles' flicker at most 60 times a second of real time, as the game did drawing at the screen's
/// refresh. Returns whether this frame is a step.
class Pace
{
public:
	static constexpr float k_StepMs = 1000.0f / 60.0f;
	bool Advance(float realMs);

private:
	float _sinceStepMs {k_StepMs};
};

} // namespace openblack::creature_fight::marks
