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
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <bgfx/bgfx.h>
#include <glm/gtx/norm.hpp>
#include <spdlog/spdlog.h>

#include "3D/ChimneySmoke.h"
#include "3D/HandMorph.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSkinTexels.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLightTable.h"
#include "3D/SnowCover.h"
#include "3D/TempleDoors.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/WaterRings.h"
#include "Camera/Camera.h"
#include "Common/MainThreadQueue.h"
#include "Creature/CreatureEnvironmentMap.h"
#include "Creature/CreatureFizzLook.h"
#include "Creature/CreatureHair.h"
#include "Creature/CreatureHairLight.h"
#include "Creature/CreatureSkin.h"
#include "Creature/CreatureSpells.h"
#include "Creature/LeashOrders.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HandMorph.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/DrawnOnScreen.h"
#include "ECS/DrawnRegistry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureHairSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "EngineConfig.h"
#include "Game.h"
#include "Graphics/DebugLines.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/HandLight.h"
#include "Graphics/LandFootprints.h"
#include "Graphics/LandShadowBake.h"
#include "Graphics/RendererDetail.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ZSort.h"
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
float CreatureLandLightScale(RenderPass viewId)
{
	constexpr float k_ReflectionLight = 0.5f;
	return viewId == RenderPass::Reflection || viewId == RenderPass::ReflectionTranslucent ||
	               viewId == RenderPass::ReflectionMirrored
	           ? k_ReflectionLight
	           : 1.0f;
}
} // namespace openblack::graphics::detail

void Renderer::DrawCreatureEyes(const DrawSceneDesc& desc, const ecs::components::CreatureEyes& eyes,
                                const L3DMeshSubmitDesc& bodyDesc, float fizz, float freeze, uint32_t fizzSortDepth) const
{
	using ecs::components::CreatureEyes;
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(CreatureEyes::k_EyeballMeshId) || !meshes.Contains(CreatureEyes::k_EyelidMeshId) ||
	    !creature_fizz_look::Drawn(fizz))
	{
		return;
	}
	const auto eyeball = meshes.Handle(CreatureEyes::k_EyeballMeshId);
	const auto eyelid = meshes.Handle(CreatureEyes::k_EyelidMeshId);

	// Each eye is drawn on its own, in the creature's light: its eyeball, then its eyelid in the colour of the skin under it
	L3DMeshSubmitDesc submitDesc = {};
	submitDesc.viewId = bodyDesc.viewId;
	submitDesc.program = _shaderManager->GetShader("Object");
	submitDesc.state = bodyDesc.state;
	submitDesc.matrixCount = 1;
	submitDesc.lightMultiply = bodyDesc.lightMultiply;
	submitDesc.lightAdd = bodyDesc.lightAdd;
	submitDesc.landLightScale = bodyDesc.landLightScale;
	submitDesc.mirrored = bodyDesc.mirrored;
	submitDesc.groundShadows = false;
	submitDesc.sortDepth = bodyDesc.sortDepth;
	submitDesc.modelLight = bodyDesc.modelLight;
	// A body that has colours of its own alone, and no light of the land, gives its eyes the same colours. Its eyelids
	// take its colour times the skin's colour under them, over 255 in whole steps, and its added colour.
	std::optional<L3DMeshSubmitDesc::ObjectLook> lidLook;
	if (!bodyDesc.landLightAndHaze)
	{
		submitDesc.landLightAndHaze = false;
		submitDesc.objectLook = bodyDesc.objectLook;
		submitDesc.alwaysLit = bodyDesc.alwaysLit;
		if (bodyDesc.objectLook.has_value())
		{
			const auto skin = glm::round(glm::clamp(eyes.lidColour, 0.0f, 1.0f) * 255.0f);
			lidLook = L3DMeshSubmitDesc::ObjectLook {.colour = glm::floor(bodyDesc.objectLook->colour * skin / 255.0f),
			                                         .alpha = bodyDesc.objectLook->alpha};
		}
	}
	auto tint = glm::vec4(bodyDesc.tint.r, bodyDesc.tint.g, bodyDesc.tint.b, 0.0f);
	// Frozen, each piece takes the body's freeze: its light tinted towards the icy colour and a sheen of ice over it
	if (freeze > 0.0f)
	{
		const auto icy = creature_spells::FrozenTint(freeze);
		tint *= glm::vec4(glm::vec3((icy >> 16) & 0xFFu, (icy >> 8) & 0xFFu, icy & 0xFFu) / 256.0f, 1.0f);
		submitDesc.freeze = freeze;
	}

	// While the creature fizzes, its eyes fizz with it, each piece drawn twice as its body is, just after it: but through
	// the one static all the creatures' eyes share. A frozen eye is drawn frozen instead.
	const bool fizzing = creature_fizz_look::EyesFizz(fizz, freeze) && Locator::creatureFizzSystem::has_value();
	const auto scroll = fizzing ? Locator::creatureFizzSystem::value().EyeStaticScroll() : glm::vec2(0.0f);
	auto sortDepth = fizzSortDepth;
	// Drawn as usual, the eyeball adds the glass environment map, as each of its primitives' materials says. A fizzing or
	// frozen eye is drawn without it.
	const auto& textures = Locator::resources::value().GetTextures();
	constexpr auto k_Glass = entt::hashed_string("raw/envmap_glass_fx");
	const auto glass = k_Glass.value();
	const auto* eyeballEnvironment =
	    !fizzing && freeze <= 0.0f && textures.Contains(glass) ? &*textures.Handle(glass) : nullptr;
	const auto draw = [&](const L3DMesh& piece) {
		if (!fizzing)
		{
			DrawMesh(piece, submitDesc, std::numeric_limits<uint8_t>::max());
			return;
		}
		using Pass = L3DMeshSubmitDesc::Fizz::Pass;
		const auto level = creature_fizz_look::Level(fizz);
		submitDesc.viewId = TranslucentView(desc.viewId);
		for (const auto pass : {Pass::Depth, Pass::Body})
		{
			sortDepth = sortDepth > 0 ? sortDepth - 1 : 0;
			submitDesc.sortDepth = sortDepth;
			submitDesc.fizz = L3DMeshSubmitDesc::Fizz {.pass = pass, .level = level, .scroll = scroll};
			submitDesc.state = pass == Pass::Depth ? BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA
			                                       : BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_EQUAL |
			                                             BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA;
			DrawMesh(piece, submitDesc, std::numeric_limits<uint8_t>::max());
		}
	};
	for (const auto& eye : eyes.drawn)
	{
		if (eye.eyeball.has_value())
		{
			submitDesc.modelMatrices = &*eye.eyeball;
			submitDesc.tint = tint;
			if (eyeballEnvironment != nullptr)
			{
				submitDesc.program = _shaderManager->GetShader("ObjectEnvironment");
				submitDesc.environment = eyeballEnvironment;
			}
			draw(*eyeball);
			submitDesc.program = _shaderManager->GetShader("Object");
			submitDesc.environment = nullptr;
		}
		if (eye.eyelid.has_value())
		{
			submitDesc.modelMatrices = &*eye.eyelid;
			// Wholly frozen, the eyelids are white rather than the colour of the skin under them
			const bool white = creature_fizz_look::EyelidsWhite(freeze);
			if (lidLook.has_value() && !white)
			{
				submitDesc.objectLook = lidLook;
				submitDesc.tint = tint;
			}
			else
			{
				submitDesc.tint = white ? tint : tint * glm::vec4(eyes.lidColour, 1.0f);
			}
			draw(*eyelid);
			// The next eyeball takes the body's colours again
			if (lidLook.has_value())
			{
				submitDesc.objectLook = bodyDesc.objectLook;
			}
		}
	}
}

void Renderer::DrawCreatureHair(const DrawSceneDesc& desc, const ecs::components::CreatureHair& hair, glm::vec3 origin) const
{
	DrawCreatureHair(desc, hair, origin, TranslucentPassOf(desc.viewId), desc.camera->GetOrigin());
}

void Renderer::DrawCreatureHair(const DrawSceneDesc& desc, const ecs::components::CreatureHair& hair, glm::vec3 origin,
                                RenderPass pass, glm::vec3 eye, std::optional<glm::vec3> ownLight,
                                std::optional<glm::vec3> ownAdded, bool keptColours) const
{
	using ecs::components::CreatureHair;
	const auto& textures = Locator::resources::value().GetTextures();
	struct Vertex
	{
		glm::vec3 position;
		glm::vec2 uv;
		uint32_t colour;
	};
	static const auto k_Layout = [] {
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		    .end();
		return layout;
	}();
	const bool hasTexture = textures.Contains(CreatureHair::k_TextureId) && textures.Contains(CreatureHair::k_AlphaTextureId);
	const auto viewId = static_cast<bgfx::ViewId>(pass);
	const auto* program = _shaderManager->GetShader("CreatureHair");
	// The hair takes the land's light and colour under the creature, and the haze at it, as its body does
	const auto extent = Locator::terrainSystem::value().GetExtent();
	const auto islandExtent = glm::vec4(extent.minimum, extent.maximum);
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	const bool landLit = !inTemple && _landLightTexture.has_value();
	const glm::vec4 u_landLight {landLit ? 1.0f : 0.0f, CreatureLandLightScale(desc.viewId), 0.0f, 0.0f};
	const glm::vec4 u_hairOrigin {origin, 0.0f};
	const glm::vec4 u_hairAlphaCutOff {creature_hair::k_AlphaCutOff, 0.0f, 0.0f, 0.0f};
	const auto u_haze = inTemple ? glm::vec4(0.0f) : _haze[0];
	// Each strand's colour times the creature's own light over 255, plus its added colour, at most 255. The kept colours
	// come from the target that keeps them.
	const bool kept = keptColours && _hairLightKept != nullptr;
	const float lightSource = kept ? 2.0f : (ownLight.has_value() ? 1.0f : 0.0f);
	const glm::vec4 u_hairLight {ownLight.value_or(glm::vec3(255.0f)), lightSource};
	const glm::vec4 u_hairAdded {ownAdded.value_or(glm::vec3(0.0f)), 0.0f};
	std::vector<creature_hair::RibbonVertex> ribbon;
	for (const auto& group : hair.groups)
	{
		// Every strand of a group in one draw, each a strip of quads between its points
		uint32_t vertexCount = 0;
		uint32_t indexCount = 0;
		for (const auto& strand : group.strands)
		{
			if (strand.positions.size() >= 2)
			{
				vertexCount += static_cast<uint32_t>(strand.positions.size() * 2);
				indexCount += static_cast<uint32_t>((strand.positions.size() - 1) * 6);
			}
		}
		const bool textured = group.textured && hasTexture;
		if (vertexCount == 0 || vertexCount > std::numeric_limits<uint16_t>::max() ||
		    (!textured && !_whiteTexture.has_value()) ||
		    bgfx::getAvailTransientVertexBuffer(vertexCount, k_Layout) < vertexCount ||
		    bgfx::getAvailTransientIndexBuffer(indexCount) < indexCount)
		{
			continue;
		}
		bgfx::TransientVertexBuffer vertexBuffer;
		bgfx::TransientIndexBuffer indexBuffer;
		bgfx::allocTransientVertexBuffer(&vertexBuffer, vertexCount, k_Layout);
		bgfx::allocTransientIndexBuffer(&indexBuffer, indexCount);
		const auto vertices = std::span(reinterpret_cast<Vertex*>(vertexBuffer.data), vertexCount);
		const auto indices = std::span(reinterpret_cast<uint16_t*>(indexBuffer.data), indexCount);
		// The texture multiplies the strands' colour, which the shader lights
		const auto colour = glm::clamp(group.colour, 0, 255);
		const auto abgr = 0xFF000000u | (static_cast<uint32_t>(colour.b) << 16u) | (static_cast<uint32_t>(colour.g) << 8u) |
		                  static_cast<uint32_t>(colour.r);
		size_t vertex = 0;
		size_t index = 0;
		glm::vec3 middle(0.0f);
		for (const auto& strand : group.strands)
		{
			if (strand.positions.size() < 2)
			{
				continue;
			}
			ribbon.resize(strand.positions.size() * 2);
			creature_hair::BuildRibbon(strand.positions, eye, group.halfWidth, ribbon);
			const auto first = static_cast<uint16_t>(vertex);
			for (const auto& corner : ribbon)
			{
				vertices[vertex++] = {corner.position, corner.uv, abgr};
			}
			for (uint16_t i = 0; i + 1 < static_cast<uint16_t>(strand.positions.size()); ++i)
			{
				const auto a = static_cast<uint16_t>(first + (i * 2));
				for (const auto offset : {0, 1, 2, 2, 1, 3})
				{
					indices[index++] = static_cast<uint16_t>(a + offset);
				}
			}
			middle += strand.positions.front();
		}
		middle /= static_cast<float>(group.strands.size());
		// Without the texture a strand is a solid ribbon in its colour
		if (textured)
		{
			program->SetTextureSampler("s_diffuse", 0, *textures.Handle(CreatureHair::k_TextureId));
			program->SetTextureSampler("s_alpha", 1, *textures.Handle(CreatureHair::k_AlphaTextureId));
		}
		else
		{
			program->SetTextureSampler("s_diffuse", 0, *_whiteTexture);
			program->SetTextureSampler("s_alpha", 1, *_whiteTexture);
		}
		program->SetTextureSampler("s_landLuminosity", 6, GetLandLuminosity());
		program->SetTextureSampler("s_landLight", 7, GetLandLightTexture());
		program->SetTextureSampler("s_landColour", 8, GetLandColour());
		if (kept)
		{
			program->SetTextureSampler("s_hairKept", 9, _hairLightKept->GetColorAttachment());
		}
		else if (_whiteTexture.has_value())
		{
			program->SetTextureSampler("s_hairKept", 9, *_whiteTexture);
		}
		program->SetUniformValue("u_islandExtent", &islandExtent);
		program->SetUniformValue("u_landLight", &u_landLight);
		program->SetUniformValue("u_hairOrigin", &u_hairOrigin);
		program->SetUniformValue("u_hairAlphaCutOff", &u_hairAlphaCutOff);
		program->SetUniformValue("u_hairLight", &u_hairLight);
		program->SetUniformValue("u_hairAdded", &u_hairAdded);
		program->SetUniformValue("u_haze", &u_haze);
		program->SetUniformValue("u_hazeColour", &_haze[1]);
		bgfx::setVertexBuffer(0, &vertexBuffer);
		bgfx::setIndexBuffer(&indexBuffer);
		// The texels that the cut-off keeps blend over the scene by the texture's alpha and write their depth. Both sides
		// of a strand show.
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA |
		               BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA));
		program->Submit(viewId, zsort::Depth(middle, eye));
	}
}

void Renderer::DrawEnvironmentHair(const DrawSceneDesc& desc, entt::entity creature, const ecs::components::CreatureHair& hair,
                                   glm::vec3 origin) const
{
	using namespace creature_hair_light;
	const auto pass = TranslucentPassOf(desc.viewId);
	const auto eye = desc.camera->GetOrigin();
	// A body that the plan does not know of takes the colours of the frame before, as a body before all usual hair
	const auto found = _environmentHairLooks.find(creature);
	const auto look =
	    found != _environmentHairLooks.end() ? found->second : LookOf(Source {FromFrameBefore {}}, {}, _hairLightCarried);
	if (const auto* colours = std::get_if<Colours>(&look); colours != nullptr)
	{
		DrawCreatureHair(desc, hair, origin, pass, eye, colours->light, colours->added);
	}
	else if (const auto* place = std::get_if<AtPlace>(&look);
	         place != nullptr && place->draw < _hairLightSources.size() && _hairLightSources[place->draw].has_value())
	{
		// The colours that the earlier hair took at its place in this frame, under the same camera
		DrawCreatureHair(desc, hair, _hairLightSources[place->draw]->origin, pass, eye);
	}
	else
	{
		DrawCreatureHair(desc, hair, origin, pass, eye, std::nullopt, std::nullopt, true);
	}
}

void Renderer::PlanCreatureHairLight(const DrawSceneDesc& drawDesc) const
{
	using namespace creature_hair_light;
	using ecs::components::Creature;
	using ecs::components::CreatureAnimation;
	using ecs::components::CreatureHair;
	using ecs::components::CreatureSpells;
	using ecs::components::Transform;
	_environmentHairLooks.clear();
	_hairLightSources.clear();
	_hairLightLast.reset();
	if (drawDesc.camera == nullptr)
	{
		return;
	}
	const auto view = drawDesc.camera->GetViewMatrix(Camera::Interpolation::Current);
	const bool hairShown = Locator::creatureHairSystem::has_value() && Locator::creatureHairSystem::value().IsShown();
	std::vector<BodyDraw> draws;
	std::vector<entt::entity> mapped;
	// A body of the world: it draws only when its sphere is on the screen. The usual draw keeps the colours of the hair
	// that it shows.
	const auto addWorldBody = [&](entt::entity entity) {
		const auto& registry = drawDesc.entities;
		if (!ecs::DrawnOnScreen(registry, entity))
		{
			return;
		}
		const auto* creature = registry.TryGet<const Creature>(entity);
		const auto* spells = registry.TryGet<const CreatureSpells>(entity);
		const auto* hair = registry.TryGet<const CreatureHair>(entity);
		const auto* transform = registry.TryGet<const Transform>(entity);
		const bool environmentMapped = creature != nullptr && creature_environment_map::DrawnWithMap(creature->environmentMap);
		const float fizz = spells != nullptr ? spells->fizz : 0.0f;
		const float freeze = spells != nullptr ? spells->freeze : 0.0f;
		const bool keeps = !environmentMapped && hairShown && hair != nullptr && transform != nullptr &&
		                   creature_fizz_look::HairShown(fizz, freeze);
		draws.push_back({.environmentMapped = environmentMapped, .keepsHairLight = keeps});
		mapped.push_back(environmentMapped ? entity : entt::null);
		if (keeps)
		{
			const auto origin =
			    ecs::components::DrawnTransform(*transform, registry.TryGet<const CreatureAnimation>(entity)).position;
			_hairLightSources.push_back(HairLightSource {.origin = origin, .view = view, .landLightScale = 1.0f});
		}
		else
		{
			_hairLightSources.emplace_back(std::nullopt);
		}
	};

	// The creatures of the world that the frame draws. A hand never holds a creature, so no creature body goes to the depth
	// sorter.
	std::vector<entt::entity> world;
	for (const auto& drawn : _drawnCreatures)
	{
		if (drawn.posedBy == entt::null)
		{
			world.push_back(drawn.entity);
		}
	}
	// First the creatures of the land's draw list, in the order of the list
	std::unordered_set<entt::entity> listed;
	if (Locator::objectDrawListSystem::has_value())
	{
		const std::unordered_set<entt::entity> inWorld(world.begin(), world.end());
		for (const auto entity : Locator::objectDrawListSystem::value().GetList())
		{
			if (inWorld.contains(entity) && listed.insert(entity).second)
			{
				addWorldBody(entity);
			}
		}
	}
	// Then the other creatures of the world, which the list does not decide
	for (const auto entity : world)
	{
		if (!listed.contains(entity))
		{
			addWorldBody(entity);
		}
	}
	// The copies of creatures in the temple's rooms
	if (Locator::temple::has_value())
	{
		const auto& rooms = Locator::temple::value().GetRooms();
		for (const auto& drawn : _drawnCreatures)
		{
			if (drawn.posedBy == entt::null || !rooms.Valid(drawn.posedBy))
			{
				continue;
			}
			const auto* hair = rooms.TryGet<const CreatureHair>(drawn.posedBy);
			const auto* transform = rooms.TryGet<const Transform>(drawn.posedBy);
			const auto* spells = rooms.TryGet<const CreatureSpells>(drawn.posedBy);
			const float fizz = spells != nullptr ? spells->fizz : 0.0f;
			const float freeze = spells != nullptr ? spells->freeze : 0.0f;
			const bool keeps =
			    hairShown && hair != nullptr && transform != nullptr && creature_fizz_look::HairShown(fizz, freeze);
			draws.push_back({.environmentMapped = false, .keepsHairLight = keeps});
			mapped.push_back(entt::null);
			if (keeps)
			{
				const auto origin = ecs::components::DrawnTransform(
				                        *transform, drawDesc.entities.TryGet<const CreatureAnimation>(drawn.posedBy))
				                        .position;
				_hairLightSources.push_back(HairLightSource {.origin = origin, .view = view, .landLightScale = 1.0f});
			}
			else
			{
				_hairLightSources.emplace_back(std::nullopt);
			}
		}
	}
	// The creature of the tattoo editor, seen by the editor's own camera
	if (_menuCreature.has_value() && Locator::drawnRegistry::has_value())
	{
		const auto& drawn = Locator::drawnRegistry::value();
		const auto* hair = drawn.Valid(_menuCreature->copy) ? drawn.TryGet<const CreatureHair>(_menuCreature->copy) : nullptr;
		const auto* transform = hair != nullptr ? drawn.TryGet<const Transform>(_menuCreature->copy) : nullptr;
		const bool keeps = hairShown && transform != nullptr;
		draws.push_back({.environmentMapped = false, .keepsHairLight = keeps});
		mapped.push_back(entt::null);
		if (keeps)
		{
			_hairLightSources.push_back(HairLightSource {
			    .origin = transform->position,
			    .view = _menuCreature->view,
			    .landLightScale = CreatureLandLightScale(RenderPass::MenuCreature),
			});
		}
		else
		{
			_hairLightSources.emplace_back(std::nullopt);
		}
	}

	// Last the creatures of the statistics box, which draws after the front end. The box draws the hair far from its own
	// camera, so the hair does not show, but each usual hair draw keeps its colours. In the land, each frame first gives
	// every creature body a white light and no added colour. The world's draw of a creature then gives the body the light
	// and the haze at its place, also when the body is off the screen. The box uses the colours that the body has.
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	if (_interfaceCreatures.has_value() && !inTemple)
	{
		const std::unordered_set<entt::entity> drawnInWorld(world.begin(), world.end());
		const auto& registry = drawDesc.entities;
		for (const auto& shown : _interfaceCreatures->creatures)
		{
			const auto* transform = registry.Valid(shown.creature) ? registry.TryGet<const Transform>(shown.creature) : nullptr;
			if (transform == nullptr)
			{
				continue;
			}
			const auto* creature = registry.TryGet<const Creature>(shown.creature);
			const auto* spells = registry.TryGet<const CreatureSpells>(shown.creature);
			const bool environmentMapped =
			    creature != nullptr && creature_environment_map::DrawnWithMap(creature->environmentMap);
			const float fizz = spells != nullptr ? spells->fizz : 0.0f;
			const float freeze = spells != nullptr ? spells->freeze : 0.0f;
			const bool keeps = !environmentMapped && hairShown && registry.AllOf<CreatureHair>(shown.creature) &&
			                   creature_fizz_look::HairShown(fizz, freeze);
			draws.push_back({.environmentMapped = environmentMapped, .keepsHairLight = keeps});
			// The box shows no hair, so the look of the world's hair of this creature stays
			mapped.push_back(entt::null);
			if (!keeps)
			{
				_hairLightSources.emplace_back(std::nullopt);
				continue;
			}
			const auto origin =
			    ecs::components::DrawnTransform(*transform, registry.TryGet<const CreatureAnimation>(shown.creature)).position;
			_hairLightSources.push_back(HairLightSource {.origin = origin,
			                                             .view = view,
			                                             .landLightScale = 1.0f,
			                                             .own = StatisticsBoxColours(drawnInWorld.contains(shown.creature))});
		}
	}

	const auto plan = PlanFrame(draws);
	// A hair with colours of its own gives these colours. Each other hair takes the colours at its place.
	std::vector<std::optional<Colours>> known(draws.size());
	for (size_t i = 0; i < known.size() && i < _hairLightSources.size(); ++i)
	{
		if (_hairLightSources[i].has_value())
		{
			known[i] = _hairLightSources[i]->own;
		}
	}
	for (size_t i = 0; i < plan.hair.size(); ++i)
	{
		if (plan.hair[i].has_value() && mapped[i] != entt::null)
		{
			_environmentHairLooks.insert_or_assign(mapped[i], LookOf(*plan.hair[i], known, _hairLightCarried));
		}
	}
	_hairLightLast = plan.last;
}

void Renderer::KeepCreatureHairLight() const
{
	using namespace creature_hair_light;
	std::optional<HairLightSource> source;
	if (_hairLightLast.has_value() && *_hairLightLast < _hairLightSources.size())
	{
		source = _hairLightSources[*_hairLightLast];
	}
	_hairLightLast.reset();
	// A hair with colours of its own gives them to the next frame on the CPU
	if (source.has_value() && source->own.has_value())
	{
		_hairLightCarried = *source->own;
		return;
	}
	// A new target keeps the start colours until a hair gives it others
	const bool made = _hairLightKept == nullptr;
	if (made)
	{
		_hairLightKept =
		    std::make_unique<FrameBuffer>("HairLight", 2, 1, TextureFormat::RGBA8, std::nullopt, 1, Wrapping::ClampEdge);
	}
	// After a restore of the graphics device, the target keeps the colours of the same hair again
	if (!source.has_value() && made && std::holds_alternative<Kept>(_hairLightCarried))
	{
		source = _hairLightKeptSource;
	}
	if ((!source.has_value() && !made) || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto* program = _shaderManager->GetShader("HairLightKept");
	const auto extent = Locator::terrainSystem::value().GetExtent();
	const auto islandExtent = glm::vec4(extent.minimum, extent.maximum);
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	const bool landLit = !inTemple && _landLightTexture.has_value();
	const auto scale = source.has_value() ? source->landLightScale : 1.0f;
	const glm::vec4 u_landLight {landLit ? 1.0f : 0.0f, scale, 0.0f, 0.0f};
	const glm::vec4 u_hairOrigin {source.has_value() ? source->origin : glm::vec3(0.0f), 0.0f};
	// Without a hair, the start colours as the colours of a look of its own
	const glm::vec4 u_hairLight {k_Start.light, source.has_value() ? 0.0f : 1.0f};
	const glm::vec4 u_hairAdded {k_Start.added, 0.0f};
	const auto u_haze = inTemple ? glm::vec4(0.0f) : _haze[0];
	const auto hairView = source.has_value() ? source->view : glm::mat4(1.0f);
	const auto viewId = SetUpLandView(RenderPass::HairLight, *_hairLightKept, {2, 1});
	for (const float texel : {0.0f, 1.0f})
	{
		program->SetTextureSampler("s_landLuminosity", 6, GetLandLuminosity());
		program->SetTextureSampler("s_landLight", 7, GetLandLightTexture());
		program->SetTextureSampler("s_landColour", 8, GetLandColour());
		program->SetUniformValue("u_islandExtent", &islandExtent);
		program->SetUniformValue("u_landLight", &u_landLight);
		program->SetUniformValue("u_hairOrigin", &u_hairOrigin);
		program->SetUniformValue("u_hairLight", &u_hairLight);
		program->SetUniformValue("u_hairAdded", &u_hairAdded);
		program->SetUniformValue("u_hairView", &hairView);
		program->SetUniformValue("u_haze", &u_haze);
		program->SetUniformValue("u_hazeColour", &_haze[1]);
		SubmitLandQuad(viewId, *program, {texel, 0.0f}, {texel + 1.0f, 1.0f}, {texel, 0.0f}, {texel, 0.0f},
		               BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
	}
	if (source.has_value())
	{
		_hairLightCarried = Kept {};
		_hairLightKeptSource = source;
	}
}

void Renderer::UploadCreatureSkins(const ecs::Registry& entities, std::unordered_map<entt::entity, CreatureSkins>& skins) const
{
	std::vector<entt::entity> seen;
	entities.Each<const ecs::components::CreatureSkin>([&](entt::entity entity, const ecs::components::CreatureSkin& skin) {
		if (skin.skins.empty())
		{
			return;
		}
		seen.push_back(entity);
		auto& entry = skins[entity];
		const painted_skin::Stamp stamp {.registryResets = entities.GetResets(), .revision = skin.revision};
		if (!painted_skin::NeedsUpload(entry.stamp, stamp) && entry.textures.size() == skin.skins.size())
		{
			return;
		}
		// The skins as painted this frame, before the body is drawn with them
		entry.textures.resize(skin.skins.size());
		entry.drawn.clear();
		for (size_t i = 0; i < skin.skins.size(); ++i)
		{
			const auto& painted = skin.skins[i];
			auto& texture = entry.textures[i];
			// The skins are at half size at the two lowest detail levels
			const auto side = l3d_skin::SideOf(painted.texels.size());
			if (texture && texture->GetResolution().x != side)
			{
				texture.reset();
			}
			if (!texture)
			{
				texture = TakeCreatureSkinTexture(side);
				// Out of textures, as with thousands of creatures: this one keeps its species' skin as it was made
				if (!texture)
				{
					if (!_warnedOutOfSkins)
					{
						_warnedOutOfSkins = true;
						SPDLOG_LOGGER_WARN(
						    spdlog::get("graphics"),
						    "Out of textures for the creatures' painted skins: the rest keep their species' skins");
					}
					continue;
				}
			}
			texture->Update(painted.texels.data(), static_cast<uint32_t>(painted.texels.size() * sizeof(painted.texels[0])));
			entry.drawn.emplace_back(painted.id, texture.get());
		}
		entry.stamp = stamp;
	});
	// The skins of creatures no longer about go, looked up sorted rather than one by one through every creature's
	std::ranges::sort(seen);
	std::erase_if(skins, [this, &seen](auto& entry) {
		if (std::ranges::binary_search(seen, entry.first))
		{
			return false;
		}
		// Keep the textures for the next creature
		for (auto& texture : entry.second.textures)
		{
			if (texture)
			{
				_spareCreatureSkins.push_back(std::move(texture));
			}
		}
		return true;
	});
}

std::unique_ptr<Texture2D> Renderer::TakeCreatureSkinTexture(uint16_t side) const
{
	if (!_spareCreatureSkins.empty() && _spareCreatureSkins.back()->GetResolution().x == side)
	{
		auto texture = std::move(_spareCreatureSkins.back());
		_spareCreatureSkins.pop_back();
		return texture;
	}
	return MakeCreatureSkinTexture(side);
}

std::unique_ptr<Texture2D> Renderer::MakeCreatureSkinTexture(uint16_t side)
{
	auto texture = std::make_unique<Texture2D>("Creature Skin");
	texture->CreateWithinFrame(side, side, 1, TextureFormat::BGRA4, Wrapping::Repeat, Filter::Linear, nullptr);
	if (!bgfx::isValid(toBgfx(texture->GetNativeHandle())))
	{
		return nullptr;
	}
	// After a restore of the graphics device the creatures' painted skins go to their textures again
	texture->SetTexelsGivenAgain();
	return texture;
}

void Renderer::PrepareCreatureSkins(size_t count) const noexcept
{
	const bool lowResolution = Locator::config::has_value() && Locator::config::value().detail.lowResTextures;
	while (_spareCreatureSkins.size() < count)
	{
		auto texture = MakeCreatureSkinTexture(l3d_skin::LoadedSide(lowResolution));
		if (!texture)
		{
			return;
		}
		_spareCreatureSkins.push_back(std::move(texture));
	}
}

void Renderer::SelectDrawnCreatures(const DrawSceneDesc& drawDesc) const
{
	const auto& draws = Locator::rendereringSystem::value().GetContext().entityDraws;
	_drawnCreatures.clear();
	for (const auto& [entity, instance] : draws)
	{
		// The animals are drawn one by one by themselves
		if (!drawDesc.entities.AnyOf<ecs::components::AnimalPose, ecs::components::VillagerPose,
		                             ecs::components::AnimatedStaticPose>(entity))
		{
			_drawnCreatures.push_back({.entity = entity, .instance = instance});
		}
	}
	// The copies of creatures drawn in poses of their own, as the creature in the temple's creature room
	for (const auto& copy : Locator::rendereringSystem::value().GetContext().creatureCopyDraws)
	{
		_drawnCreatures.push_back(
		    {.entity = copy.creature, .instance = copy.instance, .posedBy = copy.copy, .light = copy.light});
	}
	const auto cap = MaxCreaturesDrawn();
	if (_drawnCreatures.size() <= cap || drawDesc.camera == nullptr)
	{
		return;
	}
	if (!_warnedCreatureCap)
	{
		_warnedCreatureCap = true;
		SPDLOG_LOGGER_WARN(spdlog::get("graphics"),
		                   "{} creatures to draw, more than the renderer can upload the bones of each frame: only the {} "
		                   "nearest the camera are drawn",
		                   _drawnCreatures.size(), cap);
	}
	const auto camera = drawDesc.camera->GetOrigin();
	const auto distance = [&drawDesc, camera](const DrawnCreature& draw) {
		const auto* transform = drawDesc.entities.TryGet<const ecs::components::Transform>(draw.entity);
		return transform != nullptr ? glm::distance2(transform->position, camera) : std::numeric_limits<float>::max();
	};
	std::ranges::nth_element(_drawnCreatures, _drawnCreatures.begin() + static_cast<std::ptrdiff_t>(cap), std::less {},
	                         distance);
	_drawnCreatures.resize(cap);
}

size_t Renderer::MaxCreaturesDrawn()
{
	// Each creature's body is drawn a submesh at a time in several passes, with every bone's matrix uploaded with each
	// draw, and the backends set aside a fixed amount of memory for each frame's uploads. Past this many creatures it
	// would overflow, which the backends don't survive. Holding the bones in a buffer of their own would lift this.
	switch (bgfx::getRendererType())
	{
	case bgfx::RendererType::Vulkan:
		return k_MaxCreaturesDrawnVulkan;
	case bgfx::RendererType::Direct3D12:
		return k_MaxCreaturesDrawnDirect3D12;
	default:
		return std::numeric_limits<size_t>::max();
	}
}
