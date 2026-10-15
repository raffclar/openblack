/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO

#include "ExperimentCreature.h"

#include <chrono>
#include <filesystem>
#include <limits>
#include <memory>

#include <GlbFile.h>
#include <spdlog/spdlog.h>
#include <stb_image.h>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "Creature/CreatureSkin.h"
#include "Creature/GltfCreature.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::experiment_creature
{
namespace
{
/// The name the undead cow's mesh goes by, in place of a mesh file's name
constexpr std::string_view k_MeshName = "UndeadCow";
/// The skin's grey when there is no texture to read
constexpr uint8_t k_PlainGrey = 160;

std::unique_ptr<l3d::L3DTexture> ReadSkin(filesystem::FileSystemInterface& fileSystem, const std::filesystem::path& path)
{
	if (fileSystem.Exists(path))
	{
		const auto bytes = fileSystem.ReadAll(path);
		int width = 0;
		int height = 0;
		int channels = 0;
		std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> texels(
		    stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels, 4),
		    &stbi_image_free);
		if (texels != nullptr && width > 0 && height > 0)
		{
			const auto size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
			return gltf_creature::SkinFromRgba(std::span<const uint8_t>(texels.get(), size), static_cast<uint32_t>(width),
			                                   static_cast<uint32_t>(height), gltf_creature::k_SkinId);
		}
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Experiment creature: unable to read {}", path.string());
	}
	std::vector<uint8_t> grey(4, k_PlainGrey);
	grey.back() = std::numeric_limits<uint8_t>::max();
	return gltf_creature::SkinFromRgba(grey, 1, 1, gltf_creature::k_SkinId);
}
} // namespace

void Load()
{
	const auto started = std::chrono::steady_clock::now();
	auto& fileSystem = Locator::filesystem::value();
	const std::filesystem::path folder(OPENBLACK_EXPERIMENT_CREATURE_DIR);
	const auto modelPath = folder / k_ModelFile;
	if (!fileSystem.Exists(modelPath))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Experiment creature: no {}, the undead cow is left out", modelPath.string());
		return;
	}
	// Without the cow's rig (as in the tests' mock data) there are no settings to copy
	auto& resources = Locator::resources::value();
	auto& rigs = resources.GetCreatureRigs();
	const auto cowRig = creature::GetRigId(CreatureType::Cow);
	if (!rigs.Contains(cowRig))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Experiment creature: there is no cow rig to copy, the undead cow is left out");
		return;
	}
	glb::Model model;
	const auto bytes = fileSystem.ReadAll(modelPath);
	if (const auto result = glb::Read(bytes, model); result != glb::GlbResult::Success)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Experiment creature: {}: {}", modelPath.string(), glb::ResultToStr(result));
		return;
	}
	const auto bones = gltf_creature::BonesOfJoints(model);
	const auto rest = gltf_creature::RestOf(model, bones);
	if (!rest.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Experiment creature: the skeleton of {} cannot be read", modelPath.string());
		return;
	}

	const auto skin = ReadSkin(fileSystem, folder / k_TextureFile);
	const auto file = gltf_creature::BuildMesh(model, bones, *rest, *skin);
	const auto meshId = creature::GetIdFromType(CreatureType::UndeadCow, creature::CreatureBody::Appearance::Base);
	auto& meshes = resources.GetMeshes();
	meshes.Load(meshId, resources::L3DLoader::FromFileTag {}, std::string(k_MeshName), file);
	const auto mesh = meshes.Handle(meshId);
	// The skins are painted (with scars and tattoos) from the file of the mesh, as from a species' mesh file
	resources.GetL3DFiles().Load(creature_skin::MeshFileId(k_MeshName), resources::L3DFileLoader::FromFileTag {}, file);

	// The rig takes the cow's settings: the cow's rig is read now to copy them
	const auto skeleton = skeletal_animation::Skeleton::FromRestMatrices(mesh->GetBoneParents(), mesh->GetBoneMatrices());
	rigs.Load(creature::GetRigId(CreatureType::UndeadCow), resources::CreatureRigLoader::FromGlbTag {}, *rigs.Handle(cowRig),
	          model, std::span<const uint32_t>(bones), skeleton, std::string(k_MeshName));
	const auto took = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - started).count();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Experiment creature: undead cow loaded, {} vertices, {} bones, {:.0f} ms",
	                   file.GetVertices().size(), bones.size(), took);
}

} // namespace openblack::experiment_creature
