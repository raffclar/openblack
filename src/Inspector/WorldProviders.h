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

#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <InspectorProvider.h>
#include <glm/vec3.hpp>

#include "3D/WaterRings.h"
#include "ECS/Systems/ParticleSystemInterface.h"

/// Providers of the world's passing things: the sky's moon, and the splashes and particle effects
namespace openblack::inspector
{

/// The moon as it is at an hour of script time, by the real date, for a camera: exactly its position in the world
/// (null while it is down), its phase (0 to 2 pi), the whole days since the new moon, and whether it is up (an
/// overcast may still hide it)
[[nodiscard]] Json MoonState(float scriptHour, int64_t unixTime, glm::vec3 cameraOrigin);

struct SkySources
{
	/// The hour of the script clock, none without a sky
	std::function<std::optional<float>()> scriptHour;
	/// Where the player's camera is, none without one
	std::function<std::optional<glm::vec3>()> cameraOrigin;
	/// Seconds since 1970, by which the real moon's phase is shown
	std::function<int64_t()> unixTime;
};

///   sky.moon   the moon and its state, and nothing else
class SkyProvider final: public ProviderInterface
{
public:
	explicit SkyProvider(SkySources sources);

	[[nodiscard]] std::string_view Name() const override { return "sky"; }
	[[nodiscard]] std::vector<QueryDescription> Describe() const override;
	[[nodiscard]] QueryResult Run(std::string_view query, const QueryContext& context) override;

private:
	SkySources _sources;
};

using ParticleEffectInfo = ecs::systems::ParticleSystemInterface::EffectInfo;

/// The splashes there are as list items: the rings on the water, and the particle effects whose files are splashes
[[nodiscard]] Json SplashItems(std::span<const water_rings::Ring> rings, std::span<const ParticleEffectInfo> effects);
/// The running particle effects as list items, of those whose file names hold the text when it is given
[[nodiscard]] Json EffectItems(std::span<const ParticleEffectInfo> effects, std::optional<std::string_view> file);

struct ParticleSources
{
	std::function<std::span<const water_rings::Ring>()> rings;
	std::function<std::vector<ParticleEffectInfo>()> effects;
};

///   particles.splash   near, radius          the splashes within the radius, nearest first, with their state
///   particles.effects  {file?} [near, radius] the running particle effects
class ParticlesProvider final: public ProviderInterface
{
public:
	explicit ParticlesProvider(ParticleSources sources);

	[[nodiscard]] std::string_view Name() const override { return "particles"; }
	[[nodiscard]] std::vector<QueryDescription> Describe() const override;
	[[nodiscard]] QueryResult Run(std::string_view query, const QueryContext& context) override;

private:
	ParticleSources _sources;
};

} // namespace openblack::inspector
