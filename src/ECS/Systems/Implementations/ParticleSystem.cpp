/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ParticleSystem.h"

#include <algorithm>
#include <chrono>

#include <ParticleFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/InfluenceCircle.h"
#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "Common/StringUtils.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/ZSort.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Particles/ParticleTypes.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// Where the particle files are, under the game's data
std::filesystem::path ParticleFileDirectory()
{
	return Locator::filesystem::value().GetPath<filesystem::Path::Data>() / "Spells" / "ZSpellFiles";
}

/// The compressed files' names end so
constexpr std::string_view k_CompressedSuffix = "_txt.zzz";

/// A game turn in seconds, what every effect not owned by a miracle steps by
constexpr float k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();

entt::id_type ParticleFileId(std::string_view name)
{
	return entt::hashed_string(fmt::format("particles/{}", name).c_str());
}
} // namespace

float GameParticleWorld::LandHeight(glm::vec2 xz) const
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(xz) : 0.0f;
}

uint32_t GameParticleWorld::PlayerColour(int player) const
{
	return influence::k_PlayerColours.at(static_cast<size_t>(player) & (influence::k_PlayerColours.size() - 1));
}

glm::vec3 GameParticleWorld::CameraRight() const
{
	return Locator::camera::has_value() ? Locator::camera::value().GetRight() : glm::vec3(1.0f, 0.0f, 0.0f);
}

glm::vec3 GameParticleWorld::CameraUp() const
{
	return Locator::camera::has_value() ? Locator::camera::value().GetUp() : glm::vec3(0.0f, 1.0f, 0.0f);
}

ParticleSystem::ParticleSystem()
    : _classes(particles::ParticleClassRegistry::WithAllClasses())
{
}

ParticleSystem::~ParticleSystem() = default;

std::deque<ParticleSystem::Running>::iterator ParticleSystem::FindRunning(EffectId id)
{
	return std::ranges::find(_effects, id, &Running::id);
}

std::deque<ParticleSystem::Running>::const_iterator ParticleSystem::FindRunning(EffectId id) const
{
	return std::ranges::find(_effects, id, &Running::id);
}

ParticleSystemInterface::EffectId ParticleSystem::Start(std::string_view file, glm::vec3 origin, float magnitude, bool synced)
{
	if (file.empty() || !Locator::resources::has_value() || !Locator::gameRandom::has_value())
	{
		return k_NoEffect;
	}
	auto& files = Locator::resources::value().GetParticleFiles();
	const auto id = ParticleFileId(file);
	if (!files.Contains(id))
	{
		try
		{
			files.Load(id, resources::ParticleFileLoader::FromDiskTag {}, ParticleFileDirectory(), std::string(file));
		}
		catch (const std::exception& error)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "Particles: cannot load {}: {}", file, error.what());
			return k_NoEffect;
		}
	}
	const std::shared_ptr<const psys::ParticleFile> data = files.Handle(id).handle();
	if (!data)
	{
		return k_NoEffect;
	}
	auto effect = std::make_unique<particles::Effect>(
	    data, particles::EffectServices {_classes, _world, Locator::gameRandom::value(), _noise}, origin, magnitude, synced);
	for (const auto& className : effect->UnportedClasses())
	{
		if (_reportedUnported.insert(className).second)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Particles: {} (first in {}) is not run yet", className, file);
		}
	}
	ResolveTextures(*effect);
	const auto effectId = _nextId++;
	_effects.push_front({.id = effectId, .file = std::string(file), .effect = std::move(effect)});
	return effectId;
}

ParticleSystemInterface::EffectId ParticleSystem::Start(ParticleType type, glm::vec3 origin, float magnitude, bool synced)
{
	return Start(particles::ParticleTypeFile(type), origin, magnitude, synced);
}

ParticleSystemInterface::EffectId ParticleSystem::StartForSpell(ParticleType type, glm::vec3 origin, glm::vec3 direction,
                                                                float magnitude, particles::SpellSink& sink)
{
	// A miracle's own effect draws on the random numbers every machine shares
	const auto id = Start(type, origin, magnitude, true);
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->ownedBySpell = true;
		it->effect->SetDirection(direction);
		it->effect->SetSink(&sink);
	}
	return id;
}

bool ParticleSystem::ProcessForSpell(EffectId id, const particles::ProcessInfo& info, float seconds)
{
	const auto it = FindRunning(id);
	if (it == _effects.end())
	{
		return false;
	}
	it->effect->SetProcessInfo(info);
	if (StepEffect(*it->effect, seconds))
	{
		_effects.erase(it);
		return false;
	}
	return true;
}

ParticleSystemInterface::EffectId ParticleSystem::StartSpotVisual(SpotVisualType type, glm::vec3 position,
                                                                  std::optional<int> turns, entt::entity owner, float magnitude)
{
	if (!Locator::infoConstants::has_value())
	{
		return k_NoEffect;
	}
	const auto& spotVisuals = Locator::infoConstants::value().spotVisual;
	const auto index = static_cast<size_t>(type);
	if (index >= spotVisuals.size())
	{
		return k_NoEffect;
	}
	const auto& info = spotVisuals.at(index);
	// Spot visuals are the same on every machine
	const auto id = Start(info.particleType, position, magnitude, true);
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->turnsLeft = turns.value_or(static_cast<int>(info.life));
		it->owner = owner;
	}
	return id;
}

void ParticleSystem::SetOrigin(EffectId id, glm::vec3 origin)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->effect->SetOrigin(origin);
	}
}

void ParticleSystem::SetPlayer(EffectId id, int player)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->effect->SetPlayer(player);
	}
}

void ParticleSystem::CloseDown(EffectId id)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->effect->CloseDown();
	}
}

void ParticleSystem::Delete(EffectId id)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		_effects.erase(it);
	}
}

bool ParticleSystem::IsRunning(EffectId id) const
{
	return FindRunning(id) != _effects.end();
}

particles::Effect* ParticleSystem::Find(EffectId id)
{
	const auto it = FindRunning(id);
	return it != _effects.end() ? it->effect.get() : nullptr;
}

bool ParticleSystem::StepEffect(particles::Effect& effect, float seconds)
{
	if (_paused)
	{
		return false;
	}
	effect.Step(seconds);
	// Closing down ends at once an effect whose file says so
	return effect.Finished() || (effect.Closing() && effect.DeleteOnCloseDown());
}

void ParticleSystem::ProcessTurn()
{
	if (_paused)
	{
		return;
	}
	const auto* registry = Locator::entitiesRegistry::has_value() ? &Locator::entitiesRegistry::value() : nullptr;
	for (auto it = _effects.begin(); it != _effects.end();)
	{
		if (it->ownedBySpell)
		{
			++it;
			continue;
		}
		auto& effect = *it->effect;
		if (it->owner != entt::null)
		{
			// It follows its owner, and ends when the owner goes
			const auto* transform = registry != nullptr && registry->Valid(it->owner)
			                            ? registry->TryGet<ecs::components::Transform>(it->owner)
			                            : nullptr;
			if (transform != nullptr)
			{
				effect.SetOrigin(transform->position);
			}
			else
			{
				effect.CloseDown();
			}
		}
		if (it->turnsLeft.has_value() && *it->turnsLeft >= 0 && --*it->turnsLeft <= 0)
		{
			effect.CloseDown();
		}
		if (StepEffect(effect, k_TurnSeconds))
		{
			it = _effects.erase(it);
			continue;
		}
		++it;
	}
}

void ParticleSystem::Reset()
{
	_effects.clear();
}

void ParticleSystem::ResolveTextures(const particles::Effect& effect)
{
	if (_textureStems.empty() && Locator::filesystem::has_value())
	{
		auto& fileSystem = Locator::filesystem::value();
		fileSystem.Iterate(fileSystem.GetPath<filesystem::Path::Textures>(), false, [this](const std::filesystem::path& file) {
			if (string_utils::LowerCase(file.extension().string()) == ".raw")
			{
				const auto stem = file.stem().string();
				_textureStems.insert_or_assign(string_utils::LowerCase(stem), stem);
			}
		});
	}
	for (const auto& object : effect.GetFile().objects)
	{
		const auto* creator = effect.FindCreator(object.name);
		if (creator == nullptr || creator->kind != particles::Creator::Kind::Sprite || _textures.contains(creator->texture))
		{
			continue;
		}
		// The files don't always spell a sheet's name as it is on disk
		const auto found = _textureStems.find(string_utils::LowerCase(creator->texture));
		const auto stem = found != _textureStems.end() ? found->second : creator->texture;
		_textures.insert_or_assign(creator->texture,
		                           std::pair {entt::hashed_string(fmt::format("raw/{}", stem).c_str()).value(),
		                                      entt::hashed_string(fmt::format("raw/{}a", stem).c_str()).value()});
	}
}

ParticleSystemInterface::SpriteFrame ParticleSystem::CollectSprites(float turnFraction, const glm::vec3& camera) const
{
	SpriteFrame frame;
	std::vector<particles::Effect::DrawAtom> atoms;
	struct Sorted
	{
		float key;
		particles::sprites::SpriteInstance instance;
		const particles::Creator* creator;
	};
	std::vector<Sorted> sorted;
	for (const auto& running : _effects)
	{
		atoms.clear();
		sorted.clear();
		// Each effect has batches of its own, sorted among the other things that blend by their farthest sprite
		const auto effectFirstBatch = frame.batches.size();
		running.effect->Collect(turnFraction, atoms, particles::Creator::Kind::Sprite);
		for (const auto& atom : atoms)
		{
			const auto instance = particles::sprites::InstanceOf(atom);
			sorted.push_back({graphics::zsort::Key(glm::vec3(instance.positionHalfWidth), camera), instance, atom.creator});
		}
		// The farthest first, so they blend over one another in order
		std::ranges::stable_sort(sorted, std::greater {}, &Sorted::key);
		for (const auto& sprite : sorted)
		{
			const auto texture = _textures.find(sprite.creator->texture);
			if (texture == _textures.end())
			{
				continue;
			}
			const auto mode = particles::sprites::RenderMode(*sprite.creator);
			if (frame.batches.size() == effectFirstBatch || frame.batches.back().texture != texture->second.first ||
			    frame.batches.back().mode != mode)
			{
				frame.batches.push_back({
				    .texture = texture->second.first,
				    .alphaTexture = texture->second.second,
				    .mode = mode,
				    .sortPoint = glm::vec3(sprite.instance.positionHalfWidth),
				    .first = static_cast<uint32_t>(frame.instances.size()),
				    .count = 0,
				});
			}
			frame.instances.push_back(sprite.instance);
			++frame.batches.back().count;
		}
	}
	return frame;
}

std::vector<ParticleSystemInterface::EffectInfo> ParticleSystem::GetEffects() const
{
	std::vector<EffectInfo> result;
	result.reserve(_effects.size());
	for (const auto& running : _effects)
	{
		const auto& effect = *running.effect;
		std::optional<float> secondsLeft;
		if (running.turnsLeft.has_value() && *running.turnsLeft >= 0)
		{
			secondsLeft = static_cast<float>(*running.turnsLeft) * k_TurnSeconds;
		}
		result.push_back({
		    .id = running.id,
		    .file = running.file,
		    .origin = effect.GetOrigin(),
		    .age = effect.GetAge(),
		    .atoms = effect.AtomCount(),
		    .collections = effect.CollectionCount(),
		    .closing = effect.Closing(),
		    .ownedBySpell = running.ownedBySpell,
		    .secondsLeft = secondsLeft,
		    .unportedClasses = effect.UnportedClasses(),
		});
	}
	return result;
}

std::vector<std::string> ParticleSystem::GetFileNames() const
{
	std::vector<std::string> names;
	if (!Locator::filesystem::has_value())
	{
		return names;
	}
	Locator::filesystem::value().Iterate(ParticleFileDirectory(), false, [&names](const std::filesystem::path& file) {
		auto name = file.filename().string();
		if (name.ends_with(k_CompressedSuffix))
		{
			names.push_back(name.substr(0, name.size() - k_CompressedSuffix.size()));
		}
		else if (string_utils::LowerCase(file.extension().string()) == ".txt")
		{
			names.push_back(file.stem().string());
		}
	});
	std::ranges::sort(names);
	const auto [first, last] = std::ranges::unique(names);
	names.erase(first, last);
	return names;
}
