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

#include <cstdint>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <LNDFile.h>
#include <bgfx/bgfx.h>
#include <glm/gtx/transform.hpp>

#include "3D/AllMeshes.h"
#include "3D/ChimneySmoke.h"
#include "3D/InfluenceCircle.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/Mists.h"
#include "3D/StormClouds.h"
#include "3D/TempleDoors.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/TempleMap.h"
#include "3D/WaterRings.h"
#include "Camera/Camera.h"
#include "Common/WorkerPool.h"
#include "Creature/CreatureHairLight.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureSpells.h"
#include "Creature/LeashOrders.h"
#include "ECS/BoatLaunchRules.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/MovingShadowCasters.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "EngineConfig.h"
#include "Game.h"
#include "Graphics/BoneBudget.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/HandLight.h"
#include "Graphics/LandFootprints.h"
#include "Graphics/LandShadowBake.h"
#include "Graphics/Mesh.h"
#include "Graphics/MeshDetail.h"
#include "Graphics/ModelLight.h"
#include "Graphics/ObjectShadows.h"
#include "Graphics/RendererDetail.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShadowTexture.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Profiler.h"
#include "Renderer.h"
#include "Resources/Rereads.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Scenery/ObjectDrawList.h"
#include "Visual/DetailLevel.h"
#include "entt/locator/locator.hpp"

using namespace openblack;
using namespace openblack::graphics;
using namespace openblack::graphics::detail;
using namespace openblack::ecs::systems;

void Renderer::DrawObjectShadowPass(const DrawSceneDesc& drawDesc) const
{
	const auto viewId = static_cast<bgfx::ViewId>(RenderPass::ObjectShadow);
	auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::ObjectShadowPass);
	if (!drawDesc.drawIsland)
	{
		return;
	}

	// A texel for each texel of the blocks' textures, over the whole island
	const auto& island = Locator::terrainSystem::value();
	const auto indexExtent = island.GetIndexExtent();
	const auto blocks = indexExtent.maximum - indexExtent.minimum + glm::u16vec2(1, 1);
	const auto width = static_cast<uint16_t>(blocks.x * lnd::LNDMaterial::k_Width);
	const auto height = static_cast<uint16_t>(blocks.y * lnd::LNDMaterial::k_Height);
	uint16_t shadowWidth = 0;
	uint16_t shadowHeight = 0;
	if (_objectShadowFrameBuffer)
	{
		_objectShadowFrameBuffer->GetSize(shadowWidth, shadowHeight);
	}
	if (shadowWidth != width || shadowHeight != height)
	{
		_objectShadowFrameBuffer = std::make_unique<FrameBuffer>("Object Shadows", width, height, TextureFormat::R8,
		                                                         std::nullopt, static_cast<uint8_t>(1), Wrapping::ClampEdge);
	}

	_objectShadowFrameBuffer->Bind(RenderPass::ObjectShadow);
	bgfx::setViewRect(viewId, 0, 0, width, height);
	bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR, 0x00000000);
	bgfx::touch(viewId);
	_movingTreeShadows = false;
	_landShadowsBaked = false;
	if (!drawDesc.drawEntities)
	{
		return;
	}

	BakeLandShadows(drawDesc.drawVegetation);
	_landShadowsBaked = _landShadowBake->GetTexture() != nullptr;
	const bool movingTrees = drawDesc.drawVegetation && Locator::config::value().dynamicTreeShadows;
	if (!movingTrees)
	{
		return;
	}

	// The trees' shadows drawn as they are this frame, swaying with them and bent away from the hand
	const auto view = island.GetOrthoView();
	const auto proj = island.GetOrthoProj();
	bgfx::setViewTransform(viewId, &view, &proj);

	// Meshes without bones cast with the shader that declares only their model matrix
	const auto* shader = _shaderManager->GetShader("ObjectShadowInstanced");
	const auto* staticShader = _shaderManager->GetShader("ObjectShadowStaticInstanced");
	const auto sun = glm::vec4(ObjectShadows::k_Sun, 0.0f);
	shader->SetUniformValue("u_shadowSun", &sun);
	staticShader->SetUniformValue("u_shadowSun", &sun);

	const auto& meshManager = Locator::resources::value().GetMeshes();
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	L3DMeshSubmitDesc submitDesc = {};
	submitDesc.viewId = RenderPass::ObjectShadow;
	// Overlapping shadows cover the same texels, both sides of every triangle cast
	submitDesc.state = BGFX_STATE_WRITE_R;
	for (const auto& [meshId, placers] : renderCtx.treeInstancedDrawDescs)
	{
		if (placers.count == 0 || !meshManager.Contains(meshId))
		{
			continue;
		}
		const auto mesh = meshManager.Handle(meshId);
		submitDesc.instanceDesc =
		    std::make_unique<graphics::InstanceDesc>(renderCtx.treeInstanceUniformBuffer, placers.offset, placers.count);
		const static auto identity = glm::mat4(1.0f);
		submitDesc.modelMatrices = &identity;
		submitDesc.matrixCount = 1;
		if (mesh->IsBoned() && !mesh->GetBoneMatrices().empty())
		{
			submitDesc.modelMatrices = mesh->GetBoneMatrices().data();
			submitDesc.matrixCount = static_cast<uint8_t>(mesh->GetBoneMatrices().size());
		}
		submitDesc.program = bone_budget::For(submitDesc.matrixCount) == bone_budget::Budget::Single ? staticShader : shader;
		DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
	}
	_movingTreeShadows = true;
}

void Renderer::SetObjectShadowUniforms(const ShaderProgram& program) const
{
	const auto& island = Locator::terrainSystem::value();
	auto u_objectShadows = glm::vec4(0.0f);
	if (_objectShadowFrameBuffer)
	{
		uint16_t width = 0;
		uint16_t height = 0;
		_objectShadowFrameBuffer->GetSize(width, height);
		u_objectShadows = glm::vec4(_movingTreeShadows ? ObjectShadows::k_MaxDarkness : 0.0f,
		                            1.0f / static_cast<float>(std::max<uint16_t>(width, 1)),
		                            1.0f / static_cast<float>(std::max<uint16_t>(height, 1)), 0.0f);
	}
	program.SetTextureSampler("s5_objectShadows", 5,
	                          _objectShadowFrameBuffer ? _objectShadowFrameBuffer->GetColorAttachment()
	                                                   : island.GetHeightMap());
	const auto* baked = _landShadowsBaked && _landShadowBake ? _landShadowBake->GetTexture() : nullptr;
	if (baked != nullptr)
	{
		u_objectShadows.w = 1.0f;
		program.SetTextureSampler("s14_landShadows", 14, *baked);
	}
	else
	{
		// Not read without the baked shadows
		program.SetTextureSampler("s14_landShadows", 14, island.GetHeightMap());
	}
	program.SetUniformValue("u_objectShadows", &u_objectShadows);
}

namespace
{
/// A creature frozen or fizzed this far casts no shadow
constexpr float k_NoShadowSpellLook = 0.2f;
} // namespace

void Renderer::AddGroundShadowBody(const GroundShadowBody& body, GroundShadowScratch& scratch)
{
	// Each bone's matrix in the world, once
	auto& bones = scratch.bones;
	bones.resize(body.pose.size());
	std::ranges::transform(body.pose, bones.begin(), [&body](const glm::mat4& bone) { return body.model * bone; });
	const auto& subMeshes = body.mesh->GetSubMeshes();
	for (size_t index = 0; index < subMeshes.size(); ++index)
	{
		const auto& subMesh = *subMeshes[index];
		// Only the parts drawn in the most detail
		if ((subMesh.GetFlags().lodMask & 1U) == 0)
		{
			continue;
		}
		const auto& geometry = subMesh.GetBodyGeometry();
		// The matching parts of the meshes the body is blended towards, and how far
		std::array<const std::vector<glm::vec3>*, 3> targets {};
		std::array<float, 3> weights {};
		if (body.morph != nullptr)
		{
			for (size_t axis = 0; axis < targets.size(); ++axis)
			{
				const auto* target = body.morph->meshes.at(axis);
				const auto weight = body.morph->weights[static_cast<glm::length_t>(axis)];
				if (target == nullptr || weight == 0.0f || index >= target->GetSubMeshes().size())
				{
					continue;
				}
				const auto& positions = target->GetSubMeshes()[index]->GetBodyGeometry().positions;
				if (positions.size() == geometry.positions.size())
				{
					targets.at(axis) = &positions;
					weights.at(axis) = weight;
				}
			}
		}
		// A part moved by bones may dip below the level; one moved with the body whole is kept above it
		const bool boned = subMesh.GetFlags().hasBones != 0U;
		scratch.parts.push_back({.subMesh = &subMesh,
		                         .first = static_cast<uint32_t>(scratch.points.size()),
		                         .secondRowsOnly = body.secondRowsOnly});
		for (size_t i = 0; i < geometry.positions.size(); ++i)
		{
			auto local = geometry.positions[i];
			for (size_t axis = 0; axis < targets.size(); ++axis)
			{
				if (targets.at(axis) != nullptr)
				{
					local += weights.at(axis) * ((*targets.at(axis))[i] - geometry.positions[i]);
				}
			}
			// Every vertex moved by its own bone alone
			const auto bone = i < geometry.bones.size() ? geometry.bones[i] : L3DSubMesh::k_NoBone;
			const auto world = (bone < bones.size() ? bones[bone] : body.model) * glm::vec4(local, 1.0f);
			scratch.points.push_back({.position = glm::vec3(world), .keptAboveLevel = !boned});
		}
	}
}

std::array<uint8_t, GroundShadow::k_Texels * GroundShadow::k_Texels>
Renderer::GroundShadowLevels(const GroundShadow::Footprint& footprint, uint8_t alpha, GroundShadowScratch& scratch)
{
	auto& samples = scratch.samples;
	samples.clear();
	for (const auto& flat : scratch.flattened)
	{
		samples.push_back(GroundShadow::Sample(flat, footprint));
	}
	scratch.coverage = {};
	for (const auto& part : scratch.parts)
	{
		const auto& geometry = part.subMesh->GetBodyGeometry();
		for (const auto& primitive : part.subMesh->GetPrimitives())
		{
			const auto end = std::min<size_t>(primitive.indicesOffset + primitive.indicesCount, geometry.indices.size());
			for (size_t i = primitive.indicesOffset; i + 2 < end; i += 3)
			{
				const std::array<size_t, 3> corners {part.first + geometry.indices[i], part.first + geometry.indices[i + 1],
				                                     part.first + geometry.indices[i + 2]};
				if (std::ranges::any_of(corners, [&samples](size_t corner) { return corner >= samples.size(); }))
				{
					continue;
				}
				GroundShadow::Cover(scratch.coverage, {samples[corners[0]], samples[corners[1]], samples[corners[2]]},
				                    primitive.twoSided, part.secondRowsOnly);
			}
		}
	}
	return GroundShadow::Levels(scratch.coverage, alpha);
}

std::array<uint8_t, GroundShadow::k_Texels * GroundShadow::k_Texels>
Renderer::ShadowTextureLevels(const GroundShadowBody& body, const GroundShadow::Footprint& footprint,
                              const GroundShadow::Cast& cast, uint8_t alpha) const
{
	// Each triangle of the nearest detail, without the parts that bones move, through the alpha of its texture: the
	// model's own skin, or a texture of the pack that its models share. A triangle without a mask casts nothing.
	const auto& textures = Locator::resources::value().GetTextures();
	shadow_texture::Canvas canvas;
	for (const auto& subMesh : body.mesh->GetSubMeshes())
	{
		const auto flags = subMesh->GetFlags();
		if ((flags.lodMask & 1U) == 0 || flags.hasBones != 0U)
		{
			continue;
		}
		const auto& geometry = subMesh->GetBodyGeometry();
		for (const auto& primitive : subMesh->GetPrimitives())
		{
			const auto* mask = body.mesh->GetSkinAlpha(primitive.skinID);
			if (mask == nullptr)
			{
				if (const auto* shared = textures.Find(primitive.skinID); shared != nullptr)
				{
					mask = shared->GetAlphaMask();
				}
			}
			if (mask == nullptr)
			{
				continue;
			}
			const auto end = std::min<size_t>(primitive.indicesOffset + primitive.indicesCount, geometry.indices.size());
			for (size_t i = primitive.indicesOffset; i + 2 < end; i += 3)
			{
				std::array<shadow_texture::Corner, 3> corners {};
				bool valid = true;
				for (size_t k = 0; k < corners.size(); ++k)
				{
					const auto vertex = geometry.indices[i + k];
					if (vertex >= geometry.positions.size() || vertex >= geometry.uvs.size())
					{
						valid = false;
						break;
					}
					corners.at(k) =
					    shadow_texture::CornerOf(geometry.positions[vertex], geometry.uvs[vertex], body.model, footprint, cast);
				}
				if (valid)
				{
					shadow_texture::Fill(canvas, *mask, corners);
				}
			}
		}
	}
	return shadow_texture::Levels(canvas, alpha);
}

void Renderer::StartGroundShadows(const DrawSceneDesc& drawDesc) const
{
	auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::GroundShadowPass);
	_groundShadowsStarted = true;
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	const bool casts = Locator::config::value().drawCreatureShadows && drawDesc.drawEntities && drawDesc.drawIsland &&
	                   !inTemple && Locator::terrainSystem::has_value();

	const auto& meshManager = Locator::resources::value().GetMeshes();
	const auto camera = drawDesc.camera->GetOrigin();
	const auto scaleOf = [](const glm::mat4& model) {
		return std::max({glm::length(glm::vec3(model[0])), glm::length(glm::vec3(model[1])), glm::length(glm::vec3(model[2]))});
	};
	using Caster = GroundShadowCaster;
	auto& casters = _groundShadowCasters;
	casters.clear();
	// The things under the sea, whose shadows stay as they were when they went under
	auto& underSea = _groundShadowUnderSea;
	underSea.clear();
	// Kept for the bodies that are blended, in place: made room for first, thus the casters can point to them
	auto& morphs = _groundShadowMorphs;
	morphs.clear();
	morphs.reserve(renderCtx.entityDraws.size() + 1);

	if (casts)
	{
		const auto& land = Locator::terrainSystem::value();
		// The hand's, cast from the sun and laid on the ground beneath it, with what it holds
		if (drawDesc.drawHand && Locator::handSystem::has_value() && Locator::skyClockSystem::has_value())
		{
			const auto handEntity =
			    Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
			const auto& registry = Locator::entitiesRegistry::value();
			const auto [hand, mesh, transform] =
			    registry.TryGet<ecs::components::Hand, ecs::components::Mesh, ecs::components::Transform>(handEntity);
			if (hand != nullptr && mesh != nullptr && transform != nullptr && meshManager.Contains(mesh->id))
			{
				const auto* handMesh = &*meshManager.Handle(mesh->id);
				const auto model =
				    glm::translate(transform->position) * glm::mat4(transform->rotation) * glm::scale(transform->scale);
				const auto ground = land.GetHeightAt(glm::vec2(transform->position.x, transform->position.z));
				const auto radius =
				    openblack::graphics::mesh_detail::HalfDiagonal(handMesh->GetBoundingBox().Size()) * scaleOf(model);
				const auto& sky = Locator::skyClockSystem::value();
				const auto fade =
				    GroundShadow::Fade(glm::distance(camera, glm::vec3(transform->position.x, ground, transform->position.z)),
				                       radius) *
				    HandShadow::Daylight(sky.GetTime(), sky.GetDayNightTimes());
				const auto alpha = GroundShadow::ScaledAlpha(fade);
				if (alpha > 0)
				{
					const L3DMeshSubmitDesc::MorphTargets* handMorph = nullptr;
					if (const auto targets = HandMorphTargets())
					{
						handMorph = &morphs.emplace_back(*targets);
					}
					Caster caster {
					    .bodies = {},
					    .bodyCount = 1,
					    .standsAt = transform->position,
					    .cast = {.light =
					                 HandShadow::LightPoint(glm::vec3(transform->position.x, ground, transform->position.z),
					                                        sky.GetTime(), sky.GetDayNightTimes()),
					             .level = ground,
					             .ground = ground},
					    .alpha = alpha,
					    .onObjects = true,
					};
					caster.bodies.at(0) = {
					    .mesh = handMesh,
					    .model = model,
					    .pose = hand->boneMatrices.empty() ? std::span<const glm::mat4>(handMesh->GetBoneMatrices())
					                                       : std::span<const glm::mat4>(hand->boneMatrices),
					    .morph = handMorph,
					    .secondRowsOnly = true,
					};
					// What the hand holds counts every sample
					const auto held = std::ranges::find_if(renderCtx.inHandDraws, [&registry, handEntity](const auto& draw) {
						const auto* inHand = registry.TryGet<const ecs::components::InHand>(draw.entity);
						return inHand != nullptr && inHand->hand == handEntity;
					});
					if (held != renderCtx.inHandDraws.end() && meshManager.Contains(held->mesh))
					{
						const auto* heldMesh = &*meshManager.Handle(held->mesh);
						std::span<const glm::mat4> pose = heldMesh->GetBoneMatrices();
						if (const auto* villagerPose = registry.TryGet<const ecs::components::VillagerPose>(held->entity);
						    villagerPose != nullptr && villagerPose->bones.size() == pose.size())
						{
							pose = villagerPose->bones;
						}
						caster.bodies.at(1) = {
						    .mesh = heldMesh, .model = held->model, .pose = pose, .morph = nullptr, .secondRowsOnly = false};
						caster.bodyCount = 2;
					}
					casters.push_back(caster);
				}
			}
		}

		// The creatures', cast from the scene's light brought round above each, and laid straight down
		const auto light = glm::vec3(GetModelLight());
		for (const auto& [entity, instance] : renderCtx.entityDraws)
		{
			const auto* mesh = drawDesc.entities.TryGet<const ecs::components::Mesh>(entity);
			if (mesh == nullptr || !drawDesc.entities.AnyOf<ecs::components::Creature>(entity) ||
			    !meshManager.Contains(mesh->id) || instance >= renderCtx.instanceUniforms.size())
			{
				continue;
			}
			// A creature frozen or fizzed a fifth of the way or more casts no shadow
			if (const auto* spells = drawDesc.entities.TryGet<const ecs::components::CreatureSpells>(entity);
			    spells != nullptr && (spells->freeze >= k_NoShadowSpellLook || spells->fizz >= k_NoShadowSpellLook))
			{
				continue;
			}
			const auto* body = &*meshManager.Handle(mesh->id);
			const auto& model = renderCtx.instanceUniforms[instance].model;
			const auto standsAt = glm::vec3(model[3]);
			const auto radius = openblack::graphics::mesh_detail::HalfDiagonal(body->GetBoundingBox().Size()) * scaleOf(model);
			const auto ground = land.GetHeightAt(glm::vec2(standsAt.x, standsAt.z));
			const auto alpha = GroundShadow::ScaledAlpha(
			    GroundShadow::Fade(glm::distance(camera, glm::vec3(standsAt.x, ground, standsAt.z)), radius));
			if (alpha == 0)
			{
				continue;
			}
			const auto* animation = drawDesc.entities.TryGet<const ecs::components::CreatureAnimation>(entity);
			const auto pose = animation != nullptr && animation->boneMatrices.size() == body->GetBoneMatrices().size()
			                      ? std::span<const glm::mat4>(animation->boneMatrices)
			                      : std::span<const glm::mat4>(body->GetBoneMatrices());
			const auto* creature = drawDesc.entities.TryGet<const ecs::components::Creature>(entity);
			const auto* morph = drawDesc.entities.TryGet<const ecs::components::CreatureMorph>(entity);
			const L3DMeshSubmitDesc::MorphTargets* bodyMorph = nullptr;
			if (creature != nullptr && morph != nullptr)
			{
				const auto ids = creature_morph::MeshesOf(
				    creature->species, morph->drawn, [&meshManager](entt::id_type id) { return meshManager.Contains(id); });
				bodyMorph = &morphs.emplace_back(L3DMeshSubmitDesc::MorphTargets {
				    .meshes = {&*meshManager.Handle(ids.evilGood), &*meshManager.Handle(ids.thinFat),
				               &*meshManager.Handle(ids.weakStrong)},
				    .weights = glm::abs(glm::vec3(morph->drawn.evilGood, morph->drawn.thinFat, morph->drawn.weakStrong)),
				    .skins = {},
				    .blendSeams = false,
				});
			}
			Caster caster {
			    .bodies = {},
			    .bodyCount = 1,
			    .standsAt = standsAt,
			    .cast = {.light = GroundShadow::CreatureLight(light, standsAt, radius),
			             .level = standsAt.y,
			             .ground = standsAt.y},
			    .alpha = alpha,
			    .onObjects = true,
			};
			caster.bodies.at(0) = {.mesh = body, .model = model, .pose = pose, .morph = bodyMorph, .secondRowsOnly = false};
			casters.push_back(caster);
		}

		// The high-detail villagers', cast from the fixed far sun onto the ground beneath them, on the land only; and the
		// explorers' boat's as it is pushed down the beach, cast the same way onto the land and the things on it
		const auto castFromSun = [&](const ecs::systems::RenderContext::HighDetailDraw& draw, bool onObjects) {
			if (!meshManager.Contains(draw.mesh))
			{
				return;
			}
			const auto* mesh = &*meshManager.Handle(draw.mesh);
			const auto& model = draw.castModel;
			const auto standsAt = glm::vec3(model[3]);
			const auto ground = land.GetHeightAt(glm::vec2(standsAt.x, standsAt.z));
			const auto radius = openblack::graphics::mesh_detail::HalfDiagonal(mesh->GetBoundingBox().Size()) * scaleOf(model);
			const auto alpha = GroundShadow::Alpha(glm::distance(camera, glm::vec3(standsAt.x, ground, standsAt.z)), radius);
			if (alpha == 0)
			{
				return;
			}
			// The body in the pose of the clip it plays. A thing with no entity of the land, as the boat of the film, rests.
			const auto& rest = mesh->GetBoneMatrices();
			std::span<const glm::mat4> pose = rest;
			if (const auto* villagerPose = draw.entity != entt::null
			                                   ? drawDesc.entities.TryGet<const ecs::components::VillagerPose>(draw.entity)
			                                   : nullptr)
			{
				if (!rest.empty() && villagerPose->clipBones.size() == rest.size())
				{
					pose = villagerPose->clipBones;
				}
				else if (villagerPose->bones.size() == rest.size())
				{
					pose = villagerPose->bones;
				}
			}
			Caster caster {
			    .bodies = {},
			    .bodyCount = 1,
			    .standsAt = standsAt,
			    .cast = {.light = GroundShadow::k_Sun, .level = standsAt.y, .ground = ground},
			    .alpha = alpha,
			    .onObjects = onObjects,
			};
			caster.bodies.at(0) = {.mesh = mesh, .model = model, .pose = pose, .morph = nullptr, .secondRowsOnly = false};
			casters.push_back(caster);
		};
		for (const auto& draw : renderCtx.highDetailDraws)
		{
			castFromSun(draw, false);
		}
		for (const auto& draw : renderCtx.filmShadowDraws)
		{
			castFromSun(draw, true);
		}

		// The things that the physics moves, cast from a light straight above where each is drawn, onto the land only
		drawDesc.entities.Each<const ecs::components::Mesh, const ecs::components::PhysicsDrawPose>(
		    [&](entt::entity entity, const ecs::components::Mesh& mesh, const ecs::components::PhysicsDrawPose& drawn) {
			    const auto kind = ecs::MovingShadowOf(drawDesc.entities, entity);
			    if (kind == ecs::MovingShadow::None || !meshManager.Contains(mesh.id))
			    {
				    return;
			    }
			    if (drawn.underSea)
			    {
				    underSea.push_back(entity);
				    return;
			    }
			    const auto* body = &*meshManager.Handle(mesh.id);
			    // An animated thing casts in the pose of its clip, and casts nothing while it has no clip
			    const auto& rest = body->GetBoneMatrices();
			    std::span<const glm::mat4> pose = rest;
			    if (const auto* villagerPose = drawDesc.entities.TryGet<const ecs::components::VillagerPose>(entity))
			    {
				    if (villagerPose->clip == AnimId::Invalid)
				    {
					    return;
				    }
				    if (!rest.empty() && villagerPose->clipBones.size() == rest.size())
				    {
					    pose = villagerPose->clipBones;
				    }
				    else if (villagerPose->bones.size() == rest.size())
				    {
					    pose = villagerPose->bones;
				    }
			    }
			    else if (const auto* animalPose = drawDesc.entities.TryGet<const ecs::components::AnimalPose>(entity))
			    {
				    if (animalPose->bones.empty())
				    {
					    return;
				    }
				    if (animalPose->bones.size() == rest.size())
				    {
					    pose = animalPose->bones;
				    }
			    }
			    const auto model = glm::translate(glm::mat4(1.0f), drawn.origin) * glm::mat4(drawn.axes);
			    const auto ground = land.GetHeightAt(glm::vec2(drawn.origin.x, drawn.origin.z));
			    const auto radius =
			        openblack::graphics::mesh_detail::HalfDiagonal(body->GetBoundingBox().Size()) * scaleOf(model);
			    const auto alpha =
			        GroundShadow::Alpha(glm::distance(camera, glm::vec3(drawn.origin.x, ground, drawn.origin.z)), radius);
			    if (alpha == 0)
			    {
				    return;
			    }
			    Caster caster {
			        .bodies = {},
			        .bodyCount = 1,
			        .standsAt = drawn.origin,
			        .cast = {.light = shadow_texture::LightAbove(drawn.origin), .level = drawn.origin.y, .ground = ground},
			        .alpha = alpha,
			        .onObjects = false,
			        .throughAlpha = kind == ecs::MovingShadow::Texture,
			        .kept = entity,
			    };
			    caster.bodies.at(0) = {.mesh = body, .model = model, .pose = pose, .morph = nullptr, .secondRowsOnly = false};
			    casters.push_back(caster);
		    });
	}

	// Each shadow's texture counted from its bodies' triangles, as the game counts them, and handed over with where it
	// lies; what is laid on objects goes first
	std::ranges::stable_partition(casters, &Caster::onObjects);
	// Each shadow uses only its caster, thus the workers make the shadows at the same time, each into its own place.
	// The draw continues while the workers make them. Then DrawGroundShadowPass hands them over in the order of the
	// casters. A shadow that the textures' alpha fills reads the texture cache, thus the main thread makes it there.
	_groundShadowMade.resize(casters.size());
	const auto make = [this, &casters = std::as_const(casters)](size_t index, size_t lane) {
		const auto& caster = casters[index];
		auto& scratch = _groundShadowScratch[lane];
		auto& made = _groundShadowMade[index];
		scratch.points.clear();
		scratch.parts.clear();
		for (uint8_t i = 0; i < caster.bodyCount; ++i)
		{
			AddGroundShadowBody(caster.bodies.at(i), scratch);
		}
		scratch.flattened.resize(scratch.points.size());
		made.footprint = GroundShadow::Measure(scratch.points, caster.standsAt, caster.cast, scratch.flattened);
		if (made.footprint && !caster.throughAlpha)
		{
			made.levels = GroundShadowLevels(*made.footprint, caster.alpha, scratch);
		}
	};
	_workers->Start(casters.size(), make);
}

void Renderer::DrawGroundShadowPass(const DrawSceneDesc& drawDesc) const
{
	// Without a start in the draw of this scene, make the shadows here
	if (!_groundShadowsStarted)
	{
		StartGroundShadows(drawDesc);
	}
	auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::GroundShadowPass);
	_workers->Finish();
	_groundShadowsStarted = false;
	_groundShadowCount = 0;
	_groundShadowsOnObjects = 0;
	_groundShadowTexels.clear();
	std::ranges::fill(_groundShadowBlocks, glm::vec2(0.0f));
	const auto& casters = _groundShadowCasters;
	const auto& underSea = _groundShadowUnderSea;
	std::vector<GroundShadow::Footprint> footprints;
	footprints.reserve(casters.size());
	decltype(_keptGroundShadows) kept;
	for (size_t index = 0; index < casters.size(); ++index)
	{
		const auto& caster = casters[index];
		const auto& footprint = _groundShadowMade[index].footprint;
		if (!footprint)
		{
			continue;
		}
		const auto& levels = caster.throughAlpha
		                         ? ShadowTextureLevels(caster.bodies.at(0), *footprint, caster.cast, caster.alpha)
		                         : _groundShadowMade[index].levels;
		const auto first = _groundShadowTexels.size();
		const auto way = GroundShadow::LightWay(caster.standsAt, caster.cast.light);
		_groundShadowTexels.push_back(GroundShadow::Receiver(*footprint, caster.cast));
		_groundShadowTexels.emplace_back(way, footprint->nearest, 0.0f);
		_groundShadowTexels.emplace_back(footprint->low, footprint->high);
		_groundShadowTexels.emplace_back(0.0f);
		for (size_t texel = 0; texel < levels.size(); texel += 4)
		{
			_groundShadowTexels.emplace_back(levels.at(texel), levels.at(texel + 1), levels.at(texel + 2),
			                                 levels.at(texel + 3));
		}
		footprints.push_back(*footprint);
		++_groundShadowCount;
		if (caster.onObjects)
		{
			++_groundShadowsOnObjects;
		}
		if (caster.kept != entt::null)
		{
			kept[caster.kept] = {.texels =
			                         std::vector<glm::vec4>(_groundShadowTexels.begin() + static_cast<std::ptrdiff_t>(first),
			                                                _groundShadowTexels.end()),
			                     .footprint = *footprint};
		}
	}
	// A thing under the sea keeps the shadow that it had when it went under, as long as it stays in the physics
	for (const auto entity : underSea)
	{
		const auto found = _keptGroundShadows.find(entity);
		if (found == _keptGroundShadows.end())
		{
			continue;
		}
		_groundShadowTexels.insert(_groundShadowTexels.end(), found->second.texels.begin(), found->second.texels.end());
		footprints.push_back(found->second.footprint);
		++_groundShadowCount;
		kept.insert(*found);
	}
	_keptGroundShadows = std::move(kept);

	// Each land block's list of the shadows reaching it, after the shadows themselves
	if (Locator::terrainSystem::has_value())
	{
		const auto& blocks = Locator::terrainSystem::value().GetBlocks();
		_groundShadowBlocks.assign(blocks.size(), glm::vec2(0.0f));
		if (_groundShadowCount > 0)
		{
			auto& places = _groundShadowBlockPlaces;
			places.resize(blocks.size());
			std::ranges::transform(blocks, places.begin(), [](const auto& block) { return block.GetMapPosition(); });
			GroundShadow::ShadowsOfBlocks(footprints, places, _groundShadowBlockFirsts, _groundShadowBlockShadows);
			for (size_t block = 0; block < blocks.size(); ++block)
			{
				const auto first = static_cast<float>(_groundShadowTexels.size());
				for (auto entry = _groundShadowBlockFirsts[block]; entry < _groundShadowBlockFirsts[block + 1]; ++entry)
				{
					_groundShadowTexels.emplace_back(static_cast<float>(_groundShadowBlockShadows[entry]), 0.0f, 0.0f, 0.0f);
				}
				_groundShadowBlocks[block] = {first, static_cast<float>(_groundShadowTexels.size()) - first};
			}
		}
	}

	// Handed over in a texture of whole rows, grown as needed
	const auto rows = std::max<uint32_t>(
	    1, static_cast<uint32_t>((_groundShadowTexels.size() + k_GroundShadowDataWidth - 1) / k_GroundShadowDataWidth));
	if (!_groundShadowData || _groundShadowData->GetResolution().y < rows)
	{
		auto capacity = std::max<uint32_t>(_groundShadowData ? _groundShadowData->GetResolution().y : 1U, 1U);
		while (capacity < rows)
		{
			capacity *= 2;
		}
		_groundShadowData = std::make_unique<Texture2D>("Ground Shadow Data");
		_groundShadowData->Create(k_GroundShadowDataWidth, static_cast<uint16_t>(capacity), 1, TextureFormat::RGBA32F,
		                          Wrapping::ClampEdge, Filter::Nearest, nullptr);
		_groundShadowData->SetTexelsGivenAgain();
	}
	_groundShadowTexels.resize(static_cast<size_t>(_groundShadowData->GetResolution().y) * k_GroundShadowDataWidth,
	                           glm::vec4(0.0f));
	_groundShadowData->Update(_groundShadowTexels.data(),
	                          static_cast<uint32_t>(_groundShadowTexels.size() * sizeof(glm::vec4)));
}

void Renderer::SetGroundShadowUniforms(const ShaderProgram& program, bool receives) const
{
	const auto count = receives ? _groundShadowCount : 0U;
	const glm::vec4 info {static_cast<float>(receives ? _groundShadowsOnObjects : 0U), static_cast<float>(count), 0.0f, 0.0f};
	program.SetUniformValue("u_groundShadowInfo", &info);
	program.SetTextureSampler("s4_groundShadowData", 4, *_groundShadowData);
}

void Renderer::SetObjectGroundShadowUniforms(DrawEncoder& to, const ShaderProgram& program, bool receives) const
{
	const glm::vec4 info {static_cast<float>(receives ? _groundShadowsOnObjects : 0U), 0.0f, 0.0f, 0.0f};
	// By the handles the program's draws of meshes keep, as this is set for every primitive of them
	const auto& uniforms = MeshUniformsOf(program);
	const auto handle = [&uniforms](MeshUniform uniform) { return uniforms[static_cast<size_t>(uniform)]; };
	if (const auto uniform = handle(MeshUniform::GroundShadowInfo))
	{
		program.SetUniformValue(to, *uniform, &info);
	}
	if (const auto uniform = handle(MeshUniform::GroundShadowData))
	{
		program.SetTextureSampler(to, *uniform, 14, *_groundShadowData);
	}
}
