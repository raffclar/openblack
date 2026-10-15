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
#include <string>
#include <string_view>
#include <vector>

/// EXPERIMENT: a reader of binary glTF (.glb) files that keeps what a skinned, animated model needs: its nodes, the
/// first skinned mesh, the skin and the animations. It is not part of the original game's formats.
namespace openblack::glb
{

/// A node of the scene, relative to its parent
struct Node
{
	std::string name;
	/// The parent node, or none for a node at the top of the scene
	std::optional<uint32_t> parent;
	std::array<float, 3> translation {0.0f, 0.0f, 0.0f};
	/// A unit quaternion: x, y, z, then w
	std::array<float, 4> rotation {0.0f, 0.0f, 0.0f, 1.0f};
	std::array<float, 3> scale {1.0f, 1.0f, 1.0f};
};

/// The joints the mesh is skinned to, and each joint's inverse bind matrix (column-major, 16 floats)
struct Skin
{
	std::vector<uint32_t> joints;
	std::vector<std::array<float, 16>> inverseBindMatrices;
};

/// One primitive of triangles. Each vertex has up to 4 joints, which count in the skin's joints.
struct Primitive
{
	std::vector<std::array<float, 3>> positions;
	std::vector<std::array<float, 3>> normals;
	/// Empty when the mesh has no texture coordinates
	std::vector<std::array<float, 2>> texCoords;
	std::vector<std::array<uint16_t, 4>> joints;
	std::vector<std::array<float, 4>> weights;
	std::vector<uint32_t> indices;
};

enum class Interpolation : uint8_t
{
	Linear,
	Step,
	CubicSpline,
};

enum class Target : uint8_t
{
	Translation,
	Rotation,
	Scale,
};

/// The keys of one property of one node. Translations and scales use the first 3 values of each key. A cubic spline
/// keeps only the value of each key, not its tangents.
struct Channel
{
	uint32_t node;
	Target target;
	Interpolation interpolation;
	std::vector<float> times;
	std::vector<std::array<float, 4>> values;
};

struct Animation
{
	std::string name;
	std::vector<Channel> channels;
	/// The time of the last key, in seconds
	[[nodiscard]] float Duration() const;
};

/// The parts of a .glb file that a skinned, animated model needs
struct Model
{
	std::vector<Node> nodes;
	/// The primitives of the first mesh that has a skin
	std::vector<Primitive> primitives;
	std::optional<Skin> skin;
	std::vector<Animation> animations;
};

enum class GlbResult : uint8_t
{
	Success,
	ErrParse,
	ErrNoSkinnedMesh,
	ErrBadAccessor,
};

[[nodiscard]] std::string_view ResultToStr(GlbResult result);

/// Reads a .glb file from its bytes
[[nodiscard]] GlbResult Read(std::span<const uint8_t> bytes, Model& model);

} // namespace openblack::glb
