/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <atomic>
#include <memory>
#include <span>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/SkinPaintThreads.h"

using openblack::WorkerPool;
using openblack::ecs::skin_paint_threads::k_PaintsOnCaller;
using openblack::ecs::skin_paint_threads::PaintAll;

namespace
{

/// A fake skin: a few texels and the count of its paints
struct FakeSkin
{
	std::vector<uint16_t> texels;
	std::atomic<int> paints {0};
	std::thread::id paintedBy;
};

/// A fake paint job: the skin that it writes and the value that it paints from
struct FakeJob
{
	FakeSkin* skin;
	uint16_t seed;
};

/// Paints the texels of the job's skin from the seed alone, as a real paint reads only its job
void PaintFake(const FakeJob& job)
{
	auto& skin = *job.skin;
	skin.texels.resize(256);
	for (size_t i = 0; i < skin.texels.size(); ++i)
	{
		skin.texels[i] = static_cast<uint16_t>((job.seed * 31u) + (i * 7u));
	}
	skin.paintedBy = std::this_thread::get_id();
	++skin.paints;
}

std::vector<FakeJob> JobsFor(std::vector<FakeSkin>& skins)
{
	std::vector<FakeJob> jobs;
	for (size_t i = 0; i < skins.size(); ++i)
	{
		jobs.push_back({.skin = &skins[i], .seed = static_cast<uint16_t>(i * 13u)});
	}
	return jobs;
}

} // namespace

TEST(SkinPaintThreads, NoPaintMakesNoPool)
{
	std::unique_ptr<WorkerPool> pool;
	std::vector<FakeJob> jobs;
	PaintAll(pool, std::span(jobs), PaintFake);
	EXPECT_EQ(pool, nullptr);
}

TEST(SkinPaintThreads, OnePaintStaysOnTheCallingThread)
{
	ASSERT_EQ(k_PaintsOnCaller, 1u);
	std::unique_ptr<WorkerPool> pool;
	std::vector<FakeSkin> skins(1);
	auto jobs = JobsFor(skins);
	PaintAll(pool, std::span(jobs), PaintFake);
	EXPECT_EQ(pool, nullptr);
	EXPECT_EQ(skins[0].paints, 1);
	EXPECT_EQ(skins[0].paintedBy, std::this_thread::get_id());
}

TEST(SkinPaintThreads, EachSkinOfManyGetsOnePaintAndTheSameTexelsAsOneThread)
{
	std::unique_ptr<WorkerPool> pool;
	for (const size_t count : {2, 3, 32, 33, 100})
	{
		std::vector<FakeSkin> skins(count);
		auto jobs = JobsFor(skins);
		PaintAll(pool, std::span(jobs), PaintFake);
		ASSERT_NE(pool, nullptr);

		std::vector<FakeSkin> expected(count);
		for (const auto& job : JobsFor(expected))
		{
			PaintFake(job);
		}
		for (size_t i = 0; i < count; ++i)
		{
			EXPECT_EQ(skins[i].paints, 1) << "skin " << i << " of " << count;
			EXPECT_EQ(skins[i].texels, expected[i].texels) << "skin " << i << " of " << count;
		}
	}
}

TEST(SkinPaintThreads, ThePoolIsKeptFromFrameToFrame)
{
	std::unique_ptr<WorkerPool> pool;
	std::vector<FakeSkin> skins(32);
	auto jobs = JobsFor(skins);
	PaintAll(pool, std::span(jobs), PaintFake);
	ASSERT_NE(pool, nullptr);
	const auto* first = pool.get();
	PaintAll(pool, std::span(jobs), PaintFake);
	EXPECT_EQ(pool.get(), first);
	for (const auto& skin : skins)
	{
		EXPECT_EQ(skin.paints, 2);
	}
}
