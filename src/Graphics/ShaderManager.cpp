/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How to add a Shader:
// Shaders in openblack are compiled using bgfx's shaderc compiler. Shaderc
// will compile different variations for different rendering APIs and
// platforms. The implementation is in Shaders.cmake.
// Shader sources are located in assets/shaders. The shader language used is a
// subset of glsl made specifically for bgfx.
// Inputs and outputs of shaders (excluding uniforms, textures samplers, etc)
// are declared in varying.def.sc.
// Once compiled they go in ${CMAKE_BINARY_DIR}/include/generated/shaders and
// are included in the openblack binary by way of ShaderManager.cpp.
// The helper header ShaderIncluder.h used with the SHADER_NAME define will
// automatically include all the shader variants in the file.
// Once included, they must be added to the s_embeddedShaders array.
// Finally, the renderer calls the shader manager to load all shaders
// named in the Shaders array.
// tldr:
// 1. Create shaders in assets/shaders
// 2. Define SHADER_NAME and include ShaderIncluder.h
// 3. Add BGFX_EMBEDDED_SHADER entries to s_embeddedShaders
// 4. Add ShaderDefinition to Shaders array

#include "ShaderManager.h"

#include <cstdint> // Shaders below need uint8_t

#include <array>
#include <map>
#include <string_view>

#include <spdlog/spdlog.h>

#include "Camera/Camera.h"
#include "EmbeddedShaders.h"
#include "GraphicsHandleBgfx.h"
#include "SamplerDefaults.h"

// clang-format off
#define SHADER_NAME vs_line
#include "ShaderIncluder.h"
#define SHADER_NAME vs_line_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME fs_line
#include "ShaderIncluder.h"

#define SHADER_NAME vs_object
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_single_bone
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_few_bones
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_few_bones_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_palette_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_morph_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_posed_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_morph
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_hm_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_hm_static_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME fs_object
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_environment
#include "ShaderIncluder.h"
#define SHADER_NAME fs_object_environment
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_static_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_environment_static_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_lightmap_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME fs_object_lightmap
#include "ShaderIncluder.h"
#define SHADER_NAME fs_object_reflective_lightmap
#include "ShaderIncluder.h"
#define SHADER_NAME fs_reflection
#include "ShaderIncluder.h"
#define SHADER_NAME fs_sky
#include "ShaderIncluder.h"

#define SHADER_NAME vs_terrain
#include "ShaderIncluder.h"
#define SHADER_NAME fs_terrain
#include "ShaderIncluder.h"

#define SHADER_NAME vs_water
#include "ShaderIncluder.h"
#define SHADER_NAME fs_water
#include "ShaderIncluder.h"

#define SHADER_NAME vs_sprite
#include "ShaderIncluder.h"
#define SHADER_NAME fs_sprite
#include "ShaderIncluder.h"

#define SHADER_NAME vs_particle_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME vs_particle_chain
#include "ShaderIncluder.h"
#define SHADER_NAME fs_particle
#include "ShaderIncluder.h"
#define SHADER_NAME vs_particle_surface
#include "ShaderIncluder.h"
#define SHADER_NAME vs_volume_glow
#include "ShaderIncluder.h"
#define SHADER_NAME fs_particle_surface
#include "ShaderIncluder.h"
#define SHADER_NAME vs_celestial
#include "ShaderIncluder.h"
#define SHADER_NAME fs_celestial
#include "ShaderIncluder.h"
#define SHADER_NAME vs_mist
#include "ShaderIncluder.h"
#define SHADER_NAME fs_mist
#include "ShaderIncluder.h"
#define SHADER_NAME vs_land_luminosity
#include "ShaderIncluder.h"
#define SHADER_NAME fs_land_colour
#include "ShaderIncluder.h"
#define SHADER_NAME fs_sky_dome
#include "ShaderIncluder.h"
#define SHADER_NAME vs_creature_hair
#include "ShaderIncluder.h"
#define SHADER_NAME fs_creature_hair
#include "ShaderIncluder.h"
#define SHADER_NAME vs_leash
#include "ShaderIncluder.h"
#define SHADER_NAME vs_blob
#include "ShaderIncluder.h"
#define SHADER_NAME fs_blob
#include "ShaderIncluder.h"
#define SHADER_NAME fs_world_textured
#include "ShaderIncluder.h"
#define SHADER_NAME vs_advisor
#include "ShaderIncluder.h"
#define SHADER_NAME fs_advisor
#include "ShaderIncluder.h"
#define SHADER_NAME fs_advisor_quad
#include "ShaderIncluder.h"
#define SHADER_NAME vs_fragment
#include "ShaderIncluder.h"
#define SHADER_NAME fs_ghost_depth
#include "ShaderIncluder.h"
#define SHADER_NAME fs_fragment
#include "ShaderIncluder.h"

#define SHADER_NAME vs_vegetation
#include "ShaderIncluder.h"
#define SHADER_NAME vs_vegetation_hm_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME fs_vegetation
#include "ShaderIncluder.h"
#define SHADER_NAME fs_shadow_caster
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_shadow_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_shadow_static_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME fs_object_shadow
#include "ShaderIncluder.h"

#define SHADER_NAME vs_beam
#include "ShaderIncluder.h"
#define SHADER_NAME fs_beam
#include "ShaderIncluder.h"

#define SHADER_NAME vs_interface
#include "ShaderIncluder.h"
#define SHADER_NAME vs_interface_sorted
#include "ShaderIncluder.h"
#define SHADER_NAME vs_text3d
#include "ShaderIncluder.h"
#define SHADER_NAME fs_interface
#include "ShaderIncluder.h"
#define SHADER_NAME fs_video
#include "ShaderIncluder.h"

// clang-format on

namespace openblack::graphics
{

struct ShaderDefinition
{
	const std::string_view name;
	const std::string_view vertexShaderName;
	const std::string_view fragmentShaderName;
};

const std::array<bgfx::EmbeddedShader, 70> k_EmbeddedShaders = {{
    BGFX_EMBEDDED_SHADER(vs_line),
    BGFX_EMBEDDED_SHADER(vs_line_instanced), //
    BGFX_EMBEDDED_SHADER(fs_line),           //
    BGFX_EMBEDDED_SHADER(vs_object),
    BGFX_EMBEDDED_SHADER(vs_object_single_bone),
    BGFX_EMBEDDED_SHADER(vs_object_few_bones),
    BGFX_EMBEDDED_SHADER(vs_object_instanced),
    BGFX_EMBEDDED_SHADER(vs_object_few_bones_instanced),
    BGFX_EMBEDDED_SHADER(vs_object_palette_instanced),
    BGFX_EMBEDDED_SHADER(vs_object_morph_instanced),
    BGFX_EMBEDDED_SHADER(vs_object_posed_instanced),
    BGFX_EMBEDDED_SHADER(vs_object_morph),
    BGFX_EMBEDDED_SHADER(vs_object_hm_instanced),
    BGFX_EMBEDDED_SHADER(vs_object_hm_static_instanced), //
    BGFX_EMBEDDED_SHADER(fs_object),
    BGFX_EMBEDDED_SHADER(vs_object_environment),
    BGFX_EMBEDDED_SHADER(fs_object_environment),
    BGFX_EMBEDDED_SHADER(vs_object_static_instanced),
    BGFX_EMBEDDED_SHADER(vs_object_environment_static_instanced),
    BGFX_EMBEDDED_SHADER(vs_object_lightmap_instanced),
    BGFX_EMBEDDED_SHADER(fs_reflection),
    BGFX_EMBEDDED_SHADER(fs_object_lightmap),
    BGFX_EMBEDDED_SHADER(fs_object_reflective_lightmap),
    BGFX_EMBEDDED_SHADER(fs_sky), //
    BGFX_EMBEDDED_SHADER(vs_terrain),
    BGFX_EMBEDDED_SHADER(fs_terrain), //
    BGFX_EMBEDDED_SHADER(vs_water),
    BGFX_EMBEDDED_SHADER(fs_water), //
    BGFX_EMBEDDED_SHADER(vs_sprite),
    BGFX_EMBEDDED_SHADER(fs_sprite), //
    BGFX_EMBEDDED_SHADER(vs_particle_instanced),
    BGFX_EMBEDDED_SHADER(vs_particle_chain),
    BGFX_EMBEDDED_SHADER(fs_particle), //
    BGFX_EMBEDDED_SHADER(vs_particle_surface),
    BGFX_EMBEDDED_SHADER(vs_volume_glow),
    BGFX_EMBEDDED_SHADER(fs_particle_surface),
    BGFX_EMBEDDED_SHADER(vs_celestial),       //
    BGFX_EMBEDDED_SHADER(fs_celestial),       //
    BGFX_EMBEDDED_SHADER(vs_mist),            //
    BGFX_EMBEDDED_SHADER(fs_mist),            //
    BGFX_EMBEDDED_SHADER(vs_land_luminosity), //
    BGFX_EMBEDDED_SHADER(fs_land_colour),     //
    BGFX_EMBEDDED_SHADER(fs_sky_dome),        //
    BGFX_EMBEDDED_SHADER(vs_creature_hair),   //
    BGFX_EMBEDDED_SHADER(fs_creature_hair),   //
    BGFX_EMBEDDED_SHADER(vs_leash),           //
    BGFX_EMBEDDED_SHADER(vs_blob),            //
    BGFX_EMBEDDED_SHADER(fs_blob),            //
    BGFX_EMBEDDED_SHADER(fs_world_textured),  //
    BGFX_EMBEDDED_SHADER(vs_advisor),         //
    BGFX_EMBEDDED_SHADER(fs_advisor),         //
    BGFX_EMBEDDED_SHADER(fs_advisor_quad),    //
    BGFX_EMBEDDED_SHADER(vs_fragment),        //
    BGFX_EMBEDDED_SHADER(fs_ghost_depth),     //
    BGFX_EMBEDDED_SHADER(fs_fragment),        //
    BGFX_EMBEDDED_SHADER(vs_vegetation),
    BGFX_EMBEDDED_SHADER(vs_vegetation_hm_instanced),
    BGFX_EMBEDDED_SHADER(fs_vegetation),    //
    BGFX_EMBEDDED_SHADER(fs_shadow_caster), //
    BGFX_EMBEDDED_SHADER(vs_object_shadow_instanced),
    BGFX_EMBEDDED_SHADER(vs_object_shadow_static_instanced),
    BGFX_EMBEDDED_SHADER(fs_object_shadow), //
    BGFX_EMBEDDED_SHADER(vs_beam),
    BGFX_EMBEDDED_SHADER(fs_beam), //
    BGFX_EMBEDDED_SHADER(vs_interface),
    BGFX_EMBEDDED_SHADER(vs_interface_sorted),
    BGFX_EMBEDDED_SHADER(vs_text3d),
    BGFX_EMBEDDED_SHADER(fs_interface), //
    BGFX_EMBEDDED_SHADER(fs_video),     //
    BGFX_EMBEDDED_SHADER_END()          //
}};

constexpr std::array k_Shaders {
    ShaderDefinition {"DebugLine", "vs_line", "fs_line"},
    ShaderDefinition {"DebugLineInstanced", "vs_line_instanced", "fs_line"},
    ShaderDefinition {"Terrain", "vs_terrain", "fs_terrain"},
    ShaderDefinition {"Object", "vs_object", "fs_object"},
    ShaderDefinition {"ObjectEnvironment", "vs_object_environment", "fs_object_environment"},
    ShaderDefinition {"ObjectInstanced", "vs_object_instanced", "fs_object"},
    ShaderDefinition {"ObjectFewBonesInstanced", "vs_object_few_bones_instanced", "fs_object"},
    ShaderDefinition {"ObjectPaletteInstanced", "vs_object_palette_instanced", "fs_object"},
    ShaderDefinition {"ObjectMorphInstanced", "vs_object_morph_instanced", "fs_object"},
    ShaderDefinition {"ObjectPosedInstanced", "vs_object_posed_instanced", "fs_object"},
    ShaderDefinition {"ObjectHeightMapInstanced", "vs_object_hm_instanced", "fs_object"},
    ShaderDefinition {"ObjectHeightMapStaticInstanced", "vs_object_hm_static_instanced", "fs_object"},
    ShaderDefinition {"ObjectStaticInstanced", "vs_object_static_instanced", "fs_object"},
    ShaderDefinition {"ObjectEnvironmentStaticInstanced", "vs_object_environment_static_instanced", "fs_object_environment"},
    ShaderDefinition {"ObjectLightmapInstanced", "vs_object_lightmap_instanced", "fs_object_lightmap"},
    ShaderDefinition {"Reflection", "vs_object", "fs_reflection"},
    ShaderDefinition {"ObjectReflectiveLightmapInstanced", "vs_object_lightmap_instanced", "fs_object_reflective_lightmap"},
    ShaderDefinition {"Sky", "vs_object", "fs_sky"},
    ShaderDefinition {"Water", "vs_water", "fs_water"},
    ShaderDefinition {"Sprite", "vs_sprite", "fs_sprite"},
    ShaderDefinition {"ParticleInstanced", "vs_particle_instanced", "fs_particle"},
    ShaderDefinition {"ParticleChain", "vs_particle_chain", "fs_particle"},
    ShaderDefinition {"ParticleSurface", "vs_particle_surface", "fs_particle_surface"},
    ShaderDefinition {"VolumeGlow", "vs_volume_glow", "fs_particle_surface"},
    ShaderDefinition {"Celestial", "vs_celestial", "fs_celestial"},
    ShaderDefinition {"Mist", "vs_mist", "fs_mist"},
    ShaderDefinition {"LandColour", "vs_land_luminosity", "fs_land_colour"},
    ShaderDefinition {"SkyDome", "vs_land_luminosity", "fs_sky_dome"},
    ShaderDefinition {"CreatureHair", "vs_creature_hair", "fs_creature_hair"},
    ShaderDefinition {"Leash", "vs_leash", "fs_world_textured"},
    ShaderDefinition {"Blob", "vs_blob", "fs_blob"},
    ShaderDefinition {"WorldTextured", "vs_blob", "fs_world_textured"},
    ShaderDefinition {"Advisor", "vs_advisor", "fs_advisor"},
    ShaderDefinition {"AdvisorQuad", "vs_blob", "fs_advisor_quad"},
    ShaderDefinition {"Fragment", "vs_fragment", "fs_fragment"},
    ShaderDefinition {"GhostDepth", "vs_object", "fs_ghost_depth"},
    ShaderDefinition {"Vegetation", "vs_vegetation", "fs_vegetation"},
    ShaderDefinition {"VegetationHeightMapInstanced", "vs_vegetation_hm_instanced", "fs_vegetation"},
    ShaderDefinition {"ShadowCaster", "vs_object", "fs_shadow_caster"},
    ShaderDefinition {"ShadowCasterMorph", "vs_object_morph", "fs_shadow_caster"},
    ShaderDefinition {"Silhouette", "vs_line", "fs_line"},
    ShaderDefinition {"ObjectShadowInstanced", "vs_object_shadow_instanced", "fs_object_shadow"},
    ShaderDefinition {"ObjectShadowStaticInstanced", "vs_object_shadow_static_instanced", "fs_object_shadow"},
    ShaderDefinition {"Beam", "vs_beam", "fs_beam"},
    ShaderDefinition {"Interface", "vs_interface", "fs_interface"},
    ShaderDefinition {"InterfaceSorted", "vs_interface_sorted", "fs_interface"},
    ShaderDefinition {"Video", "vs_interface", "fs_video"},
    ShaderDefinition {"Text3D", "vs_text3d", "fs_interface"},
    // The programs above that draw a mesh with all of its bones, again with shaders that declare fewer bones
    ShaderDefinition {"ObjectSingleBone", "vs_object_single_bone", "fs_object"},
    ShaderDefinition {"ObjectFewBones", "vs_object_few_bones", "fs_object"},
    ShaderDefinition {"ReflectionSingleBone", "vs_object_single_bone", "fs_reflection"},
    ShaderDefinition {"ReflectionFewBones", "vs_object_few_bones", "fs_reflection"},
    ShaderDefinition {"SkySingleBone", "vs_object_single_bone", "fs_sky"},
    ShaderDefinition {"SkyFewBones", "vs_object_few_bones", "fs_sky"},
    ShaderDefinition {"GhostDepthSingleBone", "vs_object_single_bone", "fs_ghost_depth"},
    ShaderDefinition {"GhostDepthFewBones", "vs_object_few_bones", "fs_ghost_depth"},
    ShaderDefinition {"ShadowCasterSingleBone", "vs_object_single_bone", "fs_shadow_caster"},
    ShaderDefinition {"ShadowCasterFewBones", "vs_object_few_bones", "fs_shadow_caster"},
};

/// A program that draws a mesh with all of its bones, and its programs with the same fragment shader for a mesh with
/// one bone and with a few bones
struct BoneVariants
{
	std::string_view program;
	std::string_view singleBone;
	std::string_view fewBones;
};

constexpr std::array k_BoneVariants {
    BoneVariants {.program = "Object", .singleBone = "ObjectSingleBone", .fewBones = "ObjectFewBones"},
    BoneVariants {.program = "Reflection", .singleBone = "ReflectionSingleBone", .fewBones = "ReflectionFewBones"},
    BoneVariants {.program = "Sky", .singleBone = "SkySingleBone", .fewBones = "SkyFewBones"},
    BoneVariants {.program = "GhostDepth", .singleBone = "GhostDepthSingleBone", .fewBones = "GhostDepthFewBones"},
    BoneVariants {.program = "ShadowCaster", .singleBone = "ShadowCasterSingleBone", .fewBones = "ShadowCasterFewBones"},
};

namespace
{
/// The samplers a shader uses, read from its Vulkan build, which every desktop platform embeds whichever backend runs
std::vector<shader_samplers::Sampler> ReflectSamplers(std::string_view shaderName)
{
	for (const auto& shader : k_EmbeddedShaders)
	{
		if (shader.name == nullptr || shaderName != shader.name)
		{
			continue;
		}
		for (const auto& data : shader.data)
		{
			if (data.type == bgfx::RendererType::Vulkan && data.data != nullptr)
			{
				if (auto samplers = shader_samplers::ReadSpirvSamplers({data.data, data.size}))
				{
					return *std::move(samplers);
				}
			}
		}
	}
	SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Could not read the samplers of shader {}: they won't get defaults",
	                   shaderName);
	return {};
}

/// The samplers of both of a program's shaders
std::vector<shader_samplers::Sampler> SamplersOf(const ShaderDefinition& shader)
{
	return shader_samplers::Merge(ReflectSamplers(shader.vertexShaderName), ReflectSamplers(shader.fragmentShaderName));
}
} // namespace

ShaderManager::ShaderManager() = default;

ShaderManager::~ShaderManager()
{
	// delete all mapped shaders
	ShaderMap::iterator iter;
	for (iter = _shaderPrograms.begin(); iter != _shaderPrograms.end(); ++iter)
	{
		delete iter->second;
	}

	_shaderPrograms.clear();
	_samplerDefaults.reset();
}

void ShaderManager::LoadShaders()
{
	_samplerDefaults = std::make_unique<SamplerDefaults>();
	std::map<std::string_view, ShaderProgram*> made;
	for (const auto& shader : k_Shaders)
	{
		bgfx::RendererType::Enum type = bgfx::getRendererType();
		auto vs = bgfx::createEmbeddedShader(k_EmbeddedShaders.data(), type, shader.vertexShaderName.data());
		assert(bgfx::isValid(vs));
		auto fs = bgfx::createEmbeddedShader(k_EmbeddedShaders.data(), type, shader.fragmentShaderName.data());
		assert(bgfx::isValid(fs));
		auto* program =
		    new ShaderProgram(shader.name.data(), fromBgfx(vs), fromBgfx(fs), SamplersOf(shader), *_samplerDefaults);
		made[shader.name] = program;
		_shaderPrograms[shader.name.data()] = program;
	}
	for (const auto& variants : k_BoneVariants)
	{
		made.at(variants.program)->SetBoneVariants(made.at(variants.singleBone), made.at(variants.fewBones));
	}
}

std::vector<std::pair<std::string, std::vector<shader_samplers::Sampler>>> ShaderManager::ProgramSamplers()
{
	std::vector<std::pair<std::string, std::vector<shader_samplers::Sampler>>> programs;
	programs.reserve(k_Shaders.size());
	for (const auto& shader : k_Shaders)
	{
		programs.emplace_back(std::string(shader.name), SamplersOf(shader));
	}
	return programs;
}

const ShaderProgram* ShaderManager::GetShader(const std::string& name) const
{
	auto i = _shaderPrograms.find(name);
	if (i != _shaderPrograms.end())
	{
		return i->second;
	}

	// todo: return an empty shader?
	return nullptr;
}

void ShaderManager::DiscardBindings() const
{
	bgfx::discard(BGFX_DISCARD_BINDINGS);
	_samplerDefaults->Discarded(BGFX_DISCARD_BINDINGS);
}

void ShaderManager::FrameEnded() const
{
	_samplerDefaults->FrameEnded();
}

void ShaderManager::SetViewOrder(RenderPass viewId, ViewOrder order) const
{
	SetViewOrder(static_cast<uint16_t>(viewId), order);
}

void ShaderManager::SetViewOrder(uint16_t viewId, ViewOrder order) const
{
	constexpr std::array k_Modes = {
	    bgfx::ViewMode::Default,
	    bgfx::ViewMode::Sequential,
	    bgfx::ViewMode::DepthAscending,
	    bgfx::ViewMode::DepthDescending,
	};
	bgfx::setViewMode(static_cast<bgfx::ViewId>(viewId), k_Modes.at(static_cast<size_t>(order)));
}

void ShaderManager::SetCamera(graphics::RenderPass viewId, const Camera& camera)
{
	auto view = camera.GetViewMatrix(Camera::Interpolation::Current);
	auto proj = camera.GetProjectionMatrix();
	bgfx::setViewTransform(static_cast<bgfx::ViewId>(viewId), &view, &proj);
}

} // namespace openblack::graphics
