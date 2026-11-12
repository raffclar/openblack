/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The renderer is one of the locator's implementations
#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <bgfx/bgfx.h>
#include <glm/gtx/transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/ChimneySmoke.h"
#include "3D/HandMorph.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSkinTexels.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLightTable.h"
#include "3D/StormClouds.h"
#include "3D/TempleDoors.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/TempleMap.h"
#include "Camera/Camera.h"
#include "Common/WorkerPool.h"
#include "Creature/CreatureFizzLook.h"
#include "Creature/CreatureHair.h"
#include "Creature/CreatureHairLight.h"
#include "Creature/CreatureSpells.h"
#include "Creature/LeashOrders.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/ChimneySmoke.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HandMorph.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "EngineConfig.h"
#include "Game.h"
#include "Graphics/BoneBudget.h"
#include "Graphics/CreatureReflection.h"
#include "Graphics/DebugLines.h"
#include "Graphics/DrawOrder.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/HandReflection.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/LandFootprints.h"
#include "Graphics/LandShadowBake.h"
#include "Graphics/LightShine.h"
#include "Graphics/MaterialStages.h"
#include "Graphics/Mesh.h"
#include "Graphics/MeshDetail.h"
#include "Graphics/ModelLight.h"
#include "Graphics/Moon.h"
#include "Graphics/RendererDetail.h"
#include "Graphics/SeeThroughState.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ZSort.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/Rereads.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Scenery/ObjectDrawList.h"
#include "entt/locator/locator.hpp"

using namespace openblack;
using namespace openblack::graphics;
using namespace openblack::graphics::detail;
using namespace openblack::ecs::systems;

namespace openblack::graphics::detail
{
const Texture2D* GetTexture(uint32_t skinID, const std::unordered_map<SkinId, std::unique_ptr<graphics::Texture2D>>& meshSkins)
{
	const auto& textureManager = Locator::resources::value().GetTextures();

	const Texture2D* texture = nullptr;

	if (skinID != 0xFFFFFFFF)
	{
		if (const auto skin = meshSkins.find(skinID); skin != meshSkins.end())
		{
			texture = skin->second.get();
		}
		else if (const auto* shared = textureManager.Find(skinID); shared != nullptr)
		{
			texture = shared;
		}
		else
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("graphics"), "Could not find the texture {:#x}", skinID);
		}
	}

	return texture;
}
} // namespace openblack::graphics::detail

namespace
{
/// Binds the submeshes of a creature's variant meshes that match one of its base mesh as the second to fourth vertex
/// streams; the base's own where a variant's doesn't match
void BindMorphTargets(bgfx::Encoder& encoder, const L3DMesh& mesh, const L3DSubMesh& subMesh,
                      const RendererInterface::L3DMeshSubmitDesc::MorphTargets& targets, const MorphStreamLayouts& layouts)
{
	const auto& subMeshes = mesh.GetSubMeshes();
	const auto found = std::ranges::find_if(subMeshes, [&subMesh](const auto& other) { return other.get() == &subMesh; });
	const auto index = static_cast<size_t>(std::distance(subMeshes.begin(), found));
	const auto& base = subMesh.GetMesh().GetVertexBuffer();
	for (size_t axis = 0; axis < targets.meshes.size(); ++axis)
	{
		const auto* target = targets.meshes.at(axis);
		const auto* buffer = &base;
		if (target != nullptr && index < target->GetSubMeshes().size())
		{
			const auto& candidate = target->GetSubMeshes()[index]->GetMesh().GetVertexBuffer();
			if (candidate.GetCount() == base.GetCount() && candidate.GetStrideBytes() == base.GetStrideBytes())
			{
				buffer = &candidate;
			}
		}
		buffer->BindStream(encoder, static_cast<uint8_t>(axis + 1), layouts.Get(axis));
	}
}

/// Binds every vertex's position and bone in the submesh and in its matches of the creature's variant meshes, for the
/// vertex shader to blend the seams with; the base's own where a variant's doesn't match. Off where the submesh has no
/// blends, or where the blends are turned off.
void BindBlendSources(DrawEncoder& to, const L3DMesh& mesh, const L3DSubMesh& subMesh,
                      const RendererInterface::L3DMeshSubmitDesc::MorphTargets& targets, const ShaderProgram& program)
{
	const auto* base = subMesh.GetBlendSource();
	const bool blended = targets.blendSeams && subMesh.HasBlends() && base != nullptr;
	const auto width = base != nullptr ? static_cast<float>(base->GetResolution().x) : 1.0f;
	const glm::vec4 u_vertexBlend {blended ? 1.0f : 0.0f, width, 0.0f, 0.0f};
	program.SetUniformValue(to, "u_vertexBlend", &u_vertexBlend);
	if (!blended)
	{
		return;
	}
	const auto& subMeshes = mesh.GetSubMeshes();
	const auto found = std::ranges::find_if(subMeshes, [&subMesh](const auto& other) { return other.get() == &subMesh; });
	const auto index = static_cast<size_t>(std::distance(subMeshes.begin(), found));
	constexpr std::array<std::pair<const char*, uint8_t>, 3> k_Samplers {{
	    {"s_blendEvilGood", 3},
	    {"s_blendThinFat", 4},
	    {"s_blendWeakStrong", 9},
	}};
	program.SetTextureSampler(to, "s_blendBase", 2, *base);
	for (size_t axis = 0; axis < targets.meshes.size(); ++axis)
	{
		const auto* target = targets.meshes.at(axis);
		const auto* source = base;
		if (target != nullptr && index < target->GetSubMeshes().size())
		{
			const auto* candidate = target->GetSubMeshes()[index]->GetBlendSource();
			if (candidate != nullptr && candidate->GetResolution() == base->GetResolution())
			{
				source = candidate;
			}
		}
		program.SetTextureSampler(to, k_Samplers.at(axis).first, k_Samplers.at(axis).second, *source);
	}
}

/// The textures of the looks the creature spells give a creature: the ice sheen of a frozen one, and the static one
/// fizzing out of sight is drawn through
void BindCreatureSpellLooks(DrawEncoder& to, const ShaderProgram& program)
{
	const auto& textures = Locator::resources::value().GetTextures();
	// The stage is the usual stage of the sampler. A program that reads the sampler at a different stage uses its own.
	// The program with the environment map reads the alpha of the ice at the stage of the height map. A bind at the
	// usual stage puts the texture where the program does not read it. Then the program gets its default texture at
	// its own stage, and bgfx sees the sampler two times in one draw.
	constexpr std::array<std::pair<const char*, std::pair<uint8_t, entt::hashed_string>>, 3> k_Looks {{
	    {"s_iceEnvironment", {10, entt::hashed_string("raw/S_IceEnvMap")}},
	    {"s_iceEnvironmentAlpha", {5, entt::hashed_string("raw/S_IceEnvMapa")}},
	    {"s_staticAlpha", {15, entt::hashed_string("raw/S_Statica")}},
	}};
	for (const auto& [sampler, binding] : k_Looks)
	{
		if (textures.Contains(binding.second.value()))
		{
			const auto stage = program.SamplerStage(sampler).value_or(binding.first);
			program.SetTextureSampler(to, sampler, stage, *textures.Handle(binding.second));
		}
	}
}

/// How a creature fizzing out of sight is drawn in the pass at hand: xy how far the static has slid across its skin, z
/// 0 for none, 1 for its depth through the static, with w the least alpha the static must have, 2 for its body over
/// that depth, blended at w, or 3 for the ice alone over all of it, with w the ice's alpha. With z 0, w is the ice's
/// alpha. With z -1, the mesh shows no ice, also where its instance is frozen.
glm::vec4 CreatureSpellLookOf(const std::optional<RendererInterface::L3DMeshSubmitDesc::Fizz>& fizz, float freeze,
                              bool iceShown)
{
	using Pass = RendererInterface::L3DMeshSubmitDesc::Fizz::Pass;
	const float iceAlpha = freeze > 0.0f ? static_cast<float>(creature_spells::IceAlpha(freeze)) / 255.0f : 0.0f;
	if (!fizz.has_value())
	{
		return {0.0f, 0.0f, iceShown ? 0.0f : -1.0f, iceShown ? iceAlpha : 0.0f};
	}
	if (fizz->pass == Pass::Ice)
	{
		return {0.0f, 0.0f, 3.0f, iceAlpha};
	}
	const bool depth = fizz->pass == Pass::Depth;
	const float w = depth ? static_cast<float>(creature_fizz_look::StaticThreshold(fizz->level)) / 255.0f
	                      : creature_fizz_look::BodyAlpha(fizz->level);
	return {fizz->scroll, depth ? 1.0f : 2.0f, w};
}
} // namespace

const Renderer::MeshUniforms& Renderer::MeshUniformsOf(const ShaderProgram& program) const
{
	// Only a look-up for the programs loaded with the renderer, which the threads that encode draws can do together
	if (const auto known = std::as_const(_meshUniforms).find(&program); known != _meshUniforms.cend())
	{
		return known->second;
	}
	const auto [found, inserted] = _meshUniforms.try_emplace(&program);
	for (size_t i = 0; i < k_MeshUniformNames.size(); ++i)
	{
		found->second.at(i) = program.FindUniform(k_MeshUniformNames.at(i));
	}
	return found->second;
}

namespace
{
/// How far a building's inner walls stand in from its outer ones, in the model's own units: less for a two-sided
/// material
constexpr float k_InnerWallTwoSided = 0.2f;
constexpr float k_InnerWall = 0.35f;

[[nodiscard]] float InsetOf(bool twoSided)
{
	return twoSided ? k_InnerWallTwoSided : k_InnerWall;
}

/// The most caps kept at once: past it they are all made afresh as they are needed
constexpr size_t k_MostCaps = 512;
} // namespace

const Renderer::Cap& Renderer::CapOf(const L3DSubMesh& subMesh, const CapPrimitive& primitive, float height,
                                     const std::optional<glm::mat4>& landPlacement) const
{
	if (landPlacement.has_value())
	{
		return LandCapOf(subMesh, primitive, height, *landPlacement);
	}
	const auto key = std::make_pair(primitive.key, height);
	if (const auto found = _caps.find(key); found != _caps.end())
	{
		return found->second;
	}
	if (_caps.size() >= k_MostCaps)
	{
		_caps.clear();
	}
	const auto& surface = subMesh.GetSurface();
	std::span<const uint16_t> indices(surface.indices);
	const auto first = std::min<size_t>(primitive.indicesOffset, indices.size());
	indices = indices.subspan(first, std::min<size_t>(primitive.indicesCount, indices.size() - first));
	Cap cap {
	    .drawsBelow = partial_build_cap::DrawsAnythingBelow(surface.positions, indices, height),
	    .vertices = partial_build_cap::Build(surface.positions, surface.uvs, surface.normals, indices, height,
	                                         InsetOf(primitive.twoSided)),
	};
	return _caps.emplace(key, std::move(cap)).first->second;
}

const Renderer::Cap& Renderer::LandCapOf(const L3DSubMesh& subMesh, const CapPrimitive& primitive, float height,
                                         const glm::mat4& placement) const
{
	// The land under a model can differ from frame to frame, so a cap of a model that follows the land lasts one frame
	const auto key = std::make_tuple(primitive.key, height, placement[3].x, placement[3].y, placement[3].z);
	if (const auto found = _landCaps.find(key); found != _landCaps.end())
	{
		return found->second;
	}
	const auto& surface = subMesh.GetSurface();
	std::span<const uint16_t> indices(surface.indices);
	const auto first = std::min<size_t>(primitive.indicesOffset, indices.size());
	indices = indices.subspan(first, std::min<size_t>(primitive.indicesCount, indices.size() - first));
	const auto& land = Locator::terrainSystem::value();
	const auto risen = partial_build_cap::RisenWithLand(surface.positions, placement,
	                                                    [&land](glm::vec2 point) { return land.GetHeightAt(point); });
	Cap cap {
	    .drawsBelow = partial_build_cap::DrawsAnythingBelow(risen, indices, height),
	    .vertices = partial_build_cap::Build(risen, surface.uvs, surface.normals, indices, height, InsetOf(primitive.twoSided)),
	};
	return _landCaps.emplace(key, std::move(cap)).first->second;
}

bool Renderer::BindCap(bgfx::Encoder& encoder, const std::vector<partial_build_cap::CapVertex>& cap)
{
	// Laid out as a model's own vertices, each moved by the model's matrix alone
	struct Vertex
	{
		glm::vec3 position;
		glm::vec2 uv;
		glm::vec3 normal;
		std::array<int16_t, 4> indices;
	};
	static_assert(sizeof(Vertex) == 8 * sizeof(float) + 4 * sizeof(int16_t));
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Indices, 4, bgfx::AttribType::Int16, false, true)
	    .end();
	const auto count = static_cast<uint32_t>(cap.size());
	if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return false;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	auto* vertices = reinterpret_cast<Vertex*>(buffer.data);
	for (size_t i = 0; i < cap.size(); ++i)
	{
		// The first matrix, no partner to blend with
		vertices[i] = {.position = cap[i].position, .uv = cap[i].uv, .normal = cap[i].normal, .indices = {0, -1, 0, 0}};
	}
	encoder.setVertexBuffer(0, &buffer);
	return true;
}

void Renderer::DrawSubMesh(const graphics::L3DMesh& mesh, const graphics::L3DSubMesh& subMesh, const L3DMeshSubmitDesc& desc,
                           bool preserveState, const TextureHandle* subMeshTexture, glm::vec3 glow, glm::vec3 offset,
                           bool picture, bool control) const
{
	assert(&subMesh.GetMesh());
	// Physics meshes aren't drawn, nor the parts of other statuses (building stages, graves) or of another detail than
	// the one drawn. Windows fail the detail test unless their house lights them, which the object shader decides.
	const bool window = subMesh.GetFlags().isWindow;
	// A lit window is a faint glow, blended by its texture's alpha as its material says, over the wall behind it
	const bool materialBlending = desc.useMaterialBlending || window;
	const auto viewId = window ? TranslucentView(desc.viewId) : desc.viewId;
	const auto status = subMesh.GetFlags().status;
	const bool statusDrawn =
	    desc.statusesUpTo.has_value() ? status >= 1 && status <= *desc.statusesUpTo : status == desc.onlyStatus.value_or(0);
	if (!desc.drawAll &&
	    (subMesh.IsPhysics() || !statusDrawn || (!window && (subMesh.GetFlags().lodMask & desc.detailLevel) == 0)))
	{
		return;
	}

	const auto& island = Locator::terrainSystem::value();

	auto extent = island.GetExtent();
	auto islandExtent = glm::vec4(extent.minimum, extent.maximum);
	const auto& heightMap = island.GetHeightMap();

	auto const& skins = mesh.GetSkins();
	// The game draws the submeshes with a lightmap through it and the others as they are
	const auto lightmapSkinID = subMesh.GetLightmapSkinID();
	const Texture2D* lightmap = lightmapSkinID.has_value() ? GetTexture(*lightmapSkinID, skins) : nullptr;
	// A cap has no lightmap coordinates of its own
	// The temple's pictures have lightmaps, but the game draws them without
	// A draw uploads all the bones that its program declares, so a mesh with fewer bones uses a program that declares
	// fewer. This keeps a frame with many meshes inside the uniform buffer of Vulkan.
	const auto* program = desc.lightmapProgram != nullptr && lightmap != nullptr && !desc.cap && !picture
	                          ? desc.lightmapProgram
	                          : &desc.program->ForBones(desc.matrixCount);
	if (desc.onlyJoints && !subMesh.GetJoint().has_value())
	{
		return;
	}
	if (const auto& joint = subMesh.GetJoint();
	    desc.hideShutJoints && joint.has_value() &&
	    (joint->index >= desc.joints.size() || desc.joints[joint->index] == glm::mat4(1.0f)))
	{
		return;
	}

	// The game turns a submesh with a joint about its pivot by its matrix of the table, before the mesh's own matrix
	const auto* modelMatrices = desc.modelMatrices;
	glm::mat4 jointModel;
	if (const auto& joint = subMesh.GetJoint();
	    joint.has_value() && joint->index < desc.joints.size() && modelMatrices != nullptr && desc.matrixCount == 1)
	{
		jointModel = *modelMatrices * glm::translate(glm::mat4(1.0f), joint->pivot) * desc.joints[joint->index] *
		             glm::translate(glm::mat4(1.0f), -joint->pivot);
		modelMatrices = &jointModel;
	}
	// A held button of the temple moves into the wall. The movement is in the mesh, after the joint turns the submesh.
	glm::mat4 offsetModel;
	if (offset != glm::vec3(0.0f) && modelMatrices != nullptr && desc.matrixCount == 1)
	{
		offsetModel = *modelMatrices * glm::translate(glm::mat4(1.0f), offset);
		modelMatrices = &offsetModel;
	}
	// A creature's body takes its blended skins in place of its base mesh's. A draw given a skin of its own (the
	// temple's icons) binds that for every skinned primitive, and a submesh drawn with a texture of its own (the
	// citadel's leash collars) never samples its primitives' skins, which its mesh may not even have, so neither looks
	// them up.
	const auto skinOf = [&desc, &skins, subMeshTexture](uint32_t skinID) -> const Texture2D* {
		if (subMeshTexture != nullptr || (desc.skinTexture != nullptr && skinID != 0xFFFFFFFF))
		{
			return nullptr;
		}
		if (desc.morphTargets != nullptr)
		{
			const auto& blended = desc.morphTargets->skins;
			const auto found = std::ranges::find_if(blended, [skinID](const auto& skin) { return skin.first == skinID; });
			if (found != blended.end())
			{
				return found->second;
			}
		}
		return GetTexture(skinID, skins);
	};
	// The encoder that the draws go into
	auto& to = desc.encoder != nullptr ? *desc.encoder : _shaderManager->GetMainEncoder();
	// The program's uniforms by handle: a draw sets them for every primitive
	const auto& uniforms = MeshUniformsOf(*program);
	const auto has = [&uniforms](MeshUniform uniform) { return uniforms[static_cast<size_t>(uniform)].has_value(); };
	// Set by handle where the program has the uniform, or by name for its warning where it hasn't
	const auto setUniform = [&uniforms, &to, program](MeshUniform uniform, const void* value) {
		if (const auto& handle = uniforms[static_cast<size_t>(uniform)])
		{
			program->SetUniformValue(to, *handle, value);
		}
		else
		{
			program->SetUniformValue(to, k_MeshUniformNames[static_cast<size_t>(uniform)].data(), value);
		}
	};
	const auto setSampler = [&uniforms, &to, program](MeshUniform sampler, uint8_t stage, const auto& texture) {
		if (const auto& handle = uniforms[static_cast<size_t>(sampler)])
		{
			program->SetTextureSampler(to, *handle, stage, texture);
		}
		else
		{
			program->SetTextureSampler(to, k_MeshUniformNames[static_cast<size_t>(sampler)].data(), stage, texture);
		}
	};

	bool lastPreserveState = false;
	// The textures and render state of the draw, which a primitive drawn with the last one's state keeps
	draw_order::Bindings bindings {};
	uint64_t drawState = 0;
	const auto& primitives = subMesh.GetPrimitives();
	// Each primitive's skin is looked up once, as the next one's and then as its own
	const Texture2D* nextTexture = primitives.empty() ? nullptr : skinOf(primitives.front().skinID);
	for (auto it = primitives.begin(); it != primitives.end(); ++it)
	{
		// A material that the game changed while it ran draws as the changed material
		const auto changedPrim =
		    desc.changedMaterials.has_value() ? std::optional(L3DSubMesh::Changed(*it, *desc.changedMaterials)) : std::nullopt;
		const auto& prim = changedPrim.has_value() ? *changedPrim : *it;
		// The alpha textured materials, which a desc may have added over what is behind them
		constexpr uint32_t k_AlphaTextured = 4;
		constexpr uint32_t k_AlphaTexturedAlpha = 5;
		const bool addedAlphaTextured =
		    desc.alphaTexturedAdditive && (prim.materialType == k_AlphaTextured || prim.materialType == k_AlphaTexturedAlpha);
		// Those that shine in a colour of their own, solid on their second group of vertices
		const bool shining = addedAlphaTextured && desc.shineColour.has_value() && prim.secondGroupBone != L3DSubMesh::k_NoBone;
		// A see-through object draws each material in the mode of the game's second table
		const bool seeThrough = desc.seeThrough && desc.instanceAlpha && !desc.fizz.has_value();
		const auto seeThroughMode = render_modes::SeeThrough(
		    static_cast<render_modes::Mode>(std::min(prim.materialType, render_modes::k_ModeCount - 1)));
		const auto& seeThroughDesc = render_modes::Desc(seeThroughMode);

		const bool hasNext = std::next(it) != primitives.end();

		const Texture2D* texture = nextTexture;
		nextTexture = !hasNext ? nullptr : skinOf(std::next(it)->skinID);

		// Primitives drawn with their own material's blending can't share render state, nor can a submesh with a texture
		// of its own
		// nor inner walls, whose inset each primitive's material decides
		// nor primitives that may each take the environment map or a sliding texture in their own way
		// nor the submeshes of a mesh that moves some of its submeshes
		// nor the primitives of a see-through object, whose mode each primitive's material decides
		const bool primitivePreserveState = !materialBlending && !seeThrough && subMeshTexture == nullptr &&
		                                    desc.subMeshGlows.empty() && desc.subMeshOffsets.empty() && !desc.innerWalls &&
		                                    !desc.cap && desc.environment == nullptr && !desc.uvOffsetByMaterial &&
		                                    texture != nullptr && texture == nextTexture && (preserveState || hasNext);

		// A cut building's inner walls and cap belong only to a primitive with a whole triangle below the cut, and a cap
		// is drawn from geometry of its own, which a primitive not cut has none of
		const std::vector<partial_build_cap::CapVertex>* cap = nullptr;
		if ((desc.innerWalls || desc.cap) && desc.modelCutHeight.has_value())
		{
			const auto& made = CapOf(subMesh,
			                         {.key = &prim,
			                          .indicesOffset = prim.indicesOffset,
			                          .indicesCount = prim.indicesCount,
			                          .twoSided = prim.twoSided},
			                         *desc.modelCutHeight, desc.landPlacement);
			if (!made.drawsBelow || (desc.cap && made.vertices.empty()))
			{
				lastPreserveState = false;
				continue;
			}
			cap = desc.cap ? &made.vertices : nullptr;
		}

		uint32_t skip = Mesh::SkipState::SkipNone;
		if (!lastPreserveState)
		{
			if (modelMatrices != nullptr && desc.matrixCount > 0)
			{
				to.encoder->setTransform(modelMatrices, desc.matrixCount);
			}
			if (desc.boneTexture != nullptr)
			{
				program->SetUniformValue(to, "u_bones", &desc.bones);
				program->SetTextureSampler(to, "s_bones", 2, *desc.boneTexture);
			}
			if (has(MeshUniform::DepthBias))
			{
				const glm::vec4 u_depthBias {desc.depthBias, 0.0f, 0.0f, 0.0f};
				setUniform(MeshUniform::DepthBias, &u_depthBias);
			}
			if (has(MeshUniform::Tint))
			{
				setUniform(MeshUniform::Tint, &desc.tint);
			}
			if (has(MeshUniform::Glow))
			{
				// A control's glow takes the place of the light's colour added
				const glm::vec4 u_glow {glow != glm::vec3(0.0f) ? glow : desc.lightAdd, desc.environmentOnlyAlpha};
				setUniform(MeshUniform::Glow, &u_glow);
			}
			if (has(MeshUniform::Darkening))
			{
				// w: 1 for a control, whose colour the white of its control takes down a step
				const glm::vec4 u_darkening {1.0f - desc.lightMultiply, control ? 1.0f : 0.0f};
				setUniform(MeshUniform::Darkening, &u_darkening);
			}
			if (desc.morphTargets != nullptr && has(MeshUniform::MorphWeights))
			{
				const glm::vec4 u_morphWeights {desc.morphTargets->weights, 0.0f};
				setUniform(MeshUniform::MorphWeights, &u_morphWeights);
			}
			if (desc.morphTargets != nullptr && has(MeshUniform::VertexBlend))
			{
				BindBlendSources(to, mesh, subMesh, *desc.morphTargets, *program);
				BindCreatureSpellLooks(to, *program);
			}
			else if (desc.fizz.has_value() || desc.freeze > 0.0f)
			{
				BindCreatureSpellLooks(to, *program);
			}
			// Set for every mesh, so that none is drawn with another's fizz
			if (has(MeshUniform::CreatureSpellLook))
			{
				const auto u_creatureSpellLook = CreatureSpellLookOf(desc.fizz, desc.freeze, desc.iceShown);
				setUniform(MeshUniform::CreatureSpellLook, &u_creatureSpellLook);
			}
			if (has(MeshUniform::ShadeAt))
			{
				setUniform(MeshUniform::ShadeAt, &desc.shadeAt);
			}
			// Each primitive takes the environment map as its material says, and its texture slides with the skin
			// where it may; a frozen thing's ice is the map alone over every primitive
			const bool environmentMapped = desc.environment != nullptr && desc.environmentOnlyAlpha <= 0.0f;
			const auto environmentStage =
			    environmentMapped
			        ? desc.environmentStageForAll.value_or(material_stages::EnvironmentStageOf(prim.materialFlags))
			        : material_stages::EnvironmentStage::Added;
			if (has(MeshUniform::UvOffset))
			{
				const bool slides =
				    !desc.uvOffsetByMaterial || material_stages::TextureSlides(prim.materialFlags, environmentMapped);
				const glm::vec4 u_uvOffset {slides ? desc.uvOffset : glm::vec2(0.0f), desc.uvScale, 0.0f};
				setUniform(MeshUniform::UvOffset, &u_uvOffset);
			}
			if (has(MeshUniform::EnvironmentStage))
			{
				const glm::vec4 u_environmentStage {static_cast<float>(environmentStage), 0.0f, 0.0f, 0.0f};
				setUniform(MeshUniform::EnvironmentStage, &u_environmentStage);
			}
			if (has(MeshUniform::KeepBelow))
			{
				// The cap lies on the cut itself, so it isn't cut
				const bool keepBelow = desc.cutAbove.has_value() && cap == nullptr;
				const glm::vec4 u_keepBelow {keepBelow ? 1.0f : 0.0f, desc.cutAbove.value_or(0.0f), 0.0f, 0.0f};
				setUniform(MeshUniform::KeepBelow, &u_keepBelow);
			}
			if (has(MeshUniform::Inset))
			{
				// y: 1 for the cap, which is set in already and drawn unlit
				const glm::vec4 u_inset {desc.innerWalls ? desc.innerWallInset.value_or(InsetOf(prim.twoSided)) : 0.0f,
				                         cap != nullptr ? 1.0f : 0.0f, 0.0f, 0.0f};
				setUniform(MeshUniform::Inset, &u_inset);
			}
			if (has(MeshUniform::SeaClip))
			{
				// The sea mirrors only what stands above it
				const bool reflection = desc.viewId == RenderPass::Reflection ||
				                        desc.viewId == RenderPass::ReflectionTranslucent ||
				                        desc.viewId == RenderPass::ReflectionMirrored;
				// and a mesh cut by a plane shows nothing below it
				float height = desc.cutBelow.value_or(0.0f);
				if (reflection)
				{
					height = std::max(height, 0.0f);
				}
				// z: 1 to blend by each instance's alpha rather than dissolve it. w: 1 to multiply the texture's alpha by
				// the instance's before the alpha test, as the changed materials do.
				const bool alphaBeforeTest = desc.changedMaterials.has_value() || (seeThrough && seeThroughDesc.alphaModulate);
				const glm::vec4 u_seaClip {reflection || desc.cutBelow.has_value() ? 1.0f : 0.0f, height,
				                           desc.instanceAlpha || shining ? 1.0f : 0.0f, alphaBeforeTest ? 1.0f : 0.0f};
				setUniform(MeshUniform::SeaClip, &u_seaClip);
			}
			if (has(MeshUniform::Snow))
			{
				// Snow shows where the primitive writes its depth, as the game draws it over the object at the same depth
				const auto* depth = desc.snow ? SnowDepth(_snowDepth, _snowRevision) : nullptr;
				const bool snowed =
				    depth != nullptr && prim.depthWrite && _snowTexture != nullptr && _snowAlphaTexture != nullptr;
				bindings.snowed = snowed;
				// y: 1 when the submesh has bones. Then the snow is read by the normal of each vertex in its file, not by
				// the normal of the posed vertex.
				// z: 1 for the faces of a broken shape. Their snow is read by where each corner is in the world and by
				// the normal of its face, and shows from a different alpha.
				// w: 1 for a fence, which reads the snow by each face
				const glm::vec4 u_snow {snowed ? 1.0f : 0.0f, subMesh.GetFlags().hasBones ? 1.0f : 0.0f,
				                        subMesh.IsMade() ? 1.0f : 0.0f, desc.snowByFace ? 1.0f : 0.0f};
				setUniform(MeshUniform::Snow, &u_snow);
				if (snowed)
				{
					setSampler(MeshUniform::SnowDepth, 11, *depth);
					setSampler(MeshUniform::SnowTexture, 12, *_snowTexture);
					setSampler(MeshUniform::SnowAlpha, 13, *_snowAlphaTexture);
				}
				// Without snow its samplers get the program's white defaults when submitted
			}
			if (has(MeshUniform::Window))
			{
				// Window submeshes are lit by their houses at night
				glm::vec4 u_window {subMesh.GetFlags().isWindow ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f};
				if (Locator::skyClockSystem::has_value())
				{
					const auto& clock = Locator::skyClockSystem::value().GetClock();
					u_window.y = clock.GetVisualTime();
					u_window.z = clock.IsVisualNight() ? 1.0f : 0.0f;
				}
				setUniform(MeshUniform::Window, &u_window);
			}
			if (has(MeshUniform::Diffuse))
			{
				// A primitive without a skin would otherwise sample whichever texture the draw before it left bound,
				// changing as bgfx orders the draws. The sky has none either, but binds the sky's texture for it.
				bindings.diffuse = UINT16_MAX;
				if (subMeshTexture != nullptr)
				{
					setSampler(MeshUniform::Diffuse, 0, *subMeshTexture);
					bindings.diffuse = toBgfx(*subMeshTexture).idx;
				}
				else if (desc.skinTexture != nullptr && prim.skinID != 0xFFFFFFFF)
				{
					setSampler(MeshUniform::Diffuse, 0, *desc.skinTexture);
					bindings.diffuse = toBgfx(*desc.skinTexture).idx;
				}
				else if (texture != nullptr)
				{
					const auto diffuse =
					    desc.useMaterialAddressing && !material_stages::TextureRepeats(prim.materialFlags, false)
					        ? uniforms[static_cast<size_t>(MeshUniform::Diffuse)]
					        : std::nullopt;
					if (diffuse.has_value())
					{
						program->SetTextureSampler(to, *diffuse, 0, *texture, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
					}
					else
					{
						setSampler(MeshUniform::Diffuse, 0, *texture);
					}
					bindings.diffuse = toBgfx(texture->GetNativeHandle()).idx;
				}
				else if (!desc.isSky && _whiteTexture)
				{
					setSampler(MeshUniform::Diffuse, 0, *_whiteTexture);
					bindings.diffuse = toBgfx(*_whiteTexture).idx;
				}
			}
			bindings.lightmap = UINT16_MAX;
			if (program == desc.lightmapProgram)
			{
				setSampler(MeshUniform::Lightmap, 3, *lightmap);
				bindings.lightmap = toBgfx(lightmap->GetNativeHandle()).idx;
			}
			if (desc.environment != nullptr && has(MeshUniform::Environment))
			{
				setSampler(MeshUniform::Environment, 5, *desc.environment);
			}
			if (desc.bonePalette != nullptr && has(MeshUniform::BonePalette))
			{
				setSampler(MeshUniform::BonePalette, 2, *desc.bonePalette); // vs
			}
			if (desc.morphWithTerrain)
			{
				setSampler(MeshUniform::Heightmap, 1, heightMap); // vs
			}
			// The land's extent is for the land's height under a model that follows the land, and for the land's light.
			// bgfx accepts a uniform only once for each draw, so it is set one time for the two.
			if (desc.morphWithTerrain || has(MeshUniform::LandLight))
			{
				setUniform(MeshUniform::IslandExtent, &islandExtent);
			}
			if (has(MeshUniform::LandLight))
			{
				// Objects in the world take the colour of the land's light where they stand; the sky and the temple's
				// insides have lights of their own
				const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
				const bool landLit =
				    desc.landLightAndHaze && !desc.isSky && !desc.drawAll && !inTemple && _landLightTexture.has_value();
				// Below Medium detail the sun lights only the creatures and the hand
				const bool unlit =
				    desc.unlit || picture || (!desc.alwaysLit && !Locator::config::value().detail.objectLighting);
				const glm::vec4 u_landLight {landLit ? 1.0f : 0.0f, desc.landLightScale, unlit ? 1.0f : 0.0f,
				                             desc.landColourAdded ? 0.0f : 1.0f};
				// A held thing mirrored in the sea takes the land's light of the frame before
				const bool before = desc.heldMirror.has_value() && _before.texturesKept;
				setSampler(MeshUniform::LandLuminosity, 6, before ? GetLandLuminosityBefore() : GetLandLuminosity());
				// The explorers' boat in the sea takes only the land's colour of the frame before
				if ((before || desc.landColourBefore) && _before.texturesKept)
				{
					setSampler(MeshUniform::LandColour, 8, *_before.colour);
				}
				else
				{
					setSampler(MeshUniform::LandColour, 8, GetLandColour());
				}
				setSampler(MeshUniform::LandLightTexture, 7,
				           desc.heldMirror.has_value() && _before.landLight.has_value() ? *_before.landLight
				                                                                        : GetLandLightTexture());
				setUniform(MeshUniform::LandLight, &u_landLight);
			}
			if (has(MeshUniform::Haze))
			{
				// The distance haze is the world's, not the sky's or the temple's. The temple has a depth fog of its own.
				const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
				const bool hazed = desc.landLightAndHaze && desc.hazed && !desc.isSky && !desc.drawAll && !inTemple;
				if (inTemple && desc.templeFog)
				{
					const auto& fog = Locator::temple::value().GetFog();
					const auto u_haze = fog.Uniform();
					const glm::vec4 u_hazeColour {fog.colour, 0.0f};
					setUniform(MeshUniform::Haze, &u_haze);
					setUniform(MeshUniform::HazeColour, &u_hazeColour);
				}
				else
				{
					// A held thing mirrored in the sea takes the haze of the frame before
					const auto& haze = desc.heldMirror.has_value() ? _before.haze.Or(_haze) : _haze;
					const auto u_haze = hazed ? haze[0] : glm::vec4(0.0f);
					setUniform(MeshUniform::Haze, &u_haze);
					setUniform(MeshUniform::HazeColour, &haze[1]);
				}
			}
			if (has(MeshUniform::FogAt))
			{
				// A room's wall without a lightmap is fogged once, at its first vertex
				glm::vec4 u_fogAt {0.0f};
				if (desc.fogFromFirstVertex.has_value() && program != desc.lightmapProgram &&
				    !subMesh.GetSurfacePoints().empty())
				{
					u_fogAt = glm::vec4(*desc.fogFromFirstVertex + subMesh.GetSurfacePoints().front().position, 1.0f);
				}
				// A held thing mirrored in the sea: w 2, x the distance of its haze, y the colour that multiplies its light (a
				// tree's grey of 1), z the colour added to it
				if (const auto& held = desc.heldMirror; held.has_value())
				{
					const float multiply =
					    held->treeGrey > 0 ? static_cast<float>(held->treeGrey) / 256.0f : static_cast<float>(held->tint);
					// A tree has no colour added: z is the factor that brings its width back, 0 for none
					const float added = desc.heldTreeWidth != 1.0f ? desc.heldTreeWidth : static_cast<float>(held->added);
					u_fogAt = glm::vec4(held->viewDepth, multiply, added, 2.0f);
				}
				setUniform(MeshUniform::FogAt, &u_fogAt);
			}
			if (has(MeshUniform::ModelLight))
			{
				// A mesh given a light of its own keeps the frame's ambient
				const auto modelLight = desc.modelLight.has_value() ? glm::vec4(*desc.modelLight, _modelLight.w) : _modelLight;
				setUniform(MeshUniform::ModelLight, &modelLight);
			}
			if (has(MeshUniform::GroundShadowInfo))
			{
				// The creatures' and the hand's shadows fall on what stands on the land, not on the creatures, nor in
				// the reflection
				const bool mainView = desc.viewId == RenderPass::Main || desc.viewId == RenderPass::Translucent;
				const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
				SetObjectGroundShadowUniforms(to, *program,
				                              desc.groundShadows && Locator::config::value().detail.objectShadows && mainView &&
				                                  !inTemple && !desc.isSky && !desc.drawAll && desc.morphTargets == nullptr);
			}
			if (has(MeshUniform::LightRule))
			{
				// x: 1 for the light the game gives the models it lights as it poses them. y: 1 for the land's light of
				// the place on the map, with no distance haze.
				const glm::vec4 u_lightRule {desc.posedModelLight ? 1.0f : 0.0f,
				                             graphics::object_land_light::UniformValue(desc.landLightRule), 0.0f, 0.0f};
				setUniform(MeshUniform::LightRule, &u_lightRule);
			}
			if (has(MeshUniform::ObjectLook))
			{
				// xyz: the object's own colour, w: its alpha; w 0 for none, which is lit by the land as usual
				// A shining light's colour is its own
				glm::vec4 u_objectLook = shining ? glm::vec4(*desc.shineColour, 1.0f)
				                         : desc.objectLook.has_value()
				                             ? glm::vec4(desc.objectLook->colour, desc.objectLook->alpha)
				                             : glm::vec4(0.0f);
				// A control's white colour multiplies the object's colour in whole steps, so 255 becomes 254
				if ((control || picture) && desc.objectLook.has_value())
				{
					u_objectLook = glm::vec4(glm::floor(desc.objectLook->colour * 255.0f / 256.0f), desc.objectLook->alpha);
				}
				setUniform(MeshUniform::ObjectLook, &u_objectLook);
			}
			if (has(MeshUniform::Shine))
			{
				// x: 1 for a light that shines in a colour of its own, y: the bone its solid vertices are on, zw: the cosine
				// and sine of its texture's turn
				glm::vec4 u_shine {0.0f};
				if (shining)
				{
					const auto realTime =
					    Locator::time::has_value() ? Locator::time::value().GetElapsedTime() : std::chrono::milliseconds(0);
					const auto turn = light_shine::TextureTurn(realTime);
					u_shine = glm::vec4(1.0f, static_cast<float>(prim.secondGroupBone), static_cast<float>(std::cos(turn)),
					                    static_cast<float>(std::sin(turn)));
				}
				setUniform(MeshUniform::Shine, &u_shine);
			}
			if (!desc.isSky && has(MeshUniform::SkyAlphaThreshold))
			{
				// z: 0 for a primitive that is not see-through. A see-through primitive gives 1 for no alpha test, 2 for
				// a test against the reference in w, and 3 for a test against w lowered by the alpha of each instance.
				float seeThroughTest = 0.0f;
				float seeThroughRef = 0.0f;
				if (seeThrough)
				{
					seeThroughTest = static_cast<float>(see_through::TestOf(seeThroughMode));
					seeThroughRef = desc.seeThroughRef.has_value() ? static_cast<float>(*desc.seeThroughRef)
					                                               : std::round(prim.alphaCutoutThreshold * 255.0f);
				}
				const glm::vec4 u_skyAlphaThreshold = {
				    Locator::skyClockSystem::value().GetCurrentSkyType(),
				    desc.alphaThreshold.value_or(prim.thresholdAlpha || desc.materialAlphaTest ? prim.alphaCutoutThreshold
				                                                                               : 0.0f),
				    seeThroughTest,
				    seeThroughRef,
				};
				setUniform(MeshUniform::SkyAlphaThreshold, &u_skyAlphaThreshold);
			}
		}
		else
		{
			skip |= Mesh::SkipState::SkipRenderState;
			skip |= Mesh::SkipState::SkipVertexBuffer;
		}

		{
			if (desc.instanceDesc != nullptr && (skip & Mesh::SkipState::SkipInstanceBuffer) == 0)
			{
				to.encoder->setInstanceDataBuffer(toBgfx(desc.instanceDesc->GetRawHandle()), desc.instanceDesc->GetStart(),
				                                  desc.instanceDesc->GetCount());
			}
			if (cap != nullptr)
			{
				if (!BindCap(*to.encoder, *cap))
				{
					lastPreserveState = false;
					continue;
				}
			}
			else if (subMesh.GetMesh().IsIndexed() && (skip & Mesh::SkipState::SkipIndexBuffer) == 0)
			{
				subMesh.GetMesh().GetIndexBuffer().Bind(*to.encoder, prim.indicesCount, prim.indicesOffset);
			}
			if (cap == nullptr && (skip & Mesh::SkipState::SkipVertexBuffer) == 0)
			{
				subMesh.GetMesh().GetVertexBuffer().Bind(*to.encoder);
				if (desc.morphTargets != nullptr && _morphStreamLayouts)
				{
					BindMorphTargets(*to.encoder, mesh, subMesh, *desc.morphTargets, *_morphStreamLayouts);
				}
			}
			if ((skip & Mesh::SkipState::SkipRenderState) == 0)
			{
				auto state = desc.state;
				using FizzPass = L3DMeshSubmitDesc::Fizz::Pass;
				if (desc.fizz.has_value() && desc.fizz->pass != FizzPass::Body)
				{
					// A fizzing creature's depth through the static and its ice cull their back faces, whatever its materials
					// say
					state &= ~BGFX_STATE_CULL_MASK;
					state |= desc.mirrored ? BGFX_STATE_CULL_CW : BGFX_STATE_CULL_CCW;
				}
				else if (desc.twoSided)
				{
					state &= ~BGFX_STATE_CULL_MASK;
				}
				else if (desc.useMaterialCulling)
				{
					// L3D meshes face clockwise
					state &= ~BGFX_STATE_CULL_MASK;
					if (!prim.twoSided)
					{
						state |= desc.mirrored ? BGFX_STATE_CULL_CW : BGFX_STATE_CULL_CCW;
					}
				}
				// A fizzing creature's passes keep the blending they are given
				if (materialBlending && !desc.fizz.has_value())
				{
					using BlendMode = decltype(prim.blend);
					const bool swapped = addedAlphaTextured;
					switch (swapped ? BlendMode::Additive : prim.blend)
					{
					case BlendMode::Standard:
						state |= BGFX_STATE_BLEND_ALPHA;
						break;
					case BlendMode::Additive:
						state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);
						break;
					case BlendMode::JustZ:
						state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ZERO, BGFX_STATE_BLEND_ONE);
						break;
					case BlendMode::Disabled:
						// An object fading out blends what would be opaque by its alpha
						if (desc.objectLook.has_value() && desc.objectLook->alpha < 1.0f)
						{
							state |= BGFX_STATE_BLEND_ALPHA;
						}
						break;
					}
					if (!prim.depthWrite || swapped)
					{
						state &= ~BGFX_STATE_WRITE_Z;
					}
				}
				auto rgba = desc.rgba;
				if (seeThrough)
				{
					// The mode of the second table sets the blending and the depth write
					state &= ~(BGFX_STATE_BLEND_MASK | BGFX_STATE_WRITE_Z);
					state |= see_through::State(seeThroughDesc);
				}
				else if (desc.instanceAlpha && !desc.fizz.has_value())
				{
					state &= ~BGFX_STATE_BLEND_MASK;
					state |= BGFX_STATE_BLEND_ALPHA;
				}
				if (desc.additiveShare.has_value() && !desc.fizz.has_value())
				{
					// Added by a constant share, as the game draws a one-shot miracle's bubble
					state &= ~BGFX_STATE_BLEND_MASK;
					state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_FACTOR, BGFX_STATE_BLEND_ONE);
					const auto byte = static_cast<uint32_t>(std::clamp(*desc.additiveShare, 0.0f, 1.0f) * 255.0f);
					rgba = (byte << 24) | (byte << 16) | (byte << 8) | byte;
				}
				to.encoder->setState(state, rgba);
				drawState = state;
			}

			// Opaque draws whose order doesn't show are gathered by the textures they bind
			const auto sortDepth =
			    draw_order::Reorderable(viewId, drawState, desc.sortDepth) ? draw_order::GroupKey(bindings) : desc.sortDepth;
			program->Submit(to, static_cast<bgfx::ViewId>(viewId), sortDepth,
			                primitivePreserveState ? BGFX_DISCARD_NONE : BGFX_DISCARD_ALL);
		}
		lastPreserveState = primitivePreserveState;
	}
}

void Renderer::DrawMesh(const graphics::L3DMesh& mesh, const L3DMeshSubmitDesc& desc, uint8_t subMeshIndex) const noexcept
{
	// Some of the game's meshes hold no geometry at all, like the singing stones' centre: there is nothing to draw
	if (mesh.GetNumSubMeshes() == 0)
	{
		return;
	}

	const auto& subMeshes = mesh.GetSubMeshes();

	if (subMeshIndex != std::numeric_limits<uint8_t>::max())
	{
		if (subMeshIndex >= mesh.GetNumSubMeshes())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "tried to draw submesh out of range ({}/{})", subMeshIndex,
			                   mesh.GetNumSubMeshes());
		}

		DrawSubMesh(mesh, *subMeshes[subMeshIndex], desc, false);
		return;
	}

	// The state carries on from one submesh to the next, up to the last drawn
	const auto isDrawn = [&desc](uint32_t index) {
		return std::ranges::find(desc.hiddenSubMeshes, index) == desc.hiddenSubMeshes.end();
	};
	auto lastDrawn = static_cast<uint32_t>(subMeshes.size());
	for (auto index = static_cast<uint32_t>(subMeshes.size()); index > 0; --index)
	{
		if (isDrawn(index - 1))
		{
			lastDrawn = index - 1;
			break;
		}
	}
	for (auto it = subMeshes.begin(); it != subMeshes.end(); ++it)
	{
		const L3DSubMesh& subMesh = **it;
		const auto index = static_cast<uint32_t>(std::distance(subMeshes.begin(), it));
		if (!isDrawn(index))
		{
			continue;
		}
		const auto found = std::ranges::find(desc.subMeshTextures, index, &std::pair<uint32_t, TextureHandle>::first);
		const auto glow = std::ranges::find(desc.subMeshGlows, index, &std::pair<uint32_t, glm::vec3>::first);
		const auto offset = std::ranges::find(desc.subMeshOffsets, index, &std::pair<uint32_t, glm::vec3>::first);
		const bool picture = found != desc.subMeshTextures.end() &&
		                     std::ranges::find(desc.pictureSubMeshes, index) != desc.pictureSubMeshes.end();
		DrawSubMesh(mesh, subMesh, desc, index != lastDrawn, found != desc.subMeshTextures.end() ? &found->second : nullptr,
		            glow != desc.subMeshGlows.end() ? glow->second : glm::vec3(0.0f),
		            offset != desc.subMeshOffsets.end() ? offset->second : glm::vec3(0.0f), picture,
		            std::ranges::find(desc.controlSubMeshes, index) != desc.controlSubMeshes.end());
	}
}

namespace
{
/// The runs of things that each lane encodes about, so that a lane that is done early takes another run
constexpr size_t k_RunsPerLane = 4;
/// Fewer things than this the game's thread draws alone: waking the threads would take longer
constexpr size_t k_LeastThingsOnThreads = 16;

/// Drops everything set on the encoder for the next draw, as a draw that drops all its state does
void Clean(DrawEncoder& to)
{
	to.encoder->discard(BGFX_DISCARD_ALL);
	to.stages.Clear();
}
} // namespace

size_t Renderer::EncodeLanes() const
{
#if defined(__EMSCRIPTEN__)
	// The web build encodes on one thread
	return 1;
#else
	// A warm-up's draw window counts the draws of the game's thread alone
	if (!Locator::config::has_value() || !Locator::config::value().parallelDrawEncode || _warmingUp ||
	    _shaderManager->IsDrawWindowOpen())
	{
		return 1;
	}
	// Each lane but the game's thread takes an encoder of its own from bgfx
	const auto encoders = static_cast<size_t>(bgfx::getCaps()->limits.maxEncoders);
	return std::min(_workers->GetLaneCount(), encoders);
#endif
}

void Renderer::EncodeInOrder(size_t count, const EncodeDraw& draw) const
{
	auto& main = _shaderManager->GetMainEncoder();
	const size_t lanes = count >= k_LeastThingsOnThreads ? EncodeLanes() : 1;
	if (lanes <= 1)
	{
		for (size_t index = 0; index < count; ++index)
		{
			Clean(main);
			draw(index, main);
		}
		Clean(main);
		return;
	}

	// Run r has the things from count * r / runs on, and the submit order after run r - 1. Run 0 goes on in the game
	// thread's encoder, so that what the game's thread set before is set for its first draw, as when that thread draws
	// all. The snow's depth changes a texture: that is done here first, so that the draws only read it.
	static_cast<void>(SnowDepth(_snowDepth, _snowRevision));
	const size_t runs = std::min(count, lanes * k_RunsPerLane);
	const uint32_t firstOrder = ++_submitOrder;
	_submitOrder = firstOrder + static_cast<uint32_t>(runs);
	const auto runRange = [count, runs](size_t run) { return std::pair {count * run / runs, count * (run + 1) / runs}; };
	const auto encodeRun = [&](size_t run, DrawEncoder& to) {
		to.encoder->setSubmitOrder(firstOrder + static_cast<uint32_t>(run));
		const auto [first, last] = runRange(run);
		for (size_t index = first; index < last; ++index)
		{
			Clean(to);
			draw(index, to);
		}
	};
	const auto encodeRunOn = [&](size_t run, bgfx::Encoder* encoder) {
		DrawEncoder to {.encoder = encoder};
		encodeRun(run, to);
	};

	std::vector<bgfx::Encoder*> encoders(lanes, nullptr);
	std::atomic<bool> firstRunTaken {false};
	std::atomic<size_t> nextRun {1};
	_workers->ForEach(lanes, [&](size_t /*index*/, size_t lane) {
		if (lane == 0)
		{
			if (!firstRunTaken.exchange(true))
			{
				encodeRun(0, main);
			}
		}
		else if (lane < encoders.size() && encoders[lane] == nullptr)
		{
			encoders[lane] = bgfx::begin(true);
		}
		auto* encoder = lane == 0 ? main.encoder : (lane < encoders.size() ? encoders[lane] : nullptr);
		if (encoder == nullptr)
		{
			// bgfx had no encoder left for this thread: the others draw its runs
			return;
		}
		for (auto run = nextRun.fetch_add(1); run < runs; run = nextRun.fetch_add(1))
		{
			encodeRunOn(run, encoder);
		}
	});
	// The game's thread draws what no thread took
	if (!firstRunTaken.exchange(true))
	{
		encodeRun(0, main);
	}
	for (auto run = nextRun.fetch_add(1); run < runs; run = nextRun.fetch_add(1))
	{
		encodeRunOn(run, main.encoder);
	}
	for (auto* encoder : encoders)
	{
		if (encoder != nullptr)
		{
			bgfx::end(encoder);
		}
	}
	main.encoder->setSubmitOrder(_submitOrder);
	Clean(main);
}

namespace openblack::graphics::detail
{
std::optional<InstanceHolder> HolderOf(const ecs::systems::RenderContext& context, uint32_t instance)
{
	for (const bool fading : {false, true})
	{
		for (const auto& [meshId, placers] : fading ? context.fadingDrawDescs : context.instancedDrawDescs)
		{
			if (instance >= placers.offset && instance < placers.offset + placers.count)
			{
				return InstanceHolder {.mesh = meshId, .placers = &placers, .fading = fading};
			}
		}
	}
	return std::nullopt;
}
} // namespace openblack::graphics::detail

void Renderer::UploadAnimalBones(const DrawSceneDesc& drawDesc) const
{
	// The bones' texture is this many texels wide: 256 matrices a row
	constexpr uint32_t k_BoneTextureWidth = 1024;
	_animalBoneGroups.clear();
	_animalBones.clear();
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();
	struct Drawn
	{
		uint32_t instance;
		const ecs::components::AnimalPose* pose;
	};
	std::unordered_map<entt::id_type, std::vector<Drawn>> byModel;
	for (const auto& [entity, instance] : renderCtx.entityDraws)
	{
		const auto* pose = drawDesc.entities.TryGet<const ecs::components::AnimalPose>(entity);
		const auto holder = HolderOf(renderCtx, instance);
		if (pose != nullptr && holder.has_value() && !holder->fading)
		{
			byModel[holder->mesh].push_back({.instance = instance, .pose = pose});
		}
	}
	for (const auto& [meshId, animals] : byModel)
	{
		// Only a model all of whose instances placed this frame are animals, none fading, all in the same light and none
		// cut
		const auto placers = renderCtx.instancedDrawDescs.find(meshId);
		if (placers == renderCtx.instancedDrawDescs.end() || placers->second.filled != animals.size() ||
		    !meshes.Contains(meshId))
		{
			continue;
		}
		const auto model = meshes.Handle(meshId);
		const auto& rest = model->GetBoneMatrices();
		const auto light = animals.front().pose->light;
		// Those in a colour of their own or cut by a plane are drawn one by one
		if (!model->IsBoned() || rest.empty() || light == ecs::components::AnimalLight::Own ||
		    std::ranges::any_of(animals, [light](const Drawn& drawn) {
			    return drawn.pose->alpha < 255 || drawn.pose->light != light || drawn.pose->cutBelow.has_value();
		    }))
		{
			continue;
		}
		const auto first = static_cast<uint32_t>(_animalBones.size());
		const auto bones = static_cast<uint32_t>(rest.size());
		_animalBones.resize(_animalBones.size() + (static_cast<size_t>(bones) * animals.size()));
		for (const auto& drawn : animals)
		{
			// Each in its place among the model's instances, in the pose its clip gives it, or its model's own
			const auto& posed = drawn.pose->bones.size() == rest.size() ? drawn.pose->bones : rest;
			const auto place = first + ((drawn.instance - placers->second.offset) * bones);
			std::ranges::copy(posed, _animalBones.begin() + place);
		}
		_animalBoneGroups.emplace(meshId,
		                          AnimalBoneGroup {.firstMatrix = first,
		                                           .bones = bones,
		                                           .brightestLand = light == ecs::components::AnimalLight::BrightestLand,
		                                           .landWhereItStands = light == ecs::components::AnimalLight::Land});
	}
	if (_animalBones.empty())
	{
		return;
	}
	// A texture tall enough for every matrix, grown by doubling as the animals grow in number
	const auto texels = static_cast<uint32_t>(_animalBones.size()) * 4;
	const auto rows = (texels + k_BoneTextureWidth - 1) / k_BoneTextureWidth;
	if (!_animalBoneTexture || _animalBoneTexture->GetResolution().y < rows)
	{
		uint32_t height = 1;
		while (height < rows)
		{
			height *= 2;
		}
		_animalBoneTexture = std::make_unique<Texture2D>("Animal Bones");
		_animalBoneTexture->CreateWithinFrame(static_cast<uint16_t>(k_BoneTextureWidth), static_cast<uint16_t>(height), 1,
		                                      TextureFormat::RGBA32F, Wrapping::ClampEdge, Filter::Nearest, nullptr);
		// Each frame gives all the matrices, thus the graphics device keeps no copy
		_animalBoneTexture->SetTexelsGivenAgain();
	}
	const auto resolution = _animalBoneTexture->GetResolution();
	_animalBones.resize(static_cast<size_t>(resolution.x) * resolution.y / 4);
	_animalBoneTexture->Update(_animalBones.data(), static_cast<uint32_t>(_animalBones.size() * sizeof(glm::mat4)));
}

namespace
{
/// The hand the player moves, which the others follow in how they look
const ecs::components::HandMorph* PlayerHandMorph()
{
	if (!Locator::handSystem::has_value())
	{
		return nullptr;
	}
	const auto hand =
	    Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
	return Locator::entitiesRegistry::value().TryGet<const ecs::components::HandMorph>(hand);
}
} // namespace

void Renderer::UploadHandSkins() const
{
	const auto* morph = PlayerHandMorph();
	if (morph == nullptr || morph->skins.empty())
	{
		_handSkins.clear();
		return;
	}
	const painted_skin::Stamp stamp {.registryResets = Locator::entitiesRegistry::value().GetResets(),
	                                 .revision = morph->revision};
	if (!painted_skin::NeedsUpload(_handSkinStamp, stamp) && _handSkins.size() == morph->skins.size())
	{
		return;
	}
	_handSkinTextures.resize(morph->skins.size());
	_handSkins.clear();
	for (size_t i = 0; i < morph->skins.size(); ++i)
	{
		const auto& skin = morph->skins[i];
		auto& texture = _handSkinTextures[i];
		if (!texture)
		{
			// The skins are at half size at the two lowest detail levels
			const auto side = l3d_skin::SideOf(skin.texels.size());
			texture = std::make_unique<Texture2D>("Hand Skin");
			texture->CreateWithinFrame(side, side, 1, TextureFormat::BGRA4, Wrapping::Repeat, Filter::Linear, nullptr);
			// After a restore of the graphics device the skins go to their textures again
			texture->SetTexelsGivenAgain();
		}
		texture->Update(skin.texels.data(), static_cast<uint32_t>(skin.texels.size() * sizeof(skin.texels[0])));
		_handSkins.emplace_back(skin.id, texture.get());
	}
	_handSkinStamp = stamp;
}

std::optional<RendererInterface::L3DMeshSubmitDesc::MorphTargets> Renderer::HandMorphTargets() const
{
	const auto* morph = PlayerHandMorph();
	if (morph == nullptr || morph->state.drawn == 0.0f)
	{
		return std::nullopt;
	}
	// The evil or good mesh, or the base where it isn't loaded
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto lookId =
	    ecs::components::HandMorph::k_LookMeshIds.at(hand_morph::LookOf(morph->state.drawn) == hand_morph::Look::Evil ? 0 : 1);
	const auto id = meshes.Contains(lookId) ? lookId : ecs::components::Hand::k_MeshId;
	if (!meshes.Contains(id))
	{
		return std::nullopt;
	}
	return L3DMeshSubmitDesc::MorphTargets {
	    .meshes = {&*meshes.Handle(id), nullptr, nullptr},
	    .weights = {hand_morph::Weight(morph->state.drawn), 0.0f, 0.0f},
	    .skins = _handSkins,
	    // The hand has no seams to blend
	    .blendSeams = false,
	};
}
