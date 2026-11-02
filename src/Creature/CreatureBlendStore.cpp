/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureBlendStore.h"

#include <cmath>

#include <algorithm>
#include <bit>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Common/WorkerPool.h"
#include "Creature/CreatureAnimation.h"

using namespace openblack;
using namespace openblack::creature_blends;
using openblack::creature::CreatureRig;
using openblack::skeletal_animation::Animation;
using openblack::skeletal_animation::Matrix;

namespace
{

CreatureRig::Mesh EvilGoodMesh(float value)
{
	return value < 0.0f ? CreatureRig::Mesh::Evil : CreatureRig::Mesh::Good;
}

CreatureRig::Mesh ThinFatMesh(float value)
{
	return value < 0.0f ? CreatureRig::Mesh::Thin : CreatureRig::Mesh::Fat;
}

/// A variant's animation, the base's adjusted to the variant's stand where the variant has none of its own, and the
/// variant's stand
struct Variant
{
	const Animation* animation {nullptr};
	const Animation* stand {nullptr};
};

/// The animations that the adjusted animation of a variant comes from, when the variant has no animation of its own and
/// the stores have no adjusted animation from these animations yet
struct AdjustmentSources
{
	AdjustedKey key;
	const Animation* base;
	const Animation* baseStand;
	const Animation* stand;
};

std::optional<AdjustmentSources> MissingAdjustment(const Stores& stores, const CreatureRig& rig, CreatureRig::Mesh mesh,
                                                   size_t index)
{
	const auto slot = static_cast<size_t>(mesh);
	if (!rig.hasMesh.at(slot) || slot >= rig.animations.size() || index == creature_animation::k_StandAnimation)
	{
		return std::nullopt;
	}
	const auto& own = rig.animations.at(slot);
	if (index < own.size() && own[index].has_value())
	{
		return std::nullopt;
	}
	const AdjustmentSources sources {
	    .key = {.rig = &rig, .mesh = mesh, .index = index},
	    .base = rig.GetAnimation(CreatureRig::Mesh::Base, index),
	    .baseStand = rig.GetAnimation(CreatureRig::Mesh::Base, creature_animation::k_StandAnimation),
	    .stand = rig.GetAnimation(mesh, creature_animation::k_StandAnimation),
	};
	if (sources.base == nullptr || sources.baseStand == nullptr || sources.stand == nullptr)
	{
		return std::nullopt;
	}
	const auto kept = stores.adjusted.find(sources.key);
	if (kept != stores.adjusted.end() && kept->second.has_value() && kept->second->base == sources.base &&
	    kept->second->baseStand == sources.baseStand && kept->second->stand == sources.stand)
	{
		return std::nullopt;
	}
	return sources;
}

AdjustedAnimation AdjustmentOf(const AdjustmentSources& sources, Animation animation)
{
	return {.base = sources.base, .baseStand = sources.baseStand, .stand = sources.stand, .animation = std::move(animation)};
}

/// A variant's animation from the stores, which have the adjusted animation of the variant already. It changes nothing,
/// so many threads can find variants at the same time.
Variant FindVariant(const Stores& stores, const CreatureRig& rig, CreatureRig::Mesh mesh, size_t index)
{
	Variant variant;
	const auto slot = static_cast<size_t>(mesh);
	if (!rig.hasMesh.at(slot) || slot >= rig.animations.size())
	{
		return variant;
	}
	variant.stand = rig.GetAnimation(mesh, creature_animation::k_StandAnimation);
	const auto& own = rig.animations.at(slot);
	if (index < own.size() && own[index].has_value())
	{
		variant.animation = &*own[index];
	}
	else if (index == creature_animation::k_StandAnimation)
	{
		variant.animation = variant.stand;
	}
	else if (const auto kept = stores.adjusted.find(AdjustedKey {.rig = &rig, .mesh = mesh, .index = index});
	         kept != stores.adjusted.end() && kept->second.has_value())
	{
		variant.animation = &kept->second->animation;
	}
	return variant;
}

Variant VariantOf(Stores& stores, const CreatureRig& rig, CreatureRig::Mesh mesh, size_t index)
{
	// The adjustment uses only the species' animations, so every creature of the species has the same one
	if (const auto missing = MissingAdjustment(stores, rig, mesh, index))
	{
		stores.adjusted.insert_or_assign(
		    missing->key,
		    AdjustmentOf(*missing, creature_animation::AdjustFromStand(*missing->base, *missing->baseStand, *missing->stand)));
	}
	return FindVariant(stores, rig, mesh, index);
}

/// The base animation blended toward the two variants as the body is drawn
Animation BlendVariants(const Animation& base, const creature_morph::Morph& morph, const Variant& evilGood,
                        const Variant& thinFat)
{
	return creature_animation::Blend(base,
	                                 {.animation = evilGood.animation,
	                                  .stand = evilGood.stand,
	                                  .weight = evilGood.animation != nullptr ? std::abs(morph.evilGood) : 0.0f},
	                                 {.animation = thinFat.animation,
	                                  .stand = thinFat.stand,
	                                  .weight = thinFat.animation != nullptr ? std::abs(morph.thinFat) : 0.0f});
}

/// The frame turns of the animations that a Prepare uses, one time for each animation
using TurnsOfAnimations = std::unordered_map<const Animation*, std::vector<Matrix>>;

/// The frame turns of the animation, or none when the Prepare did not make them
std::span<const Matrix> TurnsOf(const TurnsOfAnimations& turns, const Animation* animation)
{
	const auto found = turns.find(animation);
	return found != turns.end() ? std::span<const Matrix>(found->second) : std::span<const Matrix> {};
}

/// The same blend as BlendVariants, with the frame turns that the Prepare made
Animation BlendVariants(const Animation& base, const creature_morph::Morph& morph, const Variant& evilGood,
                        const Variant& thinFat, const TurnsOfAnimations& turns)
{
	const auto weightOf = [](const Variant& variant, float value) {
		return variant.animation != nullptr ? std::abs(value) : 0.0f;
	};
	const creature_animation::BlendTerm towardsEvilGood {.animation = evilGood.animation,
	                                                     .stand = evilGood.stand,
	                                                     .weight = weightOf(evilGood, morph.evilGood),
	                                                     .turns = TurnsOf(turns, evilGood.animation)};
	const creature_animation::BlendTerm towardsThinFat {.animation = thinFat.animation,
	                                                    .stand = thinFat.stand,
	                                                    .weight = weightOf(thinFat, morph.thinFat),
	                                                    .turns = TurnsOf(turns, thinFat.animation)};
	const auto baseTurns = TurnsOf(turns, &base);
	return baseTurns.empty() ? creature_animation::Blend(base, towardsEvilGood, towardsThinFat)
	                         : creature_animation::Blend(base, baseTurns, towardsEvilGood, towardsThinFat);
}

} // namespace

BlendKey creature_blends::KeyOf(const CreatureRig& rig, const creature_morph::Morph& morph, size_t index)
{
	return {.rig = &rig,
	        .evilGood = std::bit_cast<uint32_t>(morph.evilGood),
	        .thinFat = std::bit_cast<uint32_t>(morph.thinFat),
	        .index = index};
}

std::optional<Animation> creature_blends::Blend(Stores& stores, const CreatureRig& rig, const creature_morph::Morph& morph,
                                                size_t index)
{
	const auto* base = rig.GetAnimation(CreatureRig::Mesh::Base, index);
	if (base == nullptr)
	{
		return std::nullopt;
	}
	const auto evilGood = VariantOf(stores, rig, EvilGoodMesh(morph.evilGood), index);
	const auto thinFat = VariantOf(stores, rig, ThinFatMesh(morph.thinFat), index);
	return BlendVariants(*base, morph, evilGood, thinFat);
}

std::optional<Animation> creature_blends::KeptBlend(Stores& stores, const CreatureRig& rig, const creature_morph::Morph& morph,
                                                    size_t index)
{
	const auto kept = stores.prepared.find(KeyOf(rig, morph, index));
	if (kept == stores.prepared.end())
	{
		return std::nullopt;
	}
	kept->second.shaping = stores.shaping;
	return kept->second.animation;
}

void creature_blends::Keep(Stores& stores, const CreatureRig& rig, const creature_morph::Morph& morph, size_t index,
                           const Animation& animation)
{
	stores.prepared.insert_or_assign(KeyOf(rig, morph, index),
	                                 PreparedBlend {.animation = animation, .shaping = stores.shaping});
}

void creature_blends::Prepare(Stores& stores, std::span<const Wanted> wanted, WorkerPool& workers, uint32_t shaping)
{
	// Each blend one time, only those that the stores do not have and that the species has
	std::vector<Wanted> blends;
	std::vector<BlendKey> keys;
	for (const auto& want : wanted)
	{
		const auto key = KeyOf(*want.rig, want.morph, want.index);
		if (want.rig->GetAnimation(CreatureRig::Mesh::Base, want.index) == nullptr || stores.prepared.contains(key) ||
		    std::ranges::find(keys, key) != keys.end())
		{
			continue;
		}
		blends.push_back(want);
		keys.push_back(key);
	}

	// First the adjusted animations of the variants that the blends use, each one time
	std::vector<AdjustmentSources> adjustments;
	for (const auto& [rig, morph, index] : blends)
	{
		for (const auto mesh : {EvilGoodMesh(morph.evilGood), ThinFatMesh(morph.thinFat)})
		{
			const auto missing = MissingAdjustment(stores, *rig, mesh, index);
			if (missing.has_value() && std::ranges::none_of(adjustments, [&missing](const AdjustmentSources& other) {
				    return other.key == missing->key;
			    }))
			{
				adjustments.push_back(*missing);
			}
		}
	}
	// The frame turns of each animation that the blends and the adjustments use, one time for each animation. The
	// creatures of a species share the base animations and the variants, so this does less work than each blend alone.
	// A variant that the adjustments make again gets its turns after its adjustment.
	TurnsOfAnimations turns;
	std::vector<const Animation*> turned;
	const auto turn = [&turns, &turned](const Animation* animation) {
		if (animation != nullptr && turns.try_emplace(animation).second)
		{
			turned.push_back(animation);
		}
	};
	for (const auto& [rig, morph, index] : blends)
	{
		turn(rig->GetAnimation(CreatureRig::Mesh::Base, index));
		for (const auto mesh : {EvilGoodMesh(morph.evilGood), ThinFatMesh(morph.thinFat)})
		{
			const AdjustedKey key {.rig = rig, .mesh = mesh, .index = index};
			if (std::ranges::none_of(adjustments, [&key](const AdjustmentSources& other) { return other.key == key; }))
			{
				turn(FindVariant(stores, *rig, mesh, index).animation);
			}
		}
	}
	for (const auto& sources : adjustments)
	{
		turn(sources.base);
	}
	std::vector<std::vector<Matrix>> madeTurns(turned.size());
	workers.ForEach(turned.size(), [&turned, &madeTurns](size_t index, size_t /*lane*/) {
		madeTurns[index] = creature_animation::FrameTurns(*turned[index]);
	});
	for (size_t i = 0; i < turned.size(); ++i)
	{
		turns.insert_or_assign(turned[i], std::move(madeTurns[i]));
	}

	std::vector<Animation> adjusted(adjustments.size());
	std::vector<std::vector<Matrix>> adjustedTurns(adjustments.size());
	workers.ForEach(adjustments.size(), [&adjustments, &adjusted, &adjustedTurns, &turns](size_t index, size_t /*lane*/) {
		const auto& sources = adjustments[index];
		const auto baseTurns = TurnsOf(turns, sources.base);
		adjusted[index] =
		    baseTurns.empty()
		        ? creature_animation::AdjustFromStand(*sources.base, *sources.baseStand, *sources.stand)
		        : creature_animation::AdjustFromStand(*sources.base, baseTurns, *sources.baseStand, *sources.stand);
		adjustedTurns[index] = creature_animation::FrameTurns(adjusted[index]);
	});
	for (size_t i = 0; i < adjustments.size(); ++i)
	{
		const auto& kept =
		    stores.adjusted.insert_or_assign(adjustments[i].key, AdjustmentOf(adjustments[i], std::move(adjusted[i])))
		        .first->second;
		turns.insert_or_assign(&kept->animation, std::move(adjustedTurns[i]));
	}

	// Then the blends. The threads only read the stores and the turns.
	std::vector<Animation> made(blends.size());
	workers.ForEach(blends.size(), [&stores, &blends, &made, &turns](size_t index, size_t /*lane*/) {
		const auto& [rig, morph, animation] = blends[index];
		made[index] = BlendVariants(*rig->GetAnimation(CreatureRig::Mesh::Base, animation), morph,
		                            FindVariant(stores, *rig, EvilGoodMesh(morph.evilGood), animation),
		                            FindVariant(stores, *rig, ThinFatMesh(morph.thinFat), animation), turns);
	});
	for (size_t i = 0; i < blends.size(); ++i)
	{
		stores.prepared.insert_or_assign(keys[i], PreparedBlend {.animation = std::move(made[i]), .shaping = shaping});
	}
}

creature_blends::BackgroundPrepare::~BackgroundPrepare()
{
	Finish();
}

void creature_blends::BackgroundPrepare::Start(Stores& stores, std::vector<Wanted> wanted, WorkerPool& workers,
                                               uint32_t shaping)
{
	Finish();
	_job = std::async(std::launch::async, [&stores, wanted = std::move(wanted), &workers, shaping]() {
		Prepare(stores, wanted, workers, shaping);
	});
}

void creature_blends::BackgroundPrepare::Finish()
{
	if (_job.valid())
	{
		_job.get();
	}
}

void creature_blends::ForgetOld(Stores& stores, uint32_t shaping)
{
	std::erase_if(stores.prepared, [shaping](const auto& kept) { return kept.second.shaping + 2 < shaping; });
}
