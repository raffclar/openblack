/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Direct3D binds an instanced draw's instance data by semantic alone: bgfx gives the instance's columns i_data0 to
// i_data4 the texture coordinates 7 down to 3, whatever the shader declares, while Vulkan binds them by their place
// after the vertex attributes. An instance column declared at any other semantic reads the vertex buffer on Direct3D
// (meshes blended by their instance's alpha were drawn opaque), and a vertex attribute at those texture coordinates is
// replaced by the instance data.

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/ShaderSamplers.h"

#if defined(_WIN32)
// The instanced shaders as the game embeds them, compiled for Direct3D
#include "generated/shaders/dx11/vs_footprint_instanced.sc.bin.h"
#include "generated/shaders/dx11/vs_object_hm_instanced.sc.bin.h"
#include "generated/shaders/dx11/vs_object_instanced.sc.bin.h"
#include "generated/shaders/dx11/vs_object_lightmap_instanced.sc.bin.h"
#include "generated/shaders/dx11/vs_object_morph_instanced.sc.bin.h"
#include "generated/shaders/dx11/vs_object_palette_instanced.sc.bin.h"
#include "generated/shaders/dx11/vs_object_static_instanced.sc.bin.h"
#include "generated/shaders/dx11/vs_particle_instanced.sc.bin.h"
#include "generated/shaders/dx11/vs_vegetation_hm_instanced.sc.bin.h"
#endif // defined(_WIN32)

namespace
{

/// The source folder, from ctest's environment, else as it was built: a compiler cache shared between worktrees can
/// hand this test an object built in another worktree
std::filesystem::path SourceDir()
{
	if (const auto* dir = std::getenv("OPENBLACK_SOURCE_DIR"); dir != nullptr && *dir != '\0')
	{
		return dir;
	}
	return OPENBLACK_SOURCE_DIR;
}

std::filesystem::path ShaderDir()
{
	return SourceDir() / "assets" / "shaders";
}

std::vector<std::string> Lines(const std::filesystem::path& path)
{
	std::ifstream file(path);
	std::vector<std::string> lines;
	for (std::string line; std::getline(file, line);)
	{
		lines.push_back(line);
	}
	return lines;
}

std::vector<std::string> Words(std::string text)
{
	std::ranges::replace(text, ',', ' ');
	std::istringstream stream(text);
	std::vector<std::string> words;
	for (std::string word; stream >> word;)
	{
		words.push_back(word);
	}
	return words;
}

/// Every input's semantic in the varying definitions, by name: "vec4 i_data0 : TEXCOORD7;" gives i_data0 TEXCOORD7
std::map<std::string, std::string> Semantics()
{
	std::map<std::string, std::string> semantics;
	for (const auto& line : Lines(ShaderDir() / "varying.def.sc"))
	{
		const auto code = line.substr(0, line.find("//"));
		const auto colon = code.find(':');
		if (colon == std::string::npos)
		{
			continue;
		}
		const auto before = Words(code.substr(0, colon));
		const auto after = Words(code.substr(colon + 1));
		if (before.size() < 2 || after.empty())
		{
			continue;
		}
		auto semantic = after.front();
		semantic.erase(std::ranges::remove(semantic, ';').begin(), semantic.end());
		semantics[before.back()] = semantic;
	}
	return semantics;
}

/// Each "$input" line of every vertex shader, with the file it is in
std::vector<std::pair<std::string, std::vector<std::string>>> VertexInputs()
{
	std::vector<std::pair<std::string, std::vector<std::string>>> inputs;
	for (const auto& entry : std::filesystem::directory_iterator(ShaderDir()))
	{
		const auto name = entry.path().filename().string();
		if (!name.starts_with("vs_") || entry.path().extension() != ".sc")
		{
			continue;
		}
		for (const auto& line : Lines(entry.path()))
		{
			if (line.starts_with("$input"))
			{
				inputs.emplace_back(name, Words(line.substr(std::string("$input").size())));
			}
		}
	}
	return inputs;
}

/// The texture coordinates bgfx binds instance column 0 to 4 to on Direct3D
std::string InstanceSemantic(int column)
{
	return "TEXCOORD" + std::to_string(7 - column);
}

TEST(InstanceDataInputs, InstanceColumnsAreAtTheTextureCoordinatesDirect3DBindsThemTo)
{
	const auto semantics = Semantics();
	for (int column = 0; column < 5; ++column)
	{
		const auto name = "i_data" + std::to_string(column);
		ASSERT_TRUE(semantics.contains(name)) << name;
		EXPECT_EQ(semantics.at(name), InstanceSemantic(column)) << name;
	}
}

TEST(InstanceDataInputs, InstancedShadersTakeNoVertexAttributeWhereTheInstanceDataGoes)
{
	const auto semantics = Semantics();
	std::set<std::string> instanceSemantics;
	for (int column = 0; column < 5; ++column)
	{
		instanceSemantics.insert(InstanceSemantic(column));
	}

	const auto inputs = VertexInputs();
	ASSERT_FALSE(inputs.empty());
	for (const auto& [file, names] : inputs)
	{
		const bool instanced = std::ranges::any_of(names, [](const auto& name) { return name.starts_with("i_data"); });
		std::set<std::string> taken;
		for (const auto& name : names)
		{
			ASSERT_TRUE(semantics.contains(name)) << file << ": " << name;
			const auto& semantic = semantics.at(name);
			// Two inputs at one semantic are one input on Direct3D
			EXPECT_TRUE(taken.insert(semantic).second) << file << ": " << name << " shares " << semantic;
			if (instanced && !name.starts_with("i_data"))
			{
				EXPECT_FALSE(instanceSemantics.contains(semantic))
				    << file << ": " << name << " is at " << semantic << ", which the instance data replaces";
			}
		}
	}
}

#if defined(_WIN32)
// bgfx's ids of the vertex attributes
constexpr uint16_t k_Colour0 = 0x0005;
constexpr uint16_t k_TexCoord1 = 0x0011;
constexpr uint16_t k_TexCoord3 = 0x0013;
constexpr uint16_t k_TexCoord7 = 0x0017;

bool Takes(const std::vector<uint16_t>& attributes, uint16_t attribute)
{
	return std::ranges::find(attributes, attribute) != attributes.end();
}

TEST(InstanceDataInputs, CompiledForDirect3DEveryInstanceColumnIsInstanceData)
{
	const std::map<std::string, std::span<const uint8_t>> binaries {
	    {"vs_footprint_instanced", vs_footprint_instanced_dx11},
	    {"vs_object_hm_instanced", vs_object_hm_instanced_dx11},
	    {"vs_object_instanced", vs_object_instanced_dx11},
	    {"vs_object_lightmap_instanced", vs_object_lightmap_instanced_dx11},
	    {"vs_object_morph_instanced", vs_object_morph_instanced_dx11},
	    {"vs_object_palette_instanced", vs_object_palette_instanced_dx11},
	    {"vs_object_static_instanced", vs_object_static_instanced_dx11},
	    {"vs_particle_instanced", vs_particle_instanced_dx11},
	    {"vs_vegetation_hm_instanced", vs_vegetation_hm_instanced_dx11},
	};
	for (const auto& [name, binary] : binaries)
	{
		const auto attributes = openblack::graphics::shader_samplers::ReadVertexAttributes(binary);
		ASSERT_TRUE(attributes.has_value()) << name;
		// None of them takes a vertex colour: the fifth column read from one is the vertex buffer's first bytes
		EXPECT_FALSE(Takes(*attributes, k_Colour0)) << name;
		for (uint16_t attribute = k_TexCoord3; attribute <= k_TexCoord7; ++attribute)
		{
			EXPECT_TRUE(Takes(*attributes, attribute)) << name << " lacks instance column " << (k_TexCoord7 - attribute);
		}
	}
}

TEST(InstanceDataInputs, CompiledForDirect3DTheLightmapCoordinatesAreAVertexAttributeOfTheirOwn)
{
	const auto attributes = openblack::graphics::shader_samplers::ReadVertexAttributes(vs_object_lightmap_instanced_dx11);
	ASSERT_TRUE(attributes.has_value());
	EXPECT_TRUE(Takes(*attributes, k_TexCoord1));
}
#endif // defined(_WIN32)

} // namespace
