/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/TattooEditorSystem.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using creature_tattoo::Sheet;
using creature_tattoo::Slot;
using creature_tattoo::Slots;
using creature_tattoo_editor::SiteOnScreen;

namespace
{
constexpr auto k_Creature = static_cast<entt::entity>(7);
constexpr auto k_Copy = static_cast<entt::entity>(8);
constexpr auto k_Shown = static_cast<entt::entity>(9);
constexpr glm::u8vec3 k_Green {0, 200, 0};

/// What the editor did to the skins
struct SkinLog
{
	std::map<entt::entity, Slots> slots {{k_Creature, Slots {}}};
	std::map<entt::entity, std::vector<uint8_t>> pictures;
	std::map<entt::entity, std::optional<Slots>> shown;
	std::vector<std::pair<entt::entity, Sheet>> remade;
	std::vector<entt::entity> painted;
	std::optional<entt::entity> caveCopy;

	void Forget()
	{
		remade.clear();
		painted.clear();
	}
};

/// The skins the editor writes to
class FakeSkin final: public TattooEditorSystem::Skin
{
public:
	explicit FakeSkin(SkinLog& log)
	    : _log(log)
	{
	}
	[[nodiscard]] std::optional<Slots> Read(entt::entity creature) const override
	{
		const auto found = _log.slots.find(creature);
		return found != _log.slots.end() ? std::optional(found->second) : std::nullopt;
	}
	void Write(entt::entity creature, const Slots& slots) override { _log.slots[creature] = slots; }
	void WritePictures(entt::entity creature, std::vector<uint8_t> pictures) override
	{
		_log.pictures[creature] = std::move(pictures);
	}
	void Show(entt::entity shown, std::optional<Slots> slots) override { _log.shown[shown] = slots; }
	void RemakeMasks(entt::entity creature, Sheet sheet) override { _log.remade.emplace_back(creature, sheet); }
	void Paint(entt::entity creature) override { _log.painted.push_back(creature); }
	[[nodiscard]] std::optional<entt::entity> CaveCopy(entt::entity creature) const override
	{
		return creature == k_Creature ? _log.caveCopy : std::nullopt;
	}

private:
	SkinLog& _log;
};

struct Fixture
{
	SkinLog log;
	TattooEditorSystem editor {std::make_unique<FakeSkin>(log)};
	/// One place, at the middle of the screen, facing the camera
	std::array<SiteOnScreen, 1> sites {SiteOnScreen {.site = 4, .screen = {400, 300}, .facingCamera = 1.0f}};

	void OverThePlace() { editor.Hover(sites, {400, 300}); }
	void AwayFromIt() { editor.Hover(sites, {0, 0}); }
};
} // namespace

TEST(TattooEditorSystem, OpensOnACreatureWithItsTattoosAndView)
{
	Fixture f;
	f.log.slots.at(k_Creature).at(2) = {.design = 3, .site = 1, .colour = k_Green};
	const creature_tattoo_editor::Orbit view {.eye = {1.0f, 2.0f, 3.0f}, .focus = {0.0f, 1.0f, 0.0f}};
	f.editor.Open(k_Creature, view);
	EXPECT_TRUE(f.editor.IsOpen());
	EXPECT_EQ(f.editor.GetCreature(), k_Creature);
	EXPECT_EQ(f.editor.GetSession().slots, f.log.slots.at(k_Creature));
	EXPECT_EQ(f.editor.GetView().eye, view.eye);
}

TEST(TattooEditorSystem, OpeningInTheCaveShowsTheEditsOnTheCopyWithTheMasksMadeAgain)
{
	Fixture f;
	f.log.caveCopy = k_Copy;
	f.log.slots.at(k_Creature).at(2) = {.design = 3, .site = 1, .colour = k_Green};
	f.editor.Open(k_Creature, {});
	EXPECT_EQ(f.log.shown.at(k_Copy), f.log.slots.at(k_Creature));
	ASSERT_EQ(f.log.remade.size(), 1u);
	EXPECT_EQ(f.log.remade.front(), std::pair(k_Creature, Sheet::OwnPictures));
	EXPECT_EQ(f.log.painted, std::vector {k_Copy});
	// Closing shows the creature's own tattoos on the copy again, and paints it
	f.log.Forget();
	f.editor.Close();
	EXPECT_FALSE(f.log.shown.at(k_Copy).has_value());
	EXPECT_EQ(f.log.painted, std::vector {k_Copy});
	EXPECT_TRUE(f.log.remade.empty());
}

TEST(TattooEditorSystem, ASymbolDroppedOverAPlaceGoesIntoTheSkin)
{
	Fixture f;
	f.editor.Open(k_Creature, {});
	f.AwayFromIt();
	EXPECT_FALSE(f.editor.Drop(9, k_Green));
	EXPECT_TRUE(f.log.slots.at(k_Creature).at(0).Empty());
	f.OverThePlace();
	EXPECT_EQ(f.editor.GetHoveredSite(), 4);
	EXPECT_TRUE(f.editor.Drop(9, k_Green));
	EXPECT_EQ(f.log.slots.at(k_Creature).at(0), (Slot {.design = 9, .site = 4, .colour = k_Green}));
}

TEST(TattooEditorSystem, EachDragEndPaintsTheCopyAndTheCreature)
{
	Fixture f;
	f.log.caveCopy = k_Copy;
	f.editor.Open(k_Creature, {});
	f.log.Forget();
	// Also a drop away from the places
	f.AwayFromIt();
	EXPECT_FALSE(f.editor.Drop(9, k_Green));
	EXPECT_EQ(f.log.remade, (std::vector {std::pair(k_Creature, Sheet::OwnPictures)}));
	EXPECT_EQ(f.log.painted, (std::vector {k_Copy, k_Creature}));
	f.log.Forget();
	f.OverThePlace();
	EXPECT_TRUE(f.editor.Drop(9, k_Green));
	EXPECT_EQ(f.log.shown.at(k_Copy), f.editor.GetSession().slots);
	EXPECT_EQ(f.log.painted, (std::vector {k_Copy, k_Creature}));
}

TEST(TattooEditorSystem, LiftingTakesTheTattooOffTheSkin)
{
	Fixture f;
	f.log.caveCopy = k_Copy;
	f.editor.Open(k_Creature, {});
	f.OverThePlace();
	ASSERT_TRUE(f.editor.Drop(9, k_Green));
	f.log.Forget();
	const auto lifted = f.editor.Lift();
	ASSERT_TRUE(lifted.has_value());
	EXPECT_EQ(lifted->design, 9);
	EXPECT_EQ(lifted->colour, k_Green);
	EXPECT_TRUE(f.log.slots.at(k_Creature).at(0).Empty());
	// Only the copy is painted again: the creature in the world waits for the end of the drag
	EXPECT_EQ(f.log.painted, std::vector {k_Copy});
	EXPECT_EQ(f.log.remade.size(), 1u);
	f.AwayFromIt();
	EXPECT_FALSE(f.editor.Lift().has_value());
}

TEST(TattooEditorSystem, APressOnAnEmptyPlaceMakesTheCreatureSad)
{
	Fixture f;
	f.log.caveCopy = k_Copy;
	f.editor.Open(k_Creature, {});
	f.OverThePlace();
	f.log.Forget();
	EXPECT_FALSE(f.editor.Lift().has_value());
	EXPECT_EQ(f.editor.TakeFace(), creature_face::Face::Sad);
	EXPECT_TRUE(f.editor.GetSession().changed);
	EXPECT_EQ(f.log.painted, std::vector {k_Copy});
	// Again on the next frame while the button stays down
	EXPECT_FALSE(f.editor.Lift().has_value());
	EXPECT_EQ(f.editor.TakeFace(), creature_face::Face::Sad);
}

TEST(TattooEditorSystem, CancelPutsBackTheTattoosItOpenedWith)
{
	Fixture f;
	f.log.caveCopy = k_Copy;
	f.log.slots.at(k_Creature).at(5) = {.design = 1, .site = 0, .colour = k_Green};
	const auto before = f.log.slots.at(k_Creature);
	f.editor.Open(k_Creature, {});
	f.OverThePlace();
	ASSERT_TRUE(f.editor.Drop(9, k_Green));
	ASSERT_NE(f.log.slots.at(k_Creature), before);
	f.log.Forget();
	f.editor.Cancel();
	EXPECT_EQ(f.log.slots.at(k_Creature), before);
	EXPECT_FALSE(f.editor.IsOpen());
	// The masks stay as the last drag made them, and only the copy is painted, as it closes
	EXPECT_TRUE(f.log.remade.empty());
	EXPECT_EQ(f.log.painted, std::vector {k_Copy});
}

TEST(TattooEditorSystem, OkKeepsTheTattoosAndClosesOrWarnsFirst)
{
	Fixture f;
	f.editor.Open(k_Creature, {});
	f.OverThePlace();
	ASSERT_TRUE(f.editor.Drop(9, k_Green));
	EXPECT_EQ(f.editor.Ok(true), creature_tattoo_editor::Accept::Warn);
	EXPECT_TRUE(f.editor.IsOpen());
	f.editor.Answered();
	EXPECT_EQ(f.editor.Ok(true), creature_tattoo_editor::Accept::Close);
	EXPECT_FALSE(f.editor.IsOpen());
	EXPECT_EQ(f.log.slots.at(k_Creature).at(0).design, 9);
}

TEST(TattooEditorSystem, TheViewTurnsOnlyWhileOpen)
{
	Fixture f;
	const creature_tattoo_editor::Orbit view {.eye = {10.0f, 5.0f, 0.0f}, .focus = {0.0f, 0.0f, 0.0f}};
	f.editor.Open(k_Creature, view);
	f.editor.Steer({100, 100}, {0, 100}, 16.0f);
	f.editor.Update(16.0f);
	EXPECT_NE(f.editor.GetView().eye, view.eye);
	f.editor.Close();
	const auto stopped = f.editor.GetView().eye;
	f.editor.Update(16.0f);
	EXPECT_EQ(f.editor.GetView().eye, stopped);
}

TEST(TattooEditorSystem, TheCreatureSmilesWhenATattooGoesOnAndIsSadWhenItComesOff)
{
	Fixture f;
	f.editor.Open(k_Creature, {});
	EXPECT_FALSE(f.editor.TakeFace().has_value());
	f.AwayFromIt();
	EXPECT_FALSE(f.editor.Drop(9, k_Green));
	EXPECT_FALSE(f.editor.TakeFace().has_value());
	f.OverThePlace();
	ASSERT_TRUE(f.editor.Drop(9, k_Green));
	EXPECT_EQ(f.editor.TakeFace(), creature_face::Face::Smile);
	// The face comes back one time
	EXPECT_FALSE(f.editor.TakeFace().has_value());
	// The same symbol again on the same place only takes the new colour, with no smile
	EXPECT_FALSE(f.editor.Drop(9, {1, 2, 3}));
	EXPECT_FALSE(f.editor.TakeFace().has_value());
	ASSERT_TRUE(f.editor.Lift().has_value());
	EXPECT_EQ(f.editor.TakeFace(), creature_face::Face::Sad);
	EXPECT_EQ(creature_tattoo_editor::k_FaceMilliseconds, 1800.0f);
}

TEST(TattooEditorSystem, EachDropKeepsThePlayersPicturesThatTheTattoosUse)
{
	Fixture f;
	creature_tattoo::Pictures pictures {};
	pictures.at(9) = creature_tattoo::Picture {};
	f.editor.SetPictures(pictures);
	f.editor.Open(k_Creature, {});
	f.OverThePlace();
	EXPECT_TRUE(f.editor.Drop(9, k_Green));
	ASSERT_TRUE(f.log.pictures.contains(k_Creature));
	const auto& block = f.log.pictures.at(k_Creature);
	ASSERT_EQ(block.size(), 0x828u);
	EXPECT_EQ(block.at(5), 1);
	// Cancel puts the tattoos back and makes the block again from them, so no slot keeps the picture
	f.editor.Cancel();
	EXPECT_TRUE(f.log.slots.at(k_Creature).at(0).Empty());
	ASSERT_EQ(f.log.pictures.at(k_Creature).size(), 0x28u);
	EXPECT_EQ(f.log.pictures.at(k_Creature).at(5), 0);
}

TEST(TattooEditorSystem, FromTheMenuTheShownCreatureAndThePlayersCreatureWearTheTattoos)
{
	Fixture f;
	f.log.slots.at(k_Creature).at(1) = {.design = 5, .site = 2, .colour = k_Green};
	f.editor.OpenFromMenu(k_Creature, k_Shown,
	                      {.eye = creature_tattoo_editor::k_MenuEye, .focus = creature_tattoo_editor::k_MenuFocus});
	EXPECT_TRUE(f.editor.IsOpen());
	EXPECT_TRUE(f.editor.IsFromMenu());
	EXPECT_EQ(f.editor.GetShown(), k_Shown);
	EXPECT_EQ(f.editor.GetCreature(), k_Creature);
	// The shown creature shows the player's creature's tattoos at once, with the masks made from the creature
	EXPECT_EQ(f.log.shown.at(k_Shown), f.log.slots.at(k_Creature));
	EXPECT_EQ(f.log.remade, (std::vector {std::pair(k_Creature, Sheet::OwnPictures)}));
	EXPECT_EQ(f.log.painted, std::vector {k_Shown});

	f.OverThePlace();
	EXPECT_TRUE(f.editor.Drop(3, k_Green));
	EXPECT_EQ(f.log.shown.at(k_Shown), f.editor.GetSession().slots);
	EXPECT_EQ(f.log.slots.at(k_Creature), f.editor.GetSession().slots);

	// Cancel puts back the tattoos the player's creature had
	const auto opened = f.editor.GetSession().opened;
	f.editor.Cancel();
	EXPECT_EQ(f.log.slots.at(k_Creature), opened);
	EXPECT_FALSE(f.editor.IsOpen());
	EXPECT_FALSE(f.editor.IsFromMenu());
}

TEST(TattooEditorSystem, FromTheMenuWithNoCreatureTheMasksStayAsTheyWere)
{
	Fixture f;
	f.editor.OpenFromMenu(std::nullopt, k_Shown, {});
	EXPECT_TRUE(f.editor.IsOpen());
	EXPECT_FALSE(f.editor.GetCreature().has_value());
	EXPECT_EQ(f.editor.GetSession().slots, Slots {});
	EXPECT_EQ(f.log.shown.at(k_Shown), Slots {});
	f.OverThePlace();
	EXPECT_TRUE(f.editor.Drop(2, k_Green));
	EXPECT_EQ(f.log.shown.at(k_Shown)->at(0).design, 2);
	// No creature in the game: no masks are made, so the shown creature paints with the masks as they were
	EXPECT_TRUE(f.log.remade.empty());
	EXPECT_FALSE(f.log.pictures.contains(k_Shown));
	EXPECT_EQ(f.editor.Ok(false), creature_tattoo_editor::Accept::Close);
	EXPECT_FALSE(f.editor.IsOpen());
}
