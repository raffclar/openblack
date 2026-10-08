/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <memory>

#include <glm/fwd.hpp>

#include "3D/SkyInterface.h"
#include "3D/SkyType.h"
#include "Graphics/RenderPass.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack
{

namespace graphics
{
class L3DMesh;
class ShaderProgram;
class Texture2D;
} // namespace graphics

class Sky final: public SkyInterface
{
public:
	Sky() noexcept;
	~Sky() noexcept;

	/// Time between 0 and 24 in hours
	void SetTime(float time) noexcept override;
	/// Deprecated: 2 - sky_type::Frame()
	void UpdateDome() noexcept override;
	[[nodiscard]] graphics::L3DMesh& GetMesh() const noexcept override { return *_mesh; }
	[[nodiscard]] graphics::L3DMesh& GetSunMesh() const noexcept override { return *_sunMesh; }
	[[nodiscard]] graphics::L3DMesh& GetMoonMesh() const noexcept override { return *_moonMesh; }
	[[nodiscard]] graphics::L3DMesh& GetCloudMesh() const noexcept override { return *_cloudMesh; }
	[[nodiscard]] float GetTime() const noexcept override { return _timeOfDay; }
	[[nodiscard]] graphics::Texture2D& GetTexture() const noexcept override { return *_texture; }

private:
	static constexpr std::array<std::string_view, 3> k_Alignments = {
	    "evil",
	    "Ntrl",
	    "good",
	};
	static constexpr std::array<std::string_view, 3> k_Times = {
	    "night",
	    "dusk",
	    "day",
	};
	static constexpr uint16_t k_Size = 256;
	static constexpr size_t k_LayerTexels = static_cast<size_t>(k_Size) * k_Size;

	/// Blends rows [block.firstRow, block.firstRow + block.rowCount) of the three dome textures; the texture
	/// goes to the GPU once the last row is done
	void BlendDome(const sky_type::DomeBlock& block) noexcept;

	// from the mesh cache
	std::shared_ptr<graphics::L3DMesh> _mesh;
	std::shared_ptr<graphics::L3DMesh> _sunMesh;
	std::shared_ptr<graphics::L3DMesh> _moonMesh;
	std::shared_ptr<graphics::L3DMesh> _cloudMesh;
	/// The three dynamic dome textures as the layers of one array, in k_Alignments order
	std::unique_ptr<graphics::Texture2D> _texture; // TODO(bwrsandman): put in a resource manager and store look-up

	/// The nine sky_<alignment>_<time>.555 sources, here at layer 3 alignment + time in k_Alignments / k_Times order
	std::array<uint16_t, k_LayerTexels * k_Alignments.size() * k_Times.size()> _bitmaps;
	/// The dome textures' texels, CPU side
	std::array<uint16_t, k_LayerTexels * k_Alignments.size()> _dome;

	float _timeOfDay;
};

} // namespace openblack
