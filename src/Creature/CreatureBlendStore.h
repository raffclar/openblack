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
#include <span>
#include <unordered_map>

#include "3D/SkeletalAnimation.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRig.h"

namespace openblack
{
class WorkerPool;
}

/// The blends of the species' animations for the drawn bodies of the creatures. A blend comes only from the species, the
/// drawn body and the animation, so a blend that the shared threads made before a turn asks for it is the same blend
/// that the turn makes.
namespace openblack::creature_blends
{

/// A species' animation adjusted to the stand of a variant mesh that has no animation of its own, and the animations
/// that it comes from
struct AdjustedAnimation
{
	const skeletal_animation::Animation* base;
	const skeletal_animation::Animation* baseStand;
	const skeletal_animation::Animation* stand;
	skeletal_animation::Animation animation;
};

/// Which species, variant mesh and animation an adjusted animation is of
struct AdjustedKey
{
	const creature::CreatureRig* rig;
	creature::CreatureRig::Mesh mesh;
	size_t index;
	[[nodiscard]] bool operator==(const AdjustedKey&) const = default;
};

struct AdjustedKeyHash
{
	[[nodiscard]] size_t operator()(const AdjustedKey& key) const noexcept
	{
		return std::hash<const void*> {}(key.rig) ^ (std::hash<size_t> {}(key.index) * 31u) ^
		       (static_cast<size_t>(key.mesh) << 20u);
	}
};

/// Which species, drawn body and animation a blend is of. The body is the bits of its two weights of shape.
struct BlendKey
{
	const creature::CreatureRig* rig;
	uint32_t evilGood;
	uint32_t thinFat;
	size_t index;
	[[nodiscard]] bool operator==(const BlendKey&) const = default;
};

struct BlendKeyHash
{
	[[nodiscard]] size_t operator()(const BlendKey& key) const noexcept
	{
		return std::hash<const void*> {}(key.rig) ^ (std::hash<size_t> {}(key.index) * 31u) ^
		       (std::hash<uint64_t> {}((static_cast<uint64_t>(key.evilGood) << 32u) | key.thinFat) * 131u);
	}
};

/// A blend that the threads made before a turn asked for it, and the count of the shapes of the bodies at that time
struct PreparedBlend
{
	skeletal_animation::Animation animation;
	uint32_t shaping;
};

/// The animations that the creatures of each species share: the animations adjusted to the variants' stands, and the
/// blends that the threads made before a turn asks for them
struct Stores
{
	std::unordered_map<AdjustedKey, std::optional<AdjustedAnimation>, AdjustedKeyHash> adjusted;
	std::unordered_map<BlendKey, PreparedBlend, BlendKeyHash> prepared;
};

/// A blend that the threads make: the species, the drawn body and the animation
struct Wanted
{
	const creature::CreatureRig* rig;
	creature_morph::Morph morph;
	size_t index;
};

/// The key of the blend of a species' animation for a drawn body
[[nodiscard]] BlendKey KeyOf(const creature::CreatureRig& rig, const creature_morph::Morph& morph, size_t index);

/// The species' animation blended as the body is drawn, or nothing when the species does not have the animation. The
/// stores keep the adjusted animations that the blend makes.
[[nodiscard]] std::optional<skeletal_animation::Animation> Blend(Stores& stores, const creature::CreatureRig& rig,
                                                                 const creature_morph::Morph& morph, size_t index);

/// The blend that the threads made for the drawn body, taken out of the stores, or nothing when they did not make it
[[nodiscard]] std::optional<skeletal_animation::Animation> TakePrepared(Stores& stores, const creature::CreatureRig& rig,
                                                                        const creature_morph::Morph& morph, size_t index);

/// The threads make the wanted blends that the stores do not have, and the stores keep them with the count of shapes.
/// First the threads make the adjusted animations that the blends use, one time each. The species must have the base
/// animation of each wanted blend.
void Prepare(Stores& stores, std::span<const Wanted> wanted, WorkerPool& workers, uint32_t shaping);

/// The stores forget each blend that no turn took during two counts of shapes
void ForgetOld(Stores& stores, uint32_t shaping);

} // namespace openblack::creature_blends
