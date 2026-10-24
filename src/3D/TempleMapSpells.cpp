/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleMapSpells.h"

#include <cmath>

#include <utility>

#include <glm/gtc/constants.hpp>

#include "Magic/DispenserRules.h"
#include "Magic/MiracleVisuals.h"

namespace openblack
{

glm::vec3 TempleMapSpells::Middle(glm::vec3 mapPoint)
{
	return mapPoint + glm::vec3(0.0f, k_Lift, 0.0f);
}

void TempleMapSpells::Update(std::span<const Cast> casts, float gameSeconds, std::optional<float> effectSeconds,
                             Effects& effects)
{
	_seeds.clear();
	std::unordered_map<entt::entity, Shown> shown;
	shown.reserve(casts.size());
	for (const auto& cast : casts)
	{
		const auto middle = Middle(cast.mapPoint);
		const auto holderPoint = middle + glm::vec3(0.0f, cast.holderHeight * k_Scale, 0.0f);
		auto found = _shown.find(cast.spell);
		Shown place;
		if (found != _shown.end())
		{
			place = found->second;
			_shown.erase(found);
		}
		else
		{
			place.seed = {.seed = cast.seed,
			              .middle = middle,
			              .spin = 0.0f,
			              .phialPhase = 0.0f,
			              .phialFrame = 0.0f,
			              .ownerAlignment = cast.ownerAlignment};
			if (cast.holderParticle != ParticleType::None)
			{
				place.effect = effects.Start(cast.holderParticle, holderPoint, k_Scale, cast.player);
			}
		}
		// The seed follows its miracle, and turns two radians a second of the game's time
		auto& seed = place.seed;
		seed.middle = middle;
		seed.ownerAlignment = cast.ownerAlignment;
		seed.spin = std::fmod(seed.spin + (magic::k_OrbSeedSpin * gameSeconds), glm::two_pi<float>());
		if (seed.spin < 0.0f)
		{
			seed.spin += glm::two_pi<float>();
		}
		if (cast.phialReceiveType.has_value())
		{
			seed.phialFrame = magic::visuals::StepPhialFrame(seed.phialFrame, gameSeconds);
			seed.phialPhase =
			    std::fmod(seed.phialPhase + (magic::visuals::PhialPulseSpeed(*cast.phialReceiveType) * gameSeconds), 1.0f);
		}
		if (place.effect.has_value() && effectSeconds.has_value())
		{
			effects.Step(*place.effect, holderPoint, *effectSeconds);
		}
		_seeds.push_back(seed);
		shown.emplace(cast.spell, place);
	}
	// The seeds of the miracles that stopped go with their effects
	for (const auto& [spell, place] : _shown)
	{
		if (place.effect.has_value())
		{
			effects.Stop(*place.effect);
		}
	}
	_shown = std::move(shown);
}

void TempleMapSpells::Clear(Effects& effects)
{
	for (const auto& [spell, place] : _shown)
	{
		if (place.effect.has_value())
		{
			effects.Stop(*place.effect);
		}
	}
	_shown.clear();
	_seeds.clear();
}

} // namespace openblack
