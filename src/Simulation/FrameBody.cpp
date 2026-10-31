/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FrameBody.h"

#include <chrono>

#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "ECS/Archetypes/VillageLightArchetype.h"
#include "ECS/Systems/AdvisorVoiceSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/AppleBarrelSystemInterface.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/ChatSystemInterface.h"
#include "ECS/Systems/ChimneySmokeSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CloudSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureHairSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/FieldSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FireflySystemInterface.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/LiquidParticleSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MistSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/RainSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/SharkSystemInterface.h"
#include "ECS/Systems/SnowfallSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TipBubbleSystemInterface.h"
#include "ECS/Systems/TornadoSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/VegetationInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "ECS/Systems/VillageLightSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "ECS/Systems/WorkshopSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "Locator.h"
#include "Particles/LiquidParticles.h"
#include "Profiler.h"

namespace openblack::simulation
{

bool LandDrawn()
{
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	const bool landHidden = Locator::videoSystem::has_value() &&
	                        (Locator::videoSystem::value().CoversScreen() || Locator::videoSystem::value().HidesWorld());
	return !inTemple && !landHidden;
}

void PlayCreaturePoses()
{
	auto& profiler = Locator::profiler::value();
	auto& clock = Locator::time::value();
	const auto gameTime = std::chrono::duration<float, std::milli>(clock.GetFrameGameTime());
	// The static that the eyes of all the creatures fizz through continues to slide as the draw starts
	Locator::creatureFizzSystem::value().UpdateFrame(std::chrono::duration<float>(gameTime).count());
	{
		// The fight's camera follows the fighters. The fights themselves move on only in the game turns.
		auto creatureCombat = profiler.BeginScoped(Profiler::Stage::CreatureCombatUpdate);
		Locator::creatureFightSystem::value().Update(gameTime);
	}
	{
		// The walking creatures are drawn between the places of their last two turns. The frame changes only their drawn
		// places.
		auto creatureLocomotion = profiler.BeginScoped(Profiler::Stage::CreatureLocomotionUpdate);
		Locator::creatureLocomotionSystem::value().Update(clock.GetTurnFraction());
	}
	{
		// The bodies are drawn between the poses of their last two turns, and their eyes blink and look about
		auto creatureAnimation = profiler.BeginScoped(Profiler::Stage::CreatureAnimationUpdate);
		Locator::creatureAnimationSystem::value().Update(gameTime);
	}
	{
		// Their hair swings from the posed bodies
		auto creatureHair = profiler.BeginScoped(Profiler::Stage::CreatureHairUpdate);
		Locator::creatureHairSystem::value().Update(gameTime);
	}
	{
		// They make the sounds of the moments their animations have played past
		auto creatureAudio = profiler.BeginScoped(Profiler::Stage::CreatureAudioUpdate);
		Locator::creatureAudioSystem::value().Update(gameTime);
	}
}

void PlayFrameEffects(FrameViewInterface& view)
{
	auto& profiler = Locator::profiler::value();
	auto& camera = Locator::camera::value();
	auto& clock = Locator::time::value();
	const auto gameTime = std::chrono::duration<float, std::milli>(clock.GetFrameGameTime());
	Locator::alignmentSystem::value().Update(gameTime);
	{
		auto actions = profiler.BeginScoped(Profiler::Stage::VegetationUpdate);
		Locator::vegetation::value().Update(gameTime);
	}
	// The clouds drift with the wind, then every mist and cloud animates. Only a computer that draws the world moves them.
	if (Locator::cloudSystem::has_value())
	{
		Locator::cloudSystem::value().Update(gameTime);
		// The storms' own clouds gather and wander over them with the frames, at the screen refresh's pace
		Locator::cloudSystem::value().UpdateStormClouds(clock.GetFrameRealTime());
	}
	// The rain falls as the storm nearest the camera has it
	Locator::rainSystem::value().Update(std::chrono::duration<float>(gameTime).count(), clock.GetFrameRealTime(),
	                                    camera.GetOrigin());
	// The rings on the water grow and fade
	Locator::waterRingSystem::value().Update(clock.GetFrameRealTime(), clock.GetFrameGameTime());
	// The ribbons' textures slide on the frames that draw them
	if (Locator::particleRuleSystem::has_value())
	{
		Locator::particleRuleSystem::value().ScrollChains(clock.GetFrameRealTime(), clock.GetFrameGameTime());
	}
	{
		// The flames, steam and smoke of what burns move on
		auto fire = profiler.BeginScoped(Profiler::Stage::FireUpdate);
		Locator::fireSystem::value().Update(clock.GetFrameRealTime(), clock.GetFrameGameTime());
	}
	// The vortices' swirls and effects step with the frame, and their marks on the ground follow their openness
	if (Locator::vortexSystem::has_value())
	{
		Locator::vortexSystem::value().UpdateFrame(clock.GetFrameRealTime(), clock.GetFrameGameTime(), LandDrawn());
	}
	// The frame draws the reward chests from the sky between their turns, and their dust fades
	if (Locator::rewardSystem::has_value())
	{
		Locator::rewardSystem::value().Update(std::chrono::duration<float, std::milli>(gameTime).count(),
		                                      clock.GetTurnFraction());
	}
	// The fireflies out drift about where they are between their last two turns
	if (Locator::fireflySystem::has_value())
	{
		Locator::fireflySystem::value().Update(std::chrono::duration<float, std::milli>(gameTime).count(),
		                                       clock.GetTurnFraction());
	}
	// The moving bodies are drawn between their last two turns
	if (Locator::dynamicsSystem::has_value())
	{
		Locator::dynamicsSystem::value().UpdateFrame(clock.GetTurnFraction());
	}
	// The drops, the dust, the smoke and the motes move on and fade by the frame's game time, and in the temple, where the
	// world stops, by its real time
	if (Locator::liquidParticleSystem::has_value())
	{
		const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
		const auto milliseconds = inTemple ? clock.GetFrameRealTime() : clock.GetFrameGameTime();
		Locator::liquidParticleSystem::value().UpdateFrame(liquid_particles::FrameSeconds(milliseconds.count()), true);
	}
	{
		// The sparkles that mark the player's queued fight moves. The hand goes to a sparkle in the next frame.
		auto creatureCombat = profiler.BeginScoped(Profiler::Stage::CreatureCombatUpdate);
		Locator::creatureFightSystem::value().DrawMarks(
		    std::chrono::duration<float, std::milli>(clock.GetFrameRealTime()).count(), camera.GetOrigin());
	}
	{
		// What the creatures hold is drawn in their drawn hands. The turns place it in the world.
		auto creatureObjectActions = profiler.BeginScoped(Profiler::Stage::CreatureObjectActionUpdate);
		Locator::creatureObjectActionSystem::value().UpdateHeld();
	}
	{
		// The animals are drawn between their last two turns, their models posed by their clips
		auto animals = profiler.BeginScoped(Profiler::Stage::AnimalsUpdate);
		Locator::animalSystem::value().Update(clock.GetFrameGameTime(), clock.GetTurnFraction());
		// The clips the villagers' states play go on, and the sounds of their frames play
		Locator::livingActionSystem::value().UpdatePoses(clock.GetFrameGameTime(), clock.GetTurnFraction());
		// The sharks swim between their last two turns and leave their wakes
		Locator::sharkSystem::value().Update(gameTime, clock.GetTurnFraction());
		// The opening's light falls and its hand lifts the boy and sets him down, by the frame's game time
		Locator::introSystem::value().Update(static_cast<uint32_t>(clock.GetFrameGameTime().count()));
		// The explorers' boat is pushed into the sea and sails away, by the frame's game time
		Locator::boatLaunchSystem::value().Update(static_cast<uint32_t>(clock.GetFrameGameTime().count()));
		// The apple barrel goes to the local player's storage pit nearest their temple and its worm wriggles on
		Locator::appleBarrelSystem::value().Update(clock.GetFrameGameTime());
	}
	{
		// Their footprints fade away, only on a frame that draws the land
		auto creatureFootprints = profiler.BeginScoped(Profiler::Stage::CreatureFootprintsUpdate);
		const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
		const bool landHidden = Locator::videoSystem::has_value() &&
		                        (Locator::videoSystem::value().CoversScreen() || Locator::videoSystem::value().HidesWorld());
		Locator::footprintSystem::value().Update(gameTime, !inTemple && !landHidden);
		// The piles of roots fade and go only on a frame that draws the land, before the draw
		if (!inTemple && !landHidden)
		{
			Locator::explosionSystem::value().AgeRootsPiles(clock.GetFrameGameTime());
		}
	}
	{
		// Their skins are painted again where their alignment, tattoos or marks have changed
		auto creatureSkin = profiler.BeginScoped(Profiler::Stage::CreatureSkinUpdate);
		Locator::creatureSkinSystem::value().Update();
	}
	{
		// What a tornado carries is drawn where it whirls, between the last two turns
		auto tornado = profiler.BeginScoped(Profiler::Stage::TornadoUpdate);
		Locator::tornadoSystem::value().Update(clock.GetTurnFraction());
	}
	// The snow falls as the rain does
	Locator::snowfallSystem::value().Update(std::chrono::duration<float>(gameTime).count(),
	                                        Locator::rainSystem::value().GetFall());
	// The homes' smoke rises while someone is in
	Locator::chimneySmokeSystem::value().Update(clock.GetFrameRealTime(), clock.GetFrameGameTime());
	Locator::influenceSystem::value().Update(gameTime);
	// What the hand shows past the border is shown with the hand
	if (view.IsHandDrawn())
	{
		Locator::influenceSystem::value().ShowHandInfluence(gameTime);
	}
	if (Locator::mistSystem::has_value())
	{
		Locator::mistSystem::value().Update(gameTime);
	}
	if (Locator::cloudSystem::has_value())
	{
		Locator::cloudSystem::value().AnimateStormClouds(gameTime);
	}
	// The lights of owners that have gone go on every computer, then the lights flicker where the world is drawn
	ecs::archetypes::VillageLightArchetype::DestroyOwnerless();
	if (Locator::villageLightSystem::has_value())
	{
		Locator::villageLightSystem::value().Update(gameTime);
	}
	// The disciples glow, and the one in or under the hand shows the symbol of its work
	ecs::villager_disciple::UpdateMarks();
	// The town totems move to their shares by the real clock, also while the game is paused
	Locator::villageTotemSystem::value().Update(static_cast<uint32_t>(clock.GetElapsedTime().count()));
	// The owners' symbols float over their worthiest artefacts
	Locator::artefactSystem::value().UpdateFrame(static_cast<float>(gameTime.count()));
	// The flags over the towns' stores ease to their desires and turn to the camera
	Locator::townDesireSystem::value().UpdateFlags(std::chrono::duration<float>(gameTime).count());
	// The wood flags of the workshops ease to the wood that their workshops need and turn to the camera
	if (Locator::workshopSystem::has_value())
	{
		Locator::workshopSystem::value().UpdateFlagsForFrame(std::chrono::duration<float>(gameTime).count());
	}
	// The worship sites' need flags ease to their needs, the rings round their dances show how full they are, and the
	// straining sites throb
	if (Locator::worshipSiteSystem::has_value())
	{
		Locator::worshipSiteSystem::value().UpdateFrame(clock.GetFrameGameTime(), clock.GetFrameRealTime(), LandDrawn());
	}
	// The icons at the worship sites show their charge: a ring of light, a shine and, while they charge, a pulse
	if (Locator::spellIconSystem::has_value())
	{
		Locator::spellIconSystem::value().UpdateChargeLooks(static_cast<int32_t>(clock.GetFrameGameTime().count()),
		                                                    LandDrawn());
		// The seeds over the stone hands of the village centres show the effects of their holders
		Locator::spellIconSystem::value().UpdateHolderEffects(clock.GetFrameRealTime(), clock.GetFrameGameTime(), LandDrawn());
	}
	// The players' symbols of belief over the village centres move on
	Locator::townSystem::value().UpdateFrame(clock.GetFrameRealTime(), LandDrawn());
	Locator::fieldSystem::value().Update(gameTime);
	// The shoals near the camera swim, and dart from what scared them
	Locator::fishFarmSystem::value().Update(std::chrono::duration<float>(gameTime).count(),
	                                        Locator::camera::value().GetOrigin());
	Locator::cinematicDirectorSystem::value().Update(gameTime);
	// The scripts' words on the screen fade by the game's time, and stand still while a video plays
	if (!Locator::videoSystem::has_value() || !Locator::videoSystem::value().IsPlaying())
	{
		Locator::cinematicDirectorSystem::value().UpdateTexts(static_cast<int32_t>(clock.GetFrameGameTime().count()));
	}
	// The advisors move and act once a frame, by the real time inside the temple and the game's otherwise. They act on
	// the screen, so a world with no view has none.
	view.UpdateAdvisors();
	// The advisors' voices that this computer plays: a line whose delay is over starts
	if (Locator::advisorVoiceSystem::has_value())
	{
		Locator::advisorVoiceSystem::value().Update();
	}
	// The scripts' dialogue: the newest text slides in. The player's click goes to the turn as a command.
	{
		const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
		Locator::helpTextSystem::value().Update({
		    .gameMs = static_cast<uint32_t>(clock.GetFrameGameTime().count()),
		    .realMs = static_cast<uint32_t>(clock.GetFrameRealTime().count()),
		    .inTemple = inTemple,
		});
		// While a script plays a sung verse, the newest text's word that the song has reached gets the glow
		view.UpdateSingAlong(inTemple);
	}
	// The gestures' hints fade while the advisors' words were drawn, and while shown their dots run and their collars
	// turn, by the game's time
	if (Locator::gestureSystem::has_value())
	{
		const auto seconds = std::chrono::duration<float>(clock.GetFrameGameTime()).count();
		Locator::gestureSystem::value().StepHints(seconds, view.DialogueWordsDrawn());
	}
	// The tip bubble's time to show runs down by the game's time while it is up
	Locator::tipBubbleSystem::value().UpdateFrame(static_cast<float>(clock.GetFrameGameTime().count()));
	// The speech bubbles of the creatures in the chat run down by the game's time
	if (Locator::chatSystem::has_value())
	{
		Locator::chatSystem::value().UpdateFrame(static_cast<float>(clock.GetFrameGameTime().count()));
	}
	// The names display grows in and out by the game's time
	view.UpdateVillagerLabels(static_cast<float>(clock.GetFrameGameTime().count()));
	// The cinema bars coming in hide the game's dialogs
	if (Locator::cinematicDirectorSystem::value().TakeHideDialogs())
	{
		view.CloseMenuForCinema();
	}
}

} // namespace openblack::simulation
