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

#include <algorithm>
#include <array>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <vector>

namespace openblack
{

class Profiler
{
public:
	enum class Stage : uint8_t
	{
		PhysicsUpdate,
		PathfindingUpdate,
		LivingActionUpdate,
		CreatureMindUpdate,
		CreaturePlannerUpdate,
		CreatureLearningUpdate,
		CreaturePhysiologyUpdate,
		CreatureLocomotionUpdate,
		CreatureAnimationUpdate,
		CreatureTurnPose,
		CreatureHairUpdate,
		CreatureSkinUpdate,
		CreatureAudioUpdate,
		CreatureObjectActionUpdate,
		CreatureHandUpdate,
		CreatureFootprintsUpdate,
		CreatureLeashUpdate,
		CreatureCombatUpdate,
		CreatureModeUpdate,
		CreatureCaveUpdate,
		VegetationUpdate,
		ParticlesUpdate,
		MagicUpdate,
		MagicShieldsUpdate,
		ForestsUpdate,
		MiracleFxUpdate,
		MiracleGlobesUpdate,
		FireUpdate,
		ExplosionUpdate,
		ReactionsUpdate,
		AnimalsUpdate,
		TeleportUpdate,
		TornadoUpdate,
		GestureUpdate,
		SdlInput,
		UpdateUniforms,
		UpdateEntities,
		UpdateEntitiesDescs,
		UpdateEntitiesUniforms,
		UpdateEntitiesTrees,
		UpdateAudio,
		GuiLoop,
		EditorUpdate,
		GameLogic,
		SceneDraw,
		FootprintPass,
		ObjectShadowPass,
		GroundShadowPass,
		ParticlesGather,
		ReflectionPass,
		ReflectionDrawSky,
		ReflectionDrawWater,
		ReflectionDrawIsland,
		ReflectionDrawModels,
		ReflectionDrawVegetation,
		ReflectionDrawSprites,
		ReflectionDrawParticles,
		MainPass,
		MainPassDrawSky,
		MainPassDrawWater,
		MainPassDrawIsland,
		MainPassDrawModels,
		MainPassDrawVegetation,
		MainPassDrawSprites,
		MainPassDrawParticles,
		GuiDraw,
		RendererFrame,

		_count,
	};

	constexpr static std::array<std::string_view, static_cast<uint8_t>(Stage::_count)> k_StageNames = {
	    "Physics Update",       //
	    "Pathfinding Update",   //
	    "Living Action Update", //
	    "Creature Mind",        //
	    "Creature Planner",     //
	    "Creature Learning",    //
	    "Creature Physiology",  //
	    "Creature Locomotion",  //
	    "Creature Animation",   //
	    "Creature Turn Pose",   //
	    "Creature Hair",        //
	    "Creature Skin",        //
	    "Creature Audio",       //
	    "Creature Objects",     //
	    "Creature Hand",        //
	    "Creature Footprints",  //
	    "Creature Leash",       //
	    "Creature Combat",      //
	    "Creature Mode",        //
	    "Creature Cave",        //
	    "Vegetation Update",    //
	    "Particles",            //
	    "Magic",                //
	    "Magic Shields",        //
	    "Forests",              //
	    "Miracle FX",           //
	    "Miracle Globes",       //
	    "Fire",                 //
	    "Explosions",           //
	    "Reactions",            //
	    "Animals",              //
	    "Teleport",             //
	    "Tornado",              //
	    "Gestures",             //
	    "SDL Input",            //
	    "Update Uniforms",      //
	    "Entities",             //
	    "Entity Draw Lists",    //
	    "Entity Instances",     //
	    "Tree Instances",       //
	    "Audio",                //
	    "GUI Loop",             //
	    "Editor",               //
	    "Game Logic",           //
	    "Encode Draw Scene",    //
	    "Footprint Pass",       //
	    "Object Shadow Pass",   //
	    "Ground Shadow Pass",   //
	    "Gather Particles",     //
	    "Reflection Pass",      //
	    "Draw Sky",             //
	    "Draw Water",           //
	    "Draw Island",          //
	    "Draw Models",          //
	    "Draw Vegetation",      //
	    "Draw Sprites",         //
	    "Draw Particles",       //
	    "Main Pass",            //
	    "Draw Sky",             //
	    "Draw Water",           //
	    "Draw Island",          //
	    "Draw Models",          //
	    "Draw Vegetation",      //
	    "Draw Sprites",         //
	    "Draw Particles",       //
	    "Encode GUI Draw",      //
	    "Renderer Frame",       //
	};
	// Every stage has a name: a short list would leave the last ones empty
	static_assert(std::ranges::none_of(k_StageNames, &std::string_view::empty));

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
		/// The time spent in the stage over the whole frame, and how often it ran: a stage may run more than once a
		/// frame, as in a game turn and again for the frame, where start and end only hold the last run
		std::chrono::system_clock::duration total {};
		uint16_t calls = 0;
	};

	struct Entry
	{
		std::chrono::system_clock::time_point frameStart;
		std::chrono::system_clock::time_point frameEnd;
		std::array<Scope, static_cast<uint8_t>(Stage::_count)> stages;
	};

	/// The thread that makes the profiler is its main thread, whose scopes go into the entries at once
	Profiler();
	Profiler(const Profiler&) = delete;
	Profiler& operator=(const Profiler&) = delete;
	~Profiler();

	/// The scopes of the other threads go into the entry of the frame that ends, and a new entry starts. Only the main
	/// thread calls it.
	void Frame();
	/// Any thread may open and close scopes. The scopes of a thread other than the main thread wait in its own list,
	/// until the main thread merges them.
	void Begin(Stage stage);
	void End(Stage stage);
	inline ScopedSection BeginScoped(Stage stage) { return ScopedSection(this, stage); }
	/// The main thread puts the scopes that the other threads closed into the current entry, one level below its own
	/// open scopes. A scope that is still open stays with its thread.
	void MergeThreads();

	[[nodiscard]] uint8_t GetEntryIndex(int8_t offset) const { return (_currentEntry + k_BufferSize + offset) % k_BufferSize; }

	constexpr static uint8_t k_BufferSize = 100;
	std::array<Entry, k_BufferSize>& GetEntries() { return _entries; }
	[[nodiscard]] const std::array<Entry, k_BufferSize>& GetEntries() const { return _entries; }

private:
	/// A scope of another thread: open while it has no end
	struct ThreadScope
	{
		Stage stage;
		uint8_t level;
		std::chrono::system_clock::time_point start;
		std::chrono::system_clock::time_point end;
	};

	/// The scopes of one thread other than the main thread. Only that thread opens and closes them; the main thread takes
	/// the closed ones.
	struct ThreadScopes
	{
		std::thread::id thread;
		std::mutex mutex;
		std::vector<ThreadScope> open;
		std::vector<ThreadScope> closed;
	};

	[[nodiscard]] bool IsMainThread() const { return std::this_thread::get_id() == _mainThread; }
	ThreadScopes& ScopesOfThisThread();
	void MergeInto(Entry& entry);

	std::array<Entry, k_BufferSize> _entries;
	uint8_t _currentEntry = k_BufferSize - 1;
	uint8_t _currentLevel = 0;
	const std::thread::id _mainThread;
	std::mutex _threadsMutex;
	std::vector<std::unique_ptr<ThreadScopes>> _threads;
};

} // namespace openblack
