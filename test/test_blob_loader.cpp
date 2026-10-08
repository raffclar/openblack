/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <filesystem>
#include <vector>

#include <gtest/gtest.h>

#include "Resources/ResourcesInterface.h"

using openblack::resources::BlobId;
using openblack::resources::BlobLoader;
using openblack::resources::BlobManager;

namespace
{
const std::filesystem::path k_Path = "Data/Textures/fake.raw";
const std::vector<uint8_t> k_Bytes = {1, 2, 3, 250};
} // namespace

TEST(BlobLoader, BufferIsKeptAsIs)
{
	const auto blob = BlobLoader {}(BlobLoader::FromBufferTag {}, k_Bytes);
	ASSERT_NE(blob, nullptr);
	EXPECT_EQ(*blob, k_Bytes);
}

TEST(BlobLoader, IdIsThePathsHash)
{
	EXPECT_EQ(BlobId(k_Path), entt::hashed_string("Data/Textures/fake.raw").value());
	EXPECT_NE(BlobId(k_Path), BlobId("Data/Textures/other.raw"));
}

TEST(BlobLoader, CachedBytesAreSharedWithoutReading)
{
	// a blob already in the cache is returned as it is: no file system is involved (none is set up here)
	BlobManager blobs;
	blobs.Load(BlobId(k_Path), BlobLoader::FromBufferTag {}, k_Bytes);
	const auto& first = openblack::resources::LoadBlob(blobs, k_Path);
	const auto& second = openblack::resources::LoadOptionalBlob(blobs, k_Path, "fake");
	EXPECT_EQ(first, k_Bytes);
	EXPECT_EQ(&first, &second);
	EXPECT_EQ(blobs.Size(), 1u);
}
