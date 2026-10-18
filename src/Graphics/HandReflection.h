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

#include <glm/vec3.hpp>

/// How the game mirrors the player's hand in the sea. It mirrors the hand on each frame of the land while the hand
/// shows, wherever the hand is. It draws the hand in one flat grey, with no light, no glow and only its front faces. The
/// game mirrors the thing that the hand holds with it, flat in the light that the thing took in the hand.
namespace openblack::graphics::hand_reflection
{

/// The colour of each vertex of the mirrored hand (0xAARRGGBB). The hand's material takes its alpha from the texture
/// alone, so the alpha of this colour has no effect.
inline constexpr uint32_t k_Colour = 0x65A0A0A0u;
/// The colour added after the texture (0xRRGGBB): none
inline constexpr uint32_t k_Added = 0x000000u;

/// The colour of each vertex, red, green and blue from 0 to 1
[[nodiscard]] glm::vec3 Colour() noexcept;
/// The colour added after the texture, red, green and blue from 0 to 1
[[nodiscard]] glm::vec3 Added() noexcept;

/// Whether the game mirrors the hand: the hand shows and has a model. The height of the hand, the land under it and the
/// camera have no effect.
[[nodiscard]] constexpr bool Shown(bool handShows, bool hasModel) noexcept
{
	return handShows && hasModel;
}

/// The order of the things the game mirrors after the land. The mirrored land writes no depth, so these things show
/// over all of it. Each thing hides the parts of the things after it that are behind it.
enum class MirroredThing : uint8_t
{
	Hand = 1,
	/// The thing that the hand holds
	Held = 2,
	/// The body of the player's creature
	Creature = 3,
};

/// The sort key of a mirrored thing in a view that draws the smallest key first
[[nodiscard]] constexpr uint32_t SortKey(MirroredThing thing) noexcept
{
	return static_cast<uint32_t>(thing);
}

/// The colour that poison multiplies the light of a poisoned pot by, 0xRRGGBB
inline constexpr uint32_t k_PoisonTint = 0xE8FFDDu;
/// The colour that poison adds to a poisoned pot, 0xRRGGBB
inline constexpr uint32_t k_PoisonAdded = 0x001000u;

/// The light of a held thing, as the draw of the thing in the hand works it out on one frame
struct HeldLook
{
	/// The point whose land light and land colour the thing takes
	glm::vec3 point {0.0f};
	/// The distance in front of the camera that sets the haze of the thing
	float viewDepth {0.0f};
	/// A colour that multiplies the light, 0xRRGGBB: the grey of a fire, the colour of poison. 0 for none.
	uint32_t tint {0};
	/// A colour that the draw adds, 0xRRGGBB: the glow of a fire, the colour of poison. 0 for none.
	uint32_t added {0};
	/// For a tree with a fire on it, the grey of 256 that its light is scaled down to; 0 for none
	uint32_t treeGrey {0};
};

/// What the draw of a held thing knows on a frame
struct HeldThing
{
	/// Where the thing is drawn in the hand
	glm::vec3 drawnAt {0.0f};
	/// Where the thing was on the map when the hand took it
	glm::vec3 takenAt {0.0f};
	/// The position and the facing of the camera
	glm::vec3 eye {0.0f};
	glm::vec3 forward {0.0f, 0.0f, 1.0f};
	bool tree {false};
	/// For a thing with a fire on it: the grey of its charring and its glow (0xRRGGBB); for a tree, the grey of 256
	/// that the fire scales its light down to
	bool onFire {false};
	uint8_t charredGrey {255};
	uint32_t glow {0};
	uint32_t treeGrey {0};
	/// A pot of poisoned food, with no fire on it
	bool poisonedPot {false};
};

/// The light that the draw of a held thing takes on a frame:
/// - A tree takes the land's light at its place on the map. Its haze is at that place at the height of the sea. A fire
///   on it scales its light down to the grey of the fire.
/// - Any other thing takes the land's light and haze at the point where the hand holds it. A fire on it multiplies its
///   light by the grey of the charring and adds the glow. Poison tints a pot.
[[nodiscard]] HeldLook HeldLookOf(const HeldThing& thing) noexcept;

/// The light of the mirrored held thing on this frame, and the light to keep for the next frame
struct HeldLightStep
{
	HeldLook mirrored;
	HeldLook kept;
};

/// The game draws the held thing in the hand after the sea, and each draw keeps the light of the thing. The sea mirrors
/// the thing in the light that the last draw kept. On the first frame in the hand, the thing has no kept light, so it
/// takes the light of this frame.
[[nodiscard]] constexpr HeldLightStep StepHeldLight(const std::optional<HeldLook>& kept, const HeldLook& now) noexcept
{
	return {.mirrored = kept.value_or(now), .kept = now};
}

/// The shade of 256 that each vertex of the mirrored held thing takes: all of its colour, as the sun does not shade it
inline constexpr float k_HeldShade = 256.0f;

/// A value of the frame before. At the start of each frame, before the frame makes its own value, the value of the
/// frame that ended goes in. The sea mirrors a held thing in the land's light of the frame before.
template <typename T>
class FrameBefore
{
public:
	/// Keep the value of the frame that ended; none before the first frame
	void StartFrame(const std::optional<T>& ending) noexcept { _before = ending; }
	/// The value of the frame before, or the value of this frame when there is no frame before
	[[nodiscard]] const T& Or(const T& now) const noexcept { return _before.has_value() ? *_before : now; }
	[[nodiscard]] bool Has() const noexcept { return _before.has_value(); }

private:
	std::optional<T> _before;
};

} // namespace openblack::graphics::hand_reflection
