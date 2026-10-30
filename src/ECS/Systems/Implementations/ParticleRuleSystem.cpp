/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ParticleRuleSystem.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <initializer_list>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <EnumHeader.h>
#include <LNDFile.h>
#include <PackFile.h>
#include <ParticleFile.h>
#include <StackedBitmap.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtx/transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/CameraPath.h"
#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "Common/StringUtils.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AudioEmitter.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptSpotVisual.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/DrawnPlace.h"
#include "ECS/PlayerColour.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/ScriptSpotVisuals.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Particles/GestureTrail.h"
#include "Particles/LightSheet.h"
#include "Particles/LightningMaths.h"
#include "Particles/ParticleArchive.h"
#include "Particles/ParticleArchiveValues.h"
#include "Particles/ParticleClassRegistry.h"
#include "Particles/ParticleMiracleMaths.h"
#include "Particles/ParticleSoundRelease.h"
#include "Particles/ParticleTurnOrder.h"
#include "Particles/ParticleTypes.h"
#include "Particles/VolumeGlow.h"
#include "PlayerSlots.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/GameSoundEffects.h"

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

/// The spells' sound bank, whose effects the particles' sounds are played from
constexpr std::string_view k_SpellSoundBank = "spells.sad";
/// The key a particle's sound is looked up by besides its size, alignment, ground and action: the default object
constexpr int32_t k_SoundObject = 1;
/// The ground's kind where there is no land, and where the land is under water
constexpr int k_DeepWaterSurface = 6;
constexpr int k_ShallowWaterSurface = 7;
/// A ground of no sound of its own sounds hard
constexpr int k_HardSurface = 3;
constexpr int k_LastSurface = 8;
/// Any rain or snow makes a fireball steam
constexpr int k_SteamingRain = 0;
/// The land's cells are this many units across
constexpr float k_CellSize = 10.0f;
/// The weather's wind bytes are eighths of a metre a second
constexpr float k_WindByteSpeed = 0.125f;
/// Sound travels this many metres a second, which thunder takes to be heard
constexpr float k_SpeedOfSound = 347.0f;

/// The sheets the players' symbols are drawn with, whatever effect first shows one
constexpr std::array<std::string_view, 2> k_SymbolTextures {"S_SpriteSheet3", "PlayersSymbols"};
} // namespace

float GameParticleWorld::LandHeight(glm::vec2 xz) const
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(xz) : 0.0f;
}

float GameParticleWorld::LandHeightAtMap(int32_t x, int32_t z) const
{
	return Locator::terrainSystem::has_value() ? static_cast<float>(Locator::terrainSystem::value().GetAltitude(x, z)) : 0.0f;
}

uint32_t GameParticleWorld::PlayerColour(int player) const
{
	// A god after the seventh has the land colour of its god slot (deviation 164)
	return player_colour::Of(static_cast<PlayerNames>(player)) & 0xFFFFFFu;
}

std::optional<particles::TownCentreBelief> GameParticleWorld::TownCentreBeliefOf(entt::entity centre) const
{
	using namespace ecs::components;
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* abode = registry.Valid(centre) ? registry.TryGet<const Abode>(centre) : nullptr;
	if (abode == nullptr)
	{
		return std::nullopt;
	}
	// Its totem's top, over the icon on the plinth, where the frames draw it as it rises with the worship
	std::optional<glm::vec3> totemTop;
	registry.Each<const VillageTotem, const Transform>(
	    [&registry, &totemTop, centre](entt::entity totem, const VillageTotem& standing, const Transform& plinth) {
		    if (totemTop.has_value() || standing.townCentre != centre)
		    {
			    return;
		    }
		    const auto* icon = registry.Valid(standing.icon) ? registry.TryGet<const Transform>(standing.icon) : nullptr;
		    auto top = icon != nullptr ? ecs::drawn_place::DrawnTransformOf(registry, standing.icon, *icon).position
		                               : ecs::drawn_place::DrawnTransformOf(registry, totem, plinth).position +
		                                     glm::vec3(0.0f, ecs::village_totem::k_IconAbovePlinth, 0.0f);
		    const float iconHeight = icon != nullptr ? ecs::world_objects::SizeOf(standing.icon).height : 0.0f;
		    top.y = ecs::village_totem::TopY(top.y, iconHeight);
		    totemTop = top;
	    });
	if (!totemTop.has_value())
	{
		return std::nullopt;
	}
	particles::TownCentreBelief belief {.totemTop = *totemTop};
	registry.Each<const Town, const TownImpression>([&belief, abode](const Town& town, const TownImpression& impression) {
		if (town.id == abode->townId)
		{
			// The symbols show only the players of a land. The table of the town has a place for each player of a
			// network game
			std::copy_n(impression.belief.belief.begin(), belief.beliefs.size(), belief.beliefs.begin());
		}
	});
	return belief;
}

std::vector<int> GameParticleWorld::PlayersInGame() const
{
	std::vector<int> players;
	Locator::entitiesRegistry::value().Each<const ecs::components::Player>([&players](const ecs::components::Player& player) {
		// Only the players that play take part, not the neutral player or a player that nobody plays
		if (player.name != PlayerNames::NEUTRAL && Locator::playerSystem::has_value() &&
		    Locator::playerSystem::value().PlaysInGame(player.name))
		{
			players.push_back(static_cast<int>(player.name));
		}
	});
	std::ranges::sort(players);
	return players;
}

glm::vec3 GameParticleWorld::CameraRight() const
{
	return Locator::camera::has_value() ? Locator::camera::value().GetRight() : glm::vec3(1.0f, 0.0f, 0.0f);
}

glm::vec3 GameParticleWorld::CameraUp() const
{
	return Locator::camera::has_value() ? Locator::camera::value().GetUp() : glm::vec3(0.0f, 1.0f, 0.0f);
}

glm::vec3 GameParticleWorld::CameraPosition() const
{
	return Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : glm::vec3(0.0f);
}

void GameParticleWorld::PlayListenerSound(uint32_t inGameSample, bool playsOnce)
{
	if (Locator::audio::has_value())
	{
		const auto id = entt::hashed_string(fmt::format("InGame.sad/{}", inGameSample).c_str()).value();
		const auto voicePlayType = playsOnce ? std::optional<pack::AudioBankLoop>(pack::AudioBankLoop::Once) : std::nullopt;
		audio::StartGameSoundEffect(id, {.voicePlayType = voicePlayType});
	}
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
	TargetInfo info {.position = transform->position, .radius = 1.0f, .height = 1.0f, .scale = transform->scale.x};
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
	return Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().IsLocked(target);
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

void GameParticleWorld::StartSound(const particles::Effect& /*effect*/,
                                   const std::shared_ptr<particles::ParticleSoundLink>& sound)
{
	auto& playing = _sounds.emplace_back(PlayingSound {.link = sound});
	playing.owner = NewSoundOwner();
	// Thunder is heard once it has come from the cloud
	if (sound->sound.travelsAtSoundSpeed && Locator::audio::has_value())
	{
		playing.wait = glm::distance(Locator::audio::value().GetListener().origin, sound->position) / k_SpeedOfSound;
		if (playing.wait > 0.0f)
		{
			return;
		}
	}
	Play(playing);
}

const std::map<std::string, int32_t, std::less<>>& GameParticleWorld::SoundActions() const
{
	if (_soundActions.has_value())
	{
		return *_soundActions;
	}
	_soundActions.emplace();
	if (Locator::filesystem::has_value())
	{
		auto& fileSystem = Locator::filesystem::value();
		const auto path = fileSystem.GetPath<filesystem::Path::Data>() / "SoundAction.h";
		if (fileSystem.Exists(path))
		{
			const auto bytes = fileSystem.ReadAll(path);
			*_soundActions = psys::ParseEnumHeader(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
		}
	}
	return *_soundActions;
}

void GameParticleWorld::Play(PlayingSound& sound) const
{
	if (!Locator::audio::has_value() || !Locator::camera::has_value())
	{
		return;
	}
	const auto& actions = SoundActions();
	const auto action = actions.find(sound.link->sound.action.sound);
	if (action == actions.end())
	{
		return;
	}
	const std::array<int32_t, 5> keys {sound.link->sound.size, sound.link->sound.alignment, k_SoundObject,
	                                   sound.link->sound.surface, action->second};
	const auto result = Locator::audio::value().PlayAnimEffect(std::string(k_SpellSoundBank), keys, sound.owner,
	                                                           sound.link->position, audio::CurrentSoundEffectConditions());
	if (result.emitter != entt::null)
	{
		sound.emitter = result.emitter;
		// The sound moves with its particle each turn (see ProcessSounds), and the audio turn stops it when the camera
		// goes too far
		Locator::audio::value().SetEmitterFollows(result.emitter, audio::Follows::Place);
	}
}

void GameParticleWorld::ProcessSounds()
{
	auto* audio = Locator::audio::has_value() ? &Locator::audio::value() : nullptr;
	// The sounds' entities are the audio's, apart from the world's
	auto* registry = audio != nullptr ? audio->Emitters() : nullptr;
	const auto playing = [&](const PlayingSound& sound) {
		return audio != nullptr && sound.emitter != entt::null && audio->EmitterExists(sound.emitter) &&
		       audio->GetStatus(sound.emitter) != audio::AudioStatus::Stopped;
	};
	std::erase_if(_sounds, [&](PlayingSound& sound) {
		auto& link = *sound.link;
		// On its way to the listener: it plays when it gets there, wherever its particle has gone
		if (sound.wait > 0.0f)
		{
			sound.wait -= k_TurnSeconds;
			if (sound.wait > 0.0f)
			{
				return false;
			}
			Play(sound);
		}
		if (link.atom != nullptr)
		{
			if (link.atom->drawn)
			{
				link.position = link.atom->current.position;
				if (link.sound.onLand)
				{
					link.position.y = LandHeight({link.position.x, link.position.z});
				}
			}
			if (registry != nullptr && sound.emitter != entt::null && registry->Valid(sound.emitter))
			{
				if (auto* emitter = registry->TryGet<ecs::components::AudioEmitter>(sound.emitter))
				{
					emitter->position = link.position;
				}
			}
			// The sound stays with its particle. It does not start again when the audio stops it, for example when
			// the camera goes past its maximum distance or when it could not start.
			return false;
		}
		if (!sound.letGo)
		{
			LetGo(sound);
		}
		else
		{
			Fade(sound);
		}
		if (playing(sound))
		{
			return false;
		}
		Forget(sound);
		return true;
	});
}

void GameParticleWorld::LetGo(PlayingSound& sound)
{
	sound.letGo = true;
	if (!Locator::audio::has_value())
	{
		return;
	}
	auto& audio = Locator::audio::value();
	if (sound.emitter == entt::null || !audio.EmitterExists(sound.emitter))
	{
		return;
	}
	const auto& action = sound.link->sound.action;
	SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Particles: {} let go of", action.sound);
	// The particle's fade step when it stopped the sound, otherwise the one its sound rule gave it
	sound.fade = particles::LetGoOf(action.softRelease, audio.IsEmitterLooping(sound.emitter), sound.link->fadeStep);
	if (sound.fade.stopNow)
	{
		audio.StopEmitter(sound.emitter);
		return;
	}
	// The first step quieter comes on the turn it is let go, before its loop is released
	Fade(sound);
	if (sound.fade.releaseLoop && audio.EmitterExists(sound.emitter))
	{
		audio.ReleaseEmitterLoop(sound.emitter);
	}
}

void GameParticleWorld::Fade(PlayingSound& sound)
{
	if (sound.fade.fadeStep <= 0 || !Locator::audio::has_value())
	{
		return;
	}
	auto& audio = Locator::audio::value();
	if (sound.emitter == entt::null || !audio.EmitterExists(sound.emitter))
	{
		return;
	}
	// Quieter each turn, and stopped once silent when it would otherwise loop or ring on
	const auto volume = particles::FadedVolume(audio.GetEmitterVolume(sound.emitter), sound.fade.fadeStep);
	audio.SetEmitterVolume(sound.emitter, volume);
	if (volume == 0 && sound.fade.stopWhenSilent)
	{
		audio.StopEmitter(sound.emitter);
	}
}

entt::entity GameParticleWorld::NewSoundOwner()
{
	// The sound's own owner only tells the audio which sounds are the same. It is an entity of the audio, not of the
	// world, so that a computer with no sound keeps the same entities in the world.
	auto* emitters = Locator::audio::has_value() ? Locator::audio::value().Emitters() : nullptr;
	return emitters != nullptr ? emitters->Create() : entt::null;
}

void GameParticleWorld::Forget(PlayingSound& sound)
{
	auto* emitters = Locator::audio::has_value() ? Locator::audio::value().Emitters() : nullptr;
	if (sound.owner != entt::null && emitters != nullptr && emitters->Valid(sound.owner))
	{
		emitters->Destroy(sound.owner);
	}
	sound.owner = entt::null;
}

int GameParticleWorld::SurfaceAt(glm::vec3 point) const
{
	if (!Locator::terrainSystem::has_value() || point.x < 0.0f || point.z < 0.0f)
	{
		return k_DeepWaterSurface;
	}
	const auto& land = Locator::terrainSystem::value();
	const auto* cell = land.FindCell(glm::u16vec2(glm::floor(glm::vec2(point.x, point.z) / k_CellSize)));
	if (cell == nullptr)
	{
		return k_DeepWaterSurface;
	}
	if (cell->properties.hasWater != 0)
	{
		return k_ShallowWaterSurface;
	}
	// The second of the two materials the cell's country blends at its altitude
	const auto& countries = land.GetCountries();
	const auto types = land.GetMaterialTypes();
	int surface = k_HardSurface;
	if (cell->properties.country < countries.size() && Locator::infoConstants::has_value())
	{
		const auto index = countries[cell->properties.country].materials.at(cell->altitude).indices[1];
		const auto& materials = Locator::infoConstants::value().terrainMaterial;
		if (index < types.size() && types[index] < materials.size())
		{
			surface = static_cast<int>(materials.at(types[index]).surfaceSound);
		}
	}
	return surface >= 1 && surface <= k_LastSurface ? surface : k_HardSurface;
}

int GameParticleWorld::SoundAlignment(int player) const
{
	if (player < 0 || !Locator::alignmentSystem::has_value())
	{
		return 2;
	}
	// The alignment in seven steps from devilish to angelic, then evil, middling or good
	const float alignment = Locator::alignmentSystem::value().GetPlayerAlignment(static_cast<PlayerNames>(player));
	const auto discrete = static_cast<int>(std::min((alignment + 1.0f) * 0.5f * 6.9999995f, 6.0f));
	return discrete <= 1 ? 1 : discrete <= 4 ? 2 : 3;
}

bool GameParticleWorld::IsWater(glm::vec3 point) const
{
	if (!Locator::terrainSystem::has_value() || point.x < 0.0f || point.z < 0.0f)
	{
		return true;
	}
	const auto cell = glm::u16vec2(glm::floor(glm::vec2(point.x, point.z) / k_CellSize));
	const auto* landCell = Locator::terrainSystem::value().FindCell(cell);
	return landCell == nullptr || landCell->properties.hasWater != 0;
}

glm::vec3 GameParticleWorld::LandNormal(glm::vec2 xz) const
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetNormalAt(xz) : glm::vec3(0.0f, 1.0f, 0.0f);
}

bool GameParticleWorld::IsRainingAt(glm::vec3 point) const
{
	if (!Locator::weatherSystem::has_value())
	{
		return false;
	}
	const auto weather = Locator::weatherSystem::value().GetWeather(point);
	return weather.rain > k_SteamingRain || weather.snow > k_SteamingRain;
}

glm::vec3 GameParticleWorld::WindAt(glm::vec3 point) const
{
	if (!Locator::weatherSystem::has_value())
	{
		return glm::vec3(0.0f);
	}
	const auto weather = Locator::weatherSystem::value().GetWeather(point);
	return glm::vec3(static_cast<float>(weather.windX), 0.0f, static_cast<float>(weather.windZ)) * k_WindByteSpeed;
}

glm::vec3 GameParticleWorld::SmoothWindAt(glm::vec3 point) const
{
	if (!Locator::weatherSystem::has_value())
	{
		return glm::vec3(0.0f);
	}
	const auto weather = Locator::weatherSystem::value().GetWeatherSmooth(point);
	return glm::vec3(static_cast<float>(weather.windX), 0.0f, static_cast<float>(weather.windZ)) * k_WindByteSpeed;
}

uint32_t GameParticleWorld::AddRainStorm(const particles::storm::RainStorm& storm)
{
	if (!Locator::weatherSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return 0;
	}
	const auto entity = Locator::weatherSystem::value().AddMiracleStorm({
	    .centre = storm.centre,
	    .innerRadius = storm.innerRadius,
	    .outerRadius = storm.outerRadius,
	    .fadeSeconds = storm.fadeInSeconds,
	    .cloudHeight = storm.cloudHeight,
	    .effect = {.temperature = storm.temperature,
	               .rain = storm.rain,
	               .snow = storm.snow,
	               .overcast = storm.overcast,
	               .windX = storm.windX,
	               .windZ = storm.windZ},
	});
	const auto id = _nextRainStorm++;
	_rainStorms.insert_or_assign(id, entity);
	return id;
}

bool GameParticleWorld::MoveRainStorm(uint32_t storm, glm::vec3 centre)
{
	const auto found = _rainStorms.find(storm);
	if (found == _rainStorms.end() || !Locator::weatherSystem::has_value())
	{
		return false;
	}
	if (Locator::weatherSystem::value().MoveMiracleStorm(found->second, centre))
	{
		return true;
	}
	// Ended by a script: the weather lets it go by itself
	_rainStorms.erase(found);
	return false;
}

void GameParticleWorld::RemoveRainStorm(uint32_t storm)
{
	const auto found = _rainStorms.find(storm);
	if (found == _rainStorms.end())
	{
		return;
	}
	if (Locator::weatherSystem::has_value() && Locator::entitiesRegistry::has_value())
	{
		Locator::weatherSystem::value().RemoveMiracleStorm(found->second);
	}
	_rainStorms.erase(found);
}

bool GameParticleWorld::LandBlocks(glm::vec3 from, glm::vec3 to) const
{
	if (!Locator::pickingSystem::has_value())
	{
		return false;
	}
	// The line continues past its end. Thus the land or the sea after the end does not stop the fork.
	const auto camera = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : from;
	const auto hit = Locator::pickingSystem::value().LandOrSeaAlong(from, to, camera);
	return hit.has_value() && particles::maths::LandHitsFirst(from, to, *hit);
}

void GameParticleWorld::AddShield(const std::shared_ptr<particles::ShieldSphere>& shield)
{
	std::erase_if(_shields, [](const auto& weak) { return weak.expired(); });
	_shields.push_back(shield);
}

std::shared_ptr<particles::ShieldSphere> GameParticleWorld::FindShield(glm::vec3 point, float margin) const
{
	// The game puts each new shield at the front of its list
	for (const auto& weak : std::views::reverse(_shields))
	{
		if (auto shield = weak.lock(); shield && particles::maths::InsideSphere(point, shield->centre, shield->radius, margin))
		{
			return shield;
		}
	}
	return nullptr;
}

std::shared_ptr<particles::ShieldSphere> GameParticleWorld::FindShieldCrossing(glm::vec3 before, glm::vec3 point,
                                                                               float margin) const
{
	for (const auto& weak : std::views::reverse(_shields))
	{
		if (auto shield = weak.lock(); shield &&
		                               particles::maths::InsideSphere(point, shield->centre, shield->radius, margin) &&
		                               !particles::maths::InsideSphere(before, shield->centre, shield->radius, margin))
		{
			return shield;
		}
	}
	return nullptr;
}

std::shared_ptr<particles::ShieldSphere> GameParticleWorld::ShieldOf(const particles::Effect& effect) const
{
	for (const auto& weak : _shields)
	{
		if (auto shield = weak.lock(); shield && shield->owner == &effect)
		{
			return shield;
		}
	}
	return nullptr;
}

std::vector<glm::vec3> GameParticleWorld::TargetExtraPoints(entt::entity target) const
{
	std::vector<glm::vec3> points;
	if (!Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return points;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(target))
	{
		return points;
	}
	const auto* transform = registry.TryGet<const ecs::components::Transform>(target);
	std::optional<entt::id_type> meshId;
	if (const auto* mesh = registry.TryGet<const ecs::components::Mesh>(target))
	{
		meshId = mesh->id;
	}
	else if (const auto* dome = registry.TryGet<const ecs::components::ShieldDome>(target))
	{
		meshId = dome->mesh;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (transform == nullptr || !meshId.has_value() || !meshes.Contains(*meshId))
	{
		return points;
	}
	const glm::mat4 model = glm::translate(glm::mat4(1.0f), transform->position) * glm::mat4(transform->rotation) *
	                        glm::scale(glm::mat4(1.0f), transform->scale);
	for (const auto& extra : meshes.Handle(*meshId)->GetExtraMetrics())
	{
		points.emplace_back(model * extra[3]);
	}
	return points;
}

uint64_t GameParticleWorld::GameMilliseconds() const
{
	if (!Locator::time::has_value())
	{
		return 0;
	}
	const auto& time = Locator::time::value();
	return particles::volume_glow::GameMilliseconds(time.GetTurn(), time.GetTurnFraction());
}

std::optional<glm::vec3> GameParticleWorld::ObjectPosition(entt::entity object) const
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.Valid(object) ? registry.TryGet<const ecs::components::Transform>(object) : nullptr;
	return transform != nullptr ? std::optional(transform->position) : std::nullopt;
}

GameParticleWorld::SurfacePoint GameParticleWorld::RandomSurfacePoint(entt::entity object, particles::Effect& effect) const
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return {};
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.Valid(object) ? registry.TryGet<const ecs::components::Transform>(object) : nullptr;
	if (transform == nullptr)
	{
		return {};
	}
	const auto* mesh = registry.TryGet<const ecs::components::Mesh>(object);
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (mesh == nullptr || !meshes.Contains(mesh->id))
	{
		return {.kind = SurfacePoint::Kind::NoModel};
	}
	// A submesh, one of its primitives and one of its triangles, each as likely as the others whatever their size
	const auto& surfaces = meshes.Handle(mesh->id)->GetSurfaces();
	if (surfaces.empty())
	{
		return {.kind = SurfacePoint::Kind::NoModel};
	}
	const auto& surface = surfaces.at(static_cast<size_t>(effect.Rand(static_cast<int32_t>(surfaces.size()))));
	if (surface.primitives.empty())
	{
		return {.kind = SurfacePoint::Kind::NoModel};
	}
	const auto& primitive =
	    surface.primitives.at(static_cast<size_t>(effect.Rand(static_cast<int32_t>(surface.primitives.size()))));
	const auto triangle = static_cast<uint32_t>(effect.Rand(static_cast<int32_t>(primitive.numTriangles)));
	std::array<glm::vec3, 3> corners {};
	for (uint32_t c = 0; c < 3; ++c)
	{
		const auto index = primitive.indexBase + (triangle * 3) + c;
		if (index >= surface.indices.size())
		{
			return {.kind = SurfacePoint::Kind::NoModel};
		}
		const auto vertex = primitive.vertexBase + surface.indices.at(index);
		if (vertex >= surface.positions.size())
		{
			return {.kind = SurfacePoint::Kind::NoModel};
		}
		corners.at(c) = surface.positions.at(vertex);
	}
	// Evenly within the triangle: a point of the parallelogram on two of its sides, folded back into it
	float a = effect.Random(1.0f);
	float b = effect.Random(1.0f);
	if (a + b > 1.0f)
	{
		a = 1.0f - a;
		b = 1.0f - b;
	}
	const auto local = corners[0] + (corners[1] - corners[0]) * a + (corners[2] - corners[0]) * b;
	const glm::mat4 model = glm::translate(glm::mat4(1.0f), transform->position) * glm::mat4(transform->rotation) *
	                        glm::scale(glm::mat4(1.0f), transform->scale);
	return {.kind = SurfacePoint::Kind::Point, .position = glm::vec3(model * glm::vec4(local, 1.0f))};
}

float GameParticleWorld::PlayerAlignment(int player) const
{
	if (!Locator::alignmentSystem::has_value() || player < 0 || player >= static_cast<int>(player_slots::k_PlayerNumbers))
	{
		return 0.0f;
	}
	return Locator::alignmentSystem::value().GetPlayerAlignment(static_cast<PlayerNames>(player));
}

void GameParticleWorld::FollowCameraPath(std::string_view file, std::string_view animation, const glm::mat4& placement,
                                         float pauseSeconds, float speedUp, entt::entity spell)
{
	if (!Locator::filesystem::has_value() || !Locator::cameraPathSystem::has_value())
	{
		return;
	}
	auto& fileSystem = Locator::filesystem::value();
	auto path = std::make_unique<CameraPath>(std::string(file));
	try
	{
		if (!path->LoadFromFile(fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / DataRelative(file))))
		{
			return;
		}
	}
	catch (const std::exception& error)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Particles: cannot load the camera path {}: {}", file, error.what());
		return;
	}
	// The path lasts as long as one play of its animation
	L3DAnim anim;
	if (!anim.LoadFromFile(fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / DataRelative(animation))))
	{
		return;
	}
	Locator::cameraPathSystem::value().FollowPlaced(std::move(path), placement, pauseSeconds, speedUp,
	                                                static_cast<float>(anim.GetPlayTime()), spell);
}

void GameParticleWorld::Reset()
{
	_claimed.clear();
	_arcsWaiting.clear();
	_beliefSprites.clear();
	_gestureTrails.clear();
	_lightSheets.clear();
	_ownedSheets.clear();
	SetHandGlow(0);
	_arcsWanted = false;
	_bolts.clear();
	// Every sound stops with the land, and its owner goes
	for (auto& sound : _sounds)
	{
		if (Locator::audio::has_value() && sound.emitter != entt::null && Locator::audio::value().EmitterExists(sound.emitter))
		{
			Locator::audio::value().StopEmitter(sound.emitter);
		}
		Forget(sound);
	}
	_sounds.clear();
	_shields.clear();
	// The effects' storms went with the land
	_rainStorms.clear();
	_objectEffects.Reset();
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

ParticleRuleSystem::ParticleRuleSystem()
    : _classes(particles::ParticleClassRegistry::WithAllClasses(&_resources))
{
	if (Locator::entitiesRegistry::has_value())
	{
		_connections.emplace_back(Locator::entitiesRegistry::value()
		                              .OnDestroy<ecs::components::ScriptSpotVisual>()
		                              .connect<&ParticleRuleSystem::OnScriptSpotVisualGone>(*this));
	}
}

void ParticleRuleSystem::OnScriptSpotVisualGone(entt::registry& registry, entt::entity thing)
{
	Delete(registry.get<ecs::components::ScriptSpotVisual>(thing).effect);
}

ParticleRuleSystem::~ParticleRuleSystem() = default;

std::deque<ParticleRuleSystem::Running>::iterator ParticleRuleSystem::FindRunning(EffectId id)
{
	return std::ranges::find(_effects, id, &Running::id);
}

std::deque<ParticleRuleSystem::Running>::const_iterator ParticleRuleSystem::FindRunning(EffectId id) const
{
	return std::ranges::find(_effects, id, &Running::id);
}

void ParticleRuleSystem::Forget(std::deque<Running>::iterator effect)
{
	const auto id = effect->id;
	_effects.erase(effect);
	if (_lifeListener != nullptr)
	{
		_lifeListener->EffectEnded(id);
	}
}

void ParticleRuleSystem::TellStarted(EffectId id) const
{
	if (_lifeListener != nullptr)
	{
		_lifeListener->EffectStarted(id);
	}
}

ParticleRuleSystemInterface::EffectId ParticleRuleSystem::Start(std::string_view file, glm::vec3 origin, float magnitude,
                                                                bool synced)
{
	if (file.empty() || !Locator::resources::has_value() || !Locator::gameRandom::has_value())
	{
		return k_NoEffect;
	}
	const auto data = LoadFile(file);
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
	AddSheetNames(*effect);
	effect->StepTwiceFirstTime();
	// An effect that the simulation asked for takes the number that the simulation gave it
	const auto effectId = _givenId != k_NoEffect ? std::exchange(_givenId, k_NoEffect) : _nextId++;
	_effects.push_front({.id = effectId, .file = std::string(file), .effect = std::move(effect)});
	TellStarted(effectId);
	return effectId;
}

std::shared_ptr<const psys::ParticleFile> ParticleRuleSystem::LoadFile(std::string_view file)
{
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
			return nullptr;
		}
	}
	return files.Handle(id).handle();
}

void ParticleRuleSystem::LoadAllTypes()
{
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return;
	}
	// The first sound of an effect reads the names of the game's sound actions
	[[maybe_unused]] const auto& soundActions = _world.SoundActions();
	for (size_t index = 0; index < particles::k_ParticleTypeCount; ++index)
	{
		const auto file = particles::ParticleTypeFile(static_cast<ParticleType>(index));
		if (file.empty())
		{
			continue;
		}
		const auto data = LoadFile(file);
		if (!data)
		{
			continue;
		}
		// The creators load the models and the light maps of the effect. They do not use random numbers.
		for (const auto& object : data->objects)
		{
			if (const auto* factory = _classes.FindCreator(object.className))
			{
				[[maybe_unused]] const auto creator = (*factory)(object);
			}
		}
	}
}

ParticleRuleSystemInterface::EffectId ParticleRuleSystem::Start(ParticleType type, glm::vec3 origin, float magnitude,
                                                                bool synced)
{
	return Start(particles::ParticleTypeFile(type), origin, magnitude, synced);
}

ParticleRuleSystemInterface::EffectId ParticleRuleSystem::StartForSpell(ParticleType type, glm::vec3 origin,
                                                                        glm::vec3 direction, float magnitude,
                                                                        particles::SpellSink& sink, bool synced)
{
	const auto id = Start(type, origin, magnitude, synced);
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->ownedBySpell = true;
		it->effect->SetDirection(direction);
		it->effect->SetSink(&sink);
	}
	return id;
}

bool ParticleRuleSystem::ProcessForSpell(EffectId id, const particles::ProcessInfo& info, float seconds)
{
	const auto it = FindRunning(id);
	if (it == _effects.end())
	{
		return false;
	}
	it->effect->SetProcessInfo(info);
	// Its step may start other effects, such as a blast's spot visuals, so it is found again after
	if (StepEffect(*it->effect, seconds))
	{
		Forget(FindRunning(id));
		return false;
	}
	return true;
}

bool ParticleRuleSystem::ProcessByFrame(EffectId id, float seconds)
{
	const auto it = FindRunning(id);
	if (it == _effects.end())
	{
		return false;
	}
	it->ownedBySpell = true;
	if (_paused)
	{
		return true;
	}
	if (StepEffect(*it->effect, seconds))
	{
		Forget(FindRunning(id));
		return false;
	}
	return true;
}

ParticleRuleSystemInterface::EffectId ParticleRuleSystem::StartByFrame(ParticleType type, glm::vec3 origin, float magnitude)
{
	const auto id = Start(type, origin, magnitude, false);
	// The turn does not step it, also before its first frame step
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->ownedBySpell = true;
	}
	return id;
}

ParticleRuleSystemInterface::EffectId ParticleRuleSystem::StartOwned(ParticleType type, glm::vec3 origin, float magnitude)
{
	const auto id = Start(type, origin, magnitude, true);
	// The turn does not step it: its owner does
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->ownedBySpell = true;
	}
	return id;
}

void ParticleRuleSystem::SetShown(EffectId id, bool shown)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->shown = shown;
	}
}

ParticleRuleSystemInterface::EffectId ParticleRuleSystem::StartSpotVisual(SpotVisualType type, glm::vec3 position,
                                                                          std::optional<int> turns, entt::entity owner,
                                                                          float magnitude, bool synced)
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
	// Spot visuals are the same on every machine, except one that shows only what the hand of this computer did
	const auto id = Start(info.particleType, position, magnitude, synced);
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->turnsLeft = turns.value_or(static_cast<int>(info.life));
		it->owner = owner;
		// Some act on their owner, which their rules take as a target
		if (info.targetOwnerObject == 1 && owner != entt::null)
		{
			it->effect->AddTarget(owner);
		}
		// Some are drawn all together at their origin rather than each sprite in its own place
		it->path = info.singleZSort == 1 ? particles::draw::DrawPath::Queued : particles::draw::DrawPath::Sorted;
	}
	return id;
}

EffectHandle ParticleRuleSystem::Serve(const EffectRequest& request, EffectHandle handle)
{
	_givenId = handle;
	EffectId id = k_NoEffect;
	if (const auto* spotVisual = std::get_if<SpotVisualType>(&request.type))
	{
		id = StartSpotVisual(*spotVisual, request.position, request.turns, request.owner, request.magnitude, request.synced);
	}
	else if (request.step == EffectStep::Frame)
	{
		id = StartByFrame(std::get<ParticleType>(request.type), request.position, request.magnitude);
	}
	else
	{
		id = Start(std::get<ParticleType>(request.type), request.position, request.magnitude, request.synced);
	}
	// A request that started no effect does not give its number to the next effect
	_givenId = k_NoEffect;
	const auto it = FindRunning(id);
	if (it == _effects.end())
	{
		return k_NoEffect;
	}
	if (request.direction.has_value())
	{
		it->effect->SetDirection(*request.direction);
	}
	// The owner steps the effect. A miracle that owns it also gets told that the effect started.
	if (request.sink != nullptr || request.ownerSteps)
	{
		it->ownedBySpell = true;
	}
	if (request.sink != nullptr)
	{
		it->effect->SetSink(request.sink);
	}
	// The event can start other effects, so find the effect again
	const auto started = FindRunning(id);
	if (started == _effects.end())
	{
		return k_NoEffect;
	}
	if (request.player.has_value())
	{
		started->effect->SetPlayer(*request.player);
	}
	if (request.processInfo.has_value())
	{
		started->effect->SetProcessInfo(*request.processInfo);
	}
	if (request.drawnWithHand)
	{
		started->path = particles::draw::DrawPath::Immediate;
	}
	if (request.hidden)
	{
		started->effect->SetHidden(true);
	}
	if (request.drawPath.has_value())
	{
		started->path = *request.drawPath;
	}
	if (!request.saved)
	{
		started->saved = false;
	}
	return id;
}

void ParticleRuleSystem::SetMagnitude(EffectHandle handle, float magnitude)
{
	if (const auto it = FindRunning(handle); it != _effects.end())
	{
		it->effect->SetMagnitude(magnitude);
	}
}

void ParticleRuleSystem::SetAlpha(EffectHandle handle, float alpha)
{
	if (const auto it = FindRunning(handle); it != _effects.end())
	{
		it->effect->SetGlobalAlpha(alpha);
	}
}

void ParticleRuleSystem::SetProcessInfo(EffectHandle handle, const particles::ProcessInfo& info)
{
	if (const auto it = FindRunning(handle); it != _effects.end())
	{
		it->effect->SetProcessInfo(info);
	}
}

void ParticleRuleSystem::SetEnabled(EffectHandle handle, bool enabled)
{
	if (const auto it = FindRunning(handle); it != _effects.end())
	{
		auto info = it->effect->GetProcessInfo();
		info.enabled = enabled;
		it->effect->SetProcessInfo(info);
	}
}

void ParticleRuleSystem::SetOrigin(EffectId id, glm::vec3 origin)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->effect->SetOrigin(origin);
	}
}

void ParticleRuleSystem::SetPlayer(EffectId id, int player)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->effect->SetPlayer(player);
	}
}

void ParticleRuleSystem::SetPower(EffectId id, float power)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		auto info = it->effect->GetProcessInfo();
		info.power = power;
		it->effect->SetProcessInfo(info);
	}
}

void ParticleRuleSystem::SetPlayerSymbols(std::vector<particles::draw::PlayerSymbol> symbols)
{
	_playerSymbols = std::move(symbols);
}

void ParticleRuleSystem::SetDrawPath(EffectId id, particles::draw::DrawPath path)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->path = path;
	}
}

void ParticleRuleSystem::SetInTemple(EffectId id)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->inTemple = true;
	}
}

void ParticleRuleSystem::SetDrawnAlone(EffectId id)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->drawnAlone = true;
	}
}

void ParticleRuleSystem::SetDrawOffset(EffectId id, glm::vec3 offset)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->drawOffset = offset;
	}
}

void ParticleRuleSystem::SetHidden(EffectId id, bool hidden)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->effect->SetHidden(hidden);
	}
}

void ParticleRuleSystem::SetLiveHand(EffectId id, glm::vec3 hand)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->liveHand = hand;
	}
}

void ParticleRuleSystem::AddHandManaPathSpark(const particles::mana_path::Spark& spark)
{
	// Only this computer's hand shows it, so its random numbers are this computer's own
	if (FindRunning(_handManaPath) == _effects.end())
	{
		_handManaPath = Start(ParticleType::ManaPath, glm::vec3(0.0f), 1.0f, false);
	}
	if (const auto it = FindRunning(_handManaPath); it != _effects.end())
	{
		it->effect->AddManaPathSpark(spark);
	}
}

void ParticleRuleSystem::AddStormBolt(glm::vec3 cloudPoint)
{
	if (IsRunning(_stormBolts))
	{
		AddTargetPosition(_stormBolts, cloudPoint);
	}
	else
	{
		_pendingStormBolts.push_back(cloudPoint);
	}
}

std::shared_ptr<particles::ShieldSphere> ParticleRuleSystem::FindShield(glm::vec3 point, float margin) const
{
	return _world.FindShield(point, margin);
}

void ParticleRuleSystem::AddTarget(EffectId id, entt::entity target)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->effect->AddTarget(target);
	}
}

void ParticleRuleSystem::AddTargetPosition(EffectId id, glm::vec3 position)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->effect->AddTargetPosition(position);
	}
}

void ParticleRuleSystem::AddPlasma(EffectId id, const particles::PlasmaCommand& command)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->effect->AddPlasma(command);
	}
}

void ParticleRuleSystem::AddManaPathSpark(EffectId id, const particles::mana_path::Spark& spark)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->effect->AddManaPathSpark(spark);
	}
}

void ParticleRuleSystem::CloseDown(EffectId id)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->effect->CloseDown();
	}
}

bool ParticleRuleSystem::Renumber(EffectId from, EffectId to)
{
	const auto it = FindRunning(from);
	if (to == k_NoEffect || it == _effects.end() || FindRunning(to) != _effects.end())
	{
		return false;
	}
	it->id = to;
	if (from + 1 == _nextId)
	{
		_nextId = from;
	}
	if (_lifeListener != nullptr)
	{
		_lifeListener->EffectEnded(from);
		_lifeListener->EffectStarted(to);
	}
	return true;
}

void ParticleRuleSystem::KeepOutOfSaves(EffectId id)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->saved = false;
	}
}

void ParticleRuleSystem::Delete(EffectId id)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		Forget(it);
	}
}

bool ParticleRuleSystem::IsRunning(EffectId id) const
{
	return FindRunning(id) != _effects.end();
}

particles::Effect* ParticleRuleSystem::Find(EffectId id)
{
	const auto it = FindRunning(id);
	return it != _effects.end() ? it->effect.get() : nullptr;
}

bool ParticleRuleSystem::StepEffect(particles::Effect& effect, float seconds)
{
	if (_paused)
	{
		return false;
	}
	effect.Step(seconds);
	// Closing down ends at once an effect whose file says so
	return effect.Finished() || (effect.Closing() && effect.DeleteOnCloseDown());
}

void ParticleRuleSystem::ProcessTurn()
{
	if (_paused)
	{
		return;
	}
	// The one effect that throws the pieces of everything the blasts break runs all the time, started again whenever
	// it ends
	if (!IsRunning(_explodeObject))
	{
		_explodeObject = Start(ParticleType::ExplodeObject, glm::vec3(0.0f), 1.0f, true);
	}
	// So does the one every storm's forked bolts come down in
	if (!IsRunning(_stormBolts))
	{
		_stormBolts = Start(ParticleType::LightningStrike, glm::vec3(0.0f), 1.0f, true);
	}
	for (const auto& point : _pendingStormBolts)
	{
		AddTargetPosition(_stormBolts, point);
	}
	_pendingStormBolts.clear();
	KeepGestureTrails();
	const auto* registry = Locator::entitiesRegistry::has_value() ? &Locator::entitiesRegistry::value() : nullptr;
	// By id: a step may start other effects, which take their first step in the next turn. The spot visuals take their
	// steps last, the newest first.
	std::vector<particles::turn_order::Entry> started;
	started.reserve(_effects.size());
	std::ranges::transform(_effects, std::back_inserter(started), [](const Running& running) {
		return particles::turn_order::Entry {.id = running.id, .spotVisual = running.turnsLeft.has_value()};
	});
	for (const auto id : particles::turn_order::Steps(started))
	{
		const auto it = FindRunning(id);
		if (it == _effects.end() || it->ownedBySpell || it->everyFrame)
		{
			continue;
		}
		auto& effect = *it->effect;
		// A spot visual stays where it was made, unless moved on purpose; it ends when its owner goes. One that acts on
		// its owner follows it through its rules, which take the owner as their target.
		if (it->owner != entt::null && (registry == nullptr || !registry->Valid(it->owner)))
		{
			it->owner = entt::null;
			effect.CloseDown();
		}
		if (it->turnsLeft.has_value() && *it->turnsLeft >= 0 && --*it->turnsLeft <= 0)
		{
			effect.CloseDown();
		}
		if (StepEffect(effect, k_TurnSeconds))
		{
			Forget(FindRunning(id));
		}
	}
	_world.ProcessSounds();
	_world.ProcessGlows();
	// The symbols of belief rise in an effect of their own too
	if (_beliefWanted &&
	    std::ranges::none_of(_effects, [](const Running& running) { return running.file == "SF_BeliefSprite"; }))
	{
		Start(ParticleType::BeliefSprite, glm::vec3(0.0f), 1.0f, true);
	}
	_beliefWanted = false;
	// The electric arcs over what lightning struck crawl in an effect of their own, kept running once wanted
	if (_world.TakeArcsWanted() &&
	    std::ranges::none_of(_effects, [](const Running& running) { return running.file == "SF_OnFire"; }))
	{
		Start(ParticleType::OnFire, glm::vec3(0.0f), 1.0f, true);
	}
}

void ParticleRuleSystem::Reset()
{
	_playerSymbols.clear();
	_beliefSpritesSeen.clear();
	_beliefSpriteCount = 0;
	_fightMarks.clear();
	_effects.clear();
	if (_lifeListener != nullptr)
	{
		_lifeListener->EffectsCleared();
	}
	_explodeObject = k_NoEffect;
	_stormBolts = k_NoEffect;
	_pendingStormBolts.clear();
	_gestureTrails = k_NoEffect;
	_gestureChain = k_NoEffect;
	_selectionEffect = k_NoEffect;
	_world.Reset();
	_loadedSynced.reset();
	if (std::exchange(_freshIdsAtNextReset, false))
	{
		_nextId = 1;
	}
}

void ParticleRuleSystem::AddSheetNames(const particles::Effect& effect)
{
	const auto add = [this](std::string_view texture) {
		if (texture.empty() || _knownSheetNames.contains(texture))
		{
			return;
		}
		_knownSheetNames.emplace(texture);
		_sheetNames.emplace_back(texture);
	};
	for (const auto& object : effect.GetFile().objects)
	{
		// The rule that draws a surface names its sheet
		if (object.className == "ZR_SurfRevol")
		{
			add(particles::TextureBaseName(object.String("TextureFileName")));
		}
		// A shower of sparks over an object uses a sheet of its own
		if (object.className == "CreateRule_GameObjectRef" && object.Bool("DoShower", false))
		{
			add(particles::rising_sparks::k_SparkSheet);
		}
		// The rule that makes the symbols of belief of a village draws them
		if (object.className == "UR_TownCentreBelief")
		{
			std::ranges::for_each(k_SymbolTextures, add);
		}
		const auto* creator = effect.FindCreator(object.name);
		if (creator == nullptr)
		{
			continue;
		}
		if (creator->kind == particles::Creator::Kind::Sprite || creator->kind == particles::Creator::Kind::Chain)
		{
			add(creator->texture);
		}
		else if (creator->kind == particles::Creator::Kind::Symbol)
		{
			std::ranges::for_each(k_SymbolTextures, add);
		}
	}
}

ParticleRuleSystemInterface::DrawnEffect ParticleRuleSystem::Drawn(const Running& running)
{
	return {
	    .id = running.id,
	    .effect = running.effect.get(),
	    .path = running.path,
	    .drawOffset = running.drawOffset,
	    .liveHand = running.liveHand,
	    .inTemple = running.inTemple,
	    .drawnAlone = running.drawnAlone,
	    .shown = running.shown,
	};
}

void ParticleRuleSystem::ForEachDrawn(const std::function<void(const DrawnEffect&)>& visit) const
{
	for (const auto& running : _effects)
	{
		visit(Drawn(running));
	}
}

std::optional<ParticleRuleSystemInterface::DrawnEffect> ParticleRuleSystem::FindDrawn(EffectId id) const
{
	const auto it = FindRunning(id);
	return it != _effects.end() ? std::optional(Drawn(*it)) : std::nullopt;
}

std::vector<ParticleRuleSystemInterface::EffectInfo> ParticleRuleSystem::GetEffects() const
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
		    .owner = running.owner,
		});
	}
	return result;
}

std::vector<std::string> ParticleRuleSystem::GetFileNames() const
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

namespace
{

/// The live things of a list of weak holds, in the order of the list
template <typename Type>
std::vector<std::shared_ptr<Type>> LiveItems(const std::vector<std::weak_ptr<Type>>& list)
{
	std::vector<std::shared_ptr<Type>> live;
	for (const auto& weak : list)
	{
		if (auto item = weak.lock())
		{
			live.push_back(std::move(item));
		}
	}
	return live;
}

/// The numbers of the live things of a list of weak holds that are not things of the effects of this computer, in the
/// order of the list
template <typename Type>
void WriteSyncedLive(particles::EffectWriter& out, const std::vector<std::weak_ptr<Type>>& list,
                     const particles::SharedTable<Type>& localThings)
{
	const auto live = LiveItems(list);
	const auto synced = [&localThings](const std::shared_ptr<Type>& item) { return !localThings.Contains(item.get()); };
	out.Out().WriteSize(static_cast<size_t>(std::ranges::count_if(live, synced)));
	for (const auto& item : live | std::views::filter(synced))
	{
		out.WriteShared(item);
	}
}

/// The live things of a list of weak holds that are things of the effects of this computer. Each thing goes with its
/// place among the live things of the list, so that a load puts it back between the synced things.
template <typename Type>
void WriteLocalLive(particles::EffectWriter& out, const std::vector<std::weak_ptr<Type>>& list,
                    const particles::SharedTable<Type>& localThings)
{
	const auto live = LiveItems(list);
	const auto local = [&localThings](const std::shared_ptr<Type>& item) { return localThings.Contains(item.get()); };
	out.Out().WriteSize(static_cast<size_t>(std::ranges::count_if(live, local)));
	for (size_t place = 0; place < live.size(); ++place)
	{
		if (local(live[place]))
		{
			out.Write(static_cast<uint32_t>(place));
			out.WriteShared(live[place]);
		}
	}
}

/// Reads the things that WriteLocalLive wrote and puts each thing back at its place in the list
template <typename Type>
void ReadLocalLive(particles::EffectReader& in, std::vector<std::weak_ptr<Type>>& list)
{
	const auto size = in.ReadSize();
	for (size_t i = 0; size.has_value() && i < *size && !in.Failed(); ++i)
	{
		uint32_t place = 0;
		in.Read(place);
		if (auto item = in.ReadShared<Type>())
		{
			const auto at = std::min(static_cast<size_t>(place), list.size());
			list.insert(list.begin() + static_cast<std::ptrdiff_t>(at), item);
		}
	}
}

/// Adds the live things of a list of weak holds to a table. With local true, it adds only the things in the local table.
/// With local false, it adds only the other things.
template <typename Type>
void NumberLive(particles::SharedTable<Type>& table, const std::vector<std::weak_ptr<Type>>& list,
                const particles::SharedTable<Type>& localThings, bool local)
{
	for (const auto& weak : list)
	{
		if (auto item = weak.lock(); item && localThings.Contains(item.get()) == local)
		{
			table.Add(item);
		}
	}
}

template <typename Type>
void ReadLive(particles::EffectReader& in, std::vector<std::weak_ptr<Type>>& list)
{
	list.clear();
	const auto size = in.In().ReadSize();
	for (size_t i = 0; size.has_value() && i < *size && !in.Failed(); ++i)
	{
		if (auto item = in.ReadShared<Type>())
		{
			list.push_back(item);
		}
	}
}

} // namespace

void GameParticleWorld::NumberShared(particles::SharedTables& tables, const LeftOutSounds& leftOut,
                                     const particles::SharedTables& localThings, bool local) const
{
	for (const auto& sound : _sounds)
	{
		if (Saved(sound, leftOut) && localThings.sounds.Contains(sound.link.get()) == local)
		{
			tables.sounds.Add(sound.link);
		}
	}
	NumberLive(tables.bolts, _bolts, localThings.bolts, local);
	NumberLive(tables.shields, _shields, localThings.shields, local);
	NumberLive(tables.sheets, _lightSheets, localThings.sheets, local);
	for (const auto& trail : _gestureTrails)
	{
		if (localThings.trails.Contains(trail.get()) == local)
		{
			tables.trails.Add(trail);
		}
	}
}

void GameParticleWorld::AddLocalThings(particles::SharedTables& localThings) const
{
	// Only the gestures of this computer make trails, and only the effect of these gestures takes them
	for (const auto& trail : _gestureTrails)
	{
		localThings.trails.Add(trail);
	}
}

bool GameParticleWorld::EndsWithLoad(const PlayingSound& sound)
{
	return !sound.link || sound.letGo || (sound.link->atom == nullptr && sound.wait <= 0.0f);
}

bool GameParticleWorld::Saved(const PlayingSound& sound, const LeftOutSounds& leftOut)
{
	return !EndsWithLoad(sound) && !leftOut.contains(sound.link.get());
}

void GameParticleWorld::SaveState(particles::EffectWriter& out, const LeftOutSounds& leftOut,
                                  const particles::SharedTables& localThings) const
{
	out.WriteValue(_claimed);
	out.WriteValue(_arcsWaiting);
	out.WritePlainList(_beliefSprites);
	// The trails of the gestures are things of this computer. The part of the save for these effects keeps them.
	out.Out().WriteSize(0);
	WriteSyncedLive(out, _lightSheets, localThings.sheets);
	out.WriteValue(_arcsWanted);
	WriteSyncedLive(out, _bolts, localThings.bolts);
	out.WriteValue(_boltOrder);
	// A sound goes with the particle that keeps its record. The audio's own emitter and the sound's owner, an entity of
	// the audio, are of this run only and do not go. A sound that a load ends at once does not go either.
	const auto synced = [&leftOut, &localThings](const PlayingSound& sound) {
		return Saved(sound, leftOut) && !localThings.sounds.Contains(sound.link.get());
	};
	out.Out().WriteSize(static_cast<size_t>(std::ranges::count_if(_sounds, synced)));
	for (const auto& sound : _sounds | std::views::filter(synced))
	{
		WriteSound(out, sound);
	}
	out.WriteValue(_rainStorms);
	out.WriteValue(_nextRainStorm);
	WriteSyncedLive(out, _shields, localThings.shields);
	for (const auto& queue : _objectEffects.Queues())
	{
		out.WritePlainList(queue);
	}
}

void GameParticleWorld::SaveLocalState(particles::EffectWriter& out, const LeftOutSounds& leftOut,
                                       const particles::SharedTables& localThings) const
{
	out.Out().WriteSize(_gestureTrails.size());
	for (const auto& trail : _gestureTrails)
	{
		out.WriteShared(trail);
	}
	WriteLocalLive(out, _lightSheets, localThings.sheets);
	WriteLocalLive(out, _bolts, localThings.bolts);
	// Each sound goes with its place among the sounds that the save keeps
	const auto kept = [&leftOut](const PlayingSound& sound) { return Saved(sound, leftOut); };
	const auto local = [&localThings](const PlayingSound& sound) { return localThings.sounds.Contains(sound.link.get()); };
	const auto keptAndLocal = [&kept, &local](const PlayingSound& sound) { return kept(sound) && local(sound); };
	out.Out().WriteSize(static_cast<size_t>(std::ranges::count_if(_sounds, keptAndLocal)));
	uint32_t place = 0;
	for (const auto& sound : _sounds | std::views::filter(kept))
	{
		if (local(sound))
		{
			out.Write(place);
			WriteSound(out, sound);
		}
		++place;
	}
	WriteLocalLive(out, _shields, localThings.shields);
}

void GameParticleWorld::WriteSound(particles::EffectWriter& out, const PlayingSound& sound)
{
	out.WriteShared(sound.link);
	out.WriteValue(sound.letGo);
	out.WritePlain(sound.fade);
	out.WriteValue(sound.wait);
}

std::optional<GameParticleWorld::PlayingSound> GameParticleWorld::ReadSound(particles::EffectReader& in)
{
	PlayingSound sound;
	sound.link = in.ReadShared<particles::ParticleSoundLink>();
	// The sound gets an owner of this run's audio
	sound.owner = NewSoundOwner();
	in.ReadValue(sound.letGo);
	in.ReadPlain(sound.fade);
	in.ReadValue(sound.wait);
	if (EndsWithLoad(sound))
	{
		Forget(sound);
		return std::nullopt;
	}
	return sound;
}

bool GameParticleWorld::LoadLocalState(particles::EffectReader& in)
{
	_gestureTrails.clear();
	const auto trails = in.ReadSize();
	for (size_t i = 0; trails.has_value() && i < *trails && !in.Failed(); ++i)
	{
		if (auto trail = in.ReadShared<particles::GestureTrail>())
		{
			_gestureTrails.push_back(std::move(trail));
		}
	}
	ReadLocalLive(in, _lightSheets);
	ReadLocalLive(in, _bolts);
	const auto sounds = in.ReadSize();
	for (size_t i = 0; sounds.has_value() && i < *sounds && !in.Failed(); ++i)
	{
		uint32_t place = 0;
		in.Read(place);
		if (auto sound = ReadSound(in))
		{
			const auto at = std::min(static_cast<size_t>(place), _sounds.size());
			_sounds.insert(_sounds.begin() + static_cast<std::ptrdiff_t>(at), std::move(*sound));
		}
	}
	ReadLocalLive(in, _shields);
	return !in.Failed();
}

bool GameParticleWorld::LoadState(particles::EffectReader& in)
{
	in.ReadValue(_claimed);
	in.ReadValue(_arcsWaiting);
	in.ReadPlainList(_beliefSprites);
	_gestureTrails.clear();
	const auto trails = in.In().ReadSize();
	for (size_t i = 0; trails.has_value() && i < *trails && !in.Failed(); ++i)
	{
		if (auto trail = in.ReadShared<particles::GestureTrail>())
		{
			_gestureTrails.push_back(std::move(trail));
		}
	}
	ReadLive(in, _lightSheets);
	in.ReadValue(_arcsWanted);
	ReadLive(in, _bolts);
	in.ReadValue(_boltOrder);
	_sounds.clear();
	const auto sounds = in.In().ReadSize();
	for (size_t i = 0; sounds.has_value() && i < *sounds && !in.Failed(); ++i)
	{
		if (auto sound = ReadSound(in))
		{
			_sounds.push_back(std::move(*sound));
		}
	}
	in.ReadValue(_rainStorms);
	in.ReadValue(_nextRainStorm);
	ReadLive(in, _shields);
	std::array<std::vector<particles::ExplodeRecord>, 2> queues;
	for (auto& queue : queues)
	{
		in.ReadPlainList(queue);
	}
	_objectEffects.RestoreQueues(std::move(queues));
	if (in.Failed())
	{
		return false;
	}
	// A load stops every sound. A particle keeps the record of its sound, so its rules do not start the sound again,
	// and the sound stays silent while the particle lives. A sound that waits to travel to the listener plays when it
	// arrives.
	return true;
}

void ParticleRuleSystem::RestoreSink(EffectId id, particles::SpellSink* sink)
{
	if (const auto it = FindRunning(id); it != _effects.end())
	{
		it->effect->RestoreSink(sink);
	}
}

namespace
{

/// The forms of the two parts of the save of the particles. A load refuses a part of another form.
constexpr uint32_t k_SyncedPartVersion = 1;
constexpr uint32_t k_LocalPartVersion = 1;

template <typename Type>
std::vector<std::weak_ptr<Type>> WeakItems(const particles::SharedTable<Type>& table)
{
	return {table.items.begin(), table.items.end()};
}

/// Puts the things of the synced part back in a table with their numbers. A thing that has gone keeps its number.
template <typename Type>
void RestoreItems(particles::SharedTable<Type>& table, const std::vector<std::weak_ptr<Type>>& held)
{
	for (const auto& item : held)
	{
		table.Append(item.lock());
	}
}

} // namespace

/// What the two parts of a save number in the same way
struct ParticleRuleSystem::SaveNumbering
{
	/// The sounds of the effects that the save leaves out
	GameParticleWorld::LeftOutSounds leftOut;
	/// The things that the effects of this computer use
	particles::SharedTables localThings;
	/// The tables of the synced part, with all their numbers
	particles::SharedTables synced;
};

void ParticleRuleSystem::NumberForSave(const reflection::BinaryWriter& out, SaveNumbering& numbering) const
{
	// The sounds of the effects that the save leaves out stay out too: the effects make them again when their owners
	// start them again
	{
		particles::SharedTables leftOutTables;
		reflection::BinaryWriter leftOutBytes(out.Context());
		particles::EffectWriter leftOutWriter(leftOutBytes, leftOutTables);
		for (const auto& running : _effects | std::views::filter([](const Running& running) { return !running.saved; }))
		{
			running.effect->Save(leftOutWriter);
		}
		for (const auto& link : leftOutTables.sounds.items)
		{
			numbering.leftOut.insert(link.get());
		}
	}
	// The effects of this computer draw their own random numbers, and only this computer steps them, also in the
	// frames. The local tables keep the things that these effects share.
	{
		reflection::BinaryWriter localBytes(out.Context());
		particles::EffectWriter localWriter(localBytes, numbering.localThings);
		for (const auto& running : _effects | std::views::filter(IsLocal))
		{
			running.effect->Save(localWriter);
		}
		_world.AddLocalThings(numbering.localThings);
	}
	// The synced things take their numbers in the order that the save finds them. A thing that a synced effect and an
	// effect of this computer both use is a synced thing.
	_world.NumberShared(numbering.synced, numbering.leftOut, numbering.localThings, false);
	{
		reflection::BinaryWriter syncedBytes(out.Context());
		particles::EffectWriter syncedWriter(syncedBytes, numbering.synced);
		for (const auto& running : _effects | std::views::filter(IsSynced))
		{
			running.effect->Save(syncedWriter);
		}
	}
	for (const auto& bolt : numbering.localThings.bolts.items)
	{
		if (!numbering.synced.bolts.Contains(bolt.get()))
		{
			numbering.synced.boltsOfOtherPart.insert(bolt.get());
		}
	}
	particles::AddLinkedBolts(numbering.synced);
}

void ParticleRuleSystem::SaveRunning(reflection::BinaryWriter& bytes, particles::EffectWriter& effects, const Running& running)
{
	reflection::Save(bytes, running.id);
	reflection::Save(bytes, running.file);
	reflection::Save(bytes, running.effect->IsSynced());
	reflection::Save(bytes, running.ownedBySpell);
	reflection::Save(bytes, running.everyFrame);
	reflection::Save(bytes, running.inTemple);
	reflection::Save(bytes, running.turnsLeft);
	reflection::Save(bytes, running.owner);
	reflection::Save(bytes, running.path);
	reflection::Save(bytes, running.drawOffset);
	const auto at = bytes.BeginLength();
	running.effect->Save(effects);
	bytes.EndLength(at);
}

void ParticleRuleSystem::WarnUnsaved(std::initializer_list<const reflection::BinaryWriter*> writers)
{
	for (const auto* writer : writers)
	{
		for (const auto& unsaved : writer->Unsaved())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "Particles: the save does not keep {} ({})", unsaved.place, unsaved.type);
		}
	}
}

void ParticleRuleSystem::SaveState(reflection::BinaryWriter& out) const
{
	reflection::Save(out, k_SyncedPartVersion);
	reflection::Save(out, _paused);
	reflection::Save(out, _explodeObject);
	reflection::Save(out, _stormBolts);
	reflection::Save(out, _pendingStormBolts);
	reflection::Save(out, _gestureTrails);
	reflection::Save(out, _beliefWanted);

	SaveNumbering numbering;
	NumberForSave(out, numbering);
	auto& tables = numbering.synced;
	// The effects and the world go into writers of their own first, so that the tables have every shared thing in them
	// when they are written
	reflection::BinaryWriter effectBytes(out.Context());
	particles::EffectWriter effects(effectBytes, tables);
	effectBytes.WriteSize(static_cast<size_t>(std::ranges::count_if(_effects, IsSynced)));
	for (const auto& running : _effects | std::views::filter(IsSynced))
	{
		SaveRunning(effectBytes, effects, running);
	}
	reflection::BinaryWriter worldBytes(out.Context());
	particles::EffectWriter world(worldBytes, tables);
	_world.SaveState(world, numbering.leftOut, numbering.localThings);
	WarnUnsaved({&effectBytes, &worldBytes});

	particles::EffectWriter tableWriter(out, tables);
	particles::SaveSharedTables(tableWriter);
	out.Append(effectBytes);
	out.Append(worldBytes);
}

void ParticleRuleSystem::SaveLocalState(reflection::BinaryWriter& out) const
{
	// Only the frames of this computer change this part, so all of it is a presentation value
	out.BeginPresentation();
	reflection::Save(out, k_LocalPartVersion);
	reflection::Save(out, _nextId);
	reflection::Save(out, _gestureChain);
	reflection::Save(out, _selectionEffect);
	// Only the hand of this computer starts the effect of its mana path, also in a frame
	reflection::Save(out, _handManaPath);
	reflection::Save(out, _chainScrollPace);

	SaveNumbering numbering;
	NumberForSave(out, numbering);
	// The things of this computer take the numbers after the numbers of the synced part
	auto tables = std::move(numbering.synced);
	tables.boltsOfOtherPart.clear();
	tables.StartLocal();
	_world.NumberShared(tables, numbering.leftOut, numbering.localThings, true);
	// Each effect goes with its place among the effects that the save keeps, so that the load puts it back between the
	// synced effects in the same order
	reflection::BinaryWriter effectBytes(out.Context());
	particles::EffectWriter effects(effectBytes, tables);
	effectBytes.WriteSize(static_cast<size_t>(std::ranges::count_if(_effects, IsLocal)));
	uint32_t place = 0;
	for (const auto& running : _effects | std::views::filter([](const Running& running) { return running.saved; }))
	{
		if (IsLocal(running))
		{
			reflection::Save(effectBytes, place);
			SaveRunning(effectBytes, effects, running);
		}
		++place;
	}
	reflection::BinaryWriter worldBytes(out.Context());
	particles::EffectWriter world(worldBytes, tables);
	_world.SaveLocalState(world, numbering.leftOut, numbering.localThings);
	WarnUnsaved({&effectBytes, &worldBytes});

	// A synced bolt can meet a bolt of this computer. The synced part has no number for that bolt, so this part keeps
	// the link.
	particles::AddLinkedBolts(tables);
	const auto firstLocal = tables.bolts.firstLocal.value_or(0);
	const auto localNumber = [&tables, firstLocal](const std::weak_ptr<particles::BoltShare>& link) -> uint32_t {
		const auto bolt = link.lock();
		const auto found = bolt != nullptr ? tables.bolts.numbers.find(bolt.get()) : tables.bolts.numbers.end();
		return found != tables.bolts.numbers.end() && found->second > firstLocal ? found->second : 0;
	};
	std::vector<std::array<uint32_t, 3>> links;
	for (size_t i = 0; i < firstLocal; ++i)
	{
		const auto& bolt = *tables.bolts.items[i];
		const auto to = localNumber(bolt.linkedTo);
		const auto from = localNumber(bolt.linkedFrom);
		if (to != 0 || from != 0)
		{
			links.push_back({static_cast<uint32_t>(i + 1), to, from});
		}
	}

	particles::EffectWriter tableWriter(out, tables);
	particles::SaveSharedTables(tableWriter);
	out.Append(effectBytes);
	out.Append(worldBytes);
	out.WriteSize(links.size());
	for (const auto& link : links)
	{
		for (const auto number : link)
		{
			reflection::Save(out, number);
		}
	}
	out.EndPresentation();
}

ParticleRuleSystem::ReadResult ParticleRuleSystem::ReadRunning(reflection::BinaryReader& in, particles::EffectReader& reader,
                                                               Running& running)
{
	bool synced = true;
	reflection::Load(in, running.id);
	reflection::Load(in, running.file);
	reflection::Load(in, synced);
	reflection::Load(in, running.ownedBySpell);
	reflection::Load(in, running.everyFrame);
	reflection::Load(in, running.inTemple);
	reflection::Load(in, running.turnsLeft);
	reflection::Load(in, running.owner);
	reflection::Load(in, running.path);
	reflection::Load(in, running.drawOffset);
	const auto length = in.ReadSize();
	if (!length.has_value())
	{
		return ReadResult::Failed;
	}
	const auto end = in.Position() + *length;
	const auto data = LoadFile(running.file);
	if (data)
	{
		// The effect is made from its file as at its start, then takes the state it had
		running.effect = std::make_unique<particles::Effect>(
		    data, particles::EffectServices {_classes, _world, Locator::gameRandom::value(), _noise}, glm::vec3(0.0f), 1.0f,
		    synced);
		if (running.effect->Load(reader) && in.Position() == end)
		{
			AddSheetNames(*running.effect);
			TellStarted(running.id);
			return ReadResult::Read;
		}
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Particles: the saved effect {} does not read", running.file);
		return ReadResult::Failed;
	}
	// An effect whose file is gone is left out
	if (in.Position() > end)
	{
		return ReadResult::Failed;
	}
	[[maybe_unused]] const auto skipped = in.Take(end - in.Position());
	return ReadResult::Skipped;
}

void ParticleRuleSystem::RestoreCarried(std::span<const std::shared_ptr<particles::CarriedObject>> carried)
{
	// A thing that a particle carries holds the same record as the particle again
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto& item : carried)
	{
		if (auto* holder = item != nullptr && registry.Valid(item->object)
		                       ? registry.TryGet<ecs::components::CarriedByTornado>(item->object)
		                       : nullptr)
		{
			holder->carried = item;
		}
	}
}

bool ParticleRuleSystem::LoadSyncedBody(reflection::BinaryReader& in)
{
	particles::SharedTables tables;
	particles::EffectReader reader(in, tables);
	particles::LoadSharedTables(reader);
	_effects.clear();
	if (_lifeListener != nullptr)
	{
		_lifeListener->EffectsCleared();
	}
	const auto count = in.ReadSize();
	for (size_t i = 0; count.has_value() && i < *count && !in.Failed(); ++i)
	{
		Running running;
		const auto result = ReadRunning(in, reader, running);
		if (result == ReadResult::Failed)
		{
			return false;
		}
		if (result == ReadResult::Read)
		{
			_effects.push_back(std::move(running));
		}
	}
	if (in.Failed() || !_world.LoadState(reader))
	{
		return false;
	}
	RestoreCarried(tables.carried.items);
	_loadedSynced = LoadedSyncedThings {
	    .sounds = WeakItems(tables.sounds),
	    .bolts = WeakItems(tables.bolts),
	    .shields = WeakItems(tables.shields),
	    .sheets = WeakItems(tables.sheets),
	    .carried = WeakItems(tables.carried),
	    .trails = WeakItems(tables.trails),
	};
	return !in.Failed();
}

bool ParticleRuleSystem::LoadState(reflection::BinaryReader& in)
{
	if (!Locator::gameRandom::has_value() || !Locator::resources::has_value())
	{
		return false;
	}
	uint32_t version = 0;
	reflection::Load(in, version);
	if (in.Failed() || version != k_SyncedPartVersion)
	{
		return false;
	}
	reflection::Load(in, _paused);
	reflection::Load(in, _explodeObject);
	reflection::Load(in, _stormBolts);
	reflection::Load(in, _pendingStormBolts);
	reflection::Load(in, _gestureTrails);
	reflection::Load(in, _beliefWanted);
	// The effects of this computer come back with their own part of the save. Until then, this computer has none.
	_gestureChain = k_NoEffect;
	_selectionEffect = k_NoEffect;
	_handManaPath = k_NoEffect;
	_chainScrollPace = {};
	if (!LoadSyncedBody(in))
	{
		return false;
	}
	// The next number of an effect is after the numbers of the loaded effects. The part of this computer gives the
	// number that the save kept.
	_nextId = 1;
	for (const auto& running : _effects)
	{
		if (running.id < k_FirstEffectHandle && running.id >= _nextId)
		{
			_nextId = running.id + 1;
		}
	}
	return true;
}

bool ParticleRuleSystem::LoadLocalState(reflection::BinaryReader& in)
{
	if (!Locator::gameRandom::has_value() || !Locator::resources::has_value())
	{
		return false;
	}
	uint32_t version = 0;
	reflection::Load(in, version);
	if (in.Failed() || version != k_LocalPartVersion)
	{
		return false;
	}
	reflection::Load(in, _nextId);
	reflection::Load(in, _gestureChain);
	reflection::Load(in, _selectionEffect);
	reflection::Load(in, _handManaPath);
	reflection::Load(in, _chainScrollPace);

	// The things of this computer have the numbers after the numbers of the synced part
	particles::SharedTables tables;
	if (_loadedSynced.has_value())
	{
		RestoreItems(tables.sounds, _loadedSynced->sounds);
		RestoreItems(tables.bolts, _loadedSynced->bolts);
		RestoreItems(tables.shields, _loadedSynced->shields);
		RestoreItems(tables.sheets, _loadedSynced->sheets);
		RestoreItems(tables.carried, _loadedSynced->carried);
		RestoreItems(tables.trails, _loadedSynced->trails);
		_loadedSynced.reset();
	}
	tables.StartLocal();
	particles::EffectReader reader(in, tables);
	particles::LoadSharedTables(reader);
	const auto count = in.ReadSize();
	for (size_t i = 0; count.has_value() && i < *count && !in.Failed(); ++i)
	{
		uint32_t place = 0;
		reflection::Load(in, place);
		Running running;
		const auto result = ReadRunning(in, reader, running);
		if (result == ReadResult::Failed)
		{
			return false;
		}
		if (result == ReadResult::Read)
		{
			const auto at = std::min(static_cast<size_t>(place), _effects.size());
			_effects.insert(_effects.begin() + static_cast<std::ptrdiff_t>(at), std::move(running));
		}
	}
	if (in.Failed() || !_world.LoadLocalState(reader))
	{
		return false;
	}
	// The links of the synced bolts to the bolts of this computer
	const auto links = in.ReadSize();
	for (size_t i = 0; links.has_value() && i < *links && !in.Failed(); ++i)
	{
		std::array<uint32_t, 3> link {};
		for (auto& number : link)
		{
			reflection::Load(in, number);
		}
		if (const auto bolt = tables.bolts.At(link[0]); bolt != nullptr)
		{
			if (link[1] != 0)
			{
				bolt->linkedTo = tables.bolts.At(link[1]);
			}
			if (link[2] != 0)
			{
				bolt->linkedFrom = tables.bolts.At(link[2]);
			}
		}
	}
	const auto firstCarried = std::min(tables.carried.firstLocal.value_or(0), tables.carried.items.size());
	RestoreCarried(std::span(tables.carried.items).subspan(firstCarried));
	return !in.Failed();
}

bool ParticleRuleSystem::LoadOldState(reflection::BinaryReader& in)
{
	if (!Locator::gameRandom::has_value() || !Locator::resources::has_value())
	{
		return false;
	}
	// A save from before the split keeps the synced effects and the effects of this computer in one part, in the order
	// of the effects
	reflection::Load(in, _nextId);
	reflection::Load(in, _paused);
	reflection::Load(in, _explodeObject);
	reflection::Load(in, _stormBolts);
	reflection::Load(in, _pendingStormBolts);
	reflection::Load(in, _gestureTrails);
	reflection::Load(in, _gestureChain);
	reflection::Load(in, _selectionEffect);
	reflection::Load(in, _handManaPath);
	reflection::Load(in, _beliefWanted);
	reflection::Load(in, _chainScrollPace);
	const bool read = LoadSyncedBody(in);
	_loadedSynced.reset();
	return read;
}
