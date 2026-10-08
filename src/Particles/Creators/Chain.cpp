/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Chain.h"

#include <cstdlib>

#include <algorithm>
#include <memory>

#include <spdlog/spdlog.h>

#include "3D/FrameAnim.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysManager.h"
#include "Particles/PSysRegistry.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
/// The original's chain creator keeps the joints of one collection; here the collection itself is the
/// chain, so the creator only holds the material and the UV layout
std::unique_ptr<Creator> MakeChainCreator(const Object& object)
{
	auto creator = std::make_unique<ChainCreator>();
	ReadCreatorProperties(object, *creator);
	creator->kind = Creator::Kind::Chain;
	creator->texture = TextureBaseName(object.String("TextureFileName"));
	creator->additive = object.Bool("UseAdditiveAlpha", true);
	creator->writeDepth = object.Bool("MaterialUpdateZBuffer", false);
	creator->doubleSided = object.Bool("MaterialSetDoubleSided", false);
	creator->dynamicLighting = object.Bool("UseDynamicLighting", false);
	creator->fileOffset = object.Int("FileOffset", 0);
	creator->frameOfHead = object.Int("FrameOfHead", 0);
	creator->frameOfTail = object.Int("FrameOfTail", 0);
	// defaults: FrameHeight 64, FrameWidth 32, NumTexturesForWholeChain -1 (the properties' ranges [1, 256] and
	// [-1, 32])
	creator->numTexturesForWholeChain = object.Int("NumTexturesForWholeChain", -1);
	creator->frameWidth = std::max(1, object.Int("FrameWidth", 32));
	creator->frameHeight = std::max(1, object.Int("FrameHeight", 64));
	return creator;
}
} // namespace

std::array<glm::vec2, 4> ChainCreator::SegmentUv(int index, int segments, float scroll, int textures) const
{
	// the ribbon draw asks it with frame 0: the chain is cut in T repeats; segment s falls in repeat
	// k = ((s + 1) T - 1) / (n - 1) (integer division), which starts at segment k (n - 1) / T and holds
	// (k + 1)(n - 1) / T - that of them; FrameOfHead in the last repeat, FrameOfTail in the first (with a single
	// repeat the head wins, the k == T - 1 test comes first), else 0, + FileOffset.
	// T = NumTexturesForWholeChain, or joints - 1 when -1.
	// (approximate) the joints drawn now, where the original counts the ones the chain was made with
	graphics::frame_anim::ChainSheet sheet {
	    .frameWidth = frameWidth,
	    .frameHeight = frameHeight,
	    .frameOfHead = frameOfHead,
	    .frameOfTail = frameOfTail,
	    .fileOffset = fileOffset,
	};
	// only -1 is replaced; 0 (which the properties allow) is left to ChainSegmentUv's openblack guard, where the
	// original would divide by zero
	sheet.textures = numTexturesForWholeChain == -1 ? segments : numTexturesForWholeChain;
	// UR_Lightning rewrites the chain's repeats itself (NumTexturesToTile), always >= 1
	if (textures != -1)
	{
		sheet.textures = textures;
	}
	return graphics::frame_anim::ChainSegmentUv(index, segments, sheet, scroll);
}

std::vector<chain_atoms::Ribbon> chain_atoms::Collect()
{
	auto chains = manager::CollectChains();
	std::erase_if(chains, [](const Ribbon& chain) {
		return chain.creator == nullptr || dynamic_cast<const ChainCreator*>(chain.creator) == nullptr;
	});
	// OPENBLACK_PSYS_CHAIN_TRACE=1: how many ribbons the frame has and where the first one runs
	static const bool trace = std::getenv("OPENBLACK_PSYS_CHAIN_TRACE") != nullptr;
	if (trace && !chains.empty())
	{
		const auto& first = chains.front();
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"),
		                   "PSys chains: {} ribbons, the first with {} joints of {} from ({:.1f}, {:.1f}, {:.1f}) to "
		                   "({:.1f}, {:.1f}, {:.1f}), scale {:.2f} alpha {:.0f}",
		                   chains.size(), first.joints.size(), first.creator->texture, first.joints.front().position.x,
		                   first.joints.front().position.y, first.joints.front().position.z, first.joints.back().position.x,
		                   first.joints.back().position.y, first.joints.back().position.z, first.joints.front().scale,
		                   first.joints.front().alpha);
	}
	return chains;
}

void chain_atoms::AdvanceScroll(float milliseconds)
{
	for (const auto& chain : manager::CollectChains())
	{
		const auto* creator = dynamic_cast<const ChainCreator*>(chain.creator);
		// the scroll and the offset are always on in the original
		if (creator != nullptr && chain.collection != nullptr)
		{
			(void)graphics::frame_anim::ChainScroll(chain.collection->chainScroll, milliseconds,
			                                        chain.collection->chainScrollRate, creator->frameHeight);
		}
	}
}

void openblack::psys::RegisterChainCreator()
{
	RegisterCreator("ParticleChainCreator", MakeChainCreator);
}
