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
#include <cstring>

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
		    AdjustmentOf(*missing, creature_animation::AdjustFromStand(*missing->base, TurnsOf(stores, *missing->base),
		                                                               *missing->baseStand, *missing->stand)));
	}
	return FindVariant(stores, rig, mesh, index);
}

/// The base's stand, which gives the base side of a bone that only a variant moves. Without a stand, the base itself.
const Animation& BaseStandOf(const CreatureRig& rig, const Animation& base)
{
	const auto* stand = rig.GetAnimation(CreatureRig::Mesh::Base, creature_animation::k_StandAnimation);
	return stand != nullptr ? *stand : base;
}

/// True when the stores made the kept turns from the angles that the animation has now. The bits must be the same, thus
/// the stores do not give the turn of minus zero for plus zero.
bool SameAngles(const KeptTurns& kept, const Animation& animation)
{
	const auto joints = animation.rotatedJoints.size();
	if (kept.angles.size() != joints * animation.frames.size())
	{
		return false;
	}
	for (size_t f = 0; f < animation.frames.size() && joints != 0; ++f)
	{
		if (std::memcmp(&kept.angles[f * joints], animation.frames[f].eulerAngles.data(), joints * sizeof(glm::vec3)) != 0)
		{
			return false;
		}
	}
	return true;
}

/// The frame turns of the animation and the angles that they come from
KeptTurns MakeTurns(const Animation& animation)
{
	KeptTurns made {.angles = {}, .turns = creature_animation::FrameTurns(animation)};
	const auto joints = animation.rotatedJoints.size();
	made.angles.reserve(joints * animation.frames.size());
	for (const auto& frame : animation.frames)
	{
		made.angles.insert(made.angles.end(), frame.eulerAngles.begin(),
		                   frame.eulerAngles.begin() + static_cast<std::ptrdiff_t>(joints));
	}
	return made;
}

/// The kept frame turns of the animation, or none when the stores have no turns from its angles now. It changes
/// nothing, so many threads can find turns at the same time.
std::span<const Matrix> FoundTurns(const Stores& stores, const Animation* animation)
{
	if (animation == nullptr)
	{
		return {};
	}
	const auto found = stores.turns.find(animation);
	return found != stores.turns.end() && SameAngles(found->second, *animation) ? std::span<const Matrix>(found->second.turns)
	                                                                            : std::span<const Matrix> {};
}

/// The base animation blended toward the two variants as the body is drawn, with the frame turns of the animations. A
/// blend with no turns makes them itself, with the same bits.
Animation BlendVariants(const Animation& base, std::span<const Matrix> baseTurns, const Animation& baseStand,
                        const creature_morph::Morph& morph, const Variant& evilGood, std::span<const Matrix> evilGoodTurns,
                        const Variant& thinFat, std::span<const Matrix> thinFatTurns)
{
	const auto weightOf = [](const Variant& variant, float value) {
		return variant.animation != nullptr ? std::abs(value) : 0.0f;
	};
	const creature_animation::BlendTerm towardsEvilGood {.animation = evilGood.animation,
	                                                     .stand = evilGood.stand,
	                                                     .weight = weightOf(evilGood, morph.evilGood),
	                                                     .turns = evilGoodTurns};
	const creature_animation::BlendTerm towardsThinFat {.animation = thinFat.animation,
	                                                    .stand = thinFat.stand,
	                                                    .weight = weightOf(thinFat, morph.thinFat),
	                                                    .turns = thinFatTurns};
	return baseTurns.empty() ? creature_animation::Blend(base, baseStand, towardsEvilGood, towardsThinFat)
	                         : creature_animation::Blend(base, baseTurns, baseStand, towardsEvilGood, towardsThinFat);
}

/// The frame turns of a side of a blend, when the blend uses them
std::span<const Matrix> SideTurns(Stores& stores, const Variant& variant)
{
	return variant.animation != nullptr && !variant.animation->frames.empty() ? TurnsOf(stores, *variant.animation)
	                                                                          : std::span<const Matrix> {};
}

} // namespace

BlendKey creature_blends::KeyOf(const CreatureRig& rig, const creature_morph::Morph& morph, size_t index)
{
	return {.rig = &rig,
	        .evilGood = std::bit_cast<uint32_t>(morph.evilGood),
	        .thinFat = std::bit_cast<uint32_t>(morph.thinFat),
	        .index = index};
}

std::span<const Matrix> creature_blends::TurnsOf(Stores& stores, const Animation& animation)
{
	auto& kept = stores.turns[&animation];
	if (!SameAngles(kept, animation))
	{
		kept = MakeTurns(animation);
	}
	return kept.turns;
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
	const auto& baseStand = BaseStandOf(rig, *base);
	// A body with no shape moves as the base, and the blend uses no turns
	if ((evilGood.animation == nullptr || morph.evilGood == 0.0f) && (thinFat.animation == nullptr || morph.thinFat == 0.0f))
	{
		return BlendVariants(*base, {}, baseStand, morph, evilGood, {}, thinFat, {});
	}
	// The spans stay valid, because each call is for another animation or for the same animation with the same angles
	const auto evilGoodTurns = SideTurns(stores, evilGood);
	const auto thinFatTurns = SideTurns(stores, thinFat);
	return BlendVariants(*base, TurnsOf(stores, *base), baseStand, morph, evilGood, evilGoodTurns, thinFat, thinFatTurns);
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
	// The stores keep the turns for the next Prepare and the next turn. A variant that the adjustments make again gets
	// its turns after its adjustment.
	std::vector<const Animation*> turned;
	const auto turn = [&stores, &turned](const Animation* animation) {
		if (animation != nullptr && FoundTurns(stores, animation).empty() &&
		    std::ranges::find(turned, animation) == turned.end())
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
	std::vector<KeptTurns> madeTurns(turned.size());
	workers.ForEach(turned.size(),
	                [&turned, &madeTurns](size_t index, size_t /*lane*/) { madeTurns[index] = MakeTurns(*turned[index]); });
	for (size_t i = 0; i < turned.size(); ++i)
	{
		stores.turns.insert_or_assign(turned[i], std::move(madeTurns[i]));
	}

	std::vector<Animation> adjusted(adjustments.size());
	std::vector<KeptTurns> adjustedTurns(adjustments.size());
	workers.ForEach(adjustments.size(), [&adjustments, &adjusted, &adjustedTurns, &stores](size_t index, size_t /*lane*/) {
		const auto& sources = adjustments[index];
		const auto baseTurns = FoundTurns(stores, sources.base);
		adjusted[index] =
		    baseTurns.empty()
		        ? creature_animation::AdjustFromStand(*sources.base, *sources.baseStand, *sources.stand)
		        : creature_animation::AdjustFromStand(*sources.base, baseTurns, *sources.baseStand, *sources.stand);
		adjustedTurns[index] = MakeTurns(adjusted[index]);
	});
	for (size_t i = 0; i < adjustments.size(); ++i)
	{
		const auto& kept =
		    stores.adjusted.insert_or_assign(adjustments[i].key, AdjustmentOf(adjustments[i], std::move(adjusted[i])))
		        .first->second;
		stores.turns.insert_or_assign(&kept->animation, std::move(adjustedTurns[i]));
	}

	// Then the blends. The threads only read the stores.
	std::vector<Animation> made(blends.size());
	workers.ForEach(blends.size(), [&stores, &blends, &made](size_t index, size_t /*lane*/) {
		const auto& [rig, morph, animation] = blends[index];
		const auto& base = *rig->GetAnimation(CreatureRig::Mesh::Base, animation);
		const auto evilGood = FindVariant(stores, *rig, EvilGoodMesh(morph.evilGood), animation);
		const auto thinFat = FindVariant(stores, *rig, ThinFatMesh(morph.thinFat), animation);
		made[index] = BlendVariants(base, FoundTurns(stores, &base), BaseStandOf(*rig, base), morph, evilGood,
		                            FoundTurns(stores, evilGood.animation), thinFat, FoundTurns(stores, thinFat.animation));
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
