/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <deque>
#include <memory>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <entt/resource/resource.hpp>

#include "ECS/Systems/GestureSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack
{
class CameraPath;
} // namespace openblack

namespace openblack::gestures
{
class GestureFile;
}

namespace openblack::ecs::systems
{

class GestureSystem final: public GestureSystemInterface
{
public:
	GestureSystem();
	~GestureSystem() override;

	// GestureEventsInterface
	[[nodiscard]] std::vector<GestureEvent> TakeEvents() override;
	void Inject(const GestureEvent& event) override;
	void Reset() override;

	// GestureSystemInterface
	void Update(const Frame& frame) override;
	void InterfaceTurn(const Frame& frame) override;
	/// The game's templates, loaded through the resource caches the first time they are wanted
	void LoadTemplates() override;
	void DrawPath(std::vector<glm::vec2> path, bool holdingAction) override;
	[[nodiscard]] bool IsDrawingPath() const override { return !_drawing.empty(); }
	[[nodiscard]] std::optional<glm::vec2> GetDrawingPoint() const override
	{
		return _drawing.empty() ? std::nullopt : std::optional(_drawing.front());
	}
	void ForgetPath() override;
	void SeedFirstTurnInHand() override;
	[[nodiscard]] std::span<const gestures::GestureTemplate> GetTemplates() const override;
	[[nodiscard]] const gesture::GestureRecorder& GetPath() const override { return _recorder; }
	[[nodiscard]] float GetScreenAspect() const override { return _screenAspect; }
	[[nodiscard]] const std::vector<gesture::Request>& GetRequests() const override { return _requests; }
	[[nodiscard]] std::optional<Recognised> GetLastRecognised() const override { return _lastRecognised; }
	[[nodiscard]] bool IsLeashPickerOpen() const override { return _picker.open; }
	[[nodiscard]] bool IsSelectionOpen() const override { return _selection.IsOpen(); }
	bool OpenSelectionByKey(PlayerNames player) override;
	bool RepeatMiracleByKey(PlayerNames player) override;
	[[nodiscard]] bool IsSelecting() const override { return _selection.IsOpen() || _picker.open; }
	[[nodiscard]] float GetCircleSecondsLeft() const override { return _circleSeconds; }
	[[nodiscard]] GestureType GetAskedPowerUp() const override { return _askedPowerUp; }
	void PoweredUpCast() override { StartPowerUps(); }
	[[nodiscard]] bool IsGesturing() const override { return _gesturing; }
	[[nodiscard]] const gesture_hints::Row& GetHints() const override { return _hints; }
	void StepHints(float seconds, bool wordsShowing) override;
	void ShowGestureOnLand(GestureType gesture, glm::vec3 corner, float size, std::optional<glm::vec3> heardAt) override;
	[[nodiscard]] std::optional<gesture::TrailSymbol> GetSymbol(GestureType gesture) const override;

private:
	/// What the hand holds and the player's creature, for the requests
	[[nodiscard]] gesture::HandContext ContextOf(const Frame& frame);
	/// Carries out a recognised gesture. Returns false when the gesture did nothing and the gestures after it are
	/// still tried.
	bool Act(const gesture::Request& request, const gesture::Match& match, const Frame& frame);
	/// Opens the miracle selection with a gesture, for the miracles that the player's icons offer for it
	bool OpenSelection(GestureType opening, PlayerNames player);
	/// The miracle selection waits for its gestures this frame, closes when the hand is not ready, and its time passes
	void StepSelection(gesture::HandContext& context, const Frame& frame);
	/// Takes a sample at a place of the pointer, or at the next point of a path drawn for the testbed
	void SampleCursor(glm::vec2 cursor, const Frame& frame);
	/// A pass of the interface: the samples, then the gesture step so many times
	void Pass(const Frame& frame, uint32_t steps);
	/// The gesture step: the times count down, the camera and the Move button can wipe the path, then the step
	/// recognises the path. The first step of a pass samples the places first.
	void Step(const Frame& frame, bool samples, std::optional<glm::vec2> placeBefore);
	/// Acts on the first gesture waited for that the path draws
	void Recognise(const Frame& frame, const gesture::HandContext& context);
	/// A point on the screen that the path uses (768 pixels high) from a point in window pixels
	[[nodiscard]] static glm::vec2 ToReference(glm::vec2 pixel, const Frame& frame);
	/// One point on the screen that the path uses goes on the path, with the land under it. Nothing goes on the path
	/// while the rest after a gesture continues. Whether the point went on the path.
	bool RecordPoint(glm::vec2 point, const Frame& frame);
	/// The recognised gesture's trail on the land, for the particles to show
	void LayTrail(GestureType gesture, const gesture::Match& match, const Frame& frame) const;
	/// The hand starts its power-ups for the seed in it: it forgets the circle remembered and the power-up asked for, and
	/// finds the power-up gestures that it offers for the seed
	void StartPowerUps();
	/// The hints for what the hand waits for now, replacing those shown when they differ
	void RefreshHints();
	/// The shape a gesture's trail takes on the land, from its symbol's file; the circle's for a gesture without one
	[[nodiscard]] std::optional<gesture::TrailSymbol> TrailSymbolOf(GestureType gesture) const;

	/// A symbol made from the points of a path. A computer god asks for the symbols of its gestures in each turn.
	struct KeptSymbol
	{
		entt::resource<const CameraPath> path;
		gesture::TrailSymbol symbol;
	};

	std::vector<GestureEvent> _events;
	std::shared_ptr<const gestures::GestureFile> _templates;
	bool _templatesTried {false};
	gesture::GestureRecorder _recorder;
	/// Points of a path being drawn for the testbed, taken in place of the cursor
	std::deque<glm::vec2> _drawing;
	bool _drawingHoldsAction {false};
	/// The last place of the pointer that the interface got, in window pixels
	std::optional<glm::vec2> _lastPlace;
	/// The path started again for a miracle's first turn in the hand: the next update takes a sample at the last place
	bool _sampleLastPlace {false};
	/// Where the camera's eye was last frame
	std::optional<glm::vec3> _lastCameraEye;
	/// Whether the camera moved in its last update
	bool _cameraMoved {false};
	/// The rest after a gesture, while nothing is recorded
	float _pause {0.0f};
	float _screenAspect {4.0f / 3.0f};
	std::vector<gesture::Request> _requests;
	std::optional<Recognised> _lastRecognised;
	uint32_t _recognisedCount {0};
	gesture::LeashPicker _picker;
	/// The leash gesture put the leash on a creature that knows more than one leash. The picker stays open while this
	/// is set, also before the leash is on.
	bool _leashPutOnByGesture {false};
	/// A miracle did its first turn in the hand on this land, or the miracle selection opened on this land: the chain
	/// follows the hand while it holds any miracle
	bool _seedTurnedOnLand {false};
	gesture::MiracleSelection _selection;
	/// The circle drawn for the seed in the hand, while it is remembered
	float _circleSeconds {0.0f};
	bool _gesturing {false};
	/// The power-up gestures that the hand offers for the seed in it, found when the hand last started its power-ups
	std::array<GestureType, 3> _powerUpGestures {GestureType::None, GestureType::None, GestureType::None};
	/// The gesture of the power-up that the hand asked for, none when it asked for none
	GestureType _askedPowerUp {GestureType::None};
	gesture_hints::Row _hints;
	/// The symbols made so far, one for each path
	mutable std::vector<KeptSymbol> _symbols;
};

} // namespace openblack::ecs::systems
