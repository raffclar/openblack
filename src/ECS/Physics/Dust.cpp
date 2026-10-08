/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Dust.h"

#include <algorithm>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <spdlog/spdlog.h>

#include "3D/FrameAnim.h"
#include "Common/GameRandom.h"
#include "ECS/Physics/PhysicsObjectsState.h"
#include "ECS/Systems/PhysicsObjectsSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::physics;

namespace
{
constexpr size_t k_MaxPuffs = 1024;
constexpr float k_Life = 1.0f;

using Puff = openblack::ecs::physics::DustPuff;

/// The physics objects' state (Locator::physicsObjectsSystem)
openblack::ecs::physics::State& PhysicsState()
{
	return openblack::Locator::physicsObjectsSystem::value().GetState();
}

/// The shape of the puffs: blobsa.raw, the alpha of data\blobs.raw (white RGB).
std::optional<graphics::TextureHandle> Texture()
{
	auto& textures = Locator::resources::value().GetTextures();
	const auto id = entt::hashed_string("raw/blobsa").value();
	if (!textures.Contains(id))
	{
		try
		{
			auto& fileSystem = Locator::filesystem::value();
			textures.Load(id, resources::Texture2DLoader::FromDiskTag {},
			              fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / "blobsa.raw"));
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "Dust: cannot load Data/blobsa.raw: {}", e.what());
			return std::nullopt;
		}
	}
	return textures.Handle(id)->GetNativeHandle();
}

glm::vec2 CellUv(uint32_t cell)
{
	return graphics::frame_anim::SpriteCellUv(static_cast<int>(cell), 8)[0];
}
} // namespace

namespace
{
/// (r - 100) x 0.02, the int converted to float first
float Axis(uint32_t r)
{
	const auto centred = static_cast<int32_t>(r) - 100;
	return static_cast<float>(centred) * 0.02f;
}
} // namespace

glm::vec3 Dust::RandomVelocity()
{
	// z, y, x in that order
	const float z = Axis(game_random::LocalRand(201));
	const float y = Axis(game_random::LocalRand(201));
	const float x = Axis(game_random::LocalRand(201));
	return {x, y, z};
}

glm::vec3 Dust::SyncedRandomVelocity()
{
	// z, y, x in that order
	const float z = Axis(game_random::GameRand(201));
	const float y = Axis(game_random::GameRand(201));
	const float x = Axis(game_random::GameRand(201));
	return {x, y, z};
}

void Dust::Emit(glm::vec3 at, glm::vec3 velocity, uint32_t argb, float size)
{
	if (PhysicsState().dustPuffs.size() >= k_MaxPuffs)
	{
		return;
	}
	// a particle kind other than 0 (all of openblack's are kind 4) takes CRT rand() % 16 (signed; rand is never
	// negative), after the 1024 particle limit test
	const auto seed = static_cast<uint32_t>(game_random::crt::Rand() % 16);
	const auto texture = Texture();
	if (!texture)
	{
		return;
	}
	const glm::vec4 colour = argb_colour::ToVec4(argb);
	const float a = colour.a;
	const glm::vec3 rgb(colour);
	// normal blending with the tint premultiplied by its alpha; size 0 until the first Update
	const DustParticleDraw draw {*texture, at, 0.0f, CellUv(16 + seed), glm::vec2(1.0f / 8.0f), glm::vec4(rgb * a, a)};
	PhysicsState().dustPuffs.push_back({velocity, size, 0.0f, seed, draw});
}

void Dust::Update(float seconds)
{
	if (PhysicsState().dustPuffs.empty() || seconds <= 0.0f)
	{
		return;
	}
	for (auto& puff : PhysicsState().dustPuffs)
	{
		puff.age += seconds;
		// age += dt, gone once past the kind's life (kind 4: 1 s; 0: 3 s; others 2 s), before moving
		if (puff.age > k_Life)
		{
			continue;
		}
		puff.draw.position += puff.velocity * seconds;
		// half size = size x (1 - age) x min(1, age / 0.125)
		puff.draw.halfSize = puff.size * (1.0f - puff.age) * std::min(1.0f, puff.age / 0.125f);
		// cell 16 + ((rand % 16 + (int)(2 age)) & 15) (frame_anim::DustCell)
		puff.draw.uvMin = CellUv(graphics::frame_anim::DustCell(puff.seed, puff.age));
	}
	std::erase_if(PhysicsState().dustPuffs, [](const Puff& p) { return p.age > k_Life; });
}

void Dust::Clear()
{
	PhysicsState().dustPuffs.clear();
}

void Dust::Snapshot(std::vector<DustParticleDraw>& out)
{
	out.clear();
	out.reserve(PhysicsState().dustPuffs.size());
	for (const auto& puff : PhysicsState().dustPuffs)
	{
		out.push_back(puff.draw);
	}
}
