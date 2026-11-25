/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>
#include <string>
#include <thread>
#include <utility>

#include <gtest/gtest.h>

#include "ECS/Components/Transform.h"
#include "ECS/DrawReadWatch.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{

constexpr auto k_Name = "land registry";

entt::entity MakeEntity(Registry& registry)
{
	const auto entity = registry.Create();
	registry.Assign<components::Transform>(entity);
	return entity;
}

} // namespace

#if defined(OPENBLACK_DEVELOPER_TOOLS)

TEST(DrawReadWatch, CountsTheReadsOfTheDrawOnly)
{
	Registry registry(Registry::Drawn::No);
	const auto entity = MakeEntity(registry);
	DrawReadWatch watch(DrawReadCheck::Report);
	registry.WatchReads(&watch, k_Name);
	// A read before the draw starts does not count
	EXPECT_NE(std::as_const(registry).TryGet<const components::Transform>(entity), nullptr);
	EXPECT_TRUE(watch.GetReads().empty());

	watch.Start();
	EXPECT_NE(std::as_const(registry).TryGet<const components::Transform>(entity), nullptr);
	EXPECT_NE(std::as_const(registry).TryGet<const components::Transform>(entity), nullptr);
	EXPECT_TRUE(registry.Valid(entity));
	watch.Stop();
	EXPECT_TRUE(registry.Valid(entity));

	const auto reads = watch.GetReads();
	ASSERT_EQ(reads.size(), 2u);
	EXPECT_EQ(reads[0].registry, k_Name);
	EXPECT_NE(reads[0].what.find("TryGet<"), std::string::npos);
	EXPECT_NE(reads[0].what.find("Transform"), std::string::npos);
	EXPECT_EQ(reads[0].count, 2u);
	EXPECT_EQ(reads[1].what, "Valid<>");
	EXPECT_EQ(reads[1].count, 1u);
}

TEST(DrawReadWatch, IgnoresTheReadsOfOtherThreads)
{
	Registry registry(Registry::Drawn::No);
	const auto entity = MakeEntity(registry);
	DrawReadWatch watch(DrawReadCheck::Report);
	registry.WatchReads(&watch, k_Name);
	watch.Start();
	std::thread other([&registry, entity] { EXPECT_TRUE(registry.Valid(entity)); });
	other.join();
	watch.Stop();
	EXPECT_TRUE(watch.GetReads().empty());
}

TEST(DrawReadWatch, TheFailCheckCallsTheHandlerAtEachNewKindOfRead)
{
	Registry registry(Registry::Drawn::No);
	MakeEntity(registry);
	std::optional<DrawReadWatch::Read> failed;
	DrawReadWatch watch(DrawReadCheck::Fail, [&failed](const DrawReadWatch::Read& read) { failed = read; });
	registry.WatchReads(&watch, k_Name);
	watch.Start();
	std::as_const(registry).Each<const components::Transform>([](const components::Transform&) {});
	watch.Stop();
	ASSERT_TRUE(failed.has_value());
	EXPECT_EQ(failed->registry, k_Name);
	EXPECT_NE(failed->what.find("Each<"), std::string::npos);
}

TEST(DrawReadWatch, TheOffCheckCountsNothing)
{
	Registry registry(Registry::Drawn::No);
	const auto entity = MakeEntity(registry);
	DrawReadWatch watch(DrawReadCheck::Off);
	registry.WatchReads(&watch, k_Name);
	watch.Start();
	EXPECT_TRUE(registry.Valid(entity));
	watch.Stop();
	EXPECT_TRUE(watch.GetReads().empty());
}

TEST(DrawReadWatch, NoWatchCountsNothing)
{
	Registry registry(Registry::Drawn::No);
	const auto entity = MakeEntity(registry);
	DrawReadWatch watch(DrawReadCheck::Report);
	registry.WatchReads(&watch, k_Name);
	registry.WatchReads(nullptr, {});
	watch.Start();
	EXPECT_TRUE(registry.Valid(entity));
	watch.Stop();
	EXPECT_TRUE(watch.GetReads().empty());
}

#endif
