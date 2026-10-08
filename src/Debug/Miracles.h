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

#include <memory>
#include <optional>
#include <string>

#include <glm/vec3.hpp>

#include "Enums.h"
#include "LeftClickCapture.h"
#include "MiraclesModel.h"
#include "Window.h"

namespace openblack::debug::gui
{

/// Casts any miracle as any player, at any power-up level and charge: where the camera looks, on the land under a
/// click, or on the thing nearest a click. Makes a one-shot orb, a seed in the hand or a miracle dispenser of the
/// chosen miracle where the camera looks, at the next click on the land or under the hand, only on its button. Shows
/// the seed in the hand and its power-up, the villagers' reactions to the miracles, and switches infinite prayer power
/// on at every worship site. Closed with every switch off, it changes nothing; infinite prayer power stays on with the
/// window closed until it is turned off.
class Miracles final: public Window
{
public:
	Miracles() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void UpdateAlways() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;
	[[nodiscard]] bool TakesEvent(const SDL_Event& event) const noexcept override;

private:
	/// What a left click on the land does while the window is open
	enum class ClickCast : uint8_t
	{
		Off,
		OnLand,
		OnNearestThing,
	};

	/// Where a dispenser from the window goes
	enum class DispenserPlace : uint8_t
	{
		CameraFocus,
		NextClick,
		Hand,
	};

	void DrawChoice() noexcept;
	void DrawCast() noexcept;
	void DrawDispenser() noexcept;
	void DrawHand() noexcept;
	void DrawPrayer() noexcept;
	void DrawReactions() noexcept;
	/// The running miracles, the dispensers and the creatures' spells
	void DrawRunning() noexcept;

	void CastAt(const miracles::Target& target) noexcept;
	void CastAtClick() noexcept;
	void CreateDispenserAt(glm::vec3 point) noexcept;
	void CreateDispenserAtClick() noexcept;
	/// The land under the hand, if the hand is on the land
	[[nodiscard]] static std::optional<glm::vec3> HandLandPoint() noexcept;
	/// Each site's cheats back as they were before the switch
	void TurnPrayerOff() noexcept;
	/// Where the camera looks, on the land
	[[nodiscard]] static glm::vec3 CameraPoint() noexcept;

	std::unique_ptr<miracles::CasterInterface> _caster;
	miracles::Choice _choice;
	/// How charged the cast is, as a seed's multiplier
	float _multiplier {1.0f};
	PlayerNames _player {PlayerNames::PLAYER_ONE};
	ClickCast _clickCast {ClickCast::Off};
	/// A click taken, to cast at the next update
	bool _clicked {false};
	/// The left press the window took, whose release it takes too
	debug::LeftClickCapture _leftCapture;
	std::unique_ptr<miracles::DispenserCreatorInterface> _dispenserCreator;
	miracles::DispenserKind _dispenserKind {miracles::DispenserKind::OneShot};
	DispenserPlace _dispenserPlace {DispenserPlace::CameraFocus};
	std::optional<AbodeInfo> _dispenserAbode;
	int _dispenserPeriod {0};
	/// The next left click on the land makes the dispenser, instead of casting
	bool _dispenserAtClick {false};
	miracles::InfinitePrayer _prayer;
	/// Every reaction, not only the miracles'
	bool _allReactions {false};
	/// What became of the last cast
	std::string _last;
};

} // namespace openblack::debug::gui
