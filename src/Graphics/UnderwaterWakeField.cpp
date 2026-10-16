/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the GPU's part of the field of small waves on the water

#include "UnderwaterWakeField.h"

#include <vector>

#include <glm/vec3.hpp>

using namespace openblack::graphics;
namespace wake = openblack::graphics::underwater::wake;

UnderwaterWakeField::UnderwaterWakeField()
{
	for (size_t i = 0; i < _targets.size(); ++i)
	{
		_targets.at(i) =
		    std::make_unique<FrameBuffer>(i == 0 ? "Underwater Wake 0" : "Underwater Wake 1", wake::k_FieldSide,
		                                  wake::k_FieldSide, TextureFormat::RGBA16F, std::nullopt, 1, Wrapping::ClampEdge);
	}
	// The kernel's rows: the weights for the distances along x 0 to 3, then 4 to 6
	const auto kernel = wake::WaveKernel();
	constexpr int k_Row = wake::k_KernelReach + 1;
	for (int z = 0; z < k_Row; ++z)
	{
		const auto at = [&kernel, z](int x) { return kernel.at(static_cast<size_t>(z * k_Row + x)); };
		_kernel.at(static_cast<size_t>(z) * 2) = {at(0), at(1), at(2), at(3)};
		_kernel.at(static_cast<size_t>(z) * 2 + 1) = {at(4), at(5), at(6), 0.0f};
	}
	_gravity = wake::GravityTerm(kernel, 1.0f / wake::k_StepsPerSecond, wake::k_FieldSpacing);
}

UnderwaterWakeField::~UnderwaterWakeField()
{
	if (bgfx::isValid(_splashVertices))
	{
		bgfx::destroy(_splashVertices);
	}
	if (bgfx::isValid(_splashIndices))
	{
		bgfx::destroy(_splashIndices);
	}
}

void UnderwaterWakeField::BindSplashMesh()
{
	if (!bgfx::isValid(_splashVertices))
	{
		std::vector<glm::vec3> vertices;
		std::vector<uint16_t> indices;
		constexpr std::array<glm::vec2, 4> k_Corners {{{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}}};
		for (uint32_t particle = 0; particle < k_SplashParticles; ++particle)
		{
			const auto first = static_cast<uint16_t>(vertices.size());
			for (const auto& corner : k_Corners)
			{
				vertices.emplace_back(corner, static_cast<float>(particle));
			}
			indices.insert(indices.end(), {first, static_cast<uint16_t>(first + 1), static_cast<uint16_t>(first + 2), first,
			                               static_cast<uint16_t>(first + 2), static_cast<uint16_t>(first + 3)});
		}
		bgfx::VertexLayout layout;
		layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
		_splashVertices = bgfx::createVertexBuffer(
		    bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(glm::vec3))), layout);
		_splashIndices =
		    bgfx::createIndexBuffer(bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint16_t))));
	}
	bgfx::setVertexBuffer(0, _splashVertices);
	bgfx::setIndexBuffer(_splashIndices);
}
