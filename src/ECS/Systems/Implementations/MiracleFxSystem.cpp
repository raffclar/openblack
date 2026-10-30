/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "MiracleFxSystem.h"

#include <cmath>

#include <algorithm>
#include <chrono>
#include <numbers>
#include <vector>

#include <PackFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "Common/PortableRotations.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandMiracleFx.h"
#include "ECS/Components/MagicPile.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ResourcePile.h"
#include "ECS/Components/Transform.h"
#include "ECS/DrawnPlace.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/DispenserRules.h"
#include "Magic/MagicTables.h"
#include "Magic/MiracleVisuals.h"
#include "Particles/DrawnEffectSteps.h"
#include "Particles/ParticleEffect.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/SoundId.h"
#include "Sound/SoundRequestInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace visuals = openblack::magic::visuals;

namespace
{
constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;

/// How a creature takes the spell of a phial, which decides its pulse
int ReceiveTypeOf(const InfoConstants& info, SpellSeedType seed)
{
	const auto type = magic::GetSpellSeedInfo(info, seed).magicTypes[0];
	const auto* spell = magic::GetMagicInfoAs<GMagicCreatureSpellInfo>(info, type);
	return spell != nullptr ? static_cast<int>(spell->creatureReceiveSpellType) : 0;
}

/// Whether a globe shows on the screen this frame, as it is tested when it is drawn: its sphere, pulled towards the
/// camera by its radius as it is drawn, must show on the screen. Its middle is the middle of its model's box.
bool GlobeOnScreen(glm::vec3 origin, float scale, glm::vec3 middle, const graphics::L3DMesh& bubble, glm::vec3 eye)
{
	if (!Locator::objectDrawListSystem::has_value())
	{
		return true;
	}
	const auto& view = Locator::objectDrawListSystem::value().GetView();
	if (!view.has_value())
	{
		return true;
	}
	const auto halfExtent = bubble.GetBoundingBox().Size() * 0.5f;
	const float pull = std::max(halfExtent.x, halfExtent.z) * scale;
	const auto towards = eye - middle;
	const auto push = glm::length(towards) > 0.0f ? glm::normalize(towards) * pull : glm::vec3(0.0f);
	return object_draw_list::SphereOnScreen(*view, origin + push, middle + push, glm::length(halfExtent) * scale);
}

HandMiracleFx* HandFx()
{
	// The hand of the player at this computer
	if (!Locator::handSystem::has_value())
	{
		return nullptr;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto hand = Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
	if (!registry.Valid(hand) || !registry.AllOf<Hand>(hand))
	{
		return nullptr;
	}
	// The hand gets its effects when it is made or loaded, not in a frame
	return registry.TryGet<HandMiracleFx>(hand);
}

void PlaySound(entt::id_type id, std::optional<pack::AudioBankLoop> voicePlayType = std::nullopt)
{
	if (Locator::sounds::has_value())
	{
		audio::StartGameSoundEffect(id, {.voicePlayType = voicePlayType});
	}
}
} // namespace

entt::id_type MiracleFxSystem::LoadMesh(entt::id_type id, std::string_view file)
{
	if (!Locator::resources::has_value())
	{
		return id;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (meshes.Contains(id) || !Locator::filesystem::has_value())
	{
		return id;
	}
	auto& fileSystem = Locator::filesystem::value();
	try
	{
		meshes.Load(id, resources::L3DLoader::FromDiskTag {},
		            fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / file));
	}
	catch (const std::exception& error)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Miracles: cannot load {}: {}", file, error.what());
	}
	return id;
}

void MiracleFxSystem::LoadMeshes()
{
	LoadMesh(OneOffSpellSeed::k_MeshId.value(), OneOffSpellSeed::k_MeshFile);
	LoadMesh(OneOffSpellSeed::k_RingMeshId.value(), OneOffSpellSeed::k_RingMeshFile);
}

entt::id_type MiracleFxSystem::BubbleMesh()
{
	return LoadMesh(OneOffSpellSeed::k_MeshId.value(), OneOffSpellSeed::k_MeshFile);
}

entt::id_type MiracleFxSystem::BandMesh()
{
	return LoadMesh(OneOffSpellSeed::k_RingMeshId.value(), OneOffSpellSeed::k_RingMeshFile);
}

void MiracleFxSystem::Update(float /*seconds*/, float gameSeconds)
{
	if (!Locator::infoConstants::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	// The hand's bands and glow keep to the game's time, stopping while it is paused
	UpdateHand(gameSeconds);
	UpdatePiles(gameSeconds);
}

void MiracleFxSystem::UpdatePiles(float gameSeconds)
{
	if (!Locator::resources::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& info = Locator::infoConstants::value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	registry.Each<ResourcePile, const Pot, const Mesh, Transform>(
	    [&](ResourcePile& pile, const Pot& pot, const Mesh& mesh, Transform& transform) {
		    if (pile.height <= 0.0f)
		    {
			    if (!meshes.Contains(mesh.id))
			    {
				    return;
			    }
			    pile.height = meshes.Handle(mesh.id)->GetBoundingBox().Size().y * transform.scale.y;
		    }
		    const auto& potInfo = info.pot.at(static_cast<size_t>(pot.type));
		    const auto target = magic::piles::SunkOffset(
		        magic::piles::ProportionRaised(potInfo.resourceType, pot.amount, potInfo.maxAmountInPot), pile.height);
		    if (!pile.risen)
		    {
			    pile.rise = magic::piles::SunkRise(pile.height);
			    magic::piles::RiseTo(pile.rise, target);
			    pile.risen = true;
			    pile.shownAmount = pot.amount;
		    }
		    else if (pot.amount != pile.shownAmount)
		    {
			    magic::piles::RiseTo(pile.rise, target);
			    pile.shownAmount = pot.amount;
		    }
		    magic::piles::StepRise(pile.rise, gameSeconds);
	    });
}

void MiracleFxSystem::UpdateDrawnGlobes(ObjectDrawListSystemInterface& drawList, std::chrono::milliseconds frameGameTime)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto bubble = OneOffSpellSeed::k_MeshId.value();
	const bool haveBubble = meshes.Contains(bubble);
	const auto eye = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : glm::vec3(0.0f);
	const float seconds = std::chrono::duration<float>(std::chrono::duration<float, std::milli>(frameGameTime)).count();
	const bool fullFrame = drawList.GetFrame().drawAll;
	registry.Each<OneOffSpellSeed, const Transform>(
	    [&](entt::entity entity, OneOffSpellSeed& globe, const Transform& transform) {
		    // A globe that the list does not draw does not run its glint on
		    if (!drawList.Drawn(registry, entity))
		    {
			    return;
		    }
		    globe.glintFrame = visuals::StepGlint(globe.glintFrame, seconds);
		    // The draw of a globe tests its sphere, pulled towards the camera by its radius, and no fade
		    if (fullFrame && drawList.Listed(entity))
		    {
			    const auto origin = globe.drawnModel.has_value() ? glm::vec3((*globe.drawnModel)[3]) : transform.position;
			    drawList.NoteFullFrame(
			        entity, !haveBubble || GlobeOnScreen(origin, transform.scale.x, globe.middle, *meshes.Handle(bubble), eye));
		    }
	    });
}

void MiracleFxSystem::PlacePilesOnLand()
{
	if (!Locator::terrainSystem::has_value())
	{
		return;
	}
	// A miracle's pile has no height of its own: it stands on the land where it is, as the land is now
	const auto& terrain = Locator::terrainSystem::value();
	Locator::entitiesRegistry::value().Each<const MagicPile, Transform>([&terrain](const MagicPile&, Transform& transform) {
		transform.position.y = terrain.GetHeightAt({transform.position.x, transform.position.z});
	});
}

void MiracleFxSystem::UpdateGlobes(std::chrono::milliseconds frameRealTime, std::chrono::milliseconds frameGameTime,
                                   bool landDrawn)
{
	const float seconds = std::chrono::duration<float>(std::chrono::duration<float, std::milli>(frameGameTime)).count();
	// The effects in the globes step at the pace of the screen's refresh
	const auto holderStep = particles::drawn_steps::StepSeconds(_holderPace, frameRealTime, frameGameTime, landDrawn);
	if (!Locator::infoConstants::has_value() || !Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& info = Locator::infoConstants::value();
	auto& meshes = Locator::resources::value().GetMeshes();
	LoadMeshes();
	const auto bubble = OneOffSpellSeed::k_MeshId.value();
	const glm::vec3 centre = meshes.Contains(bubble) ? meshes.Handle(bubble)->GetBoundingBox().Center() : glm::vec3(0.0f);
	const auto eye = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : glm::vec3(0.0f);
	auto* particles = Locator::particleRuleSystem::has_value() ? &Locator::particleRuleSystem::value() : nullptr;

	std::vector<entt::entity> globes;
	registry.Each<OneOffSpellSeed, const Transform>(
	    [&](entt::entity entity, OneOffSpellSeed& globe, const Transform& transform) {
		    globes.push_back(entity);
		    globe.spin = std::fmod(globe.spin + magic::k_OrbSeedSpin * seconds, k_TwoPi);
		    globe.ringSpin = std::fmod(globe.ringSpin + visuals::k_RingSpin * seconds, k_TwoPi);
		    // The dome's top faces the camera, turning about the middle of the globe where it is now, so that a globe thrown
		    // about turns where it flies. Only the draw turns it: the world keeps the globe where the turns put it, so the
		    // camera does not change the world.
		    // A globe that this computer threw keeps the turn of its flight and does not turn to the camera
		    glm::mat3 facing(1.0f);
		    glm::vec3 drawnOrigin(0.0f);
		    if (globe.flightModel.has_value())
		    {
			    facing = glm::mat3(*globe.flightModel);
			    drawnOrigin = glm::vec3((*globe.flightModel)[3]);
			    globe.middle = drawnOrigin + facing * (centre * transform.scale);
			    globe.flightModel.reset();
		    }
		    else
		    {
			    // A globe in a hand is drawn where the frame draws the hand, not where the turn put it
			    const auto placed = ecs::drawn_place::DrawnTransformOf(registry, entity, transform);
			    globe.middle = placed.position + placed.rotation * (centre * transform.scale);
			    facing = visuals::FacingCamera(globe.middle, eye, visuals::Facing::Globe);
			    drawnOrigin = globe.middle - facing * (centre * transform.scale);
		    }
		    glm::mat4 drawnModel(facing * glm::mat3(glm::scale(glm::mat4(1.0f), transform.scale)));
		    drawnModel[3] = glm::vec4(drawnOrigin, 1.0f);
		    globe.drawnModel = drawnModel;
		    globe.drawnTurn = facing;
		    globe.position = globe.middle - centre * transform.scale;
		    const auto& seed = magic::GetSpellSeedInfo(info, globe.seedType);
		    if (visuals::IsPhialSeed(globe.seedType))
		    {
			    const int receiveType = ReceiveTypeOf(info, globe.seedType);
			    globe.phialFrame = visuals::StepPhialFrame(globe.phialFrame, seconds);
			    globe.phialPhase = std::fmod(globe.phialPhase + visuals::PhialPulseSpeed(receiveType) * seconds, 1.0f);
		    }
		    // The miracle's own effect in the globe steps as the globe is drawn, before the seed's model is placed for this
		    // frame, so its glints sparkle on where the model was last drawn
		    // It steps only while the globe shows on the screen, in the frames where its step is due
		    if (const auto stepped = _holderEffects.find(entity);
		        holderStep.has_value() && stepped != _holderEffects.end() && particles != nullptr &&
		        (!meshes.Contains(bubble) ||
		         GlobeOnScreen(drawnOrigin, transform.scale.x, globe.middle, *meshes.Handle(bubble), eye)))
		    {
			    particles->ProcessByFrame(stepped->second, *holderStep);
		    }
		    // The seed's model as it is drawn, which glints of its effect sparkle on from the next step: below the middle,
		    // spun, at the seed's scale and a phial's swell or squash, a big one's growing copy last
		    {
			    const float drawn = transform.scale.x * visuals::k_GlobeSeedScale;
			    float size = 1.0f;
			    glm::vec3 shape(1.0f);
			    if (visuals::IsPhialSeed(globe.seedType))
			    {
				    const int receiveType = ReceiveTypeOf(info, globe.seedType);
				    size = visuals::PhialDraws(receiveType, globe.phialPhase, visuals::PhialPulse(globe.phialPhase),
				                               visuals::k_GlobeAlpha)
				               .back()
				               .size;
				    shape = visuals::PhialScale(receiveType, globe.phialPhase);
			    }
			    const auto at = globe.middle + glm::vec3(0.0f, seed.meshHeight * drawn, 0.0f);
			    globe.seedPlacement = glm::translate(glm::mat4(1.0f), at) * portable_math::EulerAngleY(globe.spin) *
			                          glm::scale(glm::mat4(1.0f), seed.scale * drawn * size * shape);
		    }
		    // The miracle's own effect plays in the globe, faint as the globe is
		    if (particles == nullptr || seed.holderParticle == ParticleType::None)
		    {
			    return;
		    }
		    const float scale = transform.scale.x * visuals::k_GlobeSeedScale;
		    const auto point = globe.middle + glm::vec3(0.0f, seed.holderHeight * scale, 0.0f);
		    auto found = _holderEffects.find(entity);
		    if (found == _holderEffects.end())
		    {
			    const auto effect = particles->Start(seed.holderParticle, point, scale);
			    if (effect == ParticleRuleSystemInterface::k_NoEffect)
			    {
				    return;
			    }
			    // What it acts on is the seed's model, for the glints a frozen creature's phial shows
			    particles->AddTarget(effect, entity);
			    if (auto* running = particles->Find(effect))
			    {
				    // Nobody owns a globe: its effect takes the neutral player's colour
				    running->SetPlayer(static_cast<int>(PlayerNames::NEUTRAL));
				    running->SetGlobalAlpha(static_cast<float>(visuals::k_GlobeAlpha));
			    }
			    found = _holderEffects.emplace(entity, effect).first;
		    }
		    particles->SetOrigin(found->second, point);
	    });
	// The effects of the globes that have gone go with them
	std::erase_if(_holderEffects, [&](const auto& entry) {
		if (std::ranges::find(globes, entry.first) != globes.end())
		{
			return false;
		}
		if (particles != nullptr)
		{
			particles->Delete(entry.second);
		}
		return true;
	});
}

void MiracleFxSystem::UpdateHand(float seconds)
{
	auto* fx = HandFx();
	if (fx == nullptr)
	{
		return;
	}
	fx->glowing = Locator::magicSystem::has_value() && Locator::magicSystem::value().GetHeldSeed().has_value();
	fx->glowFrame = visuals::StepGlowFrame(fx->glowFrame, seconds);
	visuals::StepBands(fx->bands, seconds);
}

void MiracleFxSystem::SeedInHand(int powerUp, int previousPowerUp)
{
	if (auto* fx = HandFx())
	{
		// A bracelet for each power-up, put on once the miracle has settled
		visuals::SetBracelets(fx->bands, powerUp + 1, true);
		if (previousPowerUp <= powerUp)
		{
			visuals::AddFlyIn(fx->bands, false);
		}
	}
	if (previousPowerUp <= powerUp)
	{
		PlaySound(static_cast<entt::id_type>(audio::SoundId::G_SpellPowerUpBand));
	}
	if (const auto voice = visuals::PowerUpVoiceSample(powerUp))
	{
		// The voice does not start again while it speaks
		PlaySound(entt::hashed_string(fmt::format("SpellDialogue.sad/{}", *voice).c_str()).value(), pack::AudioBankLoop::Once);
	}
}

void MiracleFxSystem::SeedLeftHand()
{
	if (auto* fx = HandFx())
	{
		visuals::SetBracelets(fx->bands, 0, false);
	}
}

void MiracleFxSystem::SeedShakenOff()
{
	if (auto* fx = HandFx())
	{
		visuals::AddFlyOff(fx->bands);
	}
}

void MiracleFxSystem::HandShaken()
{
	// The shake sound plays without a position, at its own volume
	if (Locator::sounds::has_value())
	{
		constexpr uint32_t k_ShakeVolume = 35;
		audio::StartGameSoundEffect(static_cast<entt::id_type>(audio::SoundId::G_ShakeHand_01), {.volume = k_ShakeVolume});
	}
	SeedShakenOff();
}

void MiracleFxSystem::Reset()
{
	_runners.clear();
	_ring = nullptr;
	if (Locator::particleRuleSystem::has_value())
	{
		for (const auto& [globe, effect] : _holderEffects)
		{
			Locator::particleRuleSystem::value().Delete(effect);
		}
	}
	_holderEffects.clear();
	if (Locator::entitiesRegistry::has_value())
	{
		if (auto* fx = HandFx())
		{
			*fx = {};
		}
	}
}

void MiracleFxSystem::SaveState(reflection::BinaryWriter& out) const
{
	reflection::SavePresentation(out, _holderEffects);
	// The tribes' names that spin in the world, and which of them rings the hand
	out.WriteSize(_runners.size());
	for (const auto& runner : _runners)
	{
		reflection::Save(out, *runner);
		reflection::Save(out, runner.get() == _ring);
	}
}

bool MiracleFxSystem::LoadState(reflection::BinaryReader& in)
{
	reflection::Load(in, _holderEffects);
	_runners.clear();
	_ring = nullptr;
	const auto count = in.ReadSize();
	for (size_t i = 0; count.has_value() && i < *count && !in.Failed(); ++i)
	{
		auto runner = std::make_unique<magic::tribal_spin::Runner>(u"", glm::vec3(0.0f), glm::u8vec4(0), false);
		reflection::Load(in, *runner);
		bool ring = false;
		reflection::Load(in, ring);
		if (ring)
		{
			_ring = runner.get();
		}
		_runners.push_back(std::move(runner));
	}
	return !in.Failed();
}
