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

#include <limits>
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/ScreenPick.h"

namespace openblack::ecs::systems
{

/// What the game finds along lines and under the cursor: where a line meets the land, the land or sea under a pixel,
/// the object the interface picks under the cursor as it draws, and where a line meets a model's triangles
class PickingSystemInterface
{
public:
	/// What the interface took to be under the cursor at the last frame drawn
	struct Pick
	{
		/// The object picked, if one is
		std::optional<entt::entity> object;
		/// The point picked: the object's position, else the land or sea under the cursor
		std::optional<glm::vec3> point;
		/// How far along the view the point is
		float distance {std::numeric_limits<float>::max()};
		/// The land or sea under the cursor, whatever was picked, and how far along the view it counts (a little further
		/// than it is)
		std::optional<glm::vec3> land;
		float landDistance {std::numeric_limits<float>::max()};
		/// The help's text bubble took the cursor, nearer than anything else: no object is picked, and the point is the
		/// bubble's
		bool textBubble {false};
		/// The rope of the player's tied leash took the cursor, nearer than anything else: no object is picked, and the
		/// point stays the land's
		bool rope {false};
		/// What was picked when the hover time last started again, nothing included, and how long, in seconds of the
		/// frames drawn since, it has stayed picked
		std::optional<entt::entity> hoverObject;
		float hoverSeconds {0.0f};
	};

	/// A frame as it is drawn: the camera, its view and the cursor
	struct Frame
	{
		screen_pick::View view;
		/// The cursor's point on the near plane
		glm::vec3 nearPoint {0.0f};
		/// How long the frame took, in seconds
		float seconds {0.0f};
		/// The hand is gripping the land: the interface keeps what it picked before the grip and follows only the land
		/// under the cursor
		bool locked {false};
	};

	/// What is drawn flat on the screen under the cursor and weighed in the pick's last step, once the frame's interface is
	/// drawn: the help's text bubble (how far along the view it stands, and the point it is over) and the rope of the
	/// player's tied leash (how far along the view the cursor touches it)
	struct ScreenShapes
	{
		std::optional<float> textBubble;
		glm::vec3 textBubblePoint {0.0f};
		std::optional<float> rope;
	};

	virtual ~PickingSystemInterface() = default;

	/// Where the line from one point to another first meets the land (its height there), none over the sea
	[[nodiscard]] virtual std::optional<glm::vec3> LandAlong(glm::vec3 from, glm::vec3 to) const = 0;
	/// The same, else where the line meets the sea's level going down, no further than 7500 across from the camera
	[[nodiscard]] virtual std::optional<glm::vec3> LandOrSeaAlong(glm::vec3 from, glm::vec3 to, glm::vec3 camera) const = 0;
	/// Whether the sea's level counts under a pixel where there is no land
	enum class Sea : uint8_t
	{
		/// Only the land
		No,
		/// The sea too, no further than 7500 across from the camera, as the camera feels for what is ahead of it
		NearCamera,
		/// The sea too, anywhere the line goes down, kept within reach of the map's middle, as the interface picks
		Anywhere,
	};
	/// The land under a pixel, from the camera through the pixel's point on the near plane, at the land's height there:
	/// only the land, or the sea too where there is none (see `Sea`). The land alone and the sea anywhere are kept within
	/// reach of the map's middle.
	[[nodiscard]] virtual std::optional<glm::vec3> LandUnderPixel(glm::vec3 camera, glm::vec3 nearPoint, Sea sea) const = 0;

	/// The interface's pick at a frame as it is drawn: the drawn object under the cursor, weighed against the land
	virtual void PickUnderCursor(const Frame& frame) = 0;
	/// The last step of the frame's pick, once its interface is drawn: the text bubble and the rope under the cursor are
	/// weighed against what was picked (see `picking::WeighScreenShapes`). Nothing changes while the land is gripped.
	virtual void ResolveScreenShapes(const ScreenShapes& shapes) = 0;
	/// The pick of the last frame drawn, which the interface and the hand go by
	[[nodiscard]] virtual const Pick& GetPick() const = 0;
	/// A replayed click lands on another thing, or on none: the pick's object becomes it, at the distance given, its point
	/// the thing's position (the land under the cursor for none), until the next frame is drawn
	virtual void RetargetObject(std::optional<entt::entity> object, float distance) = 0;

	/// The land behind the hand: where the line from the camera through the hand's middle, as the hand is posed for this
	/// frame, meets the land or the sea near the camera. None when it meets neither or there is no hand.
	virtual void FindBehindHand(glm::vec3 camera, std::optional<glm::vec3> handMiddle) = 0;
	[[nodiscard]] virtual std::optional<glm::vec3> GetBehindHand() const = 0;

	/// Where a line meets an object's model as it is drawn, the hand feeling it: the nearest triangle ahead of the line's
	/// start, and its normal turned along the line
	[[nodiscard]] virtual std::optional<screen_pick::MeshHit> FeelModel(entt::entity object, glm::vec3 origin,
	                                                                    glm::vec3 direction) const = 0;
};

/// The rules by which the interface carries its pick from one frame to the next
namespace picking
{
/// While the hand grips the land the interface picks no object and weighs nothing: what it picked before stays picked,
/// and only the point and its distance follow the land under the cursor, kept as they were where there is none
[[nodiscard]] PickingSystemInterface::Pick Locked(const PickingSystemInterface::Pick& previous, std::optional<glm::vec3> land,
                                                  float landDistance);
/// How long the same thing has stayed picked: the frame's seconds are added while it does, nothing picked included, and
/// the count starts again at nothing when it changes
void CarryHover(const PickingSystemInterface::Pick& previous, PickingSystemInterface::Pick& next, float seconds);
/// The pick's last step, on what the drawn things and the land left picked:
/// - the text bubble wins over a farther object, and over the land unless the land is nearer;
/// - then the rope wins over what is left (the object, unless it is a creature; else the bubble; else the land) when the
///   cursor touches it strictly nearer;
/// - what wins gives the point: the object's place, the bubble's point, or the land's (the rope keeps the land's).
void WeighScreenShapes(PickingSystemInterface::Pick& pick, const PickingSystemInterface::ScreenShapes& shapes,
                       bool objectIsCreature);
} // namespace picking

} // namespace openblack::ecs::systems
