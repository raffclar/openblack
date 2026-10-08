/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <compare>
#include <cstdint>

#include <array>
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace openblack
{

class Profiler
{
public:
	enum class Stage : uint8_t
	{
		PhysicsUpdate,
		LivingActionUpdate,
		SdlInput,
		UpdateUniforms,
		UpdateEntities,
		UpdateAudio,
		GuiLoop,
		EditorUpdate,
		GameLogic,
		ScriptsUpdate,
		CameraUpdate,
		HandRayCast,
		HandUpdate,
		HandPlace,
		SceneDraw,
		FootprintPass,
		ReflectionPass,
		ReflectionDrawSky,
		ReflectionDrawWater,
		ReflectionDrawIsland,
		ReflectionDrawModels,
		ReflectionDrawSprites,
		MainPass,
		MainPassDrawSky,
		MainPassDrawWater,
		MainPassDrawIsland,
		MainPassDrawModels,
		MainPassDrawSprites,
		GuiDraw,
		RendererFrame,
		FrameUpdaters,
		TurnMapRebuild,
		TurnMagic,
		TurnPhysicsObjects,
		TurnParticles,
		MagicFrame,
		PreDraw,
		DrawDescs,
		DrawUniforms,
		DrawUpload,
		CreaturePhysiologyUpdate,
		CreatureLeashUpdate,
		CreatureMindUpdate,
		CreaturePlannerUpdate,
		CreatureLearningUpdate,
		CreatureLocomotionUpdate,
		CreatureObjectActionUpdate,
		CreatureCombatUpdate,
		CreatureFrame,
		CreatureLeashFrame,

		_count,
	};

	constexpr static std::array<std::string_view, static_cast<uint8_t>(Stage::_count)> k_StageNames = {
	    "Physics Update",       //
	    "Living Action Update", //
	    "SDL Input",            //
	    "Update Uniforms",      //
	    "Entities",             //
	    "Audio",                //
	    "GUI Loop",             //
	    "Editor",               //
	    "Game Logic",           //
	    "Scripts (LHVM)",       //
	    "Camera",               //
	    "Hand Ray Cast",        //
	    "Hand Update",          //
	    "Hand Placement",       //
	    "Encode Draw Scene",    //
	    "Footprint Pass",       //
	    "Reflection Pass",      //
	    "Draw Sky",             //
	    "Draw Water",           //
	    "Draw Island",          //
	    "Draw Models",          //
	    "Draw Sprites",         //
	    "Main Pass",            //
	    "Draw Sky",             //
	    "Draw Water",           //
	    "Draw Island",          //
	    "Draw Models",          //
	    "Draw Sprites",         //
	    "Encode GUI Draw",      //
	    "Renderer Frame",       //
	    "Frame Updaters",       //
	    "Turn: Map Rebuild",    //
	    "Turn: Magic",          //
	    "Turn: Physics",        //
	    "Turn: Particles",      //
	    "Magic (frame)",        //
	    "Pre Draw",             //
	    "Draw Descs",           //
	    "Draw Uniforms",        //
	    "Draw Upload",          //

	    "Creature Physiology",    //
	    "Creature Leash",         //
	    "Creature Mind",          //
	    "Creature Planner",       //
	    "Creature Learning",      //
	    "Creature Locomotion",    //
	    "Creature Objects",       //
	    "Creature Combat",        //
	    "Creature (frame)",       //
	    "Creature Leash (frame)", //
	};

private:
	struct ScopedSection
	{
		inline explicit ScopedSection(Profiler* profiler, Stage stage)
		    : profiler(profiler)
		    , stage(stage)
		{
			profiler->Begin(stage);
		}
		inline ~ScopedSection() { profiler->End(stage); }

		Profiler* const profiler;
		const Stage stage;
	};

public:
	struct Scope
	{
		uint8_t level;
		std::chrono::system_clock::time_point start;
		std::chrono::system_clock::time_point end;
		bool finalized = false;
	};

	struct Entry
	{
		std::chrono::system_clock::time_point frameStart;
		std::chrono::system_clock::time_point frameEnd;
		std::array<Scope, static_cast<uint8_t>(Stage::_count)> stages;
	};

	void Frame();
	void Begin(Stage stage);
	void End(Stage stage);
	inline ScopedSection BeginScoped(Stage stage) { return ScopedSection(this, stage); }

	/// Every `seconds`, log the average and worst time of each stage per frame (0 turns it off).
	/// Set from the OPENBLACK_PROFILE environment variable (seconds) at start-up.
	void SetSummaryInterval(float seconds) { _summaryInterval = seconds; }

	/// (openblack engine) Counters the summary logs after the stages, per interval (the incremental PrepareDraw).
	/// Counted only while the summary is on; off, a call
	/// is one test (Counting)
	enum class Counter : uint8_t
	{
		DrawRebuilds,      ///< full PrepareDraw rebuilds of the instances
		DrawRebuildsDirty, ///< of them, after a Registry::SetDirty (the others: a debug view turned on or off)
		DrawRows,          ///< the instance rows they wrote: the end of the last range (the debug boxes left out)
		DrawUploadBytes,   ///< the bytes they gave bgfx (UploadInstances: the rows up to the end of the last range, the whole
		                   ///< capacity with the debug boxes, 80 a row)
		SetDirtyCalls,     ///< Registry::SetDirty calls, those of Assign / AssignOrReplace / Remove / Reset too
		DrawRefills,       ///< PrepareDraws that wrote the instances again into the ranges they had
		DrawRefillRows,    ///< the rows they wrote (the end of the last range)
		DrawRefillBytes,   ///< the bytes they gave bgfx

		_count,
	};
	[[nodiscard]] bool Counting() const { return _summaryInterval > 0.0f; }
	/// A Registry::SetDirty, by its caller: a file and line, or (`line` 0) an operation and its component ("Assign",
	/// the component's type name). The first one since the last rebuild is taken as that rebuild's reason
	void CountDirty(std::string_view where, std::string_view what, uint32_t line);
	/// A full PrepareDraw rebuild (`dirty`: after a SetDirty, else a debug view's) of `rows` rows that gave bgfx
	/// `uploadBytes`
	void CountDrawRebuild(bool dirty, uint64_t rows, uint64_t uploadBytes);
	/// A PrepareDraw that wrote the instances again into the ranges they had (no rebuild of the draw lists), `rows` rows
	/// that gave bgfx `uploadBytes`
	void CountDrawRefill(uint64_t rows, uint64_t uploadBytes);

	[[nodiscard]] uint8_t GetEntryIndex(int8_t offset) const { return (_currentEntry + k_BufferSize + offset) % k_BufferSize; }

	constexpr static uint8_t k_BufferSize = 100;
	std::array<Entry, k_BufferSize>& GetEntries() { return _entries; }
	[[nodiscard]] const std::array<Entry, k_BufferSize>& GetEntries() const { return _entries; }

private:
	std::array<Entry, k_BufferSize> _entries;
	uint8_t _currentEntry = k_BufferSize - 1;
	uint8_t _currentLevel = 0;

	void Accumulate(const Entry& entry);
	float _summaryInterval = 0.0f;
	struct StageTotals
	{
		double total = 0.0; // ms
		double worst = 0.0; // ms
		uint32_t runs = 0;
	};
	std::array<StageTotals, static_cast<uint8_t>(Stage::_count)> _totals {};
	double _framesTotal = 0.0; // ms
	double _framesWorst = 0.0; // ms
	uint32_t _frames = 0;
	std::chrono::system_clock::time_point _summaryStart {};

	void AppendCounters(std::string& text) const;
	std::array<uint64_t, static_cast<uint8_t>(Counter::_count)> _counters {};
	/// CountDirty's callers: the strings are static (source_location's file names, entt's type names)
	struct DirtyCaller
	{
		std::string_view where;
		std::string_view what;
		uint32_t line = 0;
		auto operator<=>(const DirtyCaller&) const = default;
	};
	struct DirtyCount
	{
		uint64_t calls = 0;
		uint64_t rebuilds = 0; ///< the rebuilds it was the first SetDirty of
	};
	std::map<DirtyCaller, DirtyCount> _dirtyCallers;
	std::optional<DirtyCaller> _firstDirty; ///< since the last rebuild
};

} // namespace openblack
