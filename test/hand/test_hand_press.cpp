/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <SDL_events.h>
#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/HandPressSystem.h"
#include "ECS/Systems/Implementations/HandPressWorld.h"

using namespace openblack;
using openblack::ecs::systems::HandPressSystem;
using openblack::ecs::systems::HandPressSystemInterface;
using openblack::ecs::systems::HandPressWorld;

namespace
{
constexpr auto k_Creature = static_cast<entt::entity>(7);
constexpr auto k_OtherCreature = static_cast<entt::entity>(8);

/// The world of the presses as the tests set it. It keeps the calls that change something, in order.
struct FakeWorld final: public HandPressWorld
{
	// What the world answers
	bool handTakesPresses {true};
	bool mouseOverDebugWindow {false};
	ecs::components::InterfaceLimits limits {};
	bool paused {false};
	bool hasGrab {true};
	bool holdsThing {false};
	bool grabPressTakes {false};
	bool magicBusy {false};
	bool magicPressTakes {false};
	bool fireBallTakes {false};
	bool leashPressTakes {false};
	bool waitsForLockEnd {false};
	bool fightDirects {false};
	bool fightPressTakes {false};
	bool fightPressed {false};
	bool doubleClick {false};
	std::optional<entt::entity> underCursor;
	std::optional<entt::entity> held;
	bool mayHoldCreature {true};

	std::vector<std::string> calls;

	[[nodiscard]] bool HandTakesPresses() const final { return handTakesPresses; }
	[[nodiscard]] bool MouseOverDebugWindow() const final { return mouseOverDebugWindow; }
	[[nodiscard]] ecs::components::InterfaceLimits Limits() const final { return limits; }
	[[nodiscard]] bool Paused() const final { return paused; }
	[[nodiscard]] uint32_t Ticks() const final { return 1000; }
	[[nodiscard]] uint32_t Turn() const final { return 10; }
	[[nodiscard]] PlayerNames Player() const final { return PlayerNames::PLAYER_ONE; }
	void Ray(glm::vec2 /*cursor*/, glm::vec3& origin, glm::vec3& direction) const final
	{
		origin = glm::vec3(0.0f, 10.0f, 0.0f);
		direction = glm::vec3(0.0f, -1.0f, 0.0f);
	}
	[[nodiscard]] bool HasPicking() const final { return true; }
	[[nodiscard]] std::optional<entt::entity> PickedObject() const final { return std::nullopt; }
	[[nodiscard]] std::optional<glm::vec3> PickedPoint() const final { return std::nullopt; }
	[[nodiscard]] bool HasScriptHighlight(entt::entity /*thing*/) const final { return false; }

	[[nodiscard]] bool TipBubbleGripped() const final { return false; }
	void LetGoOfTipBubble() final { calls.emplace_back("LetGoOfTipBubble"); }
	[[nodiscard]] bool TipBubbleUnderHand() const final { return false; }
	void GripTipBubble() final { calls.emplace_back("GripTipBubble"); }

	[[nodiscard]] bool HasTownTotems() const final { return false; }
	[[nodiscard]] bool TownTotemGripped() const final { return false; }
	void LetGoOfTownTotem() final { calls.emplace_back("LetGoOfTownTotem"); }
	[[nodiscard]] std::optional<entt::entity> TownTotemOf(entt::entity /*picked*/) const final { return std::nullopt; }
	bool GripTownTotem(entt::entity /*totem*/, PlayerNames /*player*/) final { return false; }
	[[nodiscard]] bool HasPuzzleTotems() const final { return false; }
	[[nodiscard]] bool PuzzleTotemGripped() const final { return false; }
	void LetGoOfPuzzleTotem() final { calls.emplace_back("LetGoOfPuzzleTotem"); }
	bool GripPuzzleTotem(entt::entity /*totem*/, PlayerNames /*player*/, glm::vec3 /*hand*/) final { return false; }

	[[nodiscard]] bool HasHandGrab() const final { return hasGrab; }
	[[nodiscard]] bool HandHoldsThing() const final { return holdsThing; }
	bool GrabPress(uint32_t /*nowMs*/, uint32_t /*turn*/) final
	{
		calls.emplace_back("GrabPress");
		return grabPressTakes;
	}
	void GrabRelease(uint32_t /*nowMs*/, uint32_t /*turn*/) final { calls.emplace_back("GrabRelease"); }
	void GrabClickReleased() final { calls.emplace_back("GrabClickReleased"); }
	void GrabClickThing(entt::entity /*thing*/) final { calls.emplace_back("GrabClickThing"); }

	[[nodiscard]] bool MagicHandBusy() const final { return magicBusy; }
	bool MagicPressAction() final
	{
		calls.emplace_back("MagicPressAction");
		return magicPressTakes;
	}
	void MagicReleaseAction() final { calls.emplace_back("MagicReleaseAction"); }
	bool MagicPressFireBall(uint32_t /*nowMs*/, uint32_t /*turn*/) final
	{
		calls.emplace_back("MagicPressFireBall");
		return fireBallTakes;
	}
	void MagicReleaseFireBall(uint32_t /*nowMs*/, uint32_t /*turn*/) final { calls.emplace_back("MagicReleaseFireBall"); }

	[[nodiscard]] leash_snap::Hand RopeHand(PlayerNames /*player*/) const final { return leash_snap::Hand::Away; }
	void LetGoOfRope(PlayerNames /*player*/, std::optional<glm::vec3> /*place*/) final { calls.emplace_back("LetGoOfRope"); }
	void LeashTapThing(PlayerNames /*player*/, entt::entity /*thing*/) final { calls.emplace_back("LeashTapThing"); }
	bool LeashPressAction(PlayerNames /*player*/, const glm::vec3& /*rayOrigin*/, const glm::vec3& /*rayDirection*/,
	                      glm::vec2 /*cursor*/, uint32_t /*nowMs*/) final
	{
		calls.emplace_back("LeashPressAction");
		return leashPressTakes;
	}
	bool PressRope(PlayerNames /*player*/) final
	{
		calls.emplace_back("PressRope");
		return false;
	}
	[[nodiscard]] std::optional<entt::entity> PlayersCreature(PlayerNames /*player*/) const final { return k_Creature; }
	[[nodiscard]] bool IsLeashed(entt::entity /*creature*/) const final { return false; }
	[[nodiscard]] std::optional<glm::vec3> HolderPoint(entt::entity /*creature*/) const final { return std::nullopt; }

	[[nodiscard]] std::optional<entt::entity> HeldCreature() const final { return held; }
	void ReleaseCreature() final
	{
		calls.emplace_back("ReleaseCreature");
		held.reset();
	}
	std::optional<entt::entity> ReleaseWait() final { return std::nullopt; }
	[[nodiscard]] bool WaitsForLockEnd() const final { return waitsForLockEnd; }
	[[nodiscard]] std::optional<entt::entity> CreatureUnderCursor() const final { return underCursor; }
	[[nodiscard]] bool MayHoldCreature(entt::entity /*creature*/) const final { return mayHoldCreature; }
	void GrabCreature() final
	{
		calls.emplace_back("GrabCreature");
		held = underCursor;
	}
	void WaitForTap(entt::entity /*creature*/, uint32_t /*nowMs*/, uint32_t /*turn*/) final
	{
		calls.emplace_back("WaitForTap");
	}

	[[nodiscard]] bool FightDirects() const final { return fightDirects; }
	bool FightPress(const glm::vec3& /*rayOrigin*/, const glm::vec3& /*rayDirection*/, creature_fight::Button button,
	                uint32_t /*nowMs*/, uint32_t /*turn*/) final
	{
		calls.emplace_back(button == creature_fight::Button::Action ? "FightPressAction" : "FightPressMove");
		fightPressed = fightPressTakes;
		return fightPressTakes;
	}
	[[nodiscard]] bool FightPressed() const final { return fightPressed; }
	void FightRelease(uint32_t /*nowMs*/, uint32_t /*turn*/) final
	{
		calls.emplace_back("FightRelease");
		fightPressed = false;
	}

	bool CreatureModePress(const creature_mode::Press& /*press*/) final
	{
		calls.emplace_back("CreatureModePress");
		return doubleClick;
	}
};

SDL_Event ButtonEvent(uint32_t type, uint8_t button)
{
	SDL_Event event {};
	event.type = type;
	event.button.button = button;
	event.button.x = 100;
	event.button.y = 200;
	return event;
}

SDL_Event Motion(uint32_t state)
{
	SDL_Event event {};
	event.type = SDL_MOUSEMOTION;
	event.motion.state = state;
	return event;
}

struct Fixture
{
	FakeWorld* world {nullptr};
	std::unique_ptr<HandPressSystem> system;
	int grabSounds {0};

	Fixture()
	{
		auto fake = std::make_unique<FakeWorld>();
		world = fake.get();
		system = std::make_unique<HandPressSystem>(std::move(fake));
	}

	void Press(const SDL_Event& event, bool inTemple = false)
	{
		system->Press(event,
		              HandPressSystemInterface::PressContext {.inTemple = inTemple, .playGrabSound = [this] { ++grabSounds; }});
	}

	[[nodiscard]] bool Called(const std::string& name) const
	{
		return std::ranges::find(world->calls, name) != world->calls.end();
	}
};
} // namespace

TEST(HandPress, AMiracleInTheHandTakesTheActionPressFirst)
{
	Fixture fixture;
	fixture.world->magicBusy = true;
	fixture.world->magicPressTakes = true;
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT));
	EXPECT_EQ(fixture.world->calls, std::vector<std::string> {"MagicPressAction"});
	EXPECT_TRUE(fixture.system->TakeActionPress());
	// The leash reads the press once
	EXPECT_FALSE(fixture.system->TakeActionPress());
}

TEST(HandPress, TheLockOnACreatureTakesThePressBeforeTheMiracle)
{
	Fixture fixture;
	fixture.world->waitsForLockEnd = true;
	fixture.world->magicBusy = true;
	fixture.world->magicPressTakes = true;
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT));
	EXPECT_TRUE(fixture.world->calls.empty());
	EXPECT_TRUE(fixture.system->TakeActionPress());
}

TEST(HandPress, AHandThatHoldsAThingMakesReadyToThrowBeforeTheLeash)
{
	Fixture fixture;
	fixture.world->holdsThing = true;
	fixture.world->grabPressTakes = true;
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT));
	EXPECT_EQ(fixture.world->calls, std::vector<std::string> {"GrabPress"});
	EXPECT_TRUE(fixture.system->TakeActionPress());
}

TEST(HandPress, AnEmptyHandTriesTheLeashTheRopeTheFireBallTheFightAndThenTheThing)
{
	Fixture fixture;
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT));
	EXPECT_EQ(fixture.world->calls, (std::vector<std::string> {"LeashPressAction", "PressRope", "MagicPressFireBall",
	                                                           "FightPressAction", "GrabPress"}));
	EXPECT_FALSE(fixture.system->TakeActionPress());
}

TEST(HandPress, APressThatTheFightRefusesWhileItDirectsTheFightDoesNothingMore)
{
	Fixture fixture;
	fixture.world->fightDirects = true;
	fixture.world->underCursor = k_OtherCreature;
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT));
	EXPECT_EQ(fixture.world->calls, (std::vector<std::string> {"LeashPressAction", "FightPressAction"}));
	EXPECT_TRUE(fixture.system->TakeActionPress());
}

TEST(HandPress, TheLetGoOfTheButtonThatPressedForTheFightChargesTheBlow)
{
	Fixture fixture;
	fixture.world->fightPressTakes = true;
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT));
	ASSERT_TRUE(fixture.system->GetFightButton().has_value());
	EXPECT_EQ(*fixture.system->GetFightButton(), creature_fight::Button::Action);
	fixture.world->calls.clear();
	// A motion that still holds the button does not charge it
	fixture.Press(Motion(SDL_BUTTON_RMASK));
	EXPECT_FALSE(fixture.Called("FightRelease"));
	fixture.Press(Motion(0));
	EXPECT_TRUE(fixture.Called("FightRelease"));
	EXPECT_FALSE(fixture.system->GetFightButton().has_value());
}

TEST(HandPress, AnEmptyHandOnACreatureClicksItAndTakesHoldOfIt)
{
	Fixture fixture;
	fixture.world->underCursor = k_OtherCreature;
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT));
	EXPECT_EQ(fixture.world->calls, (std::vector<std::string> {"LeashPressAction", "PressRope", "MagicPressFireBall",
	                                                           "FightPressAction", "GrabClickThing", "GrabCreature"}));
	EXPECT_TRUE(fixture.system->TakeActionPress());
	// The hand holds the creature, so the left button does not grip the land
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT));
	EXPECT_FALSE(fixture.system->GetHandButtons().gripping);
	// The let go of the right button lets go of the creature
	fixture.world->calls.clear();
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONUP, SDL_BUTTON_RIGHT));
	EXPECT_TRUE(fixture.Called("ReleaseCreature"));
	EXPECT_TRUE(fixture.system->GetHandButtons().gripping);
}

TEST(HandPress, ALessonThatAllowsNoHoldingKeepsTheHandOffTheCreature)
{
	Fixture fixture;
	fixture.world->underCursor = k_OtherCreature;
	fixture.world->limits.noHolding = true;
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT));
	EXPECT_FALSE(fixture.Called("GrabCreature"));
	EXPECT_TRUE(fixture.Called("GrabPress"));
}

TEST(HandPress, ALessonThatAllowsOnlyHighlightsTakesThePressAndDoesNothing)
{
	Fixture fixture;
	fixture.world->limits.onlyHighlights = true;
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT));
	EXPECT_FALSE(fixture.Called("MagicPressFireBall"));
	EXPECT_FALSE(fixture.Called("GrabPress"));
	EXPECT_TRUE(fixture.system->TakeActionPress());
}

TEST(HandPress, NoPressGoesWhileTheCinemaBarsHoldTheInterfaceOrInTheTemple)
{
	Fixture fixture;
	fixture.world->handTakesPresses = false;
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT));
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT));
	EXPECT_TRUE(fixture.world->calls.empty());
	EXPECT_EQ(fixture.grabSounds, 0);

	Fixture temple;
	temple.Press(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT), true);
	temple.Press(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT), true);
	EXPECT_TRUE(temple.world->calls.empty());
	EXPECT_EQ(temple.grabSounds, 0);
	EXPECT_FALSE(temple.system->GetHandButtons().gripping);
}

TEST(HandPress, AMovePressThatNothingTakesGripsTheLandWithItsSound)
{
	Fixture fixture;
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT));
	EXPECT_EQ(fixture.world->calls, (std::vector<std::string> {"CreatureModePress", "FightPressMove"}));
	EXPECT_EQ(fixture.grabSounds, 1);
	EXPECT_TRUE(fixture.system->GetHandButtons().gripping);
	EXPECT_TRUE(fixture.system->GetHandButtons().selectGrip);
	EXPECT_FALSE(fixture.system->GetHandButtons().rotating);
}

TEST(HandPress, ADoubleClickOnACreatureTakesTheMovePress)
{
	Fixture fixture;
	fixture.world->doubleClick = true;
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT));
	EXPECT_EQ(fixture.world->calls, std::vector<std::string> {"CreatureModePress"});
	EXPECT_EQ(fixture.grabSounds, 0);
}

TEST(HandPress, AMovePressForTheFightIsLetGoWithTheLeftButton)
{
	Fixture fixture;
	fixture.world->fightPressTakes = true;
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT));
	EXPECT_EQ(fixture.grabSounds, 0);
	ASSERT_TRUE(fixture.system->GetFightButton().has_value());
	EXPECT_EQ(*fixture.system->GetFightButton(), creature_fight::Button::Move);
	// The fight holds the press, so the left button does not grip the land
	EXPECT_FALSE(fixture.system->GetHandButtons().gripping);
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT));
	EXPECT_TRUE(fixture.Called("FightRelease"));
}

TEST(HandPress, TheLetGoOfAnEmptyHandThatTookNoPressClicksForTheScripts)
{
	Fixture fixture;
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT));
	fixture.world->calls.clear();
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONUP, SDL_BUTTON_RIGHT));
	EXPECT_EQ(fixture.world->calls,
	          (std::vector<std::string> {"GrabRelease", "GrabClickReleased", "MagicReleaseAction", "MagicReleaseFireBall"}));
}

TEST(HandPress, TheLetGoAfterAPressThatWasTakenDoesNotClick)
{
	Fixture fixture;
	fixture.world->grabPressTakes = true;
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT));
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONUP, SDL_BUTTON_RIGHT));
	EXPECT_TRUE(fixture.Called("GrabRelease"));
	EXPECT_FALSE(fixture.Called("GrabClickReleased"));
}

TEST(HandPress, TheLetGoWhilePausedDoesNotClick)
{
	Fixture fixture;
	fixture.world->paused = true;
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONUP, SDL_BUTTON_RIGHT));
	EXPECT_TRUE(fixture.Called("GrabRelease"));
	EXPECT_FALSE(fixture.Called("GrabClickReleased"));
}

TEST(HandPress, ALeftPressClicksTheDialogueUnlessTheDebugWindowsHaveTheMouse)
{
	Fixture fixture;
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT));
	EXPECT_TRUE(fixture.system->TakeDialogueClick());
	EXPECT_FALSE(fixture.system->TakeDialogueClick());
	fixture.world->mouseOverDebugWindow = true;
	fixture.Press(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT));
	EXPECT_FALSE(fixture.system->TakeDialogueClick());
}

TEST(HandPress, TheHandFollowsTheGamesPointerUntilTheMouseMoves)
{
	Fixture fixture;
	fixture.system->FramePointer(glm::ivec2(40, 50));
	EXPECT_EQ(fixture.system->GetMousePosition(), glm::ivec2(40, 50));
	// A cursor that the game sets keeps the hand on the game's pointer
	fixture.system->SetMousePosition({1, 2});
	fixture.system->FramePointer(glm::ivec2(40, 50));
	EXPECT_EQ(fixture.system->GetMousePosition(), glm::ivec2(40, 50));
	fixture.system->MovePointer({300, 400});
	fixture.system->FramePointer(glm::ivec2(40, 50));
	EXPECT_EQ(fixture.system->GetMousePosition(), glm::ivec2(300, 400));
	EXPECT_TRUE(fixture.system->GetHandPointer().HasMoved());
}
