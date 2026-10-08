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

#include <array>
#include <functional>
#include <memory>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "GestureBuffer.h"
#include "GestureMatch.h"
#include "GestureTemplates.h"

// What the interface looks for, and when: the circle that sizes a storm or a shield, the power-up gestures of a seed
// in the hand, the scribble that cancels, the spiral that opens the miracle selection, the selection's stages and R,
// "repeat the last miracle". The worship code registers the worship icons as an IconProvider (SetIconProvider); with
// none, the selection never opens.
// Wiki: docs/bw1-notes/magic.md, "Gestures".

namespace openblack::magic::gestures
{
/// One entry per gesture, only for the HUD gesture icons
struct LookingFor
{
	/// Display order: 2 leash start, 8 spiral, 9 inverse spiral, 0xA repeat, 0xB selection stage, 0xC circle,
	/// 0xD power down, 0xE + k power-up k (0 = not looked for)
	int type {0};
	int seed {-1};
	int leash {-1};
	int powerUp {-1};
};
using LookingForArray = std::array<LookingFor, k_GestureCount>;

/// The local player's worship-site spell icons, as the interface asks for them. The worship code implements it and
/// registers it with SetIconProvider.
class IconProvider
{
public:
	IconProvider() = default;
	IconProvider(const IconProvider&) = default;
	IconProvider(IconProvider&&) = default;
	IconProvider& operator=(const IconProvider&) = default;
	IconProvider& operator=(IconProvider&&) = default;
	virtual ~IconProvider() = default;

	/// An icon whose seed's selectionGesture is `category` can be requested
	[[nodiscard]] virtual bool AnyRequestableIconOfCategory(Gesture category) const = 0;
	/// Walks the icons of the player's worship sites: every icon whose seed's selectionGesture is `category` and that is
	/// valid for a spell request
	virtual void ForEachRequestableIcon(Gesture category, const std::function<void(int seedType)>& visit) const = 0;
	/// The icon of this seed type can be requested
	[[nodiscard]] virtual bool IconValidForRequest(int seedType) const = 0;
	/// IconValidForRequest, then the best icon for that seed requests its spell
	virtual void RequestSpell(int seedType) = 0;

	/// R may repeat the last miracle
	[[nodiscard]] virtual bool CanRepeat() const { return false; }
	/// The same request with the interface's last seed type
	virtual void RepeatLast(int /*lastSeedType*/) {}
	/// One of the player's icons is charging for this hand
	[[nodiscard]] virtual bool AnyIconChargingForHand() const { return false; }
	/// The largest charge fraction of those icons, for the hand effect's charge bands
	[[nodiscard]] virtual float MaxChargeFraction() const { return 0.0f; }
	/// Cancels the charge of the most charged icon charging for this hand
	virtual void CancelMostChargedIcon() {}
	/// The icon can charge that power-up level
	[[nodiscard]] virtual bool PowerUpAvailable(entt::entity /*icon*/, int /*powerUp*/) const { return false; }
	/// Sets the power-up level being charged (-1 = power down)
	virtual void SetPowerUpCharge(entt::entity /*icon*/, int /*powerUp*/) {}
};

/// nullptr (the default) = no worship icons. The gesture state owns the provider
void SetIconProvider(std::unique_ptr<IconProvider> provider);
[[nodiscard]] IconProvider* GetIconProvider();

/// The seed info fields the selection reads, so that it can run without the info tables in tests
struct SelectionTables
{
	std::function<Gesture(int seedType)> selectionGesture;
	std::function<Gesture(int seedType)> gesture;
	std::function<Gesture(int seedType)> gestureStage2;
	/// From info.dat's GSpellSeedInfo
	[[nodiscard]] static SelectionTables FromInfo();
};

/// The miracle selection
struct Selection
{
	static constexpr size_t k_Seeds = 30;
	std::array<std::array<Gesture, 3>, k_Seeds> seedStage {}; ///< {selection, gesture, stage 2} per seed type
	std::array<bool, k_Seeds> candidate {};
	std::array<bool, k_GestureCount> activeGesture {};
	uint32_t stage {0};
	bool open {false};
	float timer {0.0f};
	Gesture category {k_None};

	/// The requestable icons of the category become the candidates; stage 1.
	/// Returns whether any was found (then the caller sets showHeldGestureTrail).
	bool Open(Gesture category, const IconProvider& icons, const SelectionTables& tables);

	enum class Outcome
	{
		None,      ///< nothing recognised (0)
		Cancelled, ///< SCRIBBLE: Success(0), help event 0x15 (1)
		StageOk,   ///< a stage gesture: Success(1), help event 0x10; the next stage waits (0)
		Requested, ///< the last stage: Success(1), help events 0x10 and 0x11, spell requested (1)
	};
	/// The 30 s timeout (dt = the frame's game time, not while paused), SCRIBBLE, then the first
	/// active gesture 1..23 recognised narrows the candidates
	Outcome Stage(float dt, float timeOut, const std::function<bool(Gesture)>& recognise, IconProvider& icons,
	              LookingForArray* lookingFor = nullptr);
};

/// The interface's gesture fields and the game's gesture system
struct InterfaceGestures
{
	GestureSystem system;
	Result result;
	Packet gesture; ///< Its size is the next cast's magnitude
	std::array<Gesture, 3> powerUpGestures {};
	Gesture currentPowerUpGesture {k_None};
	bool circlePending {false};
	glm::vec3 circlePosition {0.0f}; ///< World point
	float circleSize {0.0f};
	Gesture circleGesture {k_None};
	float circleTimer {0.0f}; ///< The circle is forgotten after 5 s
	Selection selection;
	float cooldown {0.0f}; ///< 0.4 s after each Success
	/// Set by SetupPowerUpGestures and by an opened selection (the gesture trail shows while a seed
	/// is held with it)
	bool showHeldGestureTrail {false};
	LookingForArray lookingFor {}; ///< The last table for the HUD gesture icons
	int lastSeedType {-1};         ///< The R repeat's seed
};

[[nodiscard]] InterfaceGestures& State();
/// A land is loaded: everything cleared
void Reset();

/// count = head = stationary = 0
void ClearBuffer();
/// A mouse move: nothing while paused or in the 0.4 s cooldown; the land
/// point under the cursor, or the newest sample's again if it is off the land
void FeedSample(glm::ivec2 mouse);
/// Clear, then the current mouse position again (BeginApplyOnRelease, SetupPowerUpGestures)
void ReseedBuffer();
/// BuildFromSystem, then MatchGesture into the game's result
bool Recognise(Gesture gesture);
/// Success -> the recognised sparkles and force feedback; always the buffer wiped and the
/// 0.4 s cooldown
void Success(bool success);

/// The seed's first turn in the hand: TrySetupPowerUpGestures, showHeldGestureTrail set, the buffer reseeded,
/// currentPowerUpGesture = the gesture of the seed's power-up level's magic
void SetupPowerUpGestures();
/// The circle forgotten; puGesture[k] = the seed's powerUpGestures[k] when the player has that level's magic
/// and the icon can charge it. False without a seed in the hand.
bool TrySetupPowerUpGestures();
/// A ready seed in the hand, not cast yet, from a worship icon
[[nodiscard]] bool HoldingChargingSeed();
/// currentPowerUpGesture while holding a charging seed, else 0
[[nodiscard]] Gesture PowerUpLevelGesture();

/// dt = the frame's game time in seconds, 0 while paused.
/// It runs at the end of every interface action pass: once per frame and once per game turn.
void ProcessPowerUpSystem(float dt);

/// What the hand tells the interface; HandSpellSeed.cpp fills it every frame
struct HandStatus
{
	entt::entity heldSeed {entt::null}; ///< The SpellSeed in the hand
	bool holdingSomething {false};
	bool validToShake {false};  ///< The held object can be shaken out of the hand
	bool handReady {false};     ///< The hand is ready for an object
	bool inInfluence {false};   ///< The hand is inside the player's influence
	bool actionLatched {false}; ///< The action press not released yet
	bool paused {false};
	glm::ivec2 mouse {0};
	std::function<void()> forceDropHeld;    ///< Drops the held object
	std::function<void()> removeFromHandFx; ///< The hand effect's remove-from-hand visual
};
void SetHandStatus(HandStatus status);
[[nodiscard]] const HandStatus& GetHandStatus();
} // namespace openblack::magic::gestures
