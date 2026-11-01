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
#include <future>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

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

/// A blend that the threads or a turn made, and the count of the shapes of the bodies when a turn last took it
struct PreparedBlend
{
	skeletal_animation::Animation animation;
	uint32_t shaping;
};

/// The animations that the creatures of each species share: the animations adjusted to the variants' stands, and the
/// blends that the threads or a turn made. The bodies of one shape share the kept blends.
struct Stores
{
	std::unordered_map<AdjustedKey, std::optional<AdjustedAnimation>, AdjustedKeyHash> adjusted;
	std::unordered_map<BlendKey, PreparedBlend, BlendKeyHash> prepared;
	/// The count of the shapes of the bodies now
	uint32_t shaping {0};
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

/// A copy of the blend that the stores keep for the drawn body, or nothing when they keep none. The stores keep the blend
/// for two more counts of shapes.
[[nodiscard]] std::optional<skeletal_animation::Animation> KeptBlend(Stores& stores, const creature::CreatureRig& rig,
                                                                     const creature_morph::Morph& morph, size_t index);

/// The stores keep a blend that a turn made, so that the next body of the same shape copies it
void Keep(Stores& stores, const creature::CreatureRig& rig, const creature_morph::Morph& morph, size_t index,
          const skeletal_animation::Animation& animation);

/// The threads make the wanted blends that the stores do not have, and the stores keep them with the count of shapes.
/// First the threads make the adjusted animations that the blends use, one time each. The species must have the base
/// animation of each wanted blend.
void Prepare(Stores& stores, std::span<const Wanted> wanted, WorkerPool& workers, uint32_t shaping);

/// The stores forget each blend that no turn took during two counts of shapes
void ForgetOld(Stores& stores, uint32_t shaping);

/// A Prepare that runs on its own thread while the turn does other steps. Between Start and Finish, only the job uses
/// the stores and the threads: the caller must call Finish before it reads or changes the stores. The job writes the
/// same blends in the same stores as a Prepare on the calling thread.
class BackgroundPrepare
{
public:
	BackgroundPrepare() = default;
	BackgroundPrepare(const BackgroundPrepare&) = delete;
	BackgroundPrepare& operator=(const BackgroundPrepare&) = delete;
	/// Waits for the job, so that it does not use stores or threads that go
	~BackgroundPrepare();

	/// Starts a Prepare of the wanted blends. A job that runs already finishes first.
	void Start(Stores& stores, std::vector<Wanted> wanted, WorkerPool& workers, uint32_t shaping);
	/// Waits until the job wrote its blends in the stores. It does nothing when no job runs.
	void Finish();
	/// True from Start until Finish
	[[nodiscard]] bool Running() const { return _job.valid(); }

private:
	std::future<void> _job;
};

} // namespace openblack::creature_blends
