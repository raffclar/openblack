/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdlib>
#include <cstring>

#include <array>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <L3DFile.h>
#include <MorphFile.h>
#include <PackFile.h>
#include <cxxopts.hpp>

#include "GltfExport.h"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_USE_CPP14
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

int PrintRawBytes(const void* data, std::size_t size)
{
	const uint32_t bytesPerLine = 0x10;
	std::printf("       00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
	std::printf("     ,------------------------------------------------\n");
	for (std::size_t y = 0; y < size / bytesPerLine; ++y)
	{
		auto base = static_cast<uint32_t>(y * bytesPerLine);
		printf("%04X |", base);
		for (uint32_t x = 0; x < bytesPerLine; ++x)
		{
			auto byte = reinterpret_cast<const uint8_t*>(data)[base + x];
			printf(" \033[%dm%02X\033[0m", byte == 0 ? 2 : 1, byte);
		}
		printf("\n");
	}
	auto remaining = size & (bytesPerLine - 1);
	if (remaining != 0)
	{
		auto base = static_cast<uint32_t>(size & ~(bytesPerLine - 1));
		printf("%04X |", base);
		for (std::size_t x = 0; x < remaining; ++x)
		{
			auto byte = reinterpret_cast<const uint8_t*>(data)[base + x];
			printf(" \033[%dm%02X\033[0m", byte == 0 ? 2 : 1, byte);
		}
		printf("\n");
	}

	return EXIT_SUCCESS;
}

int PrintHeader(openblack::l3d::L3DFile& l3d)
{
	const auto& header = l3d.GetHeader();

	using L3DMeshFlags = openblack::l3d::L3DMeshFlags;
	auto flagToString = [](L3DMeshFlags flag) {
		std::string result;
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::Unknown1)) != 0)
		{
			result += "Unknown1|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::Unknown2)) != 0)
		{
			result += "Unknown2|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::Unknown3)) != 0)
		{
			result += "Unknown3|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::Unknown4)) != 0)
		{
			result += "Unknown4|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::Unknown5)) != 0)
		{
			result += "Unknown5|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::Unknown6)) != 0)
		{
			result += "Unknown6|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::Unknown7)) != 0)
		{
			result += "Unknown7|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::Unknown8)) != 0)
		{
			result += "Unknown8|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::HasBones)) != 0)
		{
			result += "HasBones|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::Unknown10)) != 0)
		{
			result += "Unknown10|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::HasChimney)) != 0)
		{
			result += "HasChimney|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::HasDoorPosition)) != 0)
		{
			result += "HasDoorPosition|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::Packed)) != 0)
		{
			result += "Packed|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::NoDraw)) != 0)
		{
			result += "NoDraw|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::Unknown15)) != 0)
		{
			result += "Unknown15|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::ContainsLandscapeFeature)) != 0)
		{
			result += "ContainsLandscapeFeature|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::Unknown17)) != 0)
		{
			result += "Unknown17|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::Unknown18)) != 0)
		{
			result += "Unknown18|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::ContainsUV2)) != 0)
		{
			result += "ContainsUV2|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::ContainsNameData)) != 0)
		{
			result += "ContainsNameData|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::ContainsExtraMetrics)) != 0)
		{
			result += "ContainsExtraMetrics|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::ContainsEBone)) != 0)
		{
			result += "ContainsEBone|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::ContainsTnLData)) != 0)
		{
			result += "ContainsTnLData|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::ContainsNewEP)) != 0)
		{
			result += "ContainsNewEP|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::Unknown25)) != 0)
		{
			result += "Unknown25|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::Unknown26)) != 0)
		{
			result += "Unknown26|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::Unknown27)) != 0)
		{
			result += "Unknown27|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::Unknown28)) != 0)
		{
			result += "Unknown28|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::Unknown29)) != 0)
		{
			result += "Unknown29|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::Unknown30)) != 0)
		{
			result += "Unknown30|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::Unknown31)) != 0)
		{
			result += "Unknown31|";
		}
		if ((static_cast<uint32_t>(flag) & static_cast<uint32_t>(L3DMeshFlags::Unknown32)) != 0)
		{
			result += "Unknown32|";
		}
		return result.size() > 1 ? result.substr(0, result.size() - 1) : result;
	};

	std::printf("magic: %s\n", std::string(header.magic.data(), sizeof(header.magic[0]) * header.magic.size()).c_str());
	std::printf("flags: %s\n", flagToString(header.flags).c_str());
	std::printf("size: %u\n", header.size);
	std::printf("submesh count: %u\n", header.submeshCount);
	std::printf("submesh start offset: 0x%08X\n", header.submeshOffsetsOffset);
	std::printf("bounding box:\n");
	std::printf("\tunknown 32 bits: 0x%08X\n", header.boundingBox.unknown);
	std::printf("\tcentre: {%f, %f, %f}\n", header.boundingBox.centre.x, header.boundingBox.centre.y,
	            header.boundingBox.centre.z);
	std::printf("\tsize: {%f, %f, %f}\n", header.boundingBox.size.x, header.boundingBox.size.y, header.boundingBox.size.z);
	std::printf("\tdiagonal length: %f\n", header.boundingBox.diagonalLength);
	std::printf("unknown offset: 0x%08X\n", header.anotherOffset);
	std::printf("skin count: %u\n", header.skinCount);
	std::printf("skin start offset: 0x%08X\n", header.skinOffsetsOffset);
	std::printf("extra data count: %u\n", header.extraDataCount);
	std::printf("extra data offset: 0x%08X\n", header.extraDataOffset);
	std::printf("footprint data offset: 0x%08X\n", header.footprintDataOffset);
	std::printf("\n");

	return EXIT_SUCCESS;
}

int PrintMeshHeaders(openblack::l3d::L3DFile& l3d)
{
	const auto& meshHeaders = l3d.GetSubmeshHeaders();

	uint32_t i = 0;
	using Flags = openblack::l3d::L3DSubmeshHeader::Flags;
	auto flagToString = [](Flags flags) {
		std::string result;
		if (flags.hasBones)
		{
			result += "hasBones";
		}
		result += "|lodMask=" + std::to_string(flags.lodMask);
		result += "|status=" + std::to_string(flags.status);
		if (flags.unknown1 != 0u)
		{
			result += "|unknown1";
		}
		if (flags.isWindow)
		{
			result += "|isWindow";
		}
		if (flags.isPhysics)
		{
			result += "|isPhysics";
		}
		return result;
	};
	for (const auto& header : meshHeaders)
	{
		std::printf("mesh #%u\n", ++i);
		std::printf("flags: %s\n", flagToString(header.flags).c_str());
		std::printf("primitive count: %u\n", header.numPrimitives);
		std::printf("primitives start offset: 0x%08X\n", header.primitivesOffset);
		std::printf("bone count: %u\n", header.numBones);
		std::printf("bones start offset: 0x%08X\n", header.bonesOffset);
		std::printf("\n");
	}

	return EXIT_SUCCESS;
}

int PrintSkins(openblack::l3d::L3DFile& l3d)
{
	const auto& skins = l3d.GetSkins();

	for (const auto& skin : skins)
	{
		std::printf("skin id: 0x%X\n", skin.id);
		std::printf("data:\n");
		constexpr uint16_t k_Subsample = 8;
		constexpr uint16_t k_Magnitude = (k_Subsample * k_Subsample) / 16;
		for (uint16_t y = 0; y < openblack::l3d::L3DTexture::k_Height / k_Subsample; ++y)
		{
			for (uint16_t x = 0; x < openblack::l3d::L3DTexture::k_Width / k_Subsample; ++x)
			{
				uint32_t red = 0;
				uint32_t green = 0;
				uint32_t blue = 0;
				for (uint16_t j = 0; j < k_Subsample; ++j)
				{
					for (uint16_t i = 0; i < k_Subsample; ++i)
					{
						const auto& color =
						    skin.texels.at(x * k_Subsample + i + (y * k_Subsample + j) * openblack::l3d::L3DTexture::k_Width);
						red += color.r;
						green += color.g;
						blue += color.b;
					}
				}
				red /= k_Magnitude;
				green /= k_Magnitude;
				blue /= k_Magnitude;
				std::printf("\x1b[48;2;%u;%u;%um  \x1b[0m", static_cast<uint8_t>(red), static_cast<uint8_t>(green),
				            static_cast<uint8_t>(blue));
			}
			std::printf("\n");
		}
	}

	return EXIT_SUCCESS;
}

int PrintExtraPoints(openblack::l3d::L3DFile& l3d)
{
	const auto& points = l3d.GetExtraPoints();

	for (const auto& point : points)
	{
		std::printf("(%.3f, %.3f, %.3f)\n", point.x, point.y, point.z);
	}

	return EXIT_SUCCESS;
}

int PrintPrimitiveHeaders(openblack::l3d::L3DFile& l3d)
{
	const auto& primitiveHeaders = l3d.GetPrimitiveHeaders();

	uint32_t i = 0;
	for (const auto& header : primitiveHeaders)
	{
		std::printf("primitive #%u\n", ++i);
		static const std::array<const char*, static_cast<uint32_t>(openblack::l3d::L3DMaterial::Type::_Count)> typeMap = {{
		    "Smooth",
		    "SmoothAlpha",
		    "Textured",
		    "TexturedAlpha",
		    "AlphaTextured",
		    "AlphaTexturedAlpha",
		    "AlphaTexturedAlphaNz",
		    "SmoothAlphaNz",
		    "TexturedAlphaNz",
		    "TexturedChroma",
		    "AlphaTexturedAlphaAdditiveChroma",
		    "AlphaTexturedAlphaAdditiveChromaNz",
		    "AlphaTexturedAlphaAdditive",
		    "AlphaTexturedAlphaAdditiveNz",
		    "Unknown",
		    "TexturedChromaAlpha",
		    "TexturedChromaAlphaNz",
		    "Unknown",
		    "ChromaJustZ",
		}};
		if (static_cast<uint32_t>(header.material.type) >= static_cast<uint32_t>(openblack::l3d::L3DMaterial::Type::_Count))
		{
			std::printf("material's type: Unknown (0x%08X)\n", static_cast<uint32_t>(header.material.type));
		}
		else
		{
			std::printf("material's type: %s (0x%08X)\n", typeMap.at(static_cast<uint32_t>(header.material.type)),
			            static_cast<uint32_t>(header.material.type));
		}
		std::printf("material's alpha cut out threshold: 0x%02X (%f)\n", header.material.alphaCutoutThreshold,
		            header.material.alphaCutoutThreshold / 255.0f);
		std::printf("material's cull mode: 0x%02X\n", header.material.cullMode);
		std::printf("material's skinID: 0x%08X\n", header.material.skinID);
		std::printf("material's color: 0x%08X (%f, %f, %f, %f)\n", header.material.color.raw,
		            header.material.color.bgra.b / 255.0f, header.material.color.bgra.g / 255.0f,
		            header.material.color.bgra.r / 255.0f, header.material.color.bgra.a / 255.0f);
		std::printf("vertex count: %u\n", header.numVertices);
		std::printf("vertex start offset: 0x%08X\n", header.verticesOffset);
		std::printf("triangle count: %u\n", header.numTriangles);
		std::printf("triangle start offset: 0x%08X\n", header.trianglesOffset);
		std::printf("vertex group count: %u\n", header.numGroups);
		std::printf("vertex group start offset: 0x%08X\n", header.groupsOffset);
		std::printf("vertex blend count: %u\n", header.numVertexBlends);
		std::printf("vertex blend start offset: 0x%08X\n", header.vertexBlendsOffset);
		std::printf("\n");
	}

	return EXIT_SUCCESS;
}

int PrintBones(openblack::l3d::L3DFile& l3d)
{
	const auto& bones = l3d.GetBones();

	uint32_t i = 0;
	for (const auto& bone : bones)
	{
		std::printf("bone #%u\n", i++);
		if (bone.parent == std::numeric_limits<uint32_t>::max())
		{
			std::printf("parent: None\n");
		}
		else
		{
			std::printf("parent: %X\n", bone.parent);
		}
		if (bone.firstChild == std::numeric_limits<uint32_t>::max())
		{
			std::printf("first child: None\n");
		}
		else
		{
			std::printf("first child: %X\n", bone.firstChild);
		}
		if (bone.rightSibling == std::numeric_limits<uint32_t>::max())
		{
			std::printf("right sibling: None\n");
		}
		else
		{
			std::printf("right sibling: %X\n", bone.rightSibling);
		}
		std::printf("rotation:\n");
		std::printf("     [%6.3f %6.3f %6.3f]\n", bone.orientation[0], bone.orientation[1], bone.orientation[2]);
		std::printf("     [%6.3f %6.3f %6.3f]\n", bone.orientation[3], bone.orientation[4], bone.orientation[5]);
		std::printf("     [%6.3f %6.3f %6.3f]\n", bone.orientation[6], bone.orientation[7], bone.orientation[8]);
		std::printf("position: (%5.1f %5.1f %5.1f)\n", bone.position.x, bone.position.y, bone.position.z);
		std::printf("\n");
	}

	return EXIT_SUCCESS;
}

int PrintVertices(openblack::l3d::L3DFile& l3d)
{
	const auto& vertices = l3d.GetVertices();
	std::printf("|     position     |   uv coord   |        normal        |\n");
	std::printf("|------------------|--------------|----------------------|\n");

	for (const auto& vertex : vertices)
	{
		std::printf("|%5.1f %5.1f %5.1f |%6.3f %6.3f | %6.3f %6.3f %6.3f |\n", vertex.position.x, vertex.position.y,
		            vertex.position.z, vertex.texCoord.x, vertex.texCoord.y, vertex.normal.x, vertex.normal.y, vertex.normal.z);
	}

	return EXIT_SUCCESS;
}

int PrintIndices(openblack::l3d::L3DFile& l3d)
{
	constexpr uint8_t k_IndexPerLine = 12;
	const auto& indices = l3d.GetIndices();

	for (std::size_t i = 0; i < indices.size(); ++i)
	{
		if (i > 0)
		{
			std::printf((i % k_IndexPerLine != 0) ? " " : "\n");
		}
		std::printf("%4u", indices[i]);
	}
	std::printf("\n");

	return EXIT_SUCCESS;
}

int PrintLookUpTables(openblack::l3d::L3DFile& l3d)
{
	const auto& lookUpTable = l3d.GetLookUpTableData();
	return PrintRawBytes(lookUpTable.data(), lookUpTable.size() * sizeof(lookUpTable[0]));
}

int PrintBlendValues(openblack::l3d::L3DFile& l3d)
{
	// Each primitive's blends: the vertex that moves, the vertex it moves towards and how far, both counted in the
	// primitive
	const auto& blends = l3d.GetBlends();
	size_t next = 0;
	for (size_t p = 0; p < l3d.GetPrimitiveHeaders().size(); ++p)
	{
		const auto& header = l3d.GetPrimitiveHeaders()[p];
		if (header.numVertexBlends == 0 || header.vertexBlendsOffset == std::numeric_limits<uint32_t>::max())
		{
			continue;
		}
		std::printf("primitive %zu: %u blends\n", p, header.numVertexBlends);
		std::printf("| vertex | towards | weight   |\n");
		std::printf("|--------|---------|----------|\n");
		for (uint32_t i = 0; i < header.numVertexBlends && next < blends.size(); ++i, ++next)
		{
			const auto& blend = blends[next];
			std::printf("| %6u | %7u | %8.6f |\n", blend.indices[0], blend.indices[1], static_cast<double>(blend.weight));
		}
	}
	return EXIT_SUCCESS;
}

int PrintFootprintValues(openblack::l3d::L3DFile& l3d)
{
	const auto& footprint = l3d.GetFootprint();

	if (!footprint.has_value())
	{
		printf("No data");
		return EXIT_FAILURE;
	}

	printf("header.count: %d\n", footprint->header.count);
	printf("header.offset: %d\n", footprint->header.offset);
	printf("header.size: %d\n", footprint->header.size);
	printf("header.width: %d\n", footprint->header.width);
	printf("header.height: %d\n", footprint->header.height);
	printf("header.unknown: 0x%08x\n", footprint->header.unknown);
	printf("entries:");
	for (uint32_t i = 0; const auto& entry : footprint->entries)
	{
		printf("[%d]\n", i);
		++i;

		printf("entry.unknown1: 0x%08x", entry.unknown1);
		printf("entry.unknown2: 0x%08x", entry.unknown2);
		printf("entry.triangleCount: 0x%08x", entry.triangleCount);
		std::printf("|   position   |   uv coord   |\n");
		std::printf("|--------------|--------------|\n");
		for (const auto& t : entry.triangles)
		{
			std::printf("|%6.3f %6.3f |%6.3f %6.3f |\n", t.world[0].x, t.world[0].y, t.texture[0].x, t.texture[0].y);
			std::printf("|%6.3f %6.3f |%6.3f %6.3f |\n", t.world[1].x, t.world[1].y, t.texture[1].x, t.texture[1].y);
			std::printf("|%6.3f %6.3f |%6.3f %6.3f |\n", t.world[2].x, t.world[2].y, t.texture[2].x, t.texture[2].y);
		}

		for (uint16_t y = 0; y < footprint->header.height; ++y)
		{
			for (uint16_t x = 0; x < footprint->header.width; ++x)
			{
				const auto val = entry.pixels[x + footprint->header.width * y];
				const auto red = (val & 0xF);
				const auto green = (val >> 4) & 0xF;
				const auto blue = (val >> 8) & 0xF;
				std::printf("\x1b[48;2;%u;%u;%um  \x1b[0m", static_cast<uint8_t>(red * 0x11),
				            static_cast<uint8_t>(green * 0x11), static_cast<uint8_t>(blue * 0x11));
			}
		}

		printf("entry.unknown3: 0x%08x", entry.unknown3);
		printf("entry.unknown4: 0x%08x", entry.unknown4);
		printf("entry.unknown5: 0x%08x", entry.unknown5);
	}

	printf("footer.unknown1: 0x%08x", footprint->footer.unknown1);
	printf("footer.unknown2: %f", footprint->footer.unknown2);
	printf("footer.unknown3: 0x%08x", footprint->footer.unknown3);
	return EXIT_SUCCESS;
}

int PrintUv2Values(openblack::l3d::L3DFile& l3d)
{
	const auto& uv2Values = l3d.GetUv2Data();
	return PrintRawBytes(uv2Values.data(), uv2Values.size() * sizeof(uv2Values[0]));
}

int PrintNameValue(openblack::l3d::L3DFile& l3d)
{
	const auto& nameValue = l3d.GetNameData();
	std::printf("name data: %s\n", nameValue.c_str());
	return EXIT_SUCCESS;
}

int PrintExtraMetricsValues(openblack::l3d::L3DFile& l3d)
{
	const auto& extraMetrics = l3d.GetExtraMetrics();

	if (extraMetrics.empty())
	{
		printf("No data");
		return EXIT_FAILURE;
	}

	for (uint32_t i = 0; const auto& m : extraMetrics)
	{
		printf("[%u]:\n", i);
		printf("  [ %8.3f %8.3f %8.3f ]\n", m[0], m[1], m[2]);
		printf("  [ %8.3f %8.3f %8.3f ]\n", m[3], m[4], m[5]);
		printf("  [ %8.3f %8.3f %8.3f ]\n", m[6], m[7], m[8]);
		printf("  [ %8.3f %8.3f %8.3f ]\n", m[9], m[10], m[11]);
		++i;
	}
	return EXIT_SUCCESS;
}

struct Arguments
{
	enum class Mode : uint8_t
	{
		Header,
		MeshHeader,
		Skin,
		ExtraPoint,
		PrimitiveHeader,
		Bones,
		Vertices,
		Indices,
		LookUpTables,
		VertexBlendValues,
		Footprint,
		Uv2,
		Name,
		ExtraMetrics,
		Write,
		Extract,
	};
	Mode mode;
	struct Read
	{
		std::vector<std::filesystem::path> filenames;
	} read;
	struct Write
	{
		std::filesystem::path outFilename;
		std::filesystem::path gltfFile;
	} write;
	struct Extract
	{
		std::filesystem::path inFilename;
		std::filesystem::path gltfFile;
		/// NAME=FILE pairs of meshes to add as morph targets
		std::vector<std::string> targets;
		/// A creature's .cbn file, which names its base mesh, its variant meshes and its animations
		std::filesystem::path creature;
		std::filesystem::path specDirectory;
		std::filesystem::path meshDirectory;
		bool animations;
		bool rightHanded;
	} extract;
};

namespace details
{
template <typename dstT, typename srcT>
    requires(std::is_same_v<dstT, srcT>)
bool copyBufferView(dstT* dst, const uint8_t* src, size_t count)
{
	for (size_t j = 0; j < count; ++j)
	{
		dst[j] = static_cast<dstT>(reinterpret_cast<const srcT*>(src)[j]);
	}
	return true;
}

template <typename dstT, typename srcT>
bool copyBufferView(dstT* dst, const uint8_t* src, size_t count)
{
	memcpy(dst, src, count * sizeof(dstT));
	return true;
}
} // namespace details

template <typename desT>
bool copyBufferView(desT* dst, const uint8_t* src, size_t count, uint32_t componentType)
{
	switch (componentType)
	{
	case TINYGLTF_COMPONENT_TYPE_BYTE:
		details::copyBufferView<desT, int8_t>(dst, src, count);
		break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
		details::copyBufferView<desT, uint8_t>(dst, src, count);
		break;
	case TINYGLTF_COMPONENT_TYPE_SHORT:
		details::copyBufferView<desT, int16_t>(dst, src, count);
		break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		details::copyBufferView<desT, uint16_t>(dst, src, count);
		break;
	case TINYGLTF_COMPONENT_TYPE_INT:
		details::copyBufferView<desT, int32_t>(dst, src, count);
		break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
		details::copyBufferView<desT, uint32_t>(dst, src, count);
		break;
	case TINYGLTF_COMPONENT_TYPE_FLOAT:
		details::copyBufferView<desT, float>(dst, src, count);
		break;
	case TINYGLTF_COMPONENT_TYPE_DOUBLE:
		details::copyBufferView<desT, double>(dst, src, count);
		break;
	default:
		return false;
	}
	return true;
}

bool NoopLoadImageDataFunction(tinygltf::Image* /*unused*/, const int /*unused*/, std::string* /*unused*/,
                               std::string* /*unused*/, int /*unused*/, int /*unused*/, const unsigned char* /*unused*/,
                               int /*unused*/, void* /*unused*/)
{
	std::cerr << "Warn: GLTF Image Data function called but not implemented.\n";
	return true;
}

int WriteFile(const Arguments::Write& args) noexcept
{
	openblack::l3d::L3DFile l3d {};

	if (args.gltfFile.empty())
	{
		return EXIT_FAILURE;
	}

	tinygltf::TinyGLTF loader;
	tinygltf::Model gltf;
	std::string err;
	std::string warn;
	bool ret;

	loader.SetImageLoader(NoopLoadImageDataFunction, nullptr);
	ret = loader.LoadASCIIFromFile(&gltf, &err, &warn, args.gltfFile.string(),
	                               tinygltf::REQUIRE_VERSION | tinygltf::REQUIRE_ACCESSORS | tinygltf::REQUIRE_BUFFERS |
	                                   tinygltf::REQUIRE_BUFFER_VIEWS);

	if (!warn.empty())
	{
		std::cerr << "Warn: " << warn << '\n';
	}

	if (!err.empty())
	{
		std::cerr << "Err: " << err << '\n';
	}

	if (!ret)
	{
		std::cerr << "Err: Failed to parse glTF.\n";
		return EXIT_FAILURE;
	}

	if (gltf.meshes.empty())
	{
		std::cerr << "Err: There are no meshes in glTF.\n";
		return EXIT_FAILURE;
	}

	for (auto& gltfNode : gltf.nodes)
	{
		if (gltfNode.mesh < 0)
		{
			continue;
		}

		auto& gltfMesh = gltf.meshes[gltfNode.mesh];
		using L3DSubmeshHeader = openblack::l3d::L3DSubmeshHeader;
		L3DSubmeshHeader submesh;
		submesh.flags.hasBones = false;
		submesh.flags.lodMask = 0;
		submesh.flags.status = 0;
		submesh.flags.unknown1 = 0b101;
		submesh.flags.isWindow = false;
		submesh.flags.isPhysics = false;
		submesh.flags.unknown2 = 0;
		submesh.numPrimitives = 0;
		submesh.primitivesOffset = static_cast<uint32_t>(l3d.GetPrimitiveHeaders().size());
		submesh.numBones = 0;
		submesh.bonesOffset = static_cast<uint32_t>(l3d.GetBones().size());

		// Bones
		if (gltfNode.skin >= 0)
		{
			auto& skin = gltf.skins[gltfNode.skin];
			auto skelton = skin.skeleton;
			if (skelton < 0)
			{
				if (!skin.joints.empty())
				{
					skelton = skin.joints.front();
				}
			}
			if (skelton >= 0)
			{
				submesh.flags.hasBones = true;
				std::vector<openblack::l3d::L3DBone> bones;
				std::function<bool(const std::vector<int>&, uint32_t parent)> buildJoints;
				buildJoints = [&gltf, &bones, &buildJoints](const std::vector<int>& children, uint32_t parentId) -> bool {
					openblack::l3d::L3DBone* leftSibling = nullptr;
					for (const auto& child : children)
					{
						const auto& gltfJoint = gltf.nodes[child];
						const auto id = static_cast<uint32_t>(bones.size());
						bones.emplace_back();
						auto& bone = bones.back();
						bone.parent = parentId;
						bone.firstChild = std::numeric_limits<uint32_t>::max();
						bone.rightSibling = std::numeric_limits<uint32_t>::max();
						// Convert Quaternion to matrix
						bone.orientation[0] = static_cast<float>(1.0f - 2.0f * gltfJoint.rotation[1] * gltfJoint.rotation[1] -
						                                         2.0f * gltfJoint.rotation[2] * gltfJoint.rotation[2]);
						bone.orientation[1] = static_cast<float>(2.0f * gltfJoint.rotation[0] * gltfJoint.rotation[1] -
						                                         2.0f * gltfJoint.rotation[2] * gltfJoint.rotation[3]);
						bone.orientation[2] = static_cast<float>(2.0f * gltfJoint.rotation[0] * gltfJoint.rotation[2] +
						                                         2.0f * gltfJoint.rotation[1] * gltfJoint.rotation[3]);
						bone.orientation[3] = static_cast<float>(2.0f * gltfJoint.rotation[0] * gltfJoint.rotation[1] +
						                                         2.0f * gltfJoint.rotation[2] * gltfJoint.rotation[3]);
						bone.orientation[4] = static_cast<float>(1.0f - 2.0f * gltfJoint.rotation[0] * gltfJoint.rotation[0] -
						                                         2.0f * gltfJoint.rotation[2] * gltfJoint.rotation[2]);
						bone.orientation[5] = static_cast<float>(2.0f * gltfJoint.rotation[1] * gltfJoint.rotation[2] -
						                                         2.0f * gltfJoint.rotation[0] * gltfJoint.rotation[3]);
						bone.orientation[6] = static_cast<float>(2.0f * gltfJoint.rotation[0] * gltfJoint.rotation[2] -
						                                         2.0f * gltfJoint.rotation[1] * gltfJoint.rotation[3]);
						bone.orientation[7] = static_cast<float>(2.0f * gltfJoint.rotation[1] * gltfJoint.rotation[2] +
						                                         2.0f * gltfJoint.rotation[0] * gltfJoint.rotation[3]);
						bone.orientation[8] = static_cast<float>(1.0f - 2.0f * gltfJoint.rotation[0] * gltfJoint.rotation[0] -
						                                         2.0f * gltfJoint.rotation[1] * gltfJoint.rotation[1]);
						bone.position.x = static_cast<float>(gltfJoint.translation[0]);
						bone.position.y = static_cast<float>(gltfJoint.translation[1]);
						bone.position.z = static_cast<float>(gltfJoint.translation[2]);
						if (!buildJoints(gltfJoint.children, id))
						{
							return false;
						}
						if (leftSibling != nullptr)
						{
							leftSibling->rightSibling = id;
						}
						leftSibling = &bone;
					}
					return true;
				};

				buildJoints({skelton}, std::numeric_limits<uint32_t>::max());

				l3d.AddBones(bones);
				submesh.numBones += static_cast<uint32_t>(bones.size());
			}
		}

		// Primitives
		std::vector<openblack::l3d::L3DPrimitiveHeader> primitives;
		primitives.resize(gltfMesh.primitives.size());
		for (size_t i = 0; i < gltfMesh.primitives.size(); ++i)
		{
			auto& gltfPrimitive = gltfMesh.primitives[i];
			auto& primitive = primitives[i];
			primitive.material.type = openblack::l3d::L3DMaterial::Type::Smooth;
			primitive.material.alphaCutoutThreshold = 0;
			primitive.material.skinID = std::numeric_limits<uint32_t>::max(); // TODO(#472): mesh->mMaterialIndex;
			primitive.material.color.raw = 0x00000000;
			primitive.numVertices = 0;
			primitive.verticesOffset = static_cast<uint32_t>(l3d.GetVertices().size()); // FIXME(#472): This is wrong
			primitive.numTriangles = 0;
			primitive.trianglesOffset = static_cast<uint32_t>(l3d.GetIndices().size() / 3);  // FIXME(#472): This is wrong
			primitive.numGroups = 0;                                                         // TODO(#472): Figure this out
			primitive.groupsOffset = static_cast<uint32_t>(l3d.GetLookUpTableData().size()); // FIXME(#472): This is wrong
			primitive.numVertexBlends = 0;                                                   // TODO(#472): Figure this out
			primitive.vertexBlendsOffset = static_cast<uint32_t>(l3d.GetBlends().size());    // FIXME(#472): This is wrong

			struct attribute_t
			{
				std::string name;
				std::vector<float> values;
				uint8_t type;
			};
			std::array<attribute_t, 3> attributes = {{
			    {"POSITION", {}, 0},
			    {"TEXCOORD_0", {}, 0},
			    {"NORMAL", {}, 0},
			}};
			uint32_t count = 0;
			for (auto& attribute : attributes)
			{
				auto iter = gltfPrimitive.attributes.find(attribute.name);
				if (iter != gltfPrimitive.attributes.end())
				{
					auto& accessor = gltf.accessors[iter->second];
					auto& view = gltf.bufferViews[accessor.bufferView];
					auto& buffer = gltf.buffers[view.buffer];
					auto offset = view.byteOffset + accessor.byteOffset;
					attribute.values.resize(accessor.count * accessor.type);

					if (!copyBufferView(attribute.values.data(), buffer.data.data() + offset, attribute.values.size(),
					                    accessor.componentType))
					{
						std::cerr << "Unsupported component type.\n";
						return EXIT_FAILURE;
					}
					attribute.type = static_cast<uint8_t>(accessor.type);
					if (count != 0 && count != accessor.count)
					{
						std::cerr << "Err: Attributes do not match in length.\n";
						return EXIT_FAILURE;
					}
					count = static_cast<uint32_t>(accessor.count);
				}
			}

			if (count == 0)
			{
				std::cerr << "Err: No vertex attributes found.\n";
				return EXIT_FAILURE;
			}

			std::vector<openblack::l3d::L3DVertex> vertices;
			vertices.resize(count);
			for (uint32_t j = 0; j < count; ++j)
			{
				auto& vertex = vertices[j];
				if (attributes[0].type != 0)
				{
					vertex.position.x = attributes[0].values[j * attributes[0].type + 0];
					vertex.position.y = attributes[0].values[j * attributes[0].type + 1];
					vertex.position.z = attributes[0].values[j * attributes[0].type + 2];
				}
				if (attributes[1].type != 0)
				{
					vertex.texCoord.x = attributes[1].values[j * attributes[1].type + 0];
					vertex.texCoord.y = attributes[1].values[j * attributes[1].type + 1];
				}
				if (attributes[2].type != 0)
				{
					vertex.normal.x = attributes[2].values[j * attributes[2].type + 0];
					vertex.normal.y = attributes[2].values[j * attributes[2].type + 1];
					vertex.normal.z = attributes[2].values[j * attributes[2].type + 2];
				}
			}
			l3d.AddVertices(vertices);
			primitive.numVertices += static_cast<uint32_t>(vertices.size());

			{
				auto& accessor = gltf.accessors[gltfPrimitive.indices];
				auto& view = gltf.bufferViews[accessor.bufferView];
				auto& buffer = gltf.buffers[view.buffer];
				auto offset = view.byteOffset + accessor.byteOffset;
				std::vector<uint16_t> indices;
				indices.resize(accessor.count);
				if (!copyBufferView(indices.data(), buffer.data.data() + offset, indices.size(), accessor.componentType))
				{
					std::cerr << "Unsupported component type.\n";
					return EXIT_FAILURE;
				}
				l3d.AddIndices(indices);
				primitive.numTriangles += static_cast<uint32_t>(indices.size() / 3);
			}
		}
		l3d.AddPrimitives(primitives);
		submesh.numPrimitives += static_cast<uint32_t>(primitives.size());

		l3d.AddSubmesh(submesh);
	}

	// auto decodedFootprint = tinygltf::base64_decode(gltf.extras.Get("footprint").Get<std::string>());
	// auto footprintData = std::vector<uint8_t>(decodedFootprint.begin(), decodedFootprint.end());
	// l3d.SetFootprint(footprintData);
	auto decodedUv2 = tinygltf::base64_decode(gltf.extras.Get("uv2").Get<std::string>());
	auto uv2Data = std::vector<uint8_t>(decodedUv2.begin(), decodedUv2.end());
	l3d.SetUv2Data(uv2Data);
	auto decodedName = tinygltf::base64_decode(gltf.extras.Get("name").Get<std::string>());
	l3d.SetNameData(decodedName);

	const auto result = l3d.Write(args.outFilename);
	if (result != openblack::l3d::L3DResult::Success)
	{
		std::cerr << openblack::l3d::ResultToStr(result) << '\n';
	}

	return EXIT_SUCCESS;
}

/// The names the morph targets of a creature take, in the order of the variant meshes in its .cbn file
constexpr std::array<std::string_view, 6> k_CreatureTargetNames = {"Evil", "Good", "Thin", "Fat", "Weak", "Strong"};

int ExtractFile(const Arguments::Extract& args) noexcept
{
	if (args.gltfFile.empty())
	{
		return EXIT_FAILURE;
	}

	openblack::l3dtool::GltfExportOptions options;
	options.mesh = args.inFilename;
	options.rightHanded = args.rightHanded;
	for (const auto& target : args.targets)
	{
		const auto split = target.find('=');
		if (split == std::string::npos)
		{
			std::cerr << "Err: a target is NAME=FILE, not " << target << '\n';
			return EXIT_FAILURE;
		}
		options.targets.push_back({.name = target.substr(0, split), .mesh = target.substr(split + 1)});
	}

	openblack::morph::MorphFile morph;
	if (!args.creature.empty())
	{
		openblack::pack::PackFile pack;
		if (const auto result = pack.Open(args.creature);
		    result != openblack::pack::PackResult::Success || !pack.HasBlock("Creature"))
		{
			std::cerr << "Err: no Creature block in " << args.creature.string() << '\n';
			return EXIT_FAILURE;
		}
		const auto specDirectory = args.specDirectory.empty() ? args.creature.parent_path().parent_path() : args.specDirectory;
		if (const auto result = morph.Open(pack.GetBlock("Creature"), specDirectory);
		    result != openblack::morph::MorphResult::Success)
		{
			std::cerr << "Err: " << openblack::morph::ResultToStr(result) << '\n';
			return EXIT_FAILURE;
		}
		const auto meshDirectory =
		    args.meshDirectory.empty() ? args.creature.parent_path().parent_path() / "CreatureMesh" : args.meshDirectory;
		const auto meshPath = [](const std::array<char, 0x20>& field) {
			const std::string_view raw(field.data(), field.size());
			return std::string(raw.substr(0, raw.find(char {})));
		};
		const auto& header = morph.GetHeader();
		if (options.mesh.empty())
		{
			options.mesh = meshDirectory / (meshPath(header.baseMeshName) + ".l3d");
		}
		for (size_t i = 0; i < header.variantMeshNames.size() && i < k_CreatureTargetNames.size(); ++i)
		{
			const auto variant = meshPath(header.variantMeshNames.at(i));
			if (!variant.empty())
			{
				options.targets.push_back(
				    {.name = std::string(k_CreatureTargetNames.at(i)), .mesh = meshDirectory / (variant + ".l3d")});
			}
		}
		if (args.animations)
		{
			options.animations = &morph;
		}
		options.notes.emplace_back("creatureFile", args.creature.filename().string());
	}

	if (options.mesh.empty())
	{
		std::cerr << "Err: no input mesh\n";
		return EXIT_FAILURE;
	}
	return openblack::l3dtool::ExportGltf(options, args.gltfFile);
}

bool parseOptions(int argc, char** argv, Arguments& args, int& returnCode) noexcept
{
	cxxopts::Options options("l3dtool", "Inspect and extract files from LionHead L3D files.");

	options.add_options()                                            //
	    ("h,help", "Display this help message.")                     //
	    ("subcommand", "Subcommand.", cxxopts::value<std::string>()) //
	    ;
	options.positional_help("[read|write|extract] [OPTION...]");
	options.add_options("read")                                                                                       //
	    ("H,header", "Print Header Contents.", cxxopts::value<std::vector<std::filesystem::path>>())                  //
	    ("m,mesh-header", "Print Mesh Headers.", cxxopts::value<std::vector<std::filesystem::path>>())                //
	    ("s,skins", "Print Skins.", cxxopts::value<std::vector<std::filesystem::path>>())                             //
	    ("p,extra-points", "Print Extra Points.", cxxopts::value<std::vector<std::filesystem::path>>())               //
	    ("P,primitive-header", "Print Primitive Headers.", cxxopts::value<std::vector<std::filesystem::path>>())      //
	    ("b,bones", "Print Bones.", cxxopts::value<std::vector<std::filesystem::path>>())                             //
	    ("V,vertices", "Print Vertices.", cxxopts::value<std::vector<std::filesystem::path>>())                       //
	    ("I,indices", "Print Indices.", cxxopts::value<std::vector<std::filesystem::path>>())                         //
	    ("L,look-up-tables", "Print Look Up Table Data.", cxxopts::value<std::vector<std::filesystem::path>>())       //
	    ("B,vertex-blend-values", "Print Vertex Blend Values.", cxxopts::value<std::vector<std::filesystem::path>>()) //
	    ("u,uv2-data", "Print UV2 Data.", cxxopts::value<std::vector<std::string>>())                                 //
	    ("f,footprint-data", "Print Footprint Data.", cxxopts::value<std::vector<std::string>>())                     //
	    ("n,name-data", "Print Name Data.", cxxopts::value<std::vector<std::string>>())                               //
	    ("extra-metrics", "Print Extra Metrics.", cxxopts::value<std::vector<std::string>>())                         //
	    ;
	options.add_options("write/extract from and to glTF format")                            //
	    ("o,output", "Output file (required).", cxxopts::value<std::filesystem::path>())    //
	    ("i,input-mesh", "Input file (required).", cxxopts::value<std::filesystem::path>()) //
	    ;
	options.add_options("extract")                                                                                    //
	    ("t,target", "Add a mesh as a morph target, NAME=FILE (repeat).", cxxopts::value<std::vector<std::string>>()) //
	    ("c,creature", "A creature's .cbn file: export its base mesh with its variants as morph targets.",
	     cxxopts::value<std::filesystem::path>()) //
	    ("spec-directory", "Directory of the creature spec files (default: two folders up from the .cbn file).",
	     cxxopts::value<std::filesystem::path>()) //
	    ("mesh-directory", "Directory of the creature meshes (default: CreatureMesh next to the CTR folder).",
	     cxxopts::value<std::filesystem::path>())                                                                        //
	    ("a,animations", "Add the creature's animations.", cxxopts::value<bool>()->default_value("false"))               //
	    ("right-handed", "Flip the z axis to glTF's right-handed axes.", cxxopts::value<bool>()->default_value("false")) //
	    ;

	options.parse_positional({"subcommand"});

	auto result = options.parse(argc, argv);
	if (result["help"].as<bool>())
	{
		std::cout << options.help() << '\n';
		returnCode = EXIT_SUCCESS;
		return false;
	}
	if (result["subcommand"].count() == 0)
	{
		std::cerr << options.help() << '\n';
		returnCode = EXIT_FAILURE;
		return false;
	}
	if (result["subcommand"].as<std::string>() == "read")
	{
		if (result["header"].count() > 0)
		{
			args.mode = Arguments::Mode::Header;
			args.read.filenames = result["header"].as<std::vector<std::filesystem::path>>();
			return true;
		}
		if (result["mesh-header"].count() > 0)
		{
			args.mode = Arguments::Mode::MeshHeader;
			args.read.filenames = result["mesh-header"].as<std::vector<std::filesystem::path>>();
			return true;
		}
		if (result["skins"].count() > 0)
		{
			args.mode = Arguments::Mode::Skin;
			args.read.filenames = result["skins"].as<std::vector<std::filesystem::path>>();
			return true;
		}
		if (result["extra-points"].count() > 0)
		{
			args.mode = Arguments::Mode::ExtraPoint;
			args.read.filenames = result["extra-points"].as<std::vector<std::filesystem::path>>();
			return true;
		}
		if (result["primitive-header"].count() > 0)
		{
			args.mode = Arguments::Mode::PrimitiveHeader;
			args.read.filenames = result["primitive-header"].as<std::vector<std::filesystem::path>>();
			return true;
		}
		if (result["bones"].count() > 0)
		{
			args.mode = Arguments::Mode::Bones;
			args.read.filenames = result["bones"].as<std::vector<std::filesystem::path>>();
			return true;
		}
		if (result["vertices"].count() > 0)
		{
			args.mode = Arguments::Mode::Vertices;
			args.read.filenames = result["vertices"].as<std::vector<std::filesystem::path>>();
			return true;
		}
		if (result["indices"].count() > 0)
		{
			args.mode = Arguments::Mode::Indices;
			args.read.filenames = result["indices"].as<std::vector<std::filesystem::path>>();
			return true;
		}
		if (result["look-up-tables"].count() > 0)
		{
			args.mode = Arguments::Mode::LookUpTables;
			args.read.filenames = result["look-up-tables"].as<std::vector<std::filesystem::path>>();
			return true;
		}
		if (result["vertex-blend-values"].count() > 0)
		{
			args.mode = Arguments::Mode::VertexBlendValues;
			args.read.filenames = result["vertex-blend-values"].as<std::vector<std::filesystem::path>>();
			return true;
		}
		if (result["footprint-data"].count() > 0)
		{
			args.mode = Arguments::Mode::Footprint;
			args.read.filenames = result["footprint-data"].as<std::vector<std::filesystem::path>>();
			return true;
		}
		if (result["uv2-data"].count() > 0)
		{
			args.mode = Arguments::Mode::Uv2;
			args.read.filenames = result["uv2-data"].as<std::vector<std::filesystem::path>>();
			return true;
		}
		if (result["name-data"].count() > 0)
		{
			args.mode = Arguments::Mode::Name;
			args.read.filenames = result["name-data"].as<std::vector<std::filesystem::path>>();
			return true;
		}
		if (result["extra-metrics"].count() > 0)
		{
			args.mode = Arguments::Mode::ExtraMetrics;
			args.read.filenames = result["extra-metrics"].as<std::vector<std::filesystem::path>>();
			return true;
		}
	}
	else if (result["subcommand"].as<std::string>() == "write")
	{
		args.write.outFilename = "";
		if (result["output"].count() > 0)
		{
			args.mode = Arguments::Mode::Write;
			args.write.outFilename = result["output"].as<std::filesystem::path>();
			if (result["input-mesh"].count() > 0)
			{
				args.write.gltfFile = result["input-mesh"].as<std::filesystem::path>();
			}
			return true;
		}
	}
	else if (result["subcommand"].as<std::string>() == "extract")
	{
		if (result["output"].count() > 0 && (result["input-mesh"].count() > 0 || result["creature"].count() > 0))
		{
			args.mode = Arguments::Mode::Extract;
			args.extract.gltfFile = result["output"].as<std::filesystem::path>();
			if (result["input-mesh"].count() > 0)
			{
				args.extract.inFilename = result["input-mesh"].as<std::filesystem::path>();
			}
			if (result["target"].count() > 0)
			{
				args.extract.targets = result["target"].as<std::vector<std::string>>();
			}
			if (result["creature"].count() > 0)
			{
				args.extract.creature = result["creature"].as<std::filesystem::path>();
			}
			if (result["spec-directory"].count() > 0)
			{
				args.extract.specDirectory = result["spec-directory"].as<std::filesystem::path>();
			}
			if (result["mesh-directory"].count() > 0)
			{
				args.extract.meshDirectory = result["mesh-directory"].as<std::filesystem::path>();
			}
			args.extract.animations = result["animations"].as<bool>();
			args.extract.rightHanded = result["right-handed"].as<bool>();
			return true;
		}
	}

	std::cerr << options.help() << '\n';
	returnCode = EXIT_FAILURE;
	return false;
}

int main(int argc, char* argv[]) noexcept
{
	Arguments args;
	int returnCode = EXIT_SUCCESS;
	if (!parseOptions(argc, argv, args, returnCode))
	{
		return returnCode;
	}

	if (args.mode == Arguments::Mode::Write)
	{
		return WriteFile(args.write);
	}

	if (args.mode == Arguments::Mode::Extract)
	{
		return ExtractFile(args.extract);
	}

	for (auto& filename : args.read.filenames)
	{
		openblack::l3d::L3DFile l3d;

		// Open file
		const auto result = l3d.Open(filename);
		if (result != openblack::l3d::L3DResult::Success)
		{
			std::cerr << openblack::l3d::ResultToStr(result) << "\n";
			returnCode |= EXIT_FAILURE;
			break;
		}

		switch (args.mode)
		{
		case Arguments::Mode::Header:
			std::printf("file: %s\n", filename.generic_string().c_str());
			returnCode |= PrintHeader(l3d);
			break;
		case Arguments::Mode::MeshHeader:
			std::printf("file: %s\n", filename.generic_string().c_str());
			returnCode |= PrintMeshHeaders(l3d);
			break;
		case Arguments::Mode::Skin:
			std::printf("file: %s\n", filename.generic_string().c_str());
			returnCode |= PrintSkins(l3d);
			break;
		case Arguments::Mode::ExtraPoint:
			std::printf("file: %s\n", filename.generic_string().c_str());
			returnCode |= PrintExtraPoints(l3d);
			break;
		case Arguments::Mode::PrimitiveHeader:
			std::printf("file: %s\n", filename.generic_string().c_str());
			returnCode |= PrintPrimitiveHeaders(l3d);
			break;
		case Arguments::Mode::Bones:
			std::printf("file: %s\n", filename.generic_string().c_str());
			returnCode |= PrintBones(l3d);
			break;
		case Arguments::Mode::Vertices:
			std::printf("file: %s\n", filename.generic_string().c_str());
			returnCode |= PrintVertices(l3d);
			break;
		case Arguments::Mode::Indices:
			std::printf("file: %s\n", filename.generic_string().c_str());
			returnCode |= PrintIndices(l3d);
			break;
		case Arguments::Mode::LookUpTables:
			std::printf("file: %s\n", filename.generic_string().c_str());
			returnCode |= PrintLookUpTables(l3d);
			break;
		case Arguments::Mode::VertexBlendValues:
			std::printf("file: %s\n", filename.generic_string().c_str());
			returnCode |= PrintBlendValues(l3d);
			break;
		case Arguments::Mode::Footprint:
			std::printf("file: %s\n", filename.generic_string().c_str());
			returnCode |= PrintFootprintValues(l3d);
			break;
		case Arguments::Mode::Uv2:
			std::printf("file: %s\n", filename.generic_string().c_str());
			returnCode |= PrintUv2Values(l3d);
			break;
		case Arguments::Mode::Name:
			std::printf("file: %s\n", filename.generic_string().c_str());
			returnCode |= PrintNameValue(l3d);
			break;
		case Arguments::Mode::ExtraMetrics:
			std::printf("file: %s\n", filename.generic_string().c_str());
			returnCode |= PrintExtraMetricsValues(l3d);
			break;
		default:
			returnCode = EXIT_FAILURE;
			break;
		}
	}

	return returnCode;
}
