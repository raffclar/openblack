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

#include <optional>
#include <span>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

/// SpiritAnimClip: the clip format of the advisor spirits (Data\HelpSprite\*.hd), a clip header followed by its keys.
/// It is not the .anm format of components/anm.
///
/// Semantics (verified by running the original's pose and additive samplers under an x86 emulator on both .hd files
/// against this code's formulas, max error 4.5e-5):
/// - A clip has `frameCount` evenly spaced keys over `durationMs`. A looping clip spreads them over the whole duration
///   and wraps the last key to the first; a one-shot clip puts its last key at the duration
///   (period = duration * frames / (frames - 1), integer maths). The next key after the last one is always key 0, even
///   for a one-shot clip.
/// - A rotation channel is three Euler angles in radians (x, y, z) turned into a matrix by affine::RotationYXZ called as
///   (y, x, z): with column vectors M = Rz(z) Rx(x) Ry(y). The two keys' matrices are lerped element by element and
///   each row is normalised (affine::NormaliseRows, with the original's table-driven inverse square root).
/// - The angles are not relative to the parent bone: they rotate the bone in model axes from its rest pose. The local
///   matrix is rest world (bone) x key x inverse rest world (parent) in the original's row-vector order.
/// - A position channel is the bone's translation relative to its parent, absolute (lerped).
/// - Layers are additive (ApplyAdditive): the change from a reference key of the same clip to the sampled key is
///   applied to the current local matrices (rotation in the parent's rest axes, translation added). There are no blend
///   weights: the spirit passes the "weight" as the time into the clip (the vowels 7..9 and the looks 18/19).
/// - The mirrored path of both samplers (a bone remap table) is not ported: the spirit never uses it.
namespace openblack::help
{

/// One key: the rotation channels, then the position channels.
struct SpiritAnimKey
{
	std::vector<glm::vec3> rotations; ///< per rotation channel: Euler angles (x, y, z) in radians
	std::vector<glm::vec3> positions; ///< per position channel: translation relative to the parent bone
};

/// A clip. The fields in the order the file stores them.
struct SpiritAnimClip
{
	int32_t durationMs = 0;        ///< length of the clip in milliseconds
	bool looping = false;          ///< the read word & 1
	float speed = 0.0f;            ///< (inferred) distance / durationMs in both files; never read by the spirit
	float distance = 0.0f;         ///< (inferred) |displacement| in both files; never read by the spirit
	glm::vec3 displacement {0.0f}; ///< root move of the whole clip
	uint32_t frameCount = 0;
	uint32_t boneCount = 0;              ///< bones walked by the samplers (97 good, 73 evil)
	std::vector<uint32_t> rotationBones; ///< channel -> bone, ascending
	std::vector<uint32_t> positionBones; ///< channel -> bone, ascending
	std::vector<SpiritAnimKey> frames;
};

/// Reads a clip at `offset`, which it moves past the clip. False (and `error`) if the data ends first.
bool ReadSpiritAnimClip(std::span<const uint8_t> data, size_t& offset, SpiritAnimClip& out, std::string* error);

/// The bytes ReadSpiritAnimClip consumes (0x2C + 4 per channel + 12 per channel and key). The .hd record size that
/// precedes a clip is this + 4.
[[nodiscard]] size_t SpiritAnimClipByteSize(const SpiritAnimClip& clip);

/// The rest skeleton the spirit keeps: `world` = each L3D bone matrix put under its parent, root under the identity,
/// `worldInverse` = affine::Inverse of each. glm matrices built like L3DMesh (columns = the original's rows).
struct RestSkeleton
{
	std::vector<uint32_t> parents; ///< 0xFFFFFFFF for a root
	std::vector<glm::mat4> local;  ///< the L3D bone matrices, relative to the parent
	std::vector<glm::mat4> world;
	std::vector<glm::mat4> worldInverse; ///< affine::Inverse of each world matrix
	float height = 0.0f;                 ///< max - min of the world y with 0
};

/// `local` holds the bone matrices of the L3D (parents before children, like L3DMesh)
[[nodiscard]] RestSkeleton MakeRestSkeleton(const std::vector<uint32_t>& parents, const std::vector<glm::mat4>& local);

/// The keys around `milliseconds` and the fraction between them (the start of both samplers).
struct KeySample
{
	uint32_t key0 = 0;
	uint32_t key1 = 0;
	float fraction = 0.0f;
};
/// `clampBelow`: SetPose clamps a negative key to 0; ApplyAdditive does not (negative times are taken to 0 here, the
/// original would read out of the clip).
[[nodiscard]] KeySample SampleKeys(const SpiritAnimClip& clip, int32_t milliseconds, bool clampBelow);

/// Writes the clip at `milliseconds` into the local matrices. Bones without a channel keep their matrix, or take
/// `fill`'s key indexed by bone (the stand clip's key 0) when `fill` is given.
void SetPose(const SpiritAnimClip& clip, int32_t milliseconds, const RestSkeleton& rest, const SpiritAnimKey* fill,
             std::vector<glm::mat4>& local);

/// Adds the change from `reference` (a key of the same clip) to the clip at `milliseconds` onto the local matrices.
void ApplyAdditive(const SpiritAnimClip& clip, int32_t milliseconds, const SpiritAnimKey& reference, const RestSkeleton& rest,
                   std::vector<glm::mat4>& local);

/// The clip alone at `milliseconds` (truncated): the rest locals, then SetPose without fill. The result is what
/// graphics::ComputePose chains: each bone's matrix relative to its parent, in glm's column-vector convention.
void SampleLocal(const SpiritAnimClip& clip, float milliseconds, const RestSkeleton& rest, std::vector<glm::mat4>& local);

/// Each local put under its parent's result, roots under `root` (world = parent * local here; L * W(parent) in the
/// original's row vectors).
void ComposeWorld(const RestSkeleton& rest, const std::vector<glm::mat4>& local, const glm::mat4& root,
                  std::vector<glm::mat4>& world);

/// What the spirit's apply-anim step (anim, phase, referencePhase, wrap) passes to ApplyAdditive.
struct ApplyAnimArgs
{
	float phase = 0.0f;        ///< after the wrap (fraction) or the clamp to 1, then to >= 0
	int32_t milliseconds = 0;  ///< truncated(duration * phase), at most duration - 1
	uint32_t referenceKey = 0; ///< truncated((frames - 1) * referencePhase), at most duration - 1 (sic)
	glm::vec3 rootMove {0.0f}; ///< phase * displacement: the spirit turns it and adds it to its position
};
[[nodiscard]] ApplyAnimArgs ApplyAnimArguments(const SpiritAnimClip& clip, float phase, float referencePhase, bool wrap);

} // namespace openblack::help
