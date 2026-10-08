/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DisappearSmoke.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

#include "3D/FrameAnim.h"
#include "Common/GameRandom.h"
#include "ECS/Systems/WorldEffectsInterface.h"
#include "Locator.h"

namespace openblack::ecs::disappear_smoke
{
namespace
{
/// What this module keeps between calls (Locator::worldEffects)
struct DisappearSmokeState
{
	std::vector<Cloud> clouds {};
};

DisappearSmokeState& DisappearSmokeData()
{
	return openblack::Locator::worldEffects::value().Get<DisappearSmokeState>();
}
} // namespace

void Create(const glm::vec3& position, int32_t mode, float size, uint32_t colour)
{
	Cloud cloud {
	    .life = 1.0f,
	    .mode = mode,
	    .size = size,
	    .colour = colour,
	};
	for (auto& puff : cloud.puffs)
	{
		// the order of the Random calls
		const float a = game_random::crt::Random(-size, size);
		const float b = game_random::crt::Random(-size, size);
		const float c = game_random::crt::Random(-size, size);
		puff.position = glm::vec3(c, b, a);
		puff.angle = game_random::crt::Random(0.0f, 6.2831855f);
		const float d = game_random::crt::Random(-size, size);
		const float e = game_random::crt::Random(-size, size);
		puff.velocity = glm::vec3(e, size, d);
		puff.cell = 0x10;
		if (mode == 0)
		{
			// Random(0.3, 1) x size along the direction (left as it is when it is 0)
			const float speed = game_random::crt::Random(0.3f, 1.0f) * size;
			const float length = glm::length(puff.velocity);
			if (length > 0.0f)
			{
				puff.velocity *= speed / length;
			}
		}
		else if (const float length = glm::length(puff.velocity); length > 0.0f)
		{
			// mode != 0 (ground marks, roots piles): 1.5 x size along the direction
			puff.velocity *= 1.5f * size / length;
		}
		puff.position += position;
	}
	DisappearSmokeData().clouds.push_back(cloud);
}

void Update(float seconds)
{
	auto& state = DisappearSmokeData();
	for (auto& cloud : state.clouds)
	{
		// the life, the colour and each puff
		cloud.life -= seconds * (cloud.mode != 0 ? 0.666667f : 0.333333f);
		if (cloud.life <= 0.0f)
		{
			continue;
		}
		uint32_t argb = 0;
		if (cloud.mode != 0)
		{
			const float a = cloud.life < 0.7f ? cloud.life * 1.42857f * 255.0f : 255.0f;
			argb = (static_cast<uint32_t>(static_cast<int32_t>(a)) << 24) | 0x68503Du;
		}
		else
		{
			argb = (static_cast<uint32_t>(static_cast<int32_t>(cloud.life * 100.0f)) << 24) | 0x808080u;
		}
		if (cloud.colour != 0xFFFFFFFFu)
		{
			argb = (argb & 0xFF000000u) | (cloud.colour & 0x00FFFFFFu);
		}
		for (auto& puff : cloud.puffs)
		{
			puff.argb = argb;
			const float sign = puff.velocity.x > puff.velocity.z ? -1.0f : 1.0f;
			puff.angle = sign * cloud.life * 5.0f + puff.velocity.x;
			puff.half = std::max(((1.0f - cloud.life) * 2.0f + 1.0f) * cloud.size * 0.5f, 0.0001f);
			puff.position += puff.velocity * seconds;
			puff.cell = graphics::frame_anim::DisappearSmokeCell(cloud.life);
		}
	}
	// freed when life < 0
	std::erase_if(state.clouds, [](const Cloud& cloud) { return cloud.life < 0.0f; });
}

const std::vector<Cloud>& Get()
{
	return DisappearSmokeData().clouds;
}

void Clear()
{
	DisappearSmokeData().clouds.clear();
}

} // namespace openblack::ecs::disappear_smoke
