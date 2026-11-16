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

#include <vector>

#include <GameSocket.h>
#include <gtest/gtest.h>

#include "Online/EditorWorldUpload.h"

using namespace openblack;
using online::editor_world::Upload;

namespace
{

/// A file of a count of bytes, each byte from its place
std::vector<uint8_t> FileOf(size_t size)
{
	std::vector<uint8_t> file(size);
	for (size_t i = 0; i < size; ++i)
	{
		file[i] = static_cast<uint8_t>(i * 7);
	}
	return file;
}

} // namespace

TEST(EditorWorldUpload, AFileGoesInPartsAndComesBackWhole)
{
	const auto file = FileOf(game_socket::k_MostEditorWorldChunkBytes * 2 + 17);
	const auto parts = online::editor_world::Split(file);
	ASSERT_EQ(parts.size(), 3u);
	EXPECT_EQ(parts.back().bytes.size(), 17u);
	Upload upload;
	EXPECT_EQ(upload.Add(parts[0]), Upload::Added::More);
	EXPECT_EQ(upload.Add(parts[1]), Upload::Added::More);
	EXPECT_EQ(upload.Add(parts[2]), Upload::Added::Done);
	EXPECT_EQ(upload.Take(), file);
	// The upload is empty again after the take
	EXPECT_TRUE(upload.Take().empty());
}

TEST(EditorWorldUpload, AnEmptyOrTooLargeFileHasNoParts)
{
	EXPECT_TRUE(online::editor_world::Split({}).empty());
	EXPECT_TRUE(online::editor_world::Split(FileOf(game_socket::k_MostEditorWorldBytes + 1)).empty());
	EXPECT_EQ(online::editor_world::Split(FileOf(1)).size(), 1u);
}

TEST(EditorWorldUpload, APartOutOfOrderBreaksTheFile)
{
	const auto parts = online::editor_world::Split(FileOf(game_socket::k_MostEditorWorldChunkBytes * 3));
	ASSERT_EQ(parts.size(), 3u);
	Upload upload;
	EXPECT_EQ(upload.Add(parts[1]), Upload::Added::Broken);
	EXPECT_EQ(upload.Add(parts[0]), Upload::Added::More);
	EXPECT_EQ(upload.Add(parts[2]), Upload::Added::Broken);
	EXPECT_TRUE(upload.Take().empty());
	// A part with another size breaks the file too
	EXPECT_EQ(upload.Add(parts[0]), Upload::Added::More);
	auto other = parts[1];
	other.size += 1;
	EXPECT_EQ(upload.Add(other), Upload::Added::Broken);
}

TEST(EditorWorldUpload, APartZeroStartsANewFile)
{
	const auto first = online::editor_world::Split(FileOf(game_socket::k_MostEditorWorldChunkBytes * 2));
	const auto second = online::editor_world::Split(FileOf(5));
	Upload upload;
	EXPECT_EQ(upload.Add(first[0]), Upload::Added::More);
	EXPECT_EQ(upload.Add(second[0]), Upload::Added::Done);
	EXPECT_EQ(upload.Take(), FileOf(5));
}

TEST(EditorWorldUpload, ALastPartThatDoesNotFillTheSizeBreaksTheFile)
{
	Upload upload;
	EXPECT_EQ(upload.Add({.index = 0, .count = 1, .size = 10, .bytes = {1, 2, 3}}), Upload::Added::Broken);
	EXPECT_EQ(upload.Add({.index = 0, .count = 1, .size = 2, .bytes = {1, 2, 3}}), Upload::Added::Broken);
	EXPECT_EQ(upload.Add({.index = 0, .count = 1, .size = 3, .bytes = {1, 2, 3}}), Upload::Added::Done);
}
