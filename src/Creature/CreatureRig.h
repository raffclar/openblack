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
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/SkeletalAnimation.h"
#include "Common/X87Matrix.h"
#include "Creature/CreatureAudio.h"
#include "Creature/CreatureEyes.h"
#include "Creature/CreatureHair.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureTattoo.h"

namespace openblack::creature
{
/// What moves a species' body, from its .cbn file in Data/CTR: the animations of its base mesh and of its evil, good,
/// thin and fat meshes, and where its eyes sit on it
struct CreatureRig
{
	/// The meshes a body is blended from, in the order the file names them
	enum class Mesh : uint8_t
	{
		Base,
		Evil,
		Good,
		Thin,
		Fat,
		Weak,
		Strong,
	};
	static constexpr size_t k_MeshCount = 7;
	/// The meshes that may have animations of their own: the base, evil, good, thin and fat
	static constexpr size_t k_AnimatedMeshCount = 5;

	/// The base mesh's name, which says the species
	std::string baseMeshName;
	/// Each mesh's file name in Data/CreatureMesh, without its extension, empty for the meshes the species lacks
	std::array<std::string, k_MeshCount> meshNames;
	/// Whether the species has each mesh other than the base
	std::array<bool, k_MeshCount> hasMesh {};
	/// Each animated mesh's animations, in the order of the creature spec file, absent where it has none of its own
	std::array<std::vector<std::optional<skeletal_animation::Animation>>, k_AnimatedMeshCount> animations;
	/// Each animation's line in the creature spec file, in the order of the file: its name without the type letter, the
	/// type letter and the name of its set. The developer tools show these names.
	struct SpecLine
	{
		std::string name;
		char type;
		std::string set;
	};
	std::vector<SpecLine> specLines;

	/// A point on the body the eyes are placed by
	struct EyePoint
	{
		bool enabled;
		/// How far into the body the point is sunk
		float depth;
		/// The triangle's vertices in each mesh, in the space of the bone that moves each, and where between them
		std::array<std::array<glm::vec3, 3>, k_MeshCount> vertices;
		std::array<uint32_t, 3> bones;
		float u;
		float v;
		/// The colour of the base mesh's skin there, 0 to 1
		glm::vec3 skinColour;
	};
	struct Eyes
	{
		/// The species' eye size
		float scale;
		/// The left and right eyes, then the points the left and right eyelids are turned towards
		std::array<EyePoint, 4> points;
		creature_eyes::LidAngles lidAngles;
	};
	std::optional<Eyes> eyes;

	/// A strand of hair, rooted on a triangle of the body
	struct HairStrand
	{
		/// Whether the strand grows out turned from the surface by its angles, rather than straight out of it
		bool turned;
		/// The x, y and z angles it is turned by when neutral, evil and good
		std::array<glm::vec3, 3> angles;
		/// The triangle's vertices in each mesh, in the space of the bone that moves each, and where between them
		std::array<std::array<glm::vec3, 3>, k_MeshCount> vertices;
		std::array<uint32_t, 3> bones;
		float u;
		float v;
	};
	/// A tuft of strands that look and move alike
	struct HairGroup
	{
		/// The points each strand is made of
		uint32_t segmentCount;
		/// Drawn with the hair texture rather than in the strands' colour alone
		bool textured;
		creature_hair::Variants looks;
		std::vector<HairStrand> strands;
	};
	/// The species' hair, none for most
	std::vector<HairGroup> hairGroups;

	/// Where the species' tattoos go, for species that have places for them
	std::optional<creature_tattoo::Sites> tattooSites;

	/// The bones the species acts with, counted in its meshes, and the moments its object animations take hold or let
	/// go, in milliseconds from their starts. Each left side bone is the mirror of the right.
	struct ActionPoints
	{
		uint32_t rightHand;
		uint32_t rightFoot;
		uint32_t rightArmpit;
		uint32_t belly;
		uint32_t head;
		uint32_t groin;
		float pickUpMs;
		float destroyMs;
		float discardMs;
		float eatMs;
		float throwMs;
		float putDownMs;
		/// When the catching hand closes on what it catches
		float catchMs;
		/// When the reaching hand takes what the player's hand gives it
		float takeFromHandMs;
		/// When the foot meets what it kicks
		float kickMs;
		/// When the pointing arm gets to its point and holds there
		float pointMs {0.0f};
	};
	std::optional<ActionPoints> actionPoints;
	/// The point on the body that kisses: a triangle of the meshes, in the space of the bone that moves each vertex, and
	/// where on the triangle. None when the file has no such point. Then the creature kisses with its head bone.
	struct KissPoint
	{
		std::array<std::array<glm::vec3, 3>, k_MeshCount> vertices;
		std::array<uint32_t, 3> bones;
		float u;
		float v;
	};
	std::optional<KissPoint> kissPoint;
	/// The point on the body at the lips that sick comes out of, as the point that kisses. None when the file has no such
	/// point. Then the sick comes out of the head bone.
	std::optional<KissPoint> pukePoint;
	/// The moment of the high and the low kiss animations when the lips meet, in milliseconds from their starts
	std::optional<float> kissMs;
	/// The type of wound each of its twelve fight blows leaves, in the order of the blow animations, when its file gives
	/// them
	std::optional<std::array<int32_t, 12>> blowWoundTypes;
	/// When each of its twelve fight blows lands, in milliseconds from the start of its animation, and the bone that
	/// strikes it, counted in its meshes, when its file gives them
	struct BlowStrike
	{
		int32_t landMs;
		int32_t bone;
	};
	std::optional<std::array<BlowStrike, 12>> blowStrikes;

	/// The sounds placed on moments of each animation of the base mesh, in the order of the creature spec file
	std::vector<std::vector<creature_audio::SoundEvent>> soundEvents;
	/// What the species is to its voice bank, the object key its voice is looked up by
	int32_t soundObject {0};
	/// The voice bank the file names, without its extension, empty when it names none
	std::string soundBankName;
	/// The last numbers of the species' file and its creature block's version, which give each bone's mirror bone
	/// (MirrorBones)
	std::vector<int32_t> fileTail;
	uint32_t creatureVersion {0};
	/// The bone of the right eye, when the file names one; the left is its mirror
	std::optional<uint32_t> rightEye;
	/// The bone at the creature's collar that a leash is tied to, when the file names one
	std::optional<uint32_t> leashBone;

	/// Each bone's mirror bone, the bone on the other side of the body, for the species' mesh of so many bones: the
	/// table that ends its file. None when the file has none that fits.
	[[nodiscard]] std::optional<std::vector<uint32_t>> MirrorBones(size_t boneCount) const;
	/// An animated mesh's animation, falling back on the base's when the mesh has none of its own
	[[nodiscard]] const skeletal_animation::Animation* GetAnimation(Mesh mesh, size_t index) const;
};

/// The matrix the renderer places a creature's mesh with: scaled, then turned and moved
[[nodiscard]] glm::mat4 PlacementMatrix(const glm::vec3& position, const glm::mat3& rotation, const glm::vec3& scale);

/// The order in which a vertex's three products with the axes of its bone are added, for each vertex of a triangle and
/// each world axis: three axis indices (0 x, 1 y, 2 z) each. The game adds them in different orders in different places,
/// and the order changes the last bit of the sum.
using PlacingOrder = std::array<x87::AxisOrders, 3>;

/// The order for a point on the skin: the eyes, the hair that is not turned
inline constexpr PlacingOrder k_SkinPointOrder {{
    {{{1, 2, 0}, {1, 2, 0}, {1, 2, 0}}},
    {{{2, 1, 0}, {0, 2, 1}, {0, 2, 1}}},
    {{{2, 1, 0}, {2, 1, 0}, {2, 1, 0}}},
}};

/// The order for the root of a hair strand that is turned in the frame of its bones
inline constexpr PlacingOrder k_TurnedHairOrder {{
    {{{2, 1, 0}, {2, 1, 0}, {2, 1, 0}}},
    {{{0, 2, 1}, {0, 2, 1}, {0, 2, 1}}},
    {{{0, 2, 1}, {0, 2, 1}, {0, 2, 1}}},
}};

/// A triangle of the body as it is drawn this frame, in the world: each vertex blended between the meshes as the body
/// is, then placed by its bone's posed matrix and the creature's placement, with the sums in the given order. vertices
/// are the triangle's in each mesh, in the space of the bone that moves each.
[[nodiscard]] std::array<glm::vec3, 3>
PosedTriangle(const std::array<std::array<glm::vec3, 3>, CreatureRig::k_MeshCount>& vertices,
              const std::array<uint32_t, 3>& bones, const creature_morph::Morph& morph, std::span<const glm::mat4> boneMatrices,
              const glm::mat4& placement, const PlacingOrder& order);

/// The world matrix of a bone as posed this frame, the creature's placement included; the placement alone for a bone
/// the pose lacks
[[nodiscard]] glm::mat4 PosedBone(uint32_t bone, std::span<const glm::mat4> boneMatrices, const glm::mat4& placement);

/// The world matrices of the bones of a pose at a placement, as `PosedBone` gives them. Each bone's matrix is found the
/// first time it is wanted and kept, so the triangles of one frame that share a bone find it one time. The pose and the
/// placement must not change while the object is used.
class PosedBones
{
public:
	PosedBones(std::span<const glm::mat4> boneMatrices, const glm::mat4& placement);

	/// The world matrix of a bone, the same as `PosedBone(bone, boneMatrices, placement)`
	[[nodiscard]] glm::mat4 At(uint32_t bone);

	[[nodiscard]] std::span<const glm::mat4> BoneMatrices() const { return _boneMatrices; }
	[[nodiscard]] const glm::mat4& Placement() const { return _placement; }

private:
	/// The number of bones whose matrices are kept; a bone after these is found again each time
	static constexpr size_t k_KeptBones = 128;

	std::span<const glm::mat4> _boneMatrices;
	glm::mat4 _placement;
	std::array<glm::mat4, k_KeptBones> _world;
	std::array<bool, k_KeptBones> _known {};
};

/// The same triangle as `PosedTriangle`, with the bones' world matrices taken from the posed bones
[[nodiscard]] std::array<glm::vec3, 3>
PosedTriangle(const std::array<std::array<glm::vec3, 3>, CreatureRig::k_MeshCount>& vertices,
              const std::array<uint32_t, 3>& bones, const creature_morph::Morph& morph, PosedBones& posed,
              const PlacingOrder& order);

/// A pose that game rules read, and the placement that puts it in the world
struct RulePose
{
	std::span<const glm::mat4> bones;
	glm::mat4 placement;
};

/// The pose that game rules read: hit shapes, contacts and body points. It is the pose of the newest game turn, which
/// is in the world already, when that pose has one matrix for each drawn bone. Else it is the pose drawn this frame,
/// with the creature's placement.
[[nodiscard]] RulePose TurnPose(std::span<const glm::mat4> newerTurnPose, std::span<const glm::mat4> boneMatrices,
                                const glm::mat4& placement);

/// Where each bone was in the world at the end of the last game turn, as the game's effects on a creature read them.
/// Before the first turn ends (no bones kept, or not one for each bone of the pose), the bones of this frame are the
/// only pose there is.
[[nodiscard]] std::vector<glm::vec3> TurnPoseBones(std::span<const glm::vec3> lastTurnBones,
                                                   std::span<const glm::mat4> boneMatrices, const glm::mat4& placement);
} // namespace openblack::creature
