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

#include <cstring>

#include <algorithm>

#include <bgfx/bgfx.h>
#include <glm/gtx/transform.hpp>

#include "3D/L3DMesh.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DrawUpdate.h"
#include "Graphics/DebugLines.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/ShaderManager.h"
#include "Locator.h"
#include "Profiler.h"
#include "RenderingSystemVerify.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
/// One instance as the buffer holds it: the model matrix, then its colours
constexpr uint32_t k_InstanceStride = sizeof(glm::mat4) + sizeof(glm::vec4);

/// The end of the last instance range a build wrote: the profile's rows, and the end of the upload without the debug
/// bounding boxes (in the lists' second half, left out here)
uint64_t UsedRows(const RenderContext& context)
{
	uint64_t end = 0;
	for (const auto* descs :
	     {&context.instancedDrawDescs, &context.translucentDrawDescs, &context.sortedOpaqueDrawDescs, &context.cutAtomDrawDescs,
	      &context.psysAtomDrawDescs, &context.shadowCasterDrawDescs, &context.footprintOnlyDrawDescs})
	{
		for (const auto& [meshId, desc] : *descs)
		{
			end = std::max<uint64_t>(end, static_cast<uint64_t>(desc.offset) + desc.count);
		}
	}
	return end;
}
} // namespace

RenderContext::RenderContext()
    : instanceUniformBuffer(BGFX_INVALID_HANDLE)
{
}
RenderContext::~RenderContext()
{
	if (bgfx::isValid(toBgfx(instanceUniformBuffer)))
	{
		bgfx::destroy(toBgfx(instanceUniformBuffer));
		bgfx::frame();
		bgfx::frame();
	}
}

RenderingSystemCommon::~RenderingSystemCommon() = default;

void RenderingSystemCommon::ResizeInstances(uint32_t capacity)
{
	if (bgfx::isValid(toBgfx(_renderContext.instanceUniformBuffer)))
	{
		bgfx::destroy(toBgfx(_renderContext.instanceUniformBuffer));
	}
	// i_data0..i_data3 the model matrix's columns, i_data4 the colours (bgfx maps i_data k to TEXCOORD 7 - k)
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord6, 4, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord5, 4, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord4, 4, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord3, 4, bgfx::AttribType::Float)
	    .end();
	_renderContext.instanceUniformBuffer = graphics::fromBgfx(bgfx::createDynamicVertexBuffer(capacity, layout));
	_renderContext.instanceUniforms.resize(capacity);
	_renderContext.instanceColours.assign(capacity, glm::vec4(0.0f));
}

void RenderingSystemCommon::UploadInstances()
{
	// Only the rows a draw reads: every draw takes its instances from the draw lists' ranges, and the debug boxes
	// (when shown) draw the whole second half of the lists, so with them the whole capacity is uploaded. The rows
	// after that end keep what the buffer held before, which no draw reads
	const auto capacity = static_cast<uint32_t>(_renderContext.instanceUniforms.size());
	const auto count =
	    _boundingBoxRows ? capacity : static_cast<uint32_t>(std::min<uint64_t>(UsedRows(_renderContext), capacity));
	_uploadedRows = count;
	if (count == 0)
	{
		return;
	}
	auto upload = Locator::profiler::value().BeginScoped(Profiler::Stage::DrawUpload);
	const bgfx::Memory* memory = bgfx::alloc(count * k_InstanceStride);
	for (uint32_t i = 0; i < count; ++i)
	{
		std::memcpy(memory->data + i * k_InstanceStride, &_renderContext.instanceUniforms[i], sizeof(glm::mat4));
		std::memcpy(memory->data + i * k_InstanceStride + sizeof(glm::mat4), &_renderContext.instanceColours[i],
		            sizeof(glm::vec4));
	}
	bgfx::update(toBgfx(_renderContext.instanceUniformBuffer), 0, memory);
}

void RenderingSystemCommon::SetDirty()
{
	_renderContext.dirty = true;
}

void RenderingSystemCommon::SetLayoutDirty()
{
	_renderContext.layoutDirty = true;
}

void RenderingSystemCommon::PrepareDraw(bool drawBoundingBox, bool drawFootpaths, bool drawStreams)
{
	auto& registry = Locator::entitiesRegistry::value();

	const bool optionsChanged = _renderContext.hasBoundingBoxes != drawBoundingBox ||
	                            (_renderContext.footpaths != nullptr) != drawFootpaths ||
	                            (_renderContext.streams != nullptr) != drawStreams;
	const auto update = ChooseDrawUpdate(_renderContext.dirty || _renderContext.layoutDirty, _renderContext.layoutDirty,
	                                     optionsChanged, drawBoundingBox);
	if (update != DrawUpdate::None)
	{
		// (openblack engine) the profile's "Draw Descs" / "Draw Uniforms": how long, and how often (ran N x), the
		// draw lists are made and the instances written
		auto& profiler = Locator::profiler::value();
		_boundingBoxRows = drawBoundingBox;
		BeginPrepareDraw();
		// While only what the drawn entities look like changed, the instances are written again into the ranges they
		// have; a refill that finds the draw lists no longer fit (an entity in another range, a count changed) makes
		// them again
		bool refilled = false;
		if (update == DrawUpdate::Refill)
		{
			std::fill(_renderContext.instanceColours.begin(), _renderContext.instanceColours.end(), glm::vec4(0.0f));
			auto uniforms = profiler.BeginScoped(Profiler::Stage::DrawUniforms);
			refilled = RefillKeepingDescs();
		}
		if (!refilled)
		{
			{
				auto descs = profiler.BeginScoped(Profiler::Stage::DrawDescs);
				PrepareDrawDescs(drawBoundingBox);
			}
			std::fill(_renderContext.instanceColours.begin(), _renderContext.instanceColours.end(), glm::vec4(0.0f));
			{
				auto uniforms = profiler.BeginScoped(Profiler::Stage::DrawUniforms);
				PrepareDrawUploadUniforms(drawBoundingBox);
			}
		}
		// OPENBLACK_INSTANCE_VERIFY=1: the same build once more into the context, compared byte by byte with the
		// first, which is put back and uploaded again (RenderingSystemVerify.h); the stage times are not the plain ones
		if (instance_verify::Enabled())
		{
			instance_verify::Check(_renderContext, [this, drawBoundingBox]() {
				PrepareDrawDescs(drawBoundingBox);
				std::fill(_renderContext.instanceColours.begin(), _renderContext.instanceColours.end(), glm::vec4(0.0f));
				PrepareDrawUploadUniforms(drawBoundingBox);
			});
			UploadInstances();
		}
		// the profile's counters (Profiler::Counter): why, the rows, the bytes UploadInstances gave bgfx
		if (profiler.Counting())
		{
			const uint64_t bytes = static_cast<uint64_t>(_uploadedRows) * k_InstanceStride;
			if (refilled)
			{
				profiler.CountDrawRefill(UsedRows(_renderContext), bytes);
			}
			else
			{
				profiler.CountDrawRebuild(_renderContext.dirty || _renderContext.layoutDirty, UsedRows(_renderContext), bytes);
			}
		}

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
			uint32_t edgeCount = 0;
			registry.Each<const Stream>([&edgeCount](const Stream& ent) {
				edgeCount += static_cast<uint32_t>(ent.points.size() > 1 ? ent.points.size() - 1 : 0);
			});
			std::vector<graphics::DebugLines::Vertex> edges;
			edges.reserve(edgeCount * 2);
			registry.Each<const Stream>([&edges](const Stream& ent) {
				const auto color = glm::vec4(1, 0, 0, 1);
				for (size_t i = 0; i + 1 < ent.points.size(); ++i)
				{
					edges.push_back({glm::vec4(ent.points[i], 1.0f), color});
					edges.push_back({glm::vec4(ent.points[i + 1], 1.0f), color});
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
}
