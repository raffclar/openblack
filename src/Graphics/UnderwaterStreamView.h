/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <memory>

#include <bgfx/bgfx.h>

#include "Graphics/Texture2D.h"
#include "Graphics/UnderwaterStreams.h"

namespace openblack::graphics
{

/// EXPERIMENT (not for merge): the GPU's part of the experiment's rivers and taps: the rivers' flow map, and the squares
/// of the tap splashes' particles and of the drifters
class UnderwaterStreamView
{
public:
	UnderwaterStreamView() = default;
	~UnderwaterStreamView();
	UnderwaterStreamView(const UnderwaterStreamView&) = delete;
	UnderwaterStreamView& operator=(const UnderwaterStreamView&) = delete;

	/// The rivers' flow map (see underwater::streams::PackFlowMap), made again when its revision changes. None while the
	/// land has no rivers.
	[[nodiscard]] const Texture2D* FlowMap(const underwater::streams::FlowMap& map, uint32_t revision);
	/// Binds the squares of one tap splash's particles, each corner with the number of its particle
	void BindTapMesh();
	/// Binds one square, its corners from -1 to 1, for the drifters
	void BindQuad();

private:
	std::unique_ptr<Texture2D> _flowMap;
	uint32_t _flowRevision {0};
	bgfx::VertexBufferHandle _tapVertices = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle _tapIndices = BGFX_INVALID_HANDLE;
	bgfx::VertexBufferHandle _quadVertices = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle _quadIndices = BGFX_INVALID_HANDLE;
};

} // namespace openblack::graphics
