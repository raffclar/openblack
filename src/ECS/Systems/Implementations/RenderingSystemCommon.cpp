/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "RenderingSystemCommon.h"

#include <algorithm>
#include <vector>

#include <glm/gtx/transform.hpp>

#include "3D/L3DMesh.h"
#include "Camera/Camera.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "Fire/FireGraphic.h"
#include "Graphics/BonePalette.h"
#include "Graphics/DebugLines.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{

/// The light that the sea mirrors a held thing in this frame, and the light that the thing keeps for the next frame
openblack::graphics::hand_reflection::HeldLightStep HeldLightStepOf(const openblack::ecs::Registry& registry,
                                                                    entt::entity entity, glm::vec3 drawnAt, bool tree,
                                                                    const openblack::Camera* camera)
{
	using namespace openblack::graphics::hand_reflection;
	HeldThing thing {.drawnAt = drawnAt, .takenAt = drawnAt, .tree = tree};
	if (camera != nullptr)
	{
		thing.eye = camera->GetOrigin();
		thing.forward = camera->GetForward();
	}
	std::optional<HeldLook> kept;
	if (const auto* light = registry.TryGet<const HeldLight>(entity); light != nullptr)
	{
		thing.takenAt = light->takenAt;
		if (light->drawn)
		{
			kept = HeldLook {.point = light->point,
			                 .viewDepth = light->viewDepth,
			                 .tint = light->tint,
			                 .added = light->added,
			                 .treeGrey = light->treeGrey};
		}
	}
	if (const auto* fire = registry.TryGet<const Fire>(entity); fire != nullptr && openblack::Locator::fireSystem::has_value())
	{
		const auto& fires = openblack::Locator::fireSystem::value();
		if (tree)
		{
			const auto look = fires.GetBurningTreeLook(entity);
			thing.onFire = look.has_value();
			thing.treeGrey = look.has_value() ? static_cast<uint32_t>(look->grey) : 0u;
		}
		else
		{
			thing.onFire = true;
			thing.charredGrey = openblack::fire::graphic::CharredGrey(fire->state.charring);
			thing.glow = fires.GetGlowColour(entity).value_or(0u);
		}
	}
	if (const auto* pot = registry.TryGet<const Pot>(entity); pot != nullptr && pot->poisoned)
	{
		thing.poisonedPot = true;
	}
	return StepHeldLight(kept, HeldLookOf(thing));
}
} // namespace

RenderContext::RenderContext()
    : instanceUniformBuffer(BGFX_INVALID_HANDLE)
    , treeInstanceUniformBuffer(BGFX_INVALID_HANDLE)
    , partialBuildInstanceBuffer(BGFX_INVALID_HANDLE)
    , heldInstanceBuffer(BGFX_INVALID_HANDLE)
    , soulInstanceBuffer(BGFX_INVALID_HANDLE)
{
}
RenderContext::~RenderContext()
{
	for (const auto& handle :
	     {instanceUniformBuffer, treeInstanceUniformBuffer, partialBuildInstanceBuffer, heldInstanceBuffer, soulInstanceBuffer})
	{
		if (bgfx::isValid(toBgfx(handle)))
		{
			bgfx::destroy(toBgfx(handle));
			bgfx::frame();
			bgfx::frame();
		}
	}
}

RenderingSystemCommon::~RenderingSystemCommon() = default;

void RenderingSystemCommon::SetDirty()
{
	_renderContext.dirty = true;
}

void RenderingSystemCommon::SetLayoutDirty()
{
	_renderContext.dirty = true;
	_renderContext.layoutDirty = true;
}

void RenderingSystemCommon::PrepareDraw(bool drawBoundingBox, bool drawFootpaths, bool drawStreams)
{
	const auto& registry = Locator::entitiesRegistry::value();

	_renderContext.animatedBoneMatrices.clear();
	_renderContext.handMirrored = false;
	registry.Each<const Hand, const Mesh, const Transform>(
	    [this](const Hand& hand, const Mesh& mesh, const Transform& transform) {
		    if (!hand.boneMatrices.empty())
		    {
			    _renderContext.animatedBoneMatrices.insert_or_assign(mesh.id, hand.boneMatrices);
			    _renderContext.handMirrored = transform.scale.x * transform.scale.y * transform.scale.z < 0.0f;
		    }
	    });

	_renderContext.streamSegments.clear();
	registry.Each<const StreamSegment, const Transform>(
	    // The segment tag is empty, so only the transform is passed
	    [this](const Transform& transform) {
		    _renderContext.streamSegments.push_back(glm::translate(transform.position) * glm::mat4(transform.rotation) *
		                                            glm::scale(transform.scale));
	    });

	const bool optionsChanged = _renderContext.hasBoundingBoxes != drawBoundingBox ||
	                            (_renderContext.footpaths != nullptr) != drawFootpaths ||
	                            (_renderContext.streams != nullptr) != drawStreams;
	// While things only move, the draw lists stay as they are and only the instances are uploaded again
	if (!_renderContext.layoutDirty && !optionsChanged)
	{
		if (_renderContext.dirty && !UploadUniformsKeepingDescs(drawBoundingBox))
		{
			_renderContext.layoutDirty = true;
		}
		else
		{
			_renderContext.dirty = false;
		}
	}

	if (_renderContext.layoutDirty || optionsChanged)
	{
		PrepareDrawDescs(drawBoundingBox);
		PrepareDrawUploadUniforms(drawBoundingBox);

		_renderContext.boundingBox.reset();
		if (drawBoundingBox)
		{
			_renderContext.boundingBox = graphics::DebugLines::CreateBox(glm::vec4(1.0f, 0.0f, 0.0f, 0.5f));
		}

		_renderContext.footpaths.reset();
		if (drawFootpaths)
		{
			uint32_t nodeCount = 0;
			registry.Each<const Footpath>(
			    [&nodeCount](const Footpath& ent) { nodeCount += 2 * std::max(static_cast<int>(ent.nodes.size()) - 1, 0); });

			std::vector<graphics::DebugLines::Vertex> edges;
			edges.reserve(nodeCount);
			registry.Each<const Footpath>([&edges](const Footpath& ent) {
				const auto color = glm::vec4(0, 1, 0, 1);
				const auto offset = glm::vec3(0, 1, 0);
				for (int i = 0; i < static_cast<int>(ent.nodes.size()) - 1; ++i)
				{
					edges.push_back({glm::vec4(ent.nodes[i].position + offset, 1.0f), color});
					edges.push_back({glm::vec4(ent.nodes[i + 1].position + offset, 1.0f), color});
				}
			});
			if (!edges.empty())
			{
				_renderContext.footpaths =
				    graphics::DebugLines::CreateDebugLines(edges.data(), static_cast<uint32_t>(edges.size()));
			}
		}

		_renderContext.streams.reset();
		if (drawStreams)
		{
			std::vector<graphics::DebugLines::Vertex> edges;
			registry.Each<const Stream>([&edges](const Stream& ent) {
				const auto color = glm::vec4(1, 0, 0, 1);
				for (size_t i = 1; i < ent.points.size(); ++i)
				{
					edges.push_back({glm::vec4(ent.points[i - 1], 1.0f), color});
					edges.push_back({glm::vec4(ent.points[i], 1.0f), color});
				}
			});

			if (!edges.empty())
			{
				_renderContext.streams =
				    graphics::DebugLines::CreateDebugLines(edges.data(), static_cast<uint32_t>(edges.size()));
			}
		}

		_renderContext.dirty = false;
		_renderContext.layoutDirty = false;
		_renderContext.hasBoundingBoxes = drawBoundingBox;
	}

	KeepHeldLights();
}

void RenderingSystemCommon::KeepHeldLights()
{
	auto& registry = Locator::entitiesRegistry::value();
	// A thing out of the hand keeps no light
	std::vector<entt::entity> released;
	registry.Each<const HeldLight>(
	    [&released](entt::entity entity, const HeldLight& /*unused*/) { released.push_back(entity); }, entt::exclude<InHand>);
	for (const auto entity : released)
	{
		registry.Remove<HeldLight>(entity);
	}
	// Each draw of a thing in the hand takes the light of this frame, which the thing keeps for the next frame. The sea
	// mirrors it in the light that it kept the frame before.
	const auto* camera = Locator::camera::has_value() ? &Locator::camera::value() : nullptr;
	const auto keep = [&registry, camera](bool tree) {
		return [&registry, camera, tree](RenderContext::HighDetailDraw& draw) {
			const auto step = HeldLightStepOf(registry, draw.entity, glm::vec3(draw.model[3]), tree, camera);
			draw.heldLight = step.mirrored;
			auto* kept = registry.TryGet<HeldLight>(draw.entity);
			if (kept == nullptr)
			{
				// A thing put in the hand some other way keeps its place where it is first drawn there
				kept = &registry.Assign<HeldLight>(draw.entity, HeldLight {.takenAt = step.kept.point});
			}
			// The light changes in place, so that the draw lists stay as they are
			kept->drawn = true;
			kept->point = step.kept.point;
			kept->viewDepth = step.kept.viewDepth;
			kept->tint = step.kept.tint;
			kept->added = step.kept.added;
			kept->treeGrey = step.kept.treeGrey;
		};
	};
	std::ranges::for_each(_renderContext.inHandDraws, keep(false));
	std::ranges::for_each(_renderContext.heldTreeDraws, keep(true));
}

void RenderingSystemCommon::UploadBonePalette()
{
	auto& texels = _renderContext.bonePalette;
	if (texels.empty())
	{
		return;
	}
	// Whole rows are sent, the last one filled out
	const auto rows = graphics::bone_palette::RowsFor(texels.size());
	texels.resize(static_cast<size_t>(rows) * graphics::bone_palette::k_Width, glm::vec4(0.0f));
	auto& texture = _renderContext.bonePaletteTexture;
	// The texture grows to twice the rows it needs, so that a growing crowd doesn't make it again every frame
	if (!texture || texture->GetResolution().y < rows)
	{
		texture = std::make_unique<graphics::Texture2D>("BonePalette");
		texture->CreateWithinFrame(graphics::bone_palette::k_Width, static_cast<uint16_t>(rows * 2), 1,
		                           graphics::TextureFormat::RGBA32F, graphics::Wrapping::ClampEdge, graphics::Filter::Nearest,
		                           nullptr);
	}
	const auto bytes = static_cast<uint32_t>(texels.size() * sizeof(glm::vec4));
	bgfx::updateTexture2D(toBgfx(texture->GetNativeHandle()), 0, 0, 0, 0, graphics::bone_palette::k_Width, rows,
	                      bgfx::copy(texels.data(), bytes));
}
