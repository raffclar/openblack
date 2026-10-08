/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The spell files' loader (resources::PSysFileLoader) over a file system with a game folder of its own: the loose
// .txt wins over the .zzz, the .zzz is a u32 size and a zlib stream, and what is not a spell file gives null

#include <cstdint>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Particles/PSysFile.h"
#include "Resources/Loaders.h"
#include "support/TestServices.h"

using namespace openblack;

namespace
{
constexpr std::string_view k_LooseText = "BEGINPROPERTIES\nPROPERTY MaxSpellAge FLOAT 25\nENDPROPERTIES\n"
                                         "BEGINCLASS ParticlePointCreator Point\nBEGINPROPERTIES\nENDPROPERTIES\nENDCLASS\n";
constexpr std::string_view k_PackedText = "BEGINPROPERTIES\nPROPERTY MaxSpellAge FLOAT 7\nENDPROPERTIES\n";

/// A zlib stream of one stored (uncompressed) block: what zip::Inflate reads back as `text`
std::vector<uint8_t> StoredZlib(std::string_view text)
{
	std::vector<uint8_t> out = {0x78, 0x01, 0x01}; // the zlib header, then a final stored block
	const auto length = static_cast<uint16_t>(text.size());
	const auto complement = static_cast<uint16_t>(~length);
	out.push_back(static_cast<uint8_t>(length));
	out.push_back(static_cast<uint8_t>(length >> 8));
	out.push_back(static_cast<uint8_t>(complement));
	out.push_back(static_cast<uint8_t>(complement >> 8));
	out.insert(out.end(), text.begin(), text.end());
	uint32_t a = 1;
	uint32_t b = 0;
	for (const auto c : text)
	{
		a = (a + static_cast<uint8_t>(c)) % 65521;
		b = (b + a) % 65521;
	}
	const uint32_t adler = (b << 16) | a; // big-endian at the end
	for (int shift = 24; shift >= 0; shift -= 8)
	{
		out.push_back(static_cast<uint8_t>(adler >> shift));
	}
	return out;
}

/// The .zzz layout: the inflated size as a u32, then the zlib stream
std::vector<uint8_t> Packed(std::string_view text)
{
	const auto size = static_cast<uint32_t>(text.size());
	std::vector<uint8_t> out = {static_cast<uint8_t>(size), static_cast<uint8_t>(size >> 8), static_cast<uint8_t>(size >> 16),
	                            static_cast<uint8_t>(size >> 24)};
	const auto stream = StoredZlib(text);
	out.insert(out.end(), stream.begin(), stream.end());
	return out;
}

class PSysFileLoaderTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the loader warns on the game's logger
		if (!spdlog::get("game"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		static std::atomic<int> s_count {0};
		_root = std::filesystem::temp_directory_path() /
		        ("openblack_test_psys_files_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" +
		         std::to_string(s_count++));
		std::filesystem::create_directories(_root / "Data" / "Spells" / "ZSpellFiles");
		Locator::filesystem::value().SetGamePath(_root);
	}
	void TearDown() override
	{
		std::error_code ec;
		std::filesystem::remove_all(_root, ec);
	}

	void Write(const std::string& file, std::string_view bytes) const
	{
		std::ofstream(_root / "Data" / "Spells" / "ZSpellFiles" / file, std::ios::binary)
		    .write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	}
	void Write(const std::string& file, const std::vector<uint8_t>& bytes) const
	{
		Write(file, std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
	}

	[[nodiscard]] static resources::PSysFileLoader::result_type Load(const std::string& name)
	{
		return resources::PSysFileLoader {}(resources::PSysFileLoader::FromDiskTag {}, name);
	}

	// first, so that it goes last
	const test::ScopedDefaultFileSystem _fileSystem;
	std::filesystem::path _root;
};
} // namespace

TEST_F(PSysFileLoaderTest, TheLooseTextWins)
{
	Write("openblack_test_both.txt", k_LooseText);
	Write("openblack_test_both_txt.zzz", Packed(k_PackedText));
	const auto file = Load("openblack_test_both");
	ASSERT_NE(file, nullptr);
	EXPECT_EQ(file->name, "openblack_test_both");
	EXPECT_EQ(file->objects.size(), 1u); // the .txt's one class, not the .zzz's none
}

TEST_F(PSysFileLoaderTest, ThePackedFileIsInflated)
{
	Write("openblack_test_packed_txt.zzz", Packed(k_PackedText));
	const auto file = Load("openblack_test_packed");
	ASSERT_NE(file, nullptr);
	EXPECT_EQ(file->name, "openblack_test_packed");
	EXPECT_TRUE(file->objects.empty());
	const auto parsed = psys::File::Parse(k_PackedText, "openblack_test_packed");
	ASSERT_TRUE(parsed.has_value());
	EXPECT_EQ(file->header.properties.size(), parsed->header.properties.size());
}

TEST_F(PSysFileLoaderTest, NotASpellFileOrMissingIsNull)
{
	Write("openblack_test_text.txt", std::string_view("not a spell file"));
	EXPECT_EQ(Load("openblack_test_text"), nullptr);
	EXPECT_EQ(Load("openblack_test_missing"), nullptr);
}
