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

#include <array>
#include <memory>

#include <bgfx/bgfx.h>
#include <glm/vec4.hpp>

#include "FrameBuffer.h"
#include "Graphics/UnderwaterWake.h"

namespace openblack::graphics
{

/// EXPERIMENT (not for merge): the GPU's part of the field of small waves on the water (see underwater::wake): two
/// targets that the steps write in turn, each step reading the other, the kernel as uniforms, and the splashes' mesh
class UnderwaterWakeField
{
public:
	/// The particles of one splash (see vs_underwater_splash.sc)
	static constexpr uint32_t k_SplashParticles = 160;

	UnderwaterWakeField();
	~UnderwaterWakeField();
	UnderwaterWakeField(const UnderwaterWakeField&) = delete;
	UnderwaterWakeField& operator=(const UnderwaterWakeField&) = delete;

	/// The target that the last step wrote
	[[nodiscard]] const FrameBuffer& Latest() const { return *_targets.at(_latest); }
	/// The target that the next step writes; the step reads Latest
	[[nodiscard]] const FrameBuffer& Next() const { return *_targets.at(1 - _latest); }
	/// After a step: the target it wrote is the latest
	void Swap() { _latest = 1 - _latest; }
	/// The kernel's weights as u_wakeKernel takes them
	[[nodiscard]] const std::array<glm::vec4, 14>& Kernel() const { return _kernel; }
	/// The pull back towards flat of one step
	[[nodiscard]] float Gravity() const { return _gravity; }
	/// Binds the squares of one splash's particles, each corner with the number of its particle
	void BindSplashMesh();

private:
	std::array<std::unique_ptr<FrameBuffer>, 2> _targets;
	size_t _latest {0};
	std::array<glm::vec4, 14> _kernel {};
	float _gravity {0.0f};
	bgfx::VertexBufferHandle _splashVertices = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle _splashIndices = BGFX_INVALID_HANDLE;
};

} // namespace openblack::graphics
