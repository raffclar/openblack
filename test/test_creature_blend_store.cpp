/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "3D/SkeletalAnimation.h"
#include "Common/WorkerPool.h"
#include "Creature/CreatureBlendStore.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRig.h"

using namespace openblack;
using creature::CreatureRig;
using skeletal_animation::Animation;

namespace
{
constexpr size_t k_Walk = 3;
constexpr size_t k_Wave = 4;
constexpr size_t k_Missing = 9;

/// An animation of three bones that turn and move by a different amount in each frame
Animation Moving(float seed, size_t frames = 6)
{
	Animation animation {
	    .duration = 1000, .looping = true, .rotatedJoints = {0, 1, 2}, .translatedJoints = {0, 2}, .frames = {}};
	for (size_t f = 0; f < frames; ++f)
	{
		const auto t = seed + (0.37f * static_cast<float>(f));
		animation.frames.push_back({
		    .eulerAngles = {glm::vec3(0.1f * t, 0.7f * t, -0.2f * t), glm::vec3(-0.3f * t, 0.05f, 0.4f * t),
		                    glm::vec3(0.9f, -0.6f * t, 0.15f * t)},
		    .translations = {glm::vec3(t, 0.5f, -t), glm::vec3(0.25f * t, 2.0f, 0.0f)},
		});
	}
	return animation;
}

/// A species: the base has a stand, a walk and a wave. The evil mesh has only its own stand, so its walk and wave are the
/// base's adjusted to that stand. The thin mesh has its own walk.
CreatureRig Species()
{
	CreatureRig rig;
	rig.hasMesh.fill(true);
	auto& base = rig.animations.at(static_cast<size_t>(CreatureRig::Mesh::Base));
	base.resize(k_Wave + 1);
	base[0] = Moving(0.2f, 1);
	base[k_Walk] = Moving(1.1f);
	base[k_Wave] = Moving(-0.8f, 9);
	auto& evil = rig.animations.at(static_cast<size_t>(CreatureRig::Mesh::Evil));
	evil.resize(1);
	evil[0] = Moving(0.9f, 1);
	auto& thin = rig.animations.at(static_cast<size_t>(CreatureRig::Mesh::Thin));
	thin.resize(k_Walk + 1);
	thin[0] = Moving(-0.4f, 1);
	thin[k_Walk] = Moving(2.3f);
	return rig;
}

creature_morph::Morph Body(float evilGood, float thinFat)
{
	creature_morph::Morph morph {};
	morph.evilGood = evilGood;
	morph.thinFat = thinFat;
	return morph;
}

/// True when the two animations have the same bits in each angle and each move of each frame
bool SameBits(const Animation& a, const Animation& b)
{
	if (a.frames.size() != b.frames.size() || a.rotatedJoints != b.rotatedJoints || a.translatedJoints != b.translatedJoints)
	{
		return false;
	}
	for (size_t f = 0; f < a.frames.size(); ++f)
	{
		const auto& x = a.frames[f];
		const auto& y = b.frames[f];
		if (x.eulerAngles.size() != y.eulerAngles.size() || x.translations.size() != y.translations.size() ||
		    std::memcmp(x.eulerAngles.data(), y.eulerAngles.data(), x.eulerAngles.size() * sizeof(glm::vec3)) != 0 ||
		    std::memcmp(x.translations.data(), y.translations.data(), x.translations.size() * sizeof(glm::vec3)) != 0)
		{
			return false;
		}
	}
	return true;
}
} // namespace

TEST(CreatureBlendStore, ABlendThatTheThreadsMadeIsTheBlendThatTheTurnMakes)
{
	const auto rig = Species();
	const std::array bodies {Body(-0.4f, -0.7f), Body(0.6f, -0.25f), Body(-1.0f, 0.3f)};
	WorkerPool workers(3);
	creature_blends::Stores made;
	creature_blends::Stores prepared;
	std::vector<creature_blends::Wanted> wanted;
	for (const auto& body : bodies)
	{
		for (const auto index : {size_t {0}, k_Walk, k_Wave})
		{
			wanted.push_back({.rig = &rig, .morph = body, .index = index});
		}
	}
	creature_blends::Prepare(prepared, wanted, workers, 1);
	ASSERT_EQ(prepared.prepared.size(), wanted.size());
	for (const auto& [_, morph, index] : wanted)
	{
		const auto now = creature_blends::Blend(made, rig, morph, index);
		const auto early = creature_blends::KeptBlend(prepared, rig, morph, index);
		ASSERT_TRUE(now.has_value());
		ASSERT_TRUE(early.has_value());
		EXPECT_TRUE(SameBits(*now, *early)) << "animation " << index;
	}
	// The threads made the same adjusted animations as the turn
	ASSERT_EQ(made.adjusted.size(), prepared.adjusted.size());
	for (const auto& [key, adjusted] : made.adjusted)
	{
		const auto other = prepared.adjusted.find(key);
		ASSERT_NE(other, prepared.adjusted.end());
		ASSERT_TRUE(adjusted.has_value() && other->second.has_value());
		EXPECT_TRUE(SameBits(adjusted->animation, other->second->animation));
	}
	EXPECT_EQ(prepared.prepared.size(), wanted.size());
}

// A second Prepare uses the adjusted animations that the first one kept, and its blends have the bits of the turn
TEST(CreatureBlendStore, ASecondPrepareWithKeptAdjustmentsMakesTheBlendsOfTheTurn)
{
	const auto rig = Species();
	WorkerPool workers(3);
	creature_blends::Stores prepared;
	const auto wantedOf = [&rig](const std::vector<creature_morph::Morph>& bodies) {
		std::vector<creature_blends::Wanted> wanted;
		for (const auto& body : bodies)
		{
			for (const auto index : {size_t {0}, k_Walk, k_Wave})
			{
				wanted.push_back({.rig = &rig, .morph = body, .index = index});
			}
		}
		return wanted;
	};
	creature_blends::Prepare(prepared, wantedOf({Body(-0.4f, -0.7f), Body(0.6f, 0.25f)}), workers, 1);
	const auto adjustedBefore = prepared.adjusted.size();
	const auto second = wantedOf({Body(-0.9f, -0.1f), Body(0.3f, 0.8f), Body(-0.2f, 0.5f)});
	creature_blends::Prepare(prepared, second, workers, 2);
	// The second bodies use the same variant meshes, so the stores keep the same adjusted animations
	EXPECT_EQ(prepared.adjusted.size(), adjustedBefore);
	creature_blends::Stores made;
	for (const auto& [_, morph, index] : second)
	{
		const auto now = creature_blends::Blend(made, rig, morph, index);
		const auto early = creature_blends::KeptBlend(prepared, rig, morph, index);
		ASSERT_TRUE(now.has_value());
		ASSERT_TRUE(early.has_value());
		EXPECT_TRUE(SameBits(*now, *early)) << "animation " << index;
	}
}

TEST(CreatureBlendStore, TheStoresGiveAKeptBlendToEachBodyOfItsShapeOnly)
{
	const auto rig = Species();
	WorkerPool workers(2);
	creature_blends::Stores stores;
	const std::array<creature_blends::Wanted, 2> wanted {
	    {{.rig = &rig, .morph = Body(0.5f, 0.5f), .index = k_Walk}, {.rig = &rig, .morph = Body(0.5f, 0.5f), .index = k_Wave}}};
	creature_blends::Prepare(stores, wanted, workers, 1);
	EXPECT_FALSE(creature_blends::KeptBlend(stores, rig, Body(0.5f, 0.51f), k_Walk).has_value());
	const auto first = creature_blends::KeptBlend(stores, rig, Body(0.5f, 0.5f), k_Walk);
	const auto second = creature_blends::KeptBlend(stores, rig, Body(0.5f, 0.5f), k_Walk);
	ASSERT_TRUE(first.has_value());
	ASSERT_TRUE(second.has_value());
	EXPECT_TRUE(SameBits(*first, *second));
	EXPECT_TRUE(creature_blends::KeptBlend(stores, rig, Body(0.5f, 0.5f), k_Wave).has_value());
}

TEST(CreatureBlendStore, ABlendThatATurnMadeIsTheBlendOfTheNextBodyOfTheSameShape)
{
	const auto rig = Species();
	creature_blends::Stores made;
	creature_blends::Stores kept;
	const auto body = Body(-0.35f, 0.65f);
	for (const auto index : {size_t {0}, k_Walk, k_Wave})
	{
		const auto blend = creature_blends::Blend(kept, rig, body, index);
		ASSERT_TRUE(blend.has_value());
		creature_blends::Keep(kept, rig, body, index, *blend);
		const auto copy = creature_blends::KeptBlend(kept, rig, body, index);
		const auto fresh = creature_blends::Blend(made, rig, body, index);
		ASSERT_TRUE(copy.has_value());
		ASSERT_TRUE(fresh.has_value());
		EXPECT_TRUE(SameBits(*copy, *fresh)) << "animation " << index;
	}
	EXPECT_FALSE(creature_blends::KeptBlend(kept, rig, Body(-0.35f, 0.66f), k_Walk).has_value());
}

TEST(CreatureBlendStore, ABlendThatATurnTookStaysForTwoMoreShapes)
{
	const auto rig = Species();
	creature_blends::Stores stores;
	stores.shaping = 5;
	const auto body = Body(0.1f, 0.4f);
	const auto blend = creature_blends::Blend(stores, rig, body, k_Walk);
	ASSERT_TRUE(blend.has_value());
	creature_blends::Keep(stores, rig, body, k_Walk, *blend);
	stores.shaping = 7;
	ASSERT_TRUE(creature_blends::KeptBlend(stores, rig, body, k_Walk).has_value());
	creature_blends::ForgetOld(stores, 9);
	EXPECT_EQ(stores.prepared.size(), 1u);
	creature_blends::ForgetOld(stores, 10);
	EXPECT_TRUE(stores.prepared.empty());
}

TEST(CreatureBlendStore, TheThreadsMakeEachBlendOnceAndNoneThatTheSpeciesDoesNotHave)
{
	const auto rig = Species();
	WorkerPool workers(2);
	creature_blends::Stores stores;
	const std::array<creature_blends::Wanted, 3> wanted {{{.rig = &rig, .morph = Body(-0.5f, 0.5f), .index = k_Walk},
	                                                      {.rig = &rig, .morph = Body(-0.5f, 0.5f), .index = k_Walk},
	                                                      {.rig = &rig, .morph = Body(-0.5f, 0.5f), .index = k_Missing}}};
	creature_blends::Prepare(stores, wanted, workers, 1);
	EXPECT_EQ(stores.prepared.size(), 1u);
	EXPECT_FALSE(creature_blends::Blend(stores, rig, Body(-0.5f, 0.5f), k_Missing).has_value());
}

TEST(CreatureBlendStore, ABlendThatNoTurnTookGoesAfterTwoShapes)
{
	const auto rig = Species();
	WorkerPool workers(2);
	creature_blends::Stores stores;
	const std::array<creature_blends::Wanted, 2> wanted {
	    {{.rig = &rig, .morph = Body(0.2f, -0.2f), .index = 0}, {.rig = &rig, .morph = Body(0.2f, -0.2f), .index = k_Walk}}};
	creature_blends::Prepare(stores, wanted, workers, 5);
	creature_blends::ForgetOld(stores, 7);
	EXPECT_EQ(stores.prepared.size(), 2u);
	creature_blends::ForgetOld(stores, 8);
	EXPECT_TRUE(stores.prepared.empty());
}

TEST(CreatureBlendStore, ABlendThatTheBackgroundMadeIsTheBlendOfAPrepareOnTheCallingThread)
{
	const auto rig = Species();
	const std::array bodies {Body(-0.4f, -0.7f), Body(0.6f, -0.25f), Body(-1.0f, 0.3f)};
	std::vector<creature_blends::Wanted> wanted;
	for (const auto& body : bodies)
	{
		for (const auto index : {size_t {0}, k_Walk, k_Wave})
		{
			wanted.push_back({.rig = &rig, .morph = body, .index = index});
		}
	}
	WorkerPool workers(3);
	creature_blends::Stores here;
	creature_blends::Prepare(here, wanted, workers, 1);
	creature_blends::Stores background;
	creature_blends::BackgroundPrepare job;
	EXPECT_FALSE(job.Running());
	job.Start(background, wanted, workers, 1);
	EXPECT_TRUE(job.Running());
	job.Finish();
	EXPECT_FALSE(job.Running());
	ASSERT_EQ(background.prepared.size(), here.prepared.size());
	ASSERT_EQ(background.adjusted.size(), here.adjusted.size());
	for (const auto& [key, blend] : here.prepared)
	{
		const auto other = background.prepared.find(key);
		ASSERT_NE(other, background.prepared.end());
		EXPECT_EQ(other->second.shaping, blend.shaping);
		EXPECT_TRUE(SameBits(other->second.animation, blend.animation));
	}
	// A second start finishes the first job, and a blend that the stores keep is not made again
	job.Start(background, wanted, workers, 2);
	job.Start(background, wanted, workers, 3);
	job.Finish();
	EXPECT_EQ(background.prepared.size(), here.prepared.size());
	EXPECT_TRUE(std::ranges::all_of(background.prepared, [](const auto& kept) { return kept.second.shaping == 1; }));
}

TEST(CreatureBlendStore, TheBackgroundWaitsForItsJobWhenItGoes)
{
	const auto rig = Species();
	const std::vector<creature_blends::Wanted> wanted {{.rig = &rig, .morph = Body(0.2f, 0.4f), .index = k_Walk},
	                                                   {.rig = &rig, .morph = Body(0.2f, 0.4f), .index = k_Wave}};
	WorkerPool workers(2);
	creature_blends::Stores stores;
	{
		creature_blends::BackgroundPrepare job;
		job.Start(stores, wanted, workers, 1);
	}
	EXPECT_EQ(stores.prepared.size(), wanted.size());
}
