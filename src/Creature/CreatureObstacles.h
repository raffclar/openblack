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
#include <optional>
#include <span>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureRoute.h"

/// What stands in a walking creature's way, and the circles it plans its route round.
///
/// Each kind of thing has its own rule for whether a creature walks round it or straight through it, and its own
/// circles: a building or a fence is the outline of its model's box (one circle, or a row of up to five along a long
/// box), most other things one circle at their place, a tree a small circle at its trunk, the temple a wide circle and
/// seven spokes, another creature a circle a little wider than the two of them. A circle is grown by part of the
/// creature's own radius: less of it for things taller than the creature, more for low ones, so a creature brushes
/// past tall things and keeps further from what it might trip over. Something burning is given five more units.
namespace openblack::creature_obstacles
{

/// When a creature walks round a thing rather than through it
enum class Avoidance : uint8_t
{
	/// Never: villagers, animals, fish farms, footballs, street lanterns, town flags and prayer sites
	Never,
	/// Always: animated scenery such as the creature's gate and its plinth
	Always,
	/// A building, a feature or the temple: when at least a tenth of the creature's height, unless it has only just been
	/// begun on its site; or when it burns
	Building,
	/// Anything else fixed or lying on the land: when at least a tenth of the creature's height, or when it burns
	Object,
	/// A tree: when at least one and a half times the creature's height, or when it burns
	Tree,
	/// A forest as a whole: only while it burns
	Forest,
	/// Fields, crops, dead trees and piles of food: only while they burn, and never for a creature a script controls
	BurningUnlessScripted,
	/// A fragment of something broken: only while it burns
	Burning,
	/// Another creature: only while it isn't walking along a route of its own
	Creature,
};

/// The circles a thing is planned round with
enum class Shape : uint8_t
{
	/// The outline of its model's box: one circle at the box's middle, or a row along a box much longer than wide
	ModelBox,
	/// One circle at its place, as wide as the thing less part of the creature's radius
	AtPlace,
	/// A small circle at its trunk: a tenth of its width, a quarter of a unit at most
	Trunk,
	/// Animated scenery other than the gate: one circle at its place, seven tenths of its model's half diagonal,
	/// whatever its size
	Scenery,
	/// The temple: see TempleCircles
	Temple,
	/// Another creature: a little wider than the two of them
	Creature,
};

/// The creature planning its route
struct Walker
{
	glm::vec2 position;
	float height;
	/// How far round its middle the creature reaches
	float radius;
	bool scriptControlled;
};

/// A thing that may stand in the creature's way
struct Thing
{
	Avoidance avoidance;
	Shape shape;
	/// Where it stands on the land, and how far it is turned (radians: its model's x axis points along (cos, sin))
	glm::vec2 position;
	float yAngle;
	float scale;
	/// Its model's box, unscaled
	glm::vec3 boxCentre;
	glm::vec3 boxHalfSize;
	/// How tall it stands, and how wide it is across the ground from its middle (see ModelHeight and ModelWidth; another
	/// creature's are its own height and radius)
	float height;
	float width;
	/// Burning, and having fire about it at all (which keeps the creature further off)
	bool burning;
	bool hasFire;
	/// A building only just begun on its site, with almost nothing of it built
	bool justBegun;
	/// Another creature walking along a route of its own
	bool walking;
	/// A temple: its player's alignment, and which of the six places round it have a worship site
	float playerAlignment;
	std::array<bool, 6> worshipSites;
};

/// Anything burning is kept this much further off
constexpr float k_FireMargin = 5.0f;
/// No circle is smaller than this
constexpr float k_MinRadius = 0.05f;

/// The creature's height is this many times its size
constexpr float k_HeightPerSize = 15.0f;

/// How tall a thing stands: its model's height, scaled
[[nodiscard]] float ModelHeight(glm::vec3 boxHalfSize, float scale);
/// How wide a thing is across the ground from its middle: the longer of its model's half width and half length, scaled.
/// Fields are 5 units wide and teleport stones 6 whatever their models.
[[nodiscard]] float ModelWidth(glm::vec3 boxHalfSize, float scale);
constexpr float k_FieldWidth = 5.0f;
constexpr float k_TeleportStoneWidth = 6.0f;

/// Whether the creature walks round the thing
[[nodiscard]] bool MustAvoid(const Thing& thing, const Walker& walker);

/// How much of the creature's radius is taken off what it keeps clear of a thing of a height: four tenths for things
/// taller than four fifths of the creature, more for lower ones, up to seven tenths for the lowest
[[nodiscard]] float RadiusShare(float height, float walkerHeight);

/// How far from a thing's place the creature is planned to stop when it walks up to it: its width less the share of the
/// creature's radius for its height; a tree's trunk; a third of the temple's width
[[nodiscard]] float ReachRadius(const Thing& thing, const Walker& walker);

/// The circles the creature plans its route round, for a thing it must avoid
[[nodiscard]] std::vector<creature_route::Circle> CirclesOf(const Thing& thing, const Walker& walker);

/// The temple's circles, the same whatever its model and size: a wide one over its middle; seven spokes of small circles
/// round it, the first two longer with their ends bent towards each other, the other five longer the better its player
/// (two circles for the most evil, three for the middling, four for the good); and, beyond each spoke after the first, a
/// wide circle over the worship site in that place when there is one
[[nodiscard]] std::vector<creature_route::Circle> TempleCircles(glm::vec2 centre, float yAngle, float playerAlignment,
                                                                const std::array<bool, 6>& worshipSites);

/// How far round its middle a creature of size one reaches: the furthest any bone of its body stands from its middle,
/// across the ground, at the start of its stand
[[nodiscard]] float BodyRadius(std::span<const glm::mat4> boneMatrices);

/// The way a thing's model's x axis points across the land, from its rotation
[[nodiscard]] float YAngleOf(const glm::mat3& rotation);

} // namespace openblack::creature_obstacles
