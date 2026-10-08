/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The chain ribbons of the particle effects (the lightning bolt and the storm's forks, the gesture trail, the creature
// beam): ParticleChainCreator's collection draw - a camera-facing strip through the collection's joints, each joint
// widened by its scale along normalize(cross(view direction, segment direction)), the texture frame across it and
// NumTexturesForWholeChain repeats along it. See Particles/Creators/Chain.h.
//
// Where a ribbon is drawn depends on its effect's draw path (psys::DrawPath): Sorted gives it its own Z object at the
// joint n / 2 (manager::SortedChain::key); Queued and Immediate draw it at once, at its place in its effect's items
// (manager::OrderedEffect). Renderer.cpp does both; the ribbon is drawn the same way either way.

#include <cmath>
#include <cstring>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>

#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "Camera/Camera.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Particles/Creators/Chain.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

void Renderer::DrawParticleChain(RenderPass viewId, const Camera& camera, const psys::Effect::DrawChain& chain) const
{
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	const auto& textures = Locator::resources::value().GetTextures();
	const auto* program = _shaderManager->GetShader("WorldQuad");
	const glm::vec3 eye = camera.GetOrigin();

	{
		const auto* creator = dynamic_cast<const psys::ChainCreator*>(chain.creator);
		if (creator == nullptr)
		{
			return;
		}
		const auto [texture, alphaTexture] = RawTextureIds(creator->texture);
		if (!textures.Contains(texture))
		{
			return;
		}
		const auto segments = static_cast<int>(chain.joints.size()) - 1;
		// four vertices per segment, (head + side, head - side, tail + side, tail - side), each end widened by its own
		// side vector: normalize(cross(eye - joint, segment)) (the sign of billboard::RibbonSide) x the joint's scale
		// (side x scale / |side|; a zero side stays zero), so the half width is the scale itself
		std::vector<glm::vec3> corners(static_cast<size_t>(segments) * 4);
		for (int i = 0; i < segments; ++i)
		{
			const auto& head = chain.joints[static_cast<size_t>(i)];
			const auto& tail = chain.joints[static_cast<size_t>(i) + 1];
			const auto along = tail.position - head.position;
			// (eye - joint) x segment, unnormalised as in the original, then side / |side| x scale; only an exactly
			// zero side is left as it is, so a zero-length segment (two joints at one point, the
			// gesture trail of a still hand) collapses to its joint instead of the NaN billboard::RibbonSide's
			// normalize(segment) would give
			const auto sideOf = [&eye, &along](const glm::vec3& joint, float scale) {
				const auto side = glm::cross(eye - joint, along);
				const float length = glm::length(side);
				// the half width (billboard::RibbonHalfWidth): side x the joint's scale / |side|, the same value a
				// sprite takes as its half size
				return length > 0.0f ? side * (billboard::RibbonHalfWidth(scale) / length) : glm::vec3(0.0f);
			};
			const auto sideHead = sideOf(head.position, head.scale);
			const auto sideTail = sideOf(tail.position, tail.scale);
			auto* c = &corners[static_cast<size_t>(i) * 4];
			c[0] = head.position + sideHead;
			c[1] = head.position - sideHead;
			c[2] = tail.position + sideTail;
			c[3] = tail.position - sideTail;
		}
		// where two segments meet, both pairs of vertices move to their midpoints, so the strip has no gaps or steps at
		// the joints (the original's other mode, off here, would copy the previous tail instead)
		for (int i = 1; i < segments; ++i)
		{
			auto* previous = &corners[static_cast<size_t>(i - 1) * 4];
			auto* c = &corners[static_cast<size_t>(i) * 4];
			c[0] = previous[2] = (previous[2] + c[0]) * 0.5f;
			c[1] = previous[3] = (previous[3] + c[1]) * 0.5f;
		}
		// Not ported: UseDynamicLighting (colour x clamp(0.6 + 0.4 n.L)) and the joints' jitter. The V scroll is the
		// collection's chain scroll, advanced by chain_atoms::AdvanceScroll (frame_anim::ChainScroll; its rate is 0 but
		// for UR_SimpleBeam / UR_Plasma, not ported)
		const float scroll = chain.collection != nullptr ? chain.collection->chainScroll : 0.0f;
		const frame_anim::UvOffset offset(0.0f, scroll);
		std::vector<Vertex> vertices;
		for (int i = 0; i < segments; ++i)
		{
			const auto& head = chain.joints[static_cast<size_t>(i)];
			const auto& tail = chain.joints[static_cast<size_t>(i) + 1];
			const auto* c = &corners[static_cast<size_t>(i) * 4];
			// ChainCreator::SegmentUv: (u0, v0) (u1, v0) (u0, v1) (u1, v1) on the four vertices in that order (uv0 on
			// head + side): U across, V along the segment, the v-scroll on all four. The original also puts the same
			// scroll in the texture offset of the draw, which adds it once more: the chain's material does not turn that
			// off, so the scroll is applied twice
			const auto segmentUv =
			    creator->SegmentUv(i, segments, scroll, chain.collection != nullptr ? chain.collection->chainTextures : -1);
			const std::array<glm::vec2, 4> uvs = {
			    frame_anim::OffsetUv(segmentUv[0], offset, false), frame_anim::OffsetUv(segmentUv[1], offset, false),
			    frame_anim::OffsetUv(segmentUv[2], offset, false), frame_anim::OffsetUv(segmentUv[3], offset, false)};
			const std::array<const psys::Effect::DrawAtom*, 4> ends = {&head, &head, &tail, &tail};
			// the index list: (0, 1, 2) and (1, 3, 2) per segment
			for (const int k : {0, 1, 2, 1, 3, 2})
			{
				const auto& end = *ends[static_cast<size_t>(k)];
				const auto alpha = static_cast<uint32_t>(std::clamp(end.alpha, 0.0f, 255.0f));
				const uint32_t abgr = (alpha << 24) | (static_cast<uint32_t>(end.colour[2]) << 16) |
				                      (static_cast<uint32_t>(end.colour[1]) << 8) | end.colour[0];
				vertices.push_back(
				    {c[k].x, c[k].y, c[k].z, uvs[static_cast<size_t>(k)].x, uvs[static_cast<size_t>(k)].y, abgr});
			}
		}
		if (vertices.empty())
		{
			return;
		}
		const auto& layout = ParticleQuadLayout();
		const auto count = static_cast<uint32_t>(vertices.size());
		if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
		{
			return;
		}
		bgfx::TransientVertexBuffer buffer;
		bgfx::allocTransientVertexBuffer(&buffer, count, layout);
		std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(Vertex));
		program->SetTextureSampler("s_diffuse", 0, *textures.Handle(texture));
		program->SetTextureSampler(
		    "s_alpha", 1, textures.Contains(alphaTexture) ? *textures.Handle(alphaTexture) : *textures.Handle(texture));
		bgfx::setVertexBuffer(0, &buffer);
		// the material (inferred): as the particles', modes 13 / 6 (12 / 5 with writeDepth), Z test on; no cull state,
		// so MaterialSetDoubleSided is ignored and every chain draws two-sided
		const auto mode =
		    render_modes::ModeFromProperties(render_modes::Mode::AlphaTexturedAlphaNoZWrite,
		                                     {.additive = creator->additive, .zWrite = creator->writeDepth, .alpha = true});
		bgfx::setState(render_modes::State(mode));
		bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
	}
}
