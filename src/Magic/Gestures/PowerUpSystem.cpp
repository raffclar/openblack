/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PowerUpSystem.h"

#include <cstdio>
#include <cstdlib>

#include <spdlog/spdlog.h>

#include "Debug/DebugEnv.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandMagicStateInterface.h"
#include "GestureInput.h"
#include "Help/HelpProfile.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/MagicTables.h"
#include "Particles/Utility.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::magic::gestures;

namespace
{
/// The pause after any recognition or cancel
constexpr float k_Cooldown = 0.4f;
/// the circle is forgotten 5 s after it was drawn
constexpr float k_CircleLife = 5.0f;

/// The gesture state machine, the icon provider worship sets on every land, and the hand as the gesture code sees it
/// (gestures::Reset clears only the first)
struct PowerUpState
{
	InterfaceGestures gestures;
	std::unique_ptr<IconProvider> icons;
	HandStatus hand;
};

/// This module's state (Locator::handMagicState)
PowerUpState& PowerUp()
{
	if (!Locator::handMagicState::has_value())
	{
		std::fputs("magic::gestures: no hand magic state in the locator (Locator::handMagicState)\n", stderr);
		std::abort();
	}
	return Locator::handMagicState::value().Get<PowerUpState>();
}

bool Trace()
{
	static const bool trace = debug_env::GestureTrace();
	return trace;
}

const ecs::components::SpellSeed* HeldSeed()
{
	if (PowerUp().hand.heldSeed == entt::null || !Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(PowerUp().hand.heldSeed))
	{
		return nullptr;
	}
	return registry.TryGet<ecs::components::SpellSeed>(PowerUp().hand.heldSeed);
}

/// The gestures' help events (Help/HelpProfile.h): 14 and 15 from ProcessPowerUpSystem, 16, 17 and 21 from the
/// selection stage, 18, 20 and 22 from the power-up gestures. (pending, creature) 19, the creature's fight gesture, is
/// not ported
void Help(int event)
{
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Gestures: help event 0x{:X}", event);
	}
	help_profile::Trigger(static_cast<help_profile::Event>(event));
}

void RemoveFromHandFx()
{
	if (PowerUp().hand.removeFromHandFx)
	{
		PowerUp().hand.removeFromHandFx();
	}
}

void ForceDropHeld()
{
	if (PowerUp().hand.forceDropHeld)
	{
		PowerUp().hand.forceDropHeld();
	}
}

LookingForArray FreshLookingFor()
{
	LookingForArray lf;
	for (auto& entry : lf)
	{
		entry = LookingFor {0, -1, -1, -1};
	}
	return lf;
}

/// The selection stays open only while the hand is ready for an object
bool SelectionOpenAndHandReady()
{
	const bool open = PowerUp().gestures.selection.open && PowerUp().hand.handReady;
	PowerUp().gestures.selection.open = open;
	return open;
}

/// The scribble (power down, or shake the seed out) and the seed's power-up gestures
bool PowerUpGestures(LookingForArray& lf)
{
	const auto* seed = HeldSeed();
	if (seed == nullptr)
	{
		return false;
	}
	const int seedIndex = static_cast<int>(seed->seedType);
	auto& s = PowerUp().gestures;
	if (s.currentPowerUpGesture != k_None)
	{
		lf[k_Scribble] = LookingFor {0xD, seedIndex, lf[k_Scribble].leash, -1};
		if (Recognise(k_Scribble))
		{
			RemoveFromHandFx();
			Success(false);
			s.gesture.gesture = k_None;
			TrySetupPowerUpGestures();
			Help(0x14);
			if (PowerUp().icons != nullptr)
			{
				PowerUp().icons->SetPowerUpCharge(seed->icon, -1);
			}
			return true;
		}
	}
	else
	{
		lf[k_Scribble].type = 0;
		if (Recognise(k_Scribble))
		{
			RemoveFromHandFx();
			Success(false);
			s.gesture.gesture = k_None;
			Help(0x16);
			ForceDropHeld();
			return true;
		}
	}
	for (int k = 0; k < 3; ++k)
	{
		const Gesture g = s.powerUpGestures[static_cast<size_t>(k)];
		if (g == k_None || g == s.currentPowerUpGesture)
		{
			continue;
		}
		lf[g] = LookingFor {0xE + k, seedIndex, lf[g].leash, k};
		if (Recognise(g) && seed->icon != entt::null)
		{
			Success(true);
			TrySetupPowerUpGestures();
			Help(0x12);
			s.currentPowerUpGesture = g;
			if (PowerUp().icons != nullptr)
			{
				PowerUp().icons->SetPowerUpCharge(seed->icon, k);
			}
			s.gesture.gesture = g;
			return true;
		}
	}
	return false;
}
} // namespace

void gestures::SetIconProvider(std::unique_ptr<IconProvider> provider)
{
	PowerUp().icons = std::move(provider);
}

IconProvider* gestures::GetIconProvider()
{
	return PowerUp().icons.get();
}

SelectionTables SelectionTables::FromInfo()
{
	SelectionTables tables;
	auto field = [](int seedType, auto member) -> Gesture {
		if (seedType < 0 || seedType >= static_cast<int>(k_SpellSeedCount) || !Locator::infoConstants::has_value())
		{
			return k_None;
		}
		const auto& info = GetSpellSeedInfo(Locator::infoConstants::value(), static_cast<SpellSeedType>(seedType));
		return static_cast<Gesture>(info.*member);
	};
	tables.selectionGesture = [field](int seed) { return field(seed, &GSpellSeedInfo::selectionGesture); };
	tables.gesture = [field](int seed) { return field(seed, &GSpellSeedInfo::gesture); };
	tables.gestureStage2 = [field](int seed) { return field(seed, &GSpellSeedInfo::gestureStage2); };
	return tables;
}

bool Selection::Open(Gesture openCategory, const IconProvider& icons, const SelectionTables& tables)
{
	activeGesture.fill(false);
	candidate.fill(false);
	for (auto& stages : seedStage)
	{
		stages.fill(k_None);
	}
	bool found = false;
	icons.ForEachRequestableIcon(openCategory, [&](int seedType) {
		if (seedType < 0 || seedType >= static_cast<int>(k_Seeds))
		{
			return;
		}
		const Gesture selection = tables.selectionGesture(seedType);
		if (selection == k_None || selection != openCategory)
		{
			return;
		}
		const Gesture gesture = tables.gesture(seedType);
		seedStage[static_cast<size_t>(seedType)] = {selection, gesture, tables.gestureStage2(seedType)};
		candidate[static_cast<size_t>(seedType)] = true;
		if (gesture < k_GestureCount)
		{
			activeGesture[gesture] = true;
		}
		found = true;
	});
	stage = 1;
	open = found;
	timer = 0.0f;
	category = openCategory;
	return found;
}

Selection::Outcome Selection::Stage(float dt, float timeOut, const std::function<bool(Gesture)>& recognise, IconProvider& icons,
                                    LookingForArray* lookingFor)
{
	// dt is 0 while paused
	timer += dt;
	if (timer > timeOut)
	{
		open = false; // the stage still runs this time
	}
	if (lookingFor != nullptr)
	{
		(*lookingFor)[k_Scribble].type = 0;
	}
	if (recognise(k_Scribble))
	{
		open = false;
		return Outcome::Cancelled;
	}
	if (lookingFor != nullptr)
	{
		for (size_t g = 1; g < k_GestureCount; ++g)
		{
			if (activeGesture[g])
			{
				(*lookingFor)[g].type = 0xB;
			}
		}
	}
	auto fill = [this, lookingFor]() {
		if (lookingFor == nullptr)
		{
			return;
		}
		for (size_t s = 0; s < k_Seeds; ++s)
		{
			if (candidate[s] && stage < 3 && seedStage[s][stage] < k_GestureCount)
			{
				(*lookingFor)[seedStage[s][stage]].seed = static_cast<int>(s);
			}
		}
	};
	bool anyActive = false;
	Gesture recognised = k_None;
	for (Gesture g = 1; g < k_GestureCount; ++g)
	{
		if (!activeGesture[g])
		{
			continue;
		}
		anyActive = true;
		if (recognise(g))
		{
			recognised = g;
			break;
		}
	}
	if (recognised == k_None)
	{
		if (!anyActive)
		{
			open = false;
			return Outcome::None;
		}
		fill();
		return Outcome::None;
	}
	bool first = false;
	for (size_t s = 0; s < k_Seeds; ++s)
	{
		if (!candidate[s])
		{
			continue;
		}
		if (stage < 3 && seedStage[s][stage] == recognised && icons.IconValidForRequest(static_cast<int>(s)))
		{
			if (!first)
			{
				first = true; // Success(1), help 0x10, gesture cleared and the timer back to 0 (the caller)
				timer = 0.0f;
			}
			candidate[s] = true;
			if (stage + 1 == 3 || seedStage[s][stage + 1] == k_None)
			{
				open = false;
				icons.RequestSpell(static_cast<int>(s)); // help 0x11
				return Outcome::Requested;
			}
		}
		else
		{
			candidate[s] = false;
		}
	}
	++stage;
	activeGesture.fill(false);
	bool any = false;
	for (size_t s = 0; s < k_Seeds; ++s)
	{
		if (!candidate[s])
		{
			continue;
		}
		if (icons.IconValidForRequest(static_cast<int>(s)) && stage < 3)
		{
			activeGesture[seedStage[s][stage] < k_GestureCount ? seedStage[s][stage] : 0] = true;
			any = true;
		}
		else
		{
			candidate[s] = false;
		}
	}
	if (!any)
	{
		open = false;
	}
	fill();
	return first ? Outcome::StageOk : Outcome::None;
}

InterfaceGestures& gestures::State()
{
	return PowerUp().gestures;
}

void gestures::Reset()
{
	PowerUp().gestures = InterfaceGestures {};
}

void gestures::SetHandStatus(HandStatus status)
{
	PowerUp().hand = std::move(status);
}

const HandStatus& gestures::GetHandStatus()
{
	return PowerUp().hand;
}

void gestures::ClearBuffer()
{
	PowerUp().gestures.system.Clear();
}

void gestures::FeedSample(glm::ivec2 mouse)
{
	// A restricted input mode (inferred: taken as the pause), and the cooldown
	if (PowerUp().hand.paused || PowerUp().gestures.cooldown > 0.0f)
	{
		return;
	}
	const auto world = sampling::ScreenToLand(glm::vec2(mouse));
	// (0, 0, 0) means off the land: |c| <= 1e-4, as GestureBuffer's k_Zero
	if (world && !(std::abs(world->x) <= 1e-4f && std::abs(world->y) <= 1e-4f && std::abs(world->z) <= 1e-4f))
	{
		PowerUp().gestures.system.AddSample(*world, mouse);
	}
	else
	{
		PowerUp().gestures.system.AddSampleAtLastWorld(mouse);
	}
}

void gestures::ReseedBuffer()
{
	ClearBuffer();
	FeedSample(PowerUp().hand.mouse);
}

bool gestures::Recognise(Gesture gesture)
{
	const float ratio = sampling::ScreenRatio();
	const auto data = BuildFromSystem(PowerUp().gestures.system, ratio);
	return MatchGesture(Templates(), gesture, data, PowerUp().gestures.result, ratio);
}

void gestures::Success(bool success)
{
	if (success)
	{
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Gestures: recognised {} (reversed {}, keypoints {}..{}, template {})",
			                   static_cast<int>(PowerUp().gestures.result.gesture), PowerUp().gestures.result.reversed,
			                   PowerUp().gestures.result.start, PowerUp().gestures.result.end,
			                   PowerUp().gestures.result.templateIndex);
		}
		// The sparkles along the stroke (gesture particles); force feedback is not ported
		psys::utility::GestureRecognised(PowerUp().gestures.system, PowerUp().gestures.result);
	}
	ClearBuffer();
	PowerUp().gestures.cooldown = k_Cooldown;
}

bool gestures::TrySetupPowerUpGestures()
{
	auto& s = PowerUp().gestures;
	s.circlePending = false;
	s.circleTimer = 0.0f;
	const auto* seed = HeldSeed();
	if (seed == nullptr)
	{
		return false;
	}
	const auto& info = seed::InfoOf(*seed);
	s.currentPowerUpGesture = k_None;
	bool any = false;
	for (size_t k = 0; k < 3; ++k)
	{
		s.powerUpGestures[k] = k_None;
		const MagicType magic = info.magicTypes[k + 1];
		if (magic != MagicType::None && players::IsMagicTypeEnabled(seed->creator.player, magic) && seed->icon != entt::null &&
		    PowerUp().icons != nullptr && PowerUp().icons->PowerUpAvailable(seed->icon, static_cast<int>(k)))
		{
			s.powerUpGestures[k] = static_cast<Gesture>(info.powerUpGestures[k]);
			any = true;
		}
	}
	return any;
}

void gestures::SetupPowerUpGestures()
{
	TrySetupPowerUpGestures();
	PowerUp().gestures.showHeldGestureTrail = true;
	ReseedBuffer();
	if (const auto* seed = HeldSeed(); seed != nullptr)
	{
		// the gesture the info of the seed's magic at its level names while the game runs (none for the base level)
		const auto& tables = Locator::infoConstants::value();
		const auto& magic = MagicInfoForPowerUpLevel(tables, seed::InfoOf(*seed), seed->powerUp);
		PowerUp().gestures.currentPowerUpGesture = static_cast<Gesture>(GetGestureOfMagicInfo(tables, magic.magicType));
	}
}

bool gestures::HoldingChargingSeed()
{
	const auto* seed = HeldSeed();
	return seed != nullptr && seed->ready && seed->lastMagic == MagicType::None && seed->icon != entt::null;
}

Gesture gestures::PowerUpLevelGesture()
{
	return HoldingChargingSeed() ? PowerUp().gestures.currentPowerUpGesture : k_None;
}

void gestures::ProcessPowerUpSystem(float dt)
{
	auto& s = PowerUp().gestures;
	// The early outs that wipe the buffer: inside the temple, an inactive interface and a few other interface states.
	// openblack has none of those states.
	// A fire seed in the hand with a fireball under it feeds the fireball: TODO
	// A moving camera wipes the gesture in progress, unless the camera checker holds it or the camera mode lets the
	// player gesture while it moves (an arena fight, dance, follow: none in openblack)
	if (sampling::CameraMoving())
	{
		ClearBuffer();
	}
	if (!PowerUp().hand.paused && s.cooldown > 0.0f)
	{
		s.cooldown -= dt;
	}
	auto lf = FreshLookingFor();
	if (s.circlePending)
	{
		if (!PowerUp().hand.paused)
		{
			s.circleTimer += dt;
		}
		if (s.circleTimer > k_CircleLife)
		{
			s.circlePending = false;
		}
	}
	// (a) the circle that sizes a storm or a shield, while the action press lasts
	if (PowerUp().hand.actionLatched)
	{
		if (const auto* seed = HeldSeed(); seed != nullptr && !s.circlePending)
		{
			if (const auto g = static_cast<Gesture>(seed::InfoOf(*seed).sizingGesture); g != k_None)
			{
				lf[g].type = 0xC;
				if (Recognise(g))
				{
					s.gesture = PacketFromResult(Templates(), s.system, s.result, sampling::CurrentProjection());
					s.circlePosition = s.gesture.position;
					s.circleSize = s.gesture.size;
					s.circleGesture = g;
					s.gesture.gesture = g;
					s.circlePending = true;
					s.circleTimer = 0.0f;
					if (Trace())
					{
						SPDLOG_LOGGER_INFO(spdlog::get("game"), "Gestures: circle at ({:.1f}, {:.1f}, {:.1f}) size {:.1f}",
						                   s.circlePosition.x, s.circlePosition.y, s.circlePosition.z, s.circleSize);
					}
					Success(true);
				}
			}
		}
	}
	// The local creature fighting and the leash selection need a creature: none yet
	const bool fighting = false;
	auto recognise = [](Gesture g) { return Recognise(g); };
	if (HoldingChargingSeed())
	{
		s.selection.open = false;
		if (PowerUpGestures(lf))
		{
			return;
		}
	}
	else if (SelectionOpenAndHandReady())
	{
		if (PowerUp().icons != nullptr)
		{
			const auto outcome = s.selection.Stage(dt, Locator::infoConstants::value().spellSystem.selectionSystemTimeOut,
			                                       recognise, *PowerUp().icons, &lf);
			switch (outcome)
			{
			case Selection::Outcome::Cancelled:
				RemoveFromHandFx();
				Success(false);
				Help(0x15);
				s.gesture.gesture = k_None;
				return;
			case Selection::Outcome::Requested:
				Success(true);
				Help(0x10);
				Help(0x11);
				s.gesture.gesture = k_None;
				return;
			case Selection::Outcome::StageOk:
				Success(true);
				Help(0x10);
				s.gesture.gesture = k_None;
				break;
			case Selection::Outcome::None:
				break;
			}
		}
	}
	// (b) SCRIBBLE cancels: the object in the hand is shaken out, or an icon's charge is cancelled
	if (!HoldingChargingSeed())
	{
		if (PowerUp().hand.holdingSomething)
		{
			if (PowerUp().hand.inInfluence && PowerUp().hand.validToShake)
			{
				lf[k_Scribble].type = 0;
				if (Recognise(k_Scribble))
				{
					RemoveFromHandFx();
					Success(false);
					ForceDropHeld();
					return;
				}
			}
		}
		// TODO: the leash scribble
		else if (PowerUp().icons != nullptr && PowerUp().icons->AnyIconChargingForHand())
		{
			lf[k_Scribble].type = 0;
			if (Recognise(k_Scribble))
			{
				RemoveFromHandFx();
				Success(false);
				PowerUp().icons->CancelMostChargedIcon();
				return;
			}
		}
	}
	const auto& system = Locator::infoConstants::value().spellSystem;
	// (c) the spiral opens the player's miracle selection, the inverse spiral the creature's
	if (!fighting && PowerUp().hand.handReady && PowerUp().icons != nullptr)
	{
		for (const auto category : {static_cast<Gesture>(system.selectionSystemGestureNonCreature),
		                            static_cast<Gesture>(system.selectionSystemGestureCreature)})
		{
			if (!PowerUp().icons->AnyRequestableIconOfCategory(category) ||
			    (s.selection.open && s.selection.category == category))
			{
				continue;
			}
			if (category < k_GestureCount)
			{
				lf[category].type = category == static_cast<Gesture>(system.selectionSystemGestureNonCreature) ? 8 : 9;
			}
			if (Recognise(category) && category != k_None)
			{
				const bool opened = s.selection.Open(category, *PowerUp().icons, SelectionTables::FromInfo());
				s.currentPowerUpGesture = k_None; // opening the selection clears it
				if (opened)
				{
					s.showHeldGestureTrail = true;
					Success(true);
					Help(0xF);
					s.gesture.gesture = k_None;
				}
				return;
			}
		}
	}
	// (d) R repeats the last miracle
	const auto repeat = static_cast<Gesture>(system.selectionSystemGestureR);
	if (!fighting && repeat != k_None && !SelectionOpenAndHandReady() && PowerUp().hand.handReady &&
	    PowerUp().icons != nullptr && PowerUp().icons->CanRepeat())
	{
		if (repeat < k_GestureCount)
		{
			lf[repeat].type = 0xA;
			lf[repeat].seed = s.lastSeedType;
		}
		if (Recognise(repeat))
		{
			Success(true);
			Help(0xE);
			s.gesture.gesture = k_None;
			PowerUp().icons->RepeatLast(s.lastSeedType);
			return;
		}
	}
	// (e) the leash selection (SQUARE_SPIRAL): creature only
	// The HUD gesture icons (S_Gesture0/1.raw), not drawn yet
	s.lookingFor = lf;
}
