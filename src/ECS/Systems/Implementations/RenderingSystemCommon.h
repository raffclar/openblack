/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <map>
#include <vector>

#include <glm/mat4x4.hpp>

#include "3D/AllMeshes.h"
#include "ECS/Systems/RenderingSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{

class RenderingSystemCommon: public RenderingSystemInterface
{
public:
	~RenderingSystemCommon();
	void SetDirty() override;
	void SetLayoutDirty() override;
	void PrepareDraw(bool drawBoundingBox, bool drawFootpaths, bool drawStreams) override;
	[[nodiscard]] const RenderContext& GetContext() override { return _renderContext; }

private:
	virtual void PrepareDrawDescs(bool drawBoundingBox) = 0;
	virtual void PrepareDrawUploadUniforms(bool drawBoundingBox) = 0;
	/// Writes the instances again into the draw lists the last PrepareDrawDescs made, and uploads them; false (the
	/// draw lists are then made again) when they no longer fit what is drawn. Without a refill of its own: false
	[[nodiscard]] virtual bool RefillKeepingDescs() { return false; }
	/// Called once at the start of each PrepareDraw that writes the instances, before any of the above: what every row
	/// reads and the frame does not change while they are written
	virtual void BeginPrepareDraw() {}

protected:
	/// (Re)creates instanceUniformBuffer for `capacity` instances of five columns (i_data0..i_data4: the model matrix,
	/// then instanceColours) and sizes both CPU lists to it
	void ResizeInstances(uint32_t capacity);
	/// Interleaves instanceUniforms and instanceColours into the buffer (copied: bgfx reads it a frame later), up to
	/// the end of the rows the draws read
	void UploadInstances();

	RenderContext _renderContext;

private:
	/// The build under way writes the debug boxes' rows (the lists' second half)
	bool _boundingBoxRows {false};
	/// The rows the last UploadInstances gave bgfx
	uint32_t _uploadedRows {0};
};
} // namespace openblack::ecs::systems
