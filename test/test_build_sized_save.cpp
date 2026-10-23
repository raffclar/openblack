/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <optional>
#include <vector>

#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "ECS/Reflection/BinaryArchive.h"

using namespace openblack::ecs::reflection;

// The desktop build and the web build must write the same bytes for the same world, also for size_t, which has 64 bits
// on one and 32 bits on the other

TEST(BuildSizedSave, ASizeIsSixtyFourBitsOnEveryBuild)
{
	const entt::meta_ctx context;
	BinaryWriter out(context);
	Save(out, std::size_t {0x1234});
	ASSERT_EQ(out.Bytes().size(), 8u);
	EXPECT_EQ(out.Bytes()[0], 0x34);
	EXPECT_EQ(out.Bytes()[1], 0x12);
	EXPECT_EQ(out.Bytes()[7], 0x00);
}

TEST(BuildSizedSave, AListOfSizesHasEightBytesForEach)
{
	const entt::meta_ctx context;
	BinaryWriter out(context);
	Save(out, std::vector<std::size_t> {1, 2, 3});
	// The count, then each size
	EXPECT_EQ(out.Bytes().size(), 4u + 3u * 8u);
}

TEST(BuildSizedSave, SizesComeBack)
{
	const entt::meta_ctx context;
	BinaryWriter out(context);
	Save(out, std::optional<std::size_t> {77});
	Save(out, std::vector<std::size_t> {5, 6});
	Save(out, std::ptrdiff_t {-9});
	const EntityRemap remap;
	BinaryReader in(context, out.Bytes(), remap);
	std::optional<std::size_t> single;
	std::vector<std::size_t> list;
	std::ptrdiff_t difference {0};
	Load(in, single);
	Load(in, list);
	Load(in, difference);
	ASSERT_FALSE(in.Failed());
	EXPECT_EQ(single, std::optional<std::size_t> {77});
	EXPECT_EQ(list, (std::vector<std::size_t> {5, 6}));
	EXPECT_EQ(difference, -9);
}
