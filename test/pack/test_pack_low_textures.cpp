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

#include <array>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <PackFile.h>
#include <gtest/gtest.h>

using openblack::pack::DdsHeader;
using openblack::pack::G3DTextureHeader;
using openblack::pack::PackFile;
using openblack::pack::PackResult;

namespace
{

void Append(std::vector<uint8_t>& bytes, const void* data, size_t size)
{
	const auto* begin = static_cast<const uint8_t*>(data);
	bytes.insert(bytes.end(), begin, begin + size);
}

void AppendBlock(std::vector<uint8_t>& bytes, std::string_view name, const std::vector<uint8_t>& body)
{
	std::array<char, 0x20> blockName {};
	std::memcpy(blockName.data(), name.data(), name.size());
	const auto size = static_cast<uint32_t>(body.size());
	Append(bytes, blockName.data(), blockName.size());
	Append(bytes, &size, sizeof(size));
	Append(bytes, body.data(), body.size());
}

/// A texture block: the texture's header, then a DXT1 DDS file of the side, every texel byte the fill. The game keeps an
/// empty header for a low resolution texture.
std::vector<uint8_t> TextureBlock(uint32_t id, uint32_t side, uint8_t fill, bool emptyHeader)
{
	DdsHeader dds {};
	dds.size = sizeof(DdsHeader);
	dds.height = side;
	dds.width = side;
	dds.pitchOrLinearSize = side * side / 2;
	dds.format.size = sizeof(dds.format);
	dds.format.fourCC = {'D', 'X', 'T', '1'};
	const std::vector<uint8_t> texels(dds.pitchOrLinearSize, fill);
	const auto ddsSize = static_cast<uint32_t>(sizeof(DdsHeader) + texels.size());
	G3DTextureHeader header {};
	if (!emptyHeader)
	{
		header = {.size = ddsSize, .id = id, .type = 2, .ddsSize = ddsSize};
	}
	else
	{
		header.ddsSize = ddsSize;
	}
	std::vector<uint8_t> block;
	Append(block, &header, sizeof(header));
	Append(block, &dds, sizeof(dds));
	Append(block, texels.data(), texels.size());
	return block;
}

/// A mesh pack with two textures and no meshes. Only the first texture has a low resolution texture.
std::string MeshPack()
{
	std::vector<uint8_t> bytes;
	Append(bytes, "LiOnHeAd", 8);
	std::vector<uint8_t> info;
	const std::array<uint32_t, 5> lookup = {2, 0x6e, 0, 0x6f, 0};
	Append(info, lookup.data(), sizeof(lookup));
	AppendBlock(bytes, "INFO", info);
	AppendBlock(bytes, "6e", TextureBlock(0x6e, 8, 0x11, false));
	AppendBlock(bytes, "6f", TextureBlock(0x6f, 8, 0x22, false));
	AppendBlock(bytes, "LOW6e", TextureBlock(0, 4, 0x33, true));
	std::vector<uint8_t> meshes;
	const std::array<char, 4> magic = {'M', 'K', 'J', 'C'};
	const uint32_t meshCount = 0;
	Append(meshes, magic.data(), magic.size());
	Append(meshes, &meshCount, sizeof(meshCount));
	AppendBlock(bytes, "MESHES", meshes);
	return {bytes.begin(), bytes.end()};
}

} // namespace

TEST(PackLowTextures, ATexturesLowResolutionTextureIsReadByItsName)
{
	PackFile pack;
	std::istringstream stream(MeshPack(), std::ios::binary);
	ASSERT_EQ(pack.ReadFile(stream), PackResult::Success);
	ASSERT_EQ(pack.GetTextures().size(), 2);
	ASSERT_EQ(pack.GetLowTextures().size(), 1);
	const auto& low = pack.GetLowTextures().at("6e");
	// The low texture takes the header of its texture, as its own is empty
	EXPECT_EQ(low.header.id, 0x6eu);
	EXPECT_EQ(low.ddsHeader.width, 4u);
	EXPECT_EQ(low.ddsData, std::vector<uint8_t>(8, 0x33));
	EXPECT_EQ(pack.GetTexture("6e").ddsHeader.width, 8u);
	EXPECT_FALSE(pack.GetLowTextures().contains("6f"));
}
