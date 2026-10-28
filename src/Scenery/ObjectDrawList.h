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

#include <algorithm>
#include <array>
#include <bitset>
#include <optional>
#include <span>
#include <unordered_set>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

// Which of the land's objects the game draws on a frame, and so which of them play on in their clips. The game keeps a
// list of the objects on the land's blocks that are in view and near, made again every few turns or when a block in
// view starts or stops to show land triangles. On a frame where the camera has moved it draws every object in the list and
// notes which were on screen; on a frame where it stands still it draws only those. Things that only move as they are drawn
// (the scripts' gates) freeze while they aren't. Pure functions and a small clock, tested on their own.

namespace openblack::object_draw_list
{

/// A land block's objects are in the list when the block's middle is nearer the eye than this
constexpr float k_Range = 700.0f;
/// The list is made again once more than this many turns have passed since it was last made
constexpr uint32_t k_RebuildTurns = 10;
/// The camera has moved when its eye or what it looks at has gone further than this from where they were when it last
/// moved, as a squared distance
constexpr float k_MovedSquared = 1.0f;
/// A land block is 160 units a side
constexpr float k_BlockSize = 160.0f;
/// A land cell's altitude in units of height
constexpr float k_AltitudeUnit = 0.67f;

/// A land block as the list sees it: where its corner of least x and z is, and the highest altitude of its cells. With
/// the land's reflection drawn in the sea, the block reaches as far below the water as it stands above it.
struct Block
{
	glm::vec2 corner {0.0f};
	uint8_t highestAltitude {0};
	bool reflected {true};
};

/// Whether any of a block can be seen: false only when all eight corners of the box over it lie beyond the same side
/// of the view, or all lie behind the near plane
[[nodiscard]] bool BlockInView(const glm::mat4& viewProjection, float nearPlane, const Block& block);
/// How far the eye is from the block's middle: half way up its box, or at the water's level with the reflection drawn
[[nodiscard]] float BlockDistance(glm::vec3 eye, const Block& block);
/// Whether the upper side of a land triangle faces the eye and a part of the triangle is in view, as the game tests the
/// triangles of a block that it draws. A triangle that the view cuts shows when a part of it remains.
[[nodiscard]] bool TriangleShows(const glm::mat4& viewProjection, float nearPlane, glm::vec3 eye, glm::vec3 a, glm::vec3 b,
                                 glm::vec3 c);
/// Whether the upper side of a land triangle faces the eye
[[nodiscard]] bool TriangleFacesEye(glm::vec3 eye, glm::vec3 a, glm::vec3 b, glm::vec3 c);
/// Whether a part of a triangle, given in clip space, remains after the view cuts it
[[nodiscard]] bool ClippedTriangleShows(float nearPlane, glm::vec4 a, glm::vec4 b, glm::vec4 c);
/// A bit for each side of the view that a point in clip space is beyond
[[nodiscard]] uint8_t SidesBeyond(float nearPlane, glm::vec4 p);

/// Whether one or more of a block's triangles show (see TriangleShows). `cornerAt(i)` gives corner i of the
/// `cornerCount` corners in the world; the function asks for a corner only when a triangle needs it, and only once.
template <typename CornerAt>
[[nodiscard]] bool BlockShowsWith(const glm::mat4& viewProjection, float nearPlane, glm::vec3 eye, size_t cornerCount,
                                  CornerAt&& cornerAt, std::span<const uint16_t> triangles)
{
	constexpr size_t k_MostCorners = 17 * 17;
	struct Corner
	{
		glm::vec3 world;
		glm::vec4 clip;
		uint8_t beyond;
	};
	std::array<Corner, k_MostCorners> cache;
	std::bitset<k_MostCorners> known;
	const auto cornerOf = [&](uint16_t i) -> const Corner& {
		if (!known.test(i))
		{
			const auto world = cornerAt(i);
			const auto clip = viewProjection * glm::vec4(world, 1.0f);
			cache[i] = {.world = world, .clip = clip, .beyond = SidesBeyond(nearPlane, clip)};
			known.set(i);
		}
		return cache[i];
	};
	const size_t count = std::min(cornerCount, k_MostCorners);
	for (size_t t = 0; t + 2 < triangles.size(); t += 3)
	{
		if (triangles[t] >= count || triangles[t + 1] >= count || triangles[t + 2] >= count)
		{
			continue;
		}
		const auto& a = cornerOf(triangles[t]);
		const auto& b = cornerOf(triangles[t + 1]);
		const auto& c = cornerOf(triangles[t + 2]);
		// All three corners beyond one side: the view cuts the whole triangle away
		if ((a.beyond & b.beyond & c.beyond) != 0 || !TriangleFacesEye(eye, a.world, b.world, c.world))
		{
			continue;
		}
		if ((a.beyond | b.beyond | c.beyond) == 0 || ClippedTriangleShows(nearPlane, a.clip, b.clip, c.clip))
		{
			return true;
		}
	}
	return false;
}

/// Whether one or more of a block's triangles show (see TriangleShows). The corners are the block's 17 by 17 corners,
/// [x * 17 + z], and each three triangle indices are one triangle.
[[nodiscard]] bool BlockShows(const glm::mat4& viewProjection, float nearPlane, glm::vec3 eye,
                              std::span<const glm::vec3> corners, std::span<const uint16_t> triangles);
/// Whether a block in view changes the list. The game keeps, for each block, whether it showed triangles when it was
/// last in view. When a block in view shows triangles and did not, or did and does not now, the game makes the list
/// again. A block that goes out of view keeps the last value, and does not change the list.
[[nodiscard]] constexpr bool ShowingChanged(bool shows, bool& lastShown)
{
	const bool changed = shows != lastShown;
	lastShown = shows;
	return changed;
}

/// Whether a block's objects go in the list
[[nodiscard]] bool BlockListed(const glm::mat4& viewProjection, float nearPlane, glm::vec3 eye, const Block& block);

/// The most objects the list holds; the nearest blocks fill it first
constexpr size_t k_MostObjects = 2999;

/// A land block in view, by its column and row, and how far the eye is from its middle
struct NearBlock
{
	glm::ivec2 block {0};
	float distance {0.0f};
};

/// The blocks in view from near to far, given in the land's order of its blocks. Of two blocks as far off, the one
/// later in the land's order comes first, as the game sorts them.
[[nodiscard]] std::vector<NearBlock> NearToFar(std::span<const NearBlock> inLandOrder);

/// Makes the draw list. The blocks are walked from near to far up to the first that isn't within range; each adds its
/// objects in the order it keeps them, then the objects off the land's blocks. An object already listed, or one left
/// out (`leftOut`: those drawn on their own), isn't added, and once the list holds `most` nothing more is.
/// `objectsOn(block)` gives a block's objects.
template <typename ObjectsOn, typename LeftOut>
[[nodiscard]] std::vector<entt::entity> MakeList(std::span<const NearBlock> nearToFar, ObjectsOn&& objectsOn,
                                                 std::span<const entt::entity> offBlocks, LeftOut&& leftOut,
                                                 size_t most = k_MostObjects)
{
	std::vector<entt::entity> list;
	std::unordered_set<entt::entity> listed;
	const auto add = [&](std::span<const entt::entity> objects) {
		for (const auto object : objects)
		{
			if (list.size() >= most)
			{
				return;
			}
			if (!listed.contains(object) && !leftOut(object))
			{
				list.push_back(object);
				listed.insert(object);
			}
		}
	};
	for (const auto& block : nearToFar)
	{
		if (block.distance >= k_Range)
		{
			break;
		}
		add(objectsOn(block.block));
		add(offBlocks);
	}
	return list;
}

/// How the camera sees the screen, for telling what is on it
struct View
{
	glm::mat4 viewProjection {1.0f};
	float nearPlane {1.0f};
	/// The projection's scale of height, one over the tangent of half the field of view up and down
	float focalHeight {1.0f};
	/// The screen's width over its height
	float aspect {1.0f};
	glm::vec3 eye {0.0f};
};
/// Whether an object's sphere shows on the screen, as the game tests an object it draws: not wholly behind the near
/// plane, and the eye inside it or the square its size covers on the screen overlapping the screen. The square's side
/// is scaled by the screen's width both ways, so it reaches further up and down than across.
/// `origin` is where the object stands, `centre` the middle of its model's box and `radius` the box's half diagonal,
/// both placed and sized as the object is.
[[nodiscard]] bool SphereOnScreen(const View& view, glm::vec3 origin, glm::vec3 centre, float radius);

/// What a frame draws of the list
struct Frame
{
	/// The list was made again this frame
	bool rebuilt {false};
	/// Every object in the list is drawn, and which are on screen noted; otherwise only those last on screen
	bool drawAll {false};
};
/// Whether an object is drawn on a frame: it has to be in the list, and be on screen when last noted unless the frame
/// draws them all
[[nodiscard]] constexpr bool Drawn(const Frame& frame, bool listed, bool lastOnScreen)
{
	return listed && (frame.drawAll || lastOnScreen);
}
/// What a thing is, for whether the game draws it outside the list
struct OwnDraw
{
	/// A temple: the game draws its heart and its parts every frame
	bool temple {false};
	/// A villager that a script draws in high detail
	bool highDetail {false};
	/// A thing that a script walks along a track: the game keeps it on a list of its own while it walks, and draws all of
	/// that list after the land's list
	bool walkingTrack {false};
};
/// Whether the game draws a thing on its own, whatever the land's list says
[[nodiscard]] constexpr bool DrawnOnItsOwn(const OwnDraw& thing)
{
	return thing.temple || thing.highDetail || thing.walkingTrack;
}
/// Whether a listed thing stays drawn on the next frames that do not draw all of the list. A full frame decides it:
/// - A villager or a creature stays drawn, on the screen or not.
/// - Another thing stays drawn when its sphere was on the screen and it did not fade out completely.
[[nodiscard]] constexpr bool StaysDrawn(bool alwaysDrawn, bool onScreen, bool fadedOut)
{
	return alwaysDrawn || (onScreen && !fadedOut);
}

/// The test that the draw of a listed thing does on a full frame, which decides whether the thing stays drawn
enum class KeptTest : uint8_t
{
	/// The sphere of the thing is on the screen and the thing did not fade out completely
	SphereAndFade,
	/// Only the sphere of the thing is on the screen. A building that its site draws, or a broken building, has no fade
	/// test.
	Sphere,
	/// The draw does no test, so the thing stays drawn
	Always,
};

/// What decides the test of a listed thing
struct KeptThing
{
	/// A villager or a creature
	bool alwaysDrawn {false};
	/// A broken building, drawn as its broken model
	bool broken {false};
	/// Not a building that its site of building or repair draws only as far up as it stands
	bool drawnWhole {true};
	/// A building that its site draws, of which some part shows
	bool partlyShown {false};
};

/// The test that the draw of a listed thing does:
/// - A villager or a creature: no test.
/// - A broken building: the sphere of its whole model.
/// - A building with a site that shows a part of it: the sphere of its whole model.
/// - A building with a site that shows no part of it: no test. Its site draws nothing.
/// - Another thing: the sphere and the fade.
[[nodiscard]] constexpr KeptTest KeptTestOf(const KeptThing& thing)
{
	if (thing.alwaysDrawn)
	{
		return KeptTest::Always;
	}
	if (thing.broken || thing.partlyShown)
	{
		return KeptTest::Sphere;
	}
	return thing.drawnWhole ? KeptTest::SphereAndFade : KeptTest::Always;
}

/// Whether a listed thing stays drawn on the next frames that do not draw all of the list, by the test of its draw
[[nodiscard]] constexpr bool StaysDrawn(KeptTest test, bool onScreen, bool fadedOut)
{
	switch (test)
	{
	case KeptTest::Always:
		return true;
	case KeptTest::Sphere:
		return onScreen;
	case KeptTest::SphereAndFade:
		return onScreen && !fadedOut;
	}
	return true;
}

/// The frames that make the list again because an event asks. The game keeps one count of frames. Each request sets
/// the count, so a later request replaces an earlier one, also a larger one. Each frame that makes the list again
/// decreases the count by one, for all causes.
class RebuildRequests
{
public:
	/// Make the list again on the next `frames` frames
	constexpr void Ask(uint32_t frames) { _frames = frames; }
	/// Whether a request makes the list again on this frame
	[[nodiscard]] constexpr bool Pending() const { return _frames > 0; }
	/// Record that this frame made the list again
	constexpr void Rebuilt()
	{
		if (_frames > 0)
		{
			--_frames;
		}
	}
	[[nodiscard]] constexpr uint32_t Left() const { return _frames; }
	constexpr void Reset() { _frames = 0; }

private:
	uint32_t _frames {0};
};

/// When the list is made again and whether the camera has moved, frame by frame
class Clock
{
public:
	/// The next frame, at a game turn, with the camera's eye and what it looks at. Forced makes the list again, as a
	/// new land or a change in the blocks in view does.
	[[nodiscard]] Frame Next(uint32_t turn, glm::vec3 eye, glm::vec3 focus, bool forced);
	/// A new land makes the list again on its first frame
	void Reset();

private:
	std::optional<uint32_t> _rebuiltTurn;
	glm::vec3 _eye {0.0f};
	glm::vec3 _focus {0.0f};
};

} // namespace openblack::object_draw_list
