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

#include <EnumHeader.h>
#include <ParticleFile.h>
#include <StackedBitmap.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/InfluenceCircle.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "Common/StringUtils.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
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

/// A path as the effect files give it, under the game's data folder, folders split by forward slashes
std::filesystem::path DataRelative(std::string_view path)
{
	std::string name(path);
	std::ranges::replace(name, '\\', '/');
	if (name.starts_with("./"))
	{
		name.erase(0, 2);
	}
	if (string_utils::LowerCase(name).starts_with("data/"))
	{
		name.erase(0, 5);
	}
	return name;
}

/// The sheets the players' symbols are drawn with, whatever effect first shows one
constexpr std::array<std::string_view, 2> k_SymbolTextures {"S_SpriteSheet3", "ChooseSymbol"};
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

std::optional<particles::ParticleWorldInterface::TargetInfo> GameParticleWorld::Target(entt::entity target, bool centre) const
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(target))
	{
		return std::nullopt;
	}
	const auto* transform = registry.TryGet<ecs::components::Transform>(target);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	// Its size from its model's box, scaled as it stands
	TargetInfo info {.position = transform->position, .radius = 1.0f, .height = 1.0f};
	if (const auto* mesh = registry.TryGet<ecs::components::Mesh>(target);
	    mesh != nullptr && Locator::resources::has_value() && Locator::resources::value().GetMeshes().Contains(mesh->id))
	{
		const auto box = Locator::resources::value().GetMeshes().Handle(mesh->id)->GetBoundingBox();
		const auto size = box.Size() * transform->scale;
		info.radius = std::max(size.x, size.z) * 0.5f;
		info.height = size.y;
	}
	if (centre)
	{
		info.position.y += info.height * 0.5f;
	}
	return info;
}

bool GameParticleWorld::IsTargetHeld(entt::entity target) const
{
	return Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().GetCreature() == target;
}

void GameParticleWorld::ClaimTarget(entt::entity target, bool claimed)
{
	if (claimed)
	{
		_claimed.insert(target);
	}
	else
	{
		_claimed.erase(target);
	}
}

std::optional<entt::id_type> GameCreatorResources::MeshByName(std::string_view name)
{
	if (!_meshNames.has_value())
	{
		_meshNames.emplace();
		if (Locator::filesystem::has_value())
		{
			auto& fileSystem = Locator::filesystem::value();
			const auto path = fileSystem.GetPath<filesystem::Path::Data>() / "AllMeshes.h";
			if (fileSystem.Exists(path))
			{
				const auto bytes = fileSystem.ReadAll(path);
				*_meshNames =
				    psys::ParseEnumHeader(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
			}
		}
	}
	const auto found = _meshNames->find(name);
	if (found == _meshNames->end() || found->second <= 0 || found->second >= static_cast<int32_t>(MeshId::_COUNT))
	{
		return std::nullopt;
	}
	return resources::HashIdentifier(static_cast<MeshId>(found->second));
}

std::optional<entt::id_type> GameCreatorResources::MeshByFile(std::string_view path)
{
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return std::nullopt;
	}
	const auto relative = DataRelative(path);
	const auto id = entt::hashed_string(("particles/" + string_utils::LowerCase(relative.generic_string())).c_str()).value();
	auto& meshes = Locator::resources::value().GetMeshes();
	if (meshes.Contains(id))
	{
		return id;
	}
	auto& fileSystem = Locator::filesystem::value();
	try
	{
		meshes.Load(id, resources::L3DLoader::FromDiskTag {},
		            fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / relative));
	}
	catch (const std::exception& error)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Particles: cannot load the model {}: {}", path, error.what());
		return std::nullopt;
	}
	return id;
}

std::optional<entt::id_type> GameCreatorResources::LightMap(std::string_view path, int pitch, int channels, int framesInFile,
                                                            int framesInUse)
{
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return std::nullopt;
	}
	const auto relative = DataRelative(path);
	const auto key = fmt::format("particles/{}/{}/{}/{}/{}", string_utils::LowerCase(relative.generic_string()), pitch,
	                             channels, framesInFile, framesInUse);
	const auto id = entt::hashed_string(key.c_str()).value();
	auto& bitmaps = Locator::resources::value().GetParticleBitmaps();
	if (bitmaps.Contains(id))
	{
		return id;
	}
	auto& fileSystem = Locator::filesystem::value();
	try
	{
		bitmaps.Load(id, resources::ParticleBitmapLoader::FromDiskTag {},
		             fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / relative),
		             resources::ParticleBitmapLoader::Layout {
		                 .pitch = pitch, .channels = channels, .framesInFile = framesInFile, .framesInUse = framesInUse});
	}
	catch (const std::exception& error)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Particles: cannot load the light map {}: {}", path, error.what());
		return std::nullopt;
	}
	return id;
}

ParticleSystem::ParticleSystem()
    : _classes(particles::ParticleClassRegistry::WithAllClasses(&_resources))
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
		// Some are drawn all together at their origin rather than each sprite in its own place
		it->path = info.singleZSort == 1 ? particles::draw::DrawPath::Queued : particles::draw::DrawPath::Sorted;
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

void ParticleSystem::SetDrawPath(EffectId id, particles::draw::DrawPath path)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->path = path;
	}
}

void ParticleSystem::AddTarget(EffectId id, entt::entity target)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->effect->AddTarget(target);
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
	_world.Reset();
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
	const auto resolve = [this](std::string_view texture) {
		if (texture.empty() || _textures.contains(texture))
		{
			return;
		}
		// The files don't always spell a sheet's name as it is on disk
		const auto found = _textureStems.find(string_utils::LowerCase(std::string(texture)));
		const auto stem = found != _textureStems.end() ? found->second : std::string(texture);
		_textures.insert_or_assign(std::string(texture),
		                           std::pair {entt::hashed_string(fmt::format("raw/{}", stem).c_str()).value(),
		                                      entt::hashed_string(fmt::format("raw/{}a", stem).c_str()).value()});
	};
	for (const auto& object : effect.GetFile().objects)
	{
		const auto* creator = effect.FindCreator(object.name);
		if (creator == nullptr)
		{
			continue;
		}
		if (creator->kind == particles::Creator::Kind::Sprite || creator->kind == particles::Creator::Kind::Chain)
		{
			resolve(creator->texture);
		}
		else if (creator->kind == particles::Creator::Kind::Symbol)
		{
			std::ranges::for_each(k_SymbolTextures, resolve);
		}
	}
}

void ParticleSystem::CollectDrawFrame(float turnFraction, particles::draw::Frame& frame) const
{
	frame.Clear();
	const particles::draw::Sources sources {
	    .textures = [this](std::string_view texture) -> std::optional<std::pair<entt::id_type, entt::id_type>> {
		    const auto found = _textures.find(texture);
		    if (found == _textures.end())
		    {
			    return std::nullopt;
		    }
		    return found->second;
	    },
	    .playerColour = [this](int player) { return _world.PlayerColour(player); },
	    .random =
	        [](float range) {
		        return Locator::gameRandom::has_value() ? Locator::gameRandom::value().LocalFloatRand(range) : 0.0f;
	        },
	};
	for (const auto& running : _effects)
	{
		_walk.Clear();
		running.effect->Walk(turnFraction, _walk);
		particles::draw::AddEffect(frame, _walk, running.path, running.effect->GetOrigin(), running.effect->GetPlayer(),
		                           sources);
	}
	_drawStats = {
	    .sprites = frame.sprites.size(),
	    .chains = frame.chains.size(),
	    .meshes = frame.meshes.size(),
	    .mists = frame.mists.size(),
	    .lightStamps = frame.lightStamps.size(),
	    .effects = frame.groups.size(),
	};
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
		    .path = running.path,
		    .targets = effect.TargetCount(),
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
