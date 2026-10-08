/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Resources/Loaders.h"

#include <cmath>
#include <cstddef>
#include <cstring>

#include <algorithm>
#include <array>
#include <iostream>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <utility>

#include <GLWFile.h>
#include <L3DFile.h>
#include <MorphFile.h>
#include <PackFile.h>
#include <RawImage.h>
#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "3D/Light.h"
#include "3D/SkeletalAnimation.h"
#include "Common/StringUtils.h"
#include "Common/Zip.h"
#include "ECS/SuperVillager.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/Argb4444.h"
#include "Graphics/GameFont.h"
#include "Graphics/Rgb16.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Magic/Gestures/GestureShapes.h"
#include "Magic/Gestures/GestureTemplates.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysRegistry.h"
#include "Particles/Rules/ExplodeObject.h"
#include "Particles/SoundAction.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::filesystem;
using namespace openblack::resources;

namespace
{
/// The .raw colour files the game loads without an alpha channel and stores at 16 bits, 5 bits per colour channel
/// (each byte is rgb16::Cut5). Only a colour file of exactly 256 x 256 x 3 bytes takes that path. Of the game's
/// textures only Sun.raw is one; the saved games' pictures are the others, which openblack does not load. Details in
/// docs/bw1-notes/source-notes/Resources.md.
constexpr auto k_Rgb555Stems = std::to_array<std::string_view>({
    "sun", // Data\Textures\Sun.raw
});

bool IsRgb555Stem(std::string_view lowerStem)
{
	return std::ranges::find(k_Rgb555Stems, lowerStem) != k_Rgb555Stems.end();
}

/// The bytes of a zipped .zzz file: its size unzipped, then the zipped data
std::vector<uint8_t> ReadZipped(const std::filesystem::path& path)
{
	auto stream = Locator::filesystem::value().Open(path, Stream::Mode::Read);
	uint32_t decompressedSize = 0;
	stream->Read(&decompressedSize);
	auto buffer = std::vector<uint8_t>(stream->Size() - sizeof(decompressedSize));
	stream->Read(buffer.data(), buffer.size());
	return zip::Inflate(buffer, decompressedSize);
}
} // namespace

L3DLoader::result_type L3DLoader::operator()(FromBufferTag, const std::string& debugName,
                                             const std::vector<uint8_t>& data) const
{
	auto mesh = std::make_shared<graphics::L3DMesh>(debugName);
	if (!mesh->LoadFromBuffer(data))
	{
		throw std::runtime_error("Unable to load mesh");
	}

	return mesh;
}

L3DLoader::result_type L3DLoader::operator()(FromDynamicFileTag, const std::string& debugName, const l3d::L3DFile& file) const
{
	auto mesh = std::make_shared<graphics::L3DMesh>(debugName, true);
	if (!mesh->Load(file))
	{
		throw std::runtime_error("Unable to load mesh");
	}
	return mesh;
}

L3DLoader::result_type L3DLoader::operator()(FromDiskTag, const std::filesystem::path& path) const
{
	auto mesh = std::make_shared<graphics::L3DMesh>(path.stem().string());
	auto pathExt = string_utils::LowerCase(path.extension().string());

	if (pathExt == ".l3d")
	{
		if (!mesh->LoadFromFilesystem(path))
		{
			throw std::runtime_error("Unable to load mesh");
		}
	}
	else if (pathExt == ".zzz")
	{
		if (!mesh->LoadFromBuffer(ReadZipped(path)))
		{
			throw std::runtime_error("Unable to load decompressed mesh");
		}
	}

	return mesh;
}

L3DFileLoader::result_type L3DFileLoader::operator()(FromDiskTag, const std::filesystem::path& path) const
{
	auto file = std::make_shared<l3d::L3DFile>();
	const auto result = string_utils::LowerCase(path.extension().string()) == ".zzz"
	                        ? file->Open(ReadZipped(path))
	                        : file->Open(Locator::filesystem::value().ReadAll(path));
	if (result != l3d::L3DResult::Success)
	{
		throw std::runtime_error("Unable to read L3D file: " + std::string(l3d::ResultToStr(result)));
	}
	return file;
}

Texture2DLoader::result_type Texture2DLoader::operator()(FromPackTag, const std::string& name,
                                                         const pack::G3DTexture& g3dTexture) const
{
	// some assumptions:
	// - no mipmaps
	// - no cubemap or volume textures
	// - always dxt1 or dxt3
	// - all are compressed
	auto texture2D = std::make_shared<graphics::Texture2D>(name);
	graphics::TextureFormat internalFormat;
	if (g3dTexture.ddsHeader.format.fourCC.data() == std::string("DXT1"))
	{
		internalFormat = graphics::TextureFormat::BlockCompression1;
	}
	else if (g3dTexture.ddsHeader.format.fourCC.data() == std::string("DXT3"))
	{
		internalFormat = graphics::TextureFormat::BlockCompression2;
	}
	else if (g3dTexture.ddsHeader.format.fourCC.data() == std::string("DXT5"))
	{
		internalFormat = graphics::TextureFormat::BlockCompression3;
	}
	else
	{
		throw std::runtime_error("Unsupported compressed texture format");
	}

	texture2D->Create(static_cast<uint16_t>(g3dTexture.ddsHeader.width), static_cast<uint16_t>(g3dTexture.ddsHeader.height), 1,
	                  internalFormat, graphics::Wrapping::Repeat, graphics::Filter::Linear,
	                  bgfx::copy(g3dTexture.ddsData.data(), static_cast<uint32_t>(g3dTexture.ddsData.size())));
	return texture2D;
}

Texture2DLoader::result_type Texture2DLoader::operator()(FromDiskTag, const std::filesystem::path& rawTexturePath) const
{
	bool found = false;
	const std::array<uint16_t, 12> resolutions = {{1024, 512, 256, 128, 64, 40, 32, 14, 12, 6}};

	const auto data = Locator::filesystem::value().ReadAll(rawTexturePath);
	graphics::TextureFormat format = graphics::TextureFormat::R8;
	uint16_t width = 0;
	uint16_t height = 0;
	for (auto res : resolutions)
	{
		if (found)
		{
			break;
		}

		width = res;
		height = res;

		const typename decltype(data)::size_type pixelCount = width * height;

		if (data.size() == pixelCount)
		{
			format = graphics::TextureFormat::R8;
			found = true;
		}
		if (data.size() == 3 * pixelCount)
		{
			format = graphics::TextureFormat::RGB8;
			found = true;
		}
	}
	if (!found)
	{
		throw std::runtime_error("Unable to load texture: Ambiguous size and format: " + std::to_string(data.size()));
	}

	auto texture = std::make_shared<graphics::Texture2D>(("raw" / rawTexturePath.stem()).string());
	const auto stem = string_utils::LowerCase(rawTexturePath.stem().string());
	// The game cuts every texture with an alpha channel (and human_shadow) to ARGB4444 when it loads it, and the GPU
	// filters the cut texels (Graphics/Argb4444.h). It only takes a 256 x 256 x 3 byte colour file and reads up to
	// 64 KiB of its alpha; openblack cuts an alpha file only when it has exactly 64 KiB (approximate). An alpha file
	// is cut only with a valid colour file beside it (argb4444::ColourOfAlpha), so S_IceEnvMapGreya.raw stays 8-bit.
	const auto validColourOfAlpha = [&]() {
		const auto fileStem = rawTexturePath.stem().string();
		const auto colour = graphics::argb4444::ColourOfAlpha(fileStem);
		if (colour.empty())
		{
			return false;
		}
		auto& fileSystem = Locator::filesystem::value();
		const auto colourPath = rawTexturePath.parent_path() / (std::string(colour) + ".raw");
		return fileSystem.Exists(colourPath) &&
		       fileSystem.Open(colourPath, filesystem::Stream::Mode::Read)->Size() == graphics::argb4444::k_ColourBytes;
	};
	const bool alphaFlagSize =
	    format == graphics::TextureFormat::RGB8
	        ? data.size() == graphics::argb4444::k_ColourBytes && graphics::argb4444::IsAlphaFlagColour(stem)
	        : data.size() == graphics::argb4444::k_AlphaBytes && validColourOfAlpha();
	const bool cut = alphaFlagSize || stem == graphics::argb4444::k_HumanShadowStem;
	// the 16-bit colour path: each of R, G, B cut to 5 bits and back, as the GPU samples it
	if (!cut && format == graphics::TextureFormat::RGB8 && data.size() == graphics::argb4444::k_ColourBytes &&
	    IsRgb555Stem(stem))
	{
		std::vector<uint8_t> cut555(data.size());
		std::ranges::transform(data, cut555.begin(), graphics::rgb16::Cut5);
		texture->Create(width, height, 1, format, graphics::Wrapping::Repeat, graphics::Filter::Linear,
		                bgfx::copy(cut555.data(), static_cast<uint32_t>(cut555.size())));
		return texture;
	}
	if (cut)
	{
		// x.raw and xa.raw stay two textures here (the game packs them into one): each byte cut on its own
		std::vector<uint8_t> nibbles(data.size());
		std::ranges::transform(data, nibbles.begin(), graphics::argb4444::Cut);
		texture->Create(width, height, 1, format, graphics::Wrapping::Repeat, graphics::Filter::Linear,
		                bgfx::copy(nibbles.data(), static_cast<uint32_t>(nibbles.size())));
		return texture;
	}
	texture->Create(width, height, 1, format, graphics::Wrapping::Repeat, graphics::Filter::Linear,
	                bgfx::copy(data.data(), static_cast<uint32_t>(data.size())));

	return texture;
}

L3DAnimLoader::result_type L3DAnimLoader::operator()(FromBufferTag, const std::vector<uint8_t>& data) const
{
	auto animation = std::make_shared<L3DAnim>();
	animation->LoadFromBuffer(data);
	return animation;
}

L3DAnimLoader::result_type L3DAnimLoader::operator()(FromDiskTag, const std::filesystem::path& path) const
{
	auto animation = std::make_shared<L3DAnim>();

	if (!animation->LoadFromFilesystem(path))
	{
		throw std::runtime_error("Unable to load animation");
	}

	return animation;
}

LevelLoader::result_type LevelLoader::operator()(FromDiskTag, const std::filesystem::path& path, Level::LandType landType) const
{
	return std::make_shared<Level>(Level::ParseLevel(path, landType));
}

CreatureMindLoader::result_type CreatureMindLoader::operator()(FromDiskTag, const std::filesystem::path& creatureMindPath) const
{
	// Never throws: a mind that cannot be read leaves the creature with a fresh mind of its species
	auto mind = std::make_shared<creature::CreatureMind>();
	auto& fileSystem = Locator::filesystem::value();
	if (fileSystem.Exists(creatureMindPath))
	{
		try
		{
			mind->result = creaturemind::Read(fileSystem.ReadAll(creatureMindPath), mind->data);
		}
		catch (const std::exception&)
		{
			mind->result = creaturemind::MindResult::ErrCantOpen;
		}
	}
	if (!mind->Loaded())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Creature mind {}: {}", creatureMindPath.generic_string(),
		                   creaturemind::ResultToStr(mind->result));
	}
	return mind;
}

namespace
{
using CreatureRig = creature::CreatureRig;

/// A triangle of a mesh's first submesh, as the creature file names it: its primitive and the vertices and vertex
/// groups counted in it
struct TriangleRef
{
	std::array<const l3d::L3DVertex*, 3> vertices;
	std::array<uint32_t, 3> bones;
	uint32_t skinId;
};

std::optional<TriangleRef> FindTriangle(const l3d::L3DFile& file, const morph::MeshIntersect& intersect)
{
	if (file.GetSubmeshHeaders().empty())
	{
		return std::nullopt;
	}
	const auto primitives = file.GetPrimitiveSpan(0);
	if (intersect.primitive >= primitives.size())
	{
		return std::nullopt;
	}
	uint32_t vertexOffset = 0;
	uint32_t groupOffset = 0;
	for (uint32_t i = 0; i < intersect.primitive; ++i)
	{
		vertexOffset += primitives[i].numVertices;
		groupOffset += primitives[i].numGroups;
	}
	const auto& primitive = primitives[intersect.primitive];
	const auto vertices = file.GetVertexSpan(0);
	const auto groups = file.GetVertexGroupSpan(0);
	TriangleRef triangle {.vertices = {}, .bones = {}, .skinId = primitive.material.skinID};
	for (size_t i = 0; i < 3; ++i)
	{
		const auto vertex = intersect.vertices.at(i);
		const auto group = intersect.vertexGroups.at(i);
		if (vertex >= primitive.numVertices || vertexOffset + vertex >= vertices.size())
		{
			return std::nullopt;
		}
		triangle.vertices.at(i) = &vertices[vertexOffset + vertex];
		// Meshes without bones place every vertex with the first
		triangle.bones.at(i) =
		    group < primitive.numGroups && groupOffset + group < groups.size() ? groups[groupOffset + group].boneIndex : 0;
	}
	return triangle;
}

/// The colour of a skin of the mesh at a texture coordinate, as the game reads it for the eyelids
glm::vec3 SkinColourAt(const l3d::L3DFile& file, uint32_t skinId, glm::vec2 uv)
{
	const auto skin = std::ranges::find(file.GetSkins(), skinId, &l3d::L3DTexture::id);
	if (skin == file.GetSkins().end())
	{
		return glm::vec3(1.0f);
	}
	constexpr auto k_Width = static_cast<int>(l3d::L3DTexture::k_Width);
	constexpr auto k_Height = static_cast<int>(l3d::L3DTexture::k_Height);
	const auto x = static_cast<int>(std::floor(uv.x * k_Width)) & (k_Width - 1);
	const auto y = static_cast<int>(std::floor(uv.y * k_Height)) & (k_Height - 1);
	const auto& texel = skin->texels.at(static_cast<size_t>((y * k_Width) + x));
	constexpr float k_Max = 15.0f;
	return {static_cast<float>(texel.r) / k_Max, static_cast<float>(texel.g) / k_Max, static_cast<float>(texel.b) / k_Max};
}

std::optional<CreatureRig::Eyes> LoadEyes(const morph::CreatureEyes& eyes,
                                          const std::array<std::optional<l3d::L3DFile>, CreatureRig::k_MeshCount>& meshes)
{
	const auto& base = meshes.front();
	if (!base.has_value())
	{
		return std::nullopt;
	}
	CreatureRig::Eyes result {
	    .scale = eyes.scale,
	    .points = {},
	    .lidAngles = {.open = eyes.lidAngles[0][1], .closed = eyes.lidAngles[1][1], .calm = eyes.lidAngles[2][1]},
	};
	for (size_t p = 0; p < eyes.points.size(); ++p)
	{
		const auto& source = eyes.points.at(p);
		auto& point = result.points.at(p);
		point = {.enabled = false,
		         .depth = source.depth,
		         .vertices = {},
		         .bones = {},
		         .u = source.intersect.u,
		         .v = source.intersect.v,
		         .skinColour = glm::vec3(1.0f)};
		if (!source.enabled)
		{
			continue;
		}
		const auto baseTriangle = FindTriangle(*base, source.intersect);
		if (!baseTriangle)
		{
			continue;
		}
		point.enabled = true;
		point.bones = baseTriangle->bones;
		for (size_t m = 0; m < meshes.size(); ++m)
		{
			// Variants share the base's vertices and their order; a missing one is the base
			const auto triangle = meshes.at(m).has_value() ? FindTriangle(*meshes.at(m), source.intersect) : std::nullopt;
			const auto& from = triangle ? *triangle : *baseTriangle;
			for (size_t i = 0; i < 3; ++i)
			{
				const auto& position = from.vertices.at(i)->position;
				point.vertices.at(m).at(i) = glm::vec3(position.x, position.y, position.z);
			}
		}
		const auto uvOf = [&baseTriangle](size_t i) {
			const auto& coordinate = baseTriangle->vertices.at(i)->texCoord;
			return glm::vec2(coordinate.x, coordinate.y);
		};
		const auto uv = uvOf(0) + ((uvOf(1) - uvOf(0)) * point.u) + ((uvOf(2) - uvOf(0)) * point.v);
		point.skinColour = SkinColourAt(*base, baseTriangle->skinId, uv);
	}
	return result;
}
/// A triangle's vertices in every mesh, in the space of the bone that moves each, and those bones. Variants share the
/// base's vertices and their order; a missing one is the base.
struct MeshTriangle
{
	std::array<std::array<glm::vec3, 3>, CreatureRig::k_MeshCount> vertices;
	std::array<uint32_t, 3> bones;
};

std::optional<MeshTriangle> TriangleInMeshes(const morph::MeshIntersect& intersect,
                                             const std::array<std::optional<l3d::L3DFile>, CreatureRig::k_MeshCount>& meshes)
{
	const auto& base = meshes.front();
	const auto baseTriangle = base.has_value() ? FindTriangle(*base, intersect) : std::nullopt;
	if (!baseTriangle)
	{
		return std::nullopt;
	}
	MeshTriangle result {.vertices = {}, .bones = baseTriangle->bones};
	for (size_t m = 0; m < meshes.size(); ++m)
	{
		const auto triangle = meshes.at(m).has_value() ? FindTriangle(*meshes.at(m), intersect) : std::nullopt;
		const auto& from = triangle ? *triangle : *baseTriangle;
		for (size_t i = 0; i < 3; ++i)
		{
			const auto& position = from.vertices.at(i)->position;
			result.vertices.at(m).at(i) = glm::vec3(position.x, position.y, position.z);
		}
	}
	return result;
}

std::vector<CreatureRig::HairGroup> LoadHair(std::span<const morph::HairGroup> groups,
                                             const std::array<std::optional<l3d::L3DFile>, CreatureRig::k_MeshCount>& meshes)
{
	std::vector<CreatureRig::HairGroup> result;
	for (const auto& group : groups)
	{
		// A group without segments has no strands to draw
		if (group.header.segmentCount == 0 || group.hairs.empty())
		{
			continue;
		}
		auto& hair = result.emplace_back();
		hair.segmentCount = group.header.segmentCount;
		hair.textured = group.header.mappingIndex == 1;
		for (size_t v = 0; v < hair.looks.size(); ++v)
		{
			const auto& variant = group.header.variants.at(v);
			hair.looks.at(v) = {
			    .colour = {variant.red, variant.green, variant.blue},
			    .length = variant.length,
			    .damping = variant.damping,
			    .stiffness = variant.stiffness,
			    .thickness = variant.thickness,
			};
		}
		for (const auto& source : group.hairs)
		{
			const auto triangle = TriangleInMeshes(source.intersection, meshes);
			if (!triangle)
			{
				continue;
			}
			CreatureRig::HairStrand strand {
			    .turned = (source.flags & 1u) != 0,
			    .angles = {},
			    .vertices = triangle->vertices,
			    .bones = triangle->bones,
			    .u = source.intersection.u,
			    .v = source.intersection.v,
			};
			for (size_t v = 0; v < strand.angles.size(); ++v)
			{
				const auto& angles = source.angles.at(v);
				strand.angles.at(v) = glm::vec3(angles[0], angles[1], angles[2]);
			}
			hair.strands.push_back(strand);
		}
		if (hair.strands.empty())
		{
			result.pop_back();
		}
	}
	return result;
}
} // namespace

CreatureRigLoader::result_type CreatureRigLoader::operator()(FromBufferTag, const std::vector<uint8_t>& block,
                                                             const std::filesystem::path& specDirectory,
                                                             const std::filesystem::path& meshDirectory) const
{
	morph::MorphFile file;
	const auto result = file.Open(block, specDirectory);
	if (result != morph::MorphResult::Success)
	{
		throw std::runtime_error("Unable to read creature animations: " + std::string(morph::ResultToStr(result)));
	}

	auto rig = std::make_shared<CreatureRig>();
	const auto& header = file.GetHeader();
	rig->baseMeshName = header.baseMeshName.data();
	std::array<std::string, CreatureRig::k_MeshCount> meshNames;
	meshNames.front() = rig->baseMeshName;
	rig->hasMesh.front() = true;
	for (size_t i = 0; i < header.variantMeshNames.size(); ++i)
	{
		meshNames.at(i + 1) = header.variantMeshNames.at(i).data();
		rig->hasMesh.at(i + 1) = !meshNames.at(i + 1).empty();
	}

	size_t animationCount = 0;
	for (const auto& set : file.GetAnimationSpecs().animationSets)
	{
		animationCount += set.animations.size();
	}
	for (size_t mesh = 0; mesh < CreatureRig::k_AnimatedMeshCount; ++mesh)
	{
		auto& animations = rig->animations.at(mesh);
		animations.resize(animationCount);
		for (size_t i = 0; i < animationCount; ++i)
		{
			const auto* source =
			    mesh == 0 ? file.GetBaseAnimation(i) : file.GetVariantAnimation(static_cast<uint32_t>(mesh - 1), i);
			if (source != nullptr && !source->keyframes.empty())
			{
				animations[i] = skeletal_animation::FromMorph(*source);
			}
		}
	}

	rig->meshNames = meshNames;

	// The sounds on moments of the animations
	const auto& extraData = file.GetExtraData();
	rig->soundEvents.resize(extraData.size());
	for (size_t i = 0; i < extraData.size(); ++i)
	{
		for (const auto& data : extraData[i])
		{
			rig->soundEvents[i].push_back({
			    .kind = static_cast<creature_audio::EventKind>(data.type),
			    .timeMs = static_cast<int32_t>(data.frame),
			    .action = static_cast<audio::SoundAction>(data.action),
			    .mode = static_cast<int32_t>(data.mode),
			});
		}
	}
	rig->soundObject = static_cast<int32_t>(file.GetHairHeader().soundObject);
	rig->soundBankName = file.GetSoundBankName();
	rig->leashBone = file.GetLeashBone();

	if (const auto& sites = file.GetTattooSites(); sites.has_value())
	{
		auto& tattooSites = rig->tattooSites.emplace();
		for (size_t i = 0; i < tattooSites.size(); ++i)
		{
			const auto& site = sites->at(i);
			tattooSites.at(i) = {
			    .enabled = site.enabled,
			    .u = site.u,
			    .v = site.v,
			    .skin = site.skin,
			    .size = site.size,
			    .mirror = site.mirror,
			    .rotation = static_cast<uint8_t>(site.rotation & 3u),
			};
		}
	}

	// The bones it acts with and when its object animations take hold or let go
	if (const auto& points = file.GetCreatureActionPoints(); points.has_value())
	{
		const std::array bones {points->rightHand, points->rightFoot, points->rightArmpit,
		                        points->belly,     points->head,      points->groin};
		if (std::ranges::all_of(bones, [](int32_t bone) { return bone >= 0; }))
		{
			const auto bone = [](int32_t value) { return static_cast<uint32_t>(value); };
			const auto ms = [](int32_t value) { return static_cast<float>(std::max(value, 0)); };
			rig->actionPoints = CreatureRig::ActionPoints {
			    .rightHand = bone(points->rightHand),
			    .rightFoot = bone(points->rightFoot),
			    .rightArmpit = bone(points->rightArmpit),
			    .belly = bone(points->belly),
			    .head = bone(points->head),
			    .groin = bone(points->groin),
			    .pickUpMs = ms(points->pickUpTime),
			    .destroyMs = ms(points->destroyTime),
			    .discardMs = ms(points->discardTime),
			    .eatMs = ms(points->eatTime),
			    .throwMs = ms(points->throwTime),
			    .putDownMs = ms(points->putDownTime),
			};
		}
	}

	// The eyes and the hair sit on triangles of the meshes
	const auto& eyes = file.GetCreatureEyes();
	if (eyes.has_value() || !file.GetHairGroups().empty())
	{
		auto& fileSystem = Locator::filesystem::value();
		std::array<std::optional<l3d::L3DFile>, CreatureRig::k_MeshCount> meshes;
		for (size_t i = 0; i < meshNames.size(); ++i)
		{
			const auto path = meshDirectory / (meshNames.at(i) + ".l3d");
			if (meshNames.at(i).empty() || !fileSystem.Exists(path))
			{
				continue;
			}
			auto& mesh = meshes.at(i).emplace();
			if (mesh.Open(fileSystem.ReadAll(path)) != l3d::L3DResult::Success)
			{
				meshes.at(i).reset();
			}
		}
		if (eyes.has_value())
		{
			rig->eyes = LoadEyes(*eyes, meshes);
		}
		rig->hairGroups = LoadHair(file.GetHairGroups(), meshes);
	}
	return rig;
}

CreatureSkinArtLoader::result_type CreatureSkinArtLoader::operator()(FromDiskTag, const Paths& paths) const
{
	auto& fileSystem = Locator::filesystem::value();
	constexpr uint32_t k_Size = creature_marks::k_AtlasSize;
	const auto rgb = [&fileSystem](const std::filesystem::path& path, uint32_t width, uint32_t height) {
		auto image = rawimage::DecodeRgb(fileSystem.ReadAll(path), width, height);
		if (!image)
		{
			throw std::runtime_error("Unexpected size of " + path.string());
		}
		return std::move(image->pixels);
	};
	const auto grey = [&fileSystem](const std::filesystem::path& path, uint32_t width, uint32_t height) {
		auto image = rawimage::DecodeGrey(fileSystem.ReadAll(path), width, height);
		if (!image)
		{
			throw std::runtime_error("Unexpected size of " + path.string());
		}
		return std::move(image->pixels);
	};
	auto art = std::make_shared<creature_skin::Art>();
	const auto symbols =
	    fileSystem.Exists(paths.symbols) ? rgb(paths.symbols, k_Size, k_Size) : std::vector<std::array<uint8_t, 3>> {};
	const auto defaults = rgb(paths.defaultSymbols, k_Size, k_Size);
	for (uint32_t design = 0; design < art->designs.size(); ++design)
	{
		auto written = creature_tattoo::DesignFromAtlas(symbols, k_Size, design);
		const bool blank = std::ranges::all_of(written.front().levels, [](uint8_t level) { return level == 0; });
		art->designs.at(design) = blank ? creature_tattoo::DesignFromAtlas(defaults, k_Size, design) : std::move(written);
	}
	art->damage.fresh = {.colours = rgb(paths.freshDamage, k_Size, k_Size),
	                     .alpha = grey(paths.freshDamageAlpha, k_Size, k_Size)};
	art->damage.old = {.colours = rgb(paths.oldDamage, k_Size, k_Size), .alpha = grey(paths.oldDamageAlpha, k_Size, k_Size)};
	art->palette = rgb(paths.palette, creature_tattoo::k_PaletteColumns, creature_tattoo::k_PaletteRows);
	return art;
}

void resources::LoadCreatureRigs(ResourcesInterface& resources)
{
	auto& fileSystem = Locator::filesystem::value();
	const auto specPath = fileSystem.GetPath<Path::Data>() / "ctrspec27.txt";
	if (!fileSystem.Exists(specPath))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The creatures are not animated: {} is missing", specPath.string());
		return;
	}
	const auto specDirectory = fileSystem.FindPath(specPath).parent_path();
	const auto meshDirectory = fileSystem.GetPath<Path::CreatureMesh>();
	auto& rigs = resources.GetCreatureRigs();
	std::vector<entt::id_type> loaded;
	fileSystem.Iterate(fileSystem.GetPath<Path::Data>() / "CTR", false, [&](const std::filesystem::path& path) {
		if (string_utils::LowerCase(path.extension().string()) != ".cbn")
		{
			return;
		}
		try
		{
			pack::PackFile pack;
			if (pack.ReadFile(*fileSystem.GetData(path)) != pack::PackResult::Success || !pack.HasBlock("Creature"))
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Unable to read the Creature block of {}", path.string());
				return;
			}
			const auto& block = pack.GetBlock("Creature");
			if (block.size() < sizeof(morph::MorphHeader))
			{
				return;
			}
			// The species is the one the base mesh named in the header is of
			morph::MorphHeader header {};
			std::memcpy(&header, block.data(), sizeof(header));
			const auto species = creature::GetSpeciesFromMeshName(header.baseMeshName.data());
			if (species == CreatureType::Unknown)
			{
				return;
			}
			const auto id = creature::GetRigId(species);
			rigs.Load(id, CreatureRigLoader::FromBufferTag {}, block, specDirectory, meshDirectory);
			loaded.push_back(id);
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loaded the creature animations of {}", path.string());
		}
		catch (const std::exception& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}: {}", path.string(), err.what());
		}
	});

	// The meshes whose skins are painted, read once here so that nothing reads them while the game runs
	auto& files = resources.GetL3DFiles();
	for (const auto id : loaded)
	{
		const auto& rig = *rigs.Handle(id);
		for (size_t m = 0; m < rig.meshNames.size(); ++m)
		{
			const auto& name = rig.meshNames.at(m);
			const auto fileId = entt::hashed_string(("creature/skins/" + name).c_str()).value();
			if (name.empty() || !rig.hasMesh.at(m) || files.Contains(fileId))
			{
				continue;
			}
			const auto path = meshDirectory / (name + ".l3d");
			if (!fileSystem.Exists(path))
			{
				continue;
			}
			try
			{
				files.Load(fileId, L3DFileLoader::FromDiskTag {}, path);
			}
			catch (const std::exception& err)
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}: {}", path.string(), err.what());
			}
		}
	}
}

SoundLoader::result_type SoundLoader::operator()(BaseLoader<audio::Sound>::FromBufferTag,
                                                 const pack::AudioBankSampleHeader& header,
                                                 std::vector<std::vector<uint8_t>> buffer) const
{
	auto sound = std::make_shared<audio::Sound>();
	// Let's clean up the names as they're very difficult to read from the debug GUI
	sound->name = std::filesystem::path(header.name.data()).filename().string();
	sound->id = header.id;
	sound->priority = header.priority;
	sound->sampleRate = static_cast<int>(header.sampleRate);
	sound->bitRate = 0;
	// The sample header's override flags (unknown10/11): a field of the .sad only counts when its bit is set: 0x1 the
	// pitch (percent of the wav's rate), 0x20 the volume (0..127). Otherwise the game's values apply: pitch 100,
	// volume 127. The pitch deviation (percent) always applies.
	const uint32_t overrides = static_cast<uint32_t>(header.unknown10) | (static_cast<uint32_t>(header.unknown11) << 16);
	// the volume is the low u16 of the header's word at 0x25C (openblack's `volume` byte), the user parameter its high
	// u16
	uint32_t volumeWord = 0;
	std::memcpy(&volumeWord, reinterpret_cast<const char*>(&header) + 0x25C, sizeof(volumeWord));
	sound->overrides = overrides;
	sound->volume127 = (overrides & 0x20u) != 0 ? std::min<int>(static_cast<int>(volumeWord & 0xFFFFu), 127) : 127;
	// QMixer's law (sample_play::QMixerGain): floor(main volume 127 * v / 127) * 258 / 32767
	sound->volume = static_cast<float>(sound->volume127 * 258) / 32767.0f;
	sound->userParam = static_cast<int>(volumeWord >> 16);
	sound->loops = (overrides & 0x40u) != 0 ? static_cast<int>(header.loop) : 0;
	sound->pitch = (overrides & 0x1u) != 0 && header.pitch != 0 ? header.pitch : 100;
	sound->pitchDeviation = header.pitchDeviation;
	sound->maxDistance = header.maxDist;
	// 0x80 the minimum distance, 0x100 the maximum distance, 0x200 the scale of the distance mapping (by default 1,
	// 9999 and 0.3); 0x400 the play mode, by default 3
	sound->minDistance = (overrides & 0x80u) != 0 ? header.minDist : 1.0f;
	sound->mappingMaxDistance = (overrides & 0x100u) != 0 ? header.maxDist : 9999.0f;
	sound->scale = (overrides & 0x200u) != 0 ? header.scale : 0.3f;
	sound->playMode = (overrides & 0x400u) != 0 ? static_cast<int>(header.loopType) : 3;
	sound->cloneGroup = static_cast<uint16_t>(header.group);
	sound->atmosGroup = static_cast<uint16_t>(header.atmosGroup);
	// the atmos frequency is the whole u32 at +0x27C: openblack's `atmos` (u16) plus the struct's tail padding
	static_assert(offsetof(pack::AudioBankSampleHeader, loop) == 0x248);
	static_assert(offsetof(pack::AudioBankSampleHeader, minDist) == 0x268);
	static_assert(offsetof(pack::AudioBankSampleHeader, loopType) == 0x274);
	static_assert(offsetof(pack::AudioBankSampleHeader, atmos) == 0x27C);
	static_assert(sizeof(pack::AudioBankSampleHeader) == 0x280);
	std::memcpy(&sound->atmosFrequency, reinterpret_cast<const char*>(&header) + 0x27C, sizeof(sound->atmosFrequency));
	// +0x108 the wave's sample, +0x124 WAVEFORMATEX.wFormatTag, +0x138 / +0x13C the loop section (engine.md §1.5)
	static_assert(offsetof(pack::AudioBankSampleHeader, isBank) == 0x108);
	static_assert(offsetof(pack::AudioBankSampleHeader, unknown6a) == 0x124);
	static_assert(offsetof(pack::AudioBankSampleHeader, lStart) == 0x138);
	static_assert(offsetof(pack::AudioBankSampleHeader, lEnd) == 0x13C);
	sound->wave = header.isBank;
	sound->waveFormat = static_cast<uint16_t>(header.unknown6a);
	sound->loopStart = header.lStart;
	sound->loopEnd = header.lEnd;
	sound->playType = static_cast<audio::PlayType>(header.loopType);
	sound->buffer = std::move(buffer);
	return sound;
}

CameraPathLoader::result_type CameraPathLoader::operator()(FromBufferTag, const std::string& debugName,
                                                           const std::vector<uint8_t>& data) const
{
	auto cameraPath = std::make_shared<CameraPath>(debugName);
	if (!cameraPath->LoadFromBuffer(data))
	{
		throw std::runtime_error("Unable to load camera path " + debugName);
	}
	return cameraPath;
}

CameraPathLoader::result_type CameraPathLoader::operator()(FromDiskTag, const std::filesystem::path& path) const
{
	return (*this)(FromBufferTag {}, path.stem().string(), Locator::filesystem::value().ReadAll(path));
}

LandLightPaletteLoader::result_type LandLightPaletteLoader::operator()(FromBufferTag, std::span<const uint8_t> bytes) const
{
	return std::make_shared<LandLightPalette>(bytes);
}

LandLightPaletteLoader::result_type LandLightPaletteLoader::operator()(FromDiskTag, const std::filesystem::path& path) const
{
	return (*this)(FromBufferTag {}, Locator::filesystem::value().ReadAll(path));
}

LightLoader::result_type LightLoader::operator()(BaseLoader<Lights>::FromDiskTag, const std::filesystem::path& path) const
{
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading lights from file: {}", path.string());
	glw::GLWFile glw;

	const auto result = glw.ReadFile(*Locator::filesystem::value().GetData(path));
	if (result != glw::GLWResult::Success)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to open glw file from filesystem {}: {}", path.string(),
		                    glw::ResultToStr(result));
		throw glw::ResultToStr(result);
	}
	auto lights = std::make_shared<Lights>();
	for (const auto& entry : glw.GetGlows())
	{
		lights->emitters.emplace_back(MakeLightEmitter(entry));
	}
	return lights;
}

std::optional<std::vector<uint8_t>> Texture2DLoader::PackColourAlpha(const std::vector<uint8_t>& colour,
                                                                     const std::vector<uint8_t>& alpha,
                                                                     const ColourAlphaDesc& desc)
{
	const auto pixels = static_cast<size_t>(desc.size) * desc.size;
	if (desc.packing == ColourAlpha::Argb4444)
	{
		// a colour file of another size goes to the DDS path, which openblack does not have. A short or missing alpha
		// file is read as far as it goes, a long one is truncated (PackRaw)
		if (colour.size() != graphics::argb4444::k_ColourBytes)
		{
			return std::nullopt;
		}
		return graphics::argb4444::PackRaw(colour, alpha);
	}
	if (colour.size() != pixels * 3 || alpha.size() != pixels)
	{
		return std::nullopt;
	}
	std::vector<uint8_t> rgba(pixels * 4);
	for (size_t i = 0; i < pixels; ++i)
	{
		rgba[(i * 4) + 0] = colour[(i * 3) + 0];
		rgba[(i * 4) + 1] = colour[(i * 3) + 1];
		rgba[(i * 4) + 2] = colour[(i * 3) + 2];
		rgba[(i * 4) + 3] = alpha[i];
	}
	return rgba;
}

Texture2DLoader::result_type Texture2DLoader::operator()(FromColourAlphaTag, const std::string& name,
                                                         const ColourAlphaDesc& desc) const
{
	auto& fileSystem = Locator::filesystem::value();
	const auto colour = fileSystem.ReadAll(desc.colour);
	std::vector<uint8_t> alpha;
	if (desc.packing == ColourAlpha::Argb4444)
	{
		try
		{
			alpha = fileSystem.ReadAll(desc.alpha);
		}
		catch (const std::exception&)
		{
			alpha.clear(); // allowed: PackRaw takes the alpha from what is there
		}
	}
	else
	{
		alpha = fileSystem.ReadAll(desc.alpha);
	}
	const auto rgba = PackColourAlpha(colour, alpha, desc);
	if (!rgba)
	{
		throw std::runtime_error(fmt::format("{}: unexpected size", name));
	}
	auto texture = std::make_shared<graphics::Texture2D>(name);
	texture->Create(desc.size, desc.size, 1, graphics::TextureFormat::RGBA8, desc.wrap, desc.filter,
	                bgfx::copy(rgba->data(), static_cast<uint32_t>(rgba->size())));
	return texture;
}

GameFontLoader::result_type GameFontLoader::operator()(FromBufferTag, const std::vector<uint8_t>& met,
                                                       const std::vector<uint8_t>& fnt, std::string_view name) const
{
	auto font = std::make_shared<graphics::GameFont>();
	if (!font->Load(met, fnt, name))
	{
		throw std::runtime_error(fmt::format("{} is not a font", name));
	}
	return font;
}

GameFontLoader::result_type GameFontLoader::operator()(FromDiskTag, const std::filesystem::path& base) const
{
	auto& fileSystem = Locator::filesystem::value();
	auto met = base;
	auto fnt = base;
	met += ".met";
	fnt += ".fnt";
	return (*this)(FromBufferTag {}, fileSystem.ReadAll(met), fileSystem.ReadAll(fnt), base.filename().string());
}

HelpTextLoader::result_type HelpTextLoader::operator()(FromBufferTag, const std::vector<uint8_t>& utf16) const
{
	return std::make_shared<std::vector<helptext::Entry>>(helptext::Parse(utf16));
}

HelpTextLoader::result_type HelpTextLoader::operator()(FromDiskTag, const std::filesystem::path& path) const
{
	return (*this)(FromBufferTag {}, LoadBlob(Locator::resources::value().GetBlobs(), path));
}

CameraTrackLoader::result_type CameraTrackLoader::operator()(FromBufferTag, std::span<const uint8_t> segment) const
{
	auto track = camera_tracks::ParseTrack(segment);
	if (!track)
	{
		throw std::runtime_error("not a camera track");
	}
	return std::make_shared<CameraTrack>(std::move(*track));
}

SourceMeshLoader::result_type SourceMeshLoader::operator()(FromBufferTag, uint32_t index,
                                                           const std::vector<uint8_t>& bytes) const
{
	auto mesh = psys::explode_object::ReadSourceMesh(index, bytes);
	if (!mesh)
	{
		throw std::runtime_error(fmt::format("pack mesh {} is not an L3D", index));
	}
	return std::make_shared<psys::explode_object::SourceMesh>(std::move(*mesh));
}

HdModelLoader::result_type HdModelLoader::operator()(FromDiskTag, const std::string& relative, const std::string& name) const
{
	return std::make_shared<ecs::super_villager::HdModel>(ecs::super_villager::ReadHdModel(relative, name));
}

PSysFileLoader::result_type PSysFileLoader::operator()(FromDiskTag, const std::string& name) const
{
	std::shared_ptr<psys::File> result;
	auto& fileSystem = Locator::filesystem::value();
	const auto directory = fileSystem.GetPath<filesystem::Path::Data>() / "Spells" / "ZSpellFiles";
	try
	{
		std::string text;
		if (const auto loose = directory / (name + ".txt"); fileSystem.Exists(loose))
		{
			const auto bytes = fileSystem.ReadAll(loose);
			text.assign(bytes.begin(), bytes.end());
		}
		else
		{
			const auto bytes = fileSystem.ReadAll(directory / (name + "_txt.zzz"));
			if (bytes.size() > 4)
			{
				uint32_t size = 0;
				std::memcpy(&size, bytes.data(), sizeof(size));
				const auto inflated = zip::Inflate(std::vector<uint8_t>(bytes.begin() + 4, bytes.end()), size);
				text.assign(inflated.begin(), inflated.end());
			}
		}
		if (auto parsed = psys::File::Parse(text, name); parsed.has_value())
		{
			result = std::make_shared<psys::File>(std::move(*parsed));
		}
		else
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "PSys: {} is not a spell file", name);
		}
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "PSys: cannot load {}: {}", name, e.what());
	}
	return result;
}

EnumHeaderLoader::result_type EnumHeaderLoader::operator()(FromDiskTag, const std::string& file) const
{
	return psys::ReadEnumHeader(file);
}

TextureStemsLoader::result_type TextureStemsLoader::operator()(FromDiskTag) const
{
	return psys::ReadTextureStems();
}

LightBitmapLoader::result_type LightBitmapLoader::operator()(FromDiskTag, const std::string& name, int pitch, int bpp,
                                                             int framesInFile, int framesInUse) const
{
	return land_light::ReadBitmapFile(name, pitch, bpp, framesInFile, framesInUse);
}

GestureTemplatesLoader::result_type GestureTemplatesLoader::operator()(FromBufferTag, const std::vector<uint8_t>& bytes) const
{
	auto templates = std::make_shared<std::vector<magic::gestures::GestureData>>();
	magic::gestures::LoadTemplates(bytes, *templates);
	return templates;
}

GestureTemplatesLoader::result_type GestureTemplatesLoader::operator()(FromDiskTag) const
{
	auto templates = std::make_shared<std::vector<magic::gestures::GestureData>>();
	auto& fileSystem = Locator::filesystem::value();
	// loaded with the game's files: ".\Data\Gestures.jty"
	const auto path = fileSystem.GetPath<filesystem::Path::Data>() / "Gestures.jty";
	try
	{
		if (!magic::gestures::LoadTemplates(LoadBlob(Locator::resources::value().GetBlobs(), path), *templates))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Gestures: {} is short", path.generic_string());
		}
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Gestures: cannot read {}: {}", path.generic_string(), e.what());
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Gestures: {} templates", templates->size());
	return templates;
}

GestureShapeLoader::result_type GestureShapeLoader::operator()(FromBufferTag, const std::vector<uint8_t>& bytes) const
{
	auto shape = std::make_shared<magic::gestures::Shape>();
	return magic::gestures::ParseShape(bytes, *shape) ? shape : nullptr;
}

GestureShapeLoader::result_type GestureShapeLoader::operator()(FromDiskTag, int number) const
{
	if (!Locator::filesystem::has_value())
	{
		return nullptr;
	}
	auto& fileSystem = Locator::filesystem::value();
	// ".\data\symbols\PathSymbol" + "%d.cam"
	const auto path = fileSystem.GetPath<filesystem::Path::Data>() / "Symbols" / fmt::format("PathSymbol{}.cam", number);
	try
	{
		if (!fileSystem.Exists(path))
		{
			return nullptr;
		}
		return (*this)(FromBufferTag {}, LoadBlob(Locator::resources::value().GetBlobs(), path));
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Gestures: {}: {}", path.generic_string(), e.what());
	}
	return nullptr;
}

BlobLoader::result_type BlobLoader::operator()(FromBufferTag, std::vector<uint8_t> data) const
{
	return std::make_shared<std::vector<uint8_t>>(std::move(data));
}

BlobLoader::result_type BlobLoader::operator()(FromDiskTag, const std::filesystem::path& path) const
{
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading data file: {}", path.string());
	return std::make_shared<std::vector<uint8_t>>(Locator::filesystem::value().ReadAll(path));
}
