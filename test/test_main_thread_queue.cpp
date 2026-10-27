/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The fake audio is a kind of the audio that does nothing, which only the services of the game and the tests make
#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "Audio/AudioManagerNoOp.h"
#include "Audio/QueuedAudioManager.h"
#include "Common/MainThreadQueue.h"

using openblack::DirectCall;
using openblack::MainThreadApi;
using openblack::MainThreadQueue;
using openblack::TurnApiCheck;

namespace
{

/// An audio that keeps the names of the calls that reach it, in their order
class RecordingAudio final: public openblack::audio::AudioManagerNoOp
{
public:
	explicit RecordingAudio(std::vector<std::string>& calls)
	    : _calls(calls)
	{
	}
	void PlaySound(entt::id_type id, openblack::audio::PlayType /*type*/) override
	{
		_calls.push_back("PlaySound " + std::to_string(id));
	}
	void StopEmitter(entt::entity emitter) override
	{
		_calls.push_back("StopEmitter " + std::to_string(entt::to_integral(emitter)));
	}
	void EndAnimEffect(const std::string& bankName, std::span<const int32_t> keys, entt::entity /*owner*/,
	                   const glm::vec3& /*position*/, openblack::audio::AnimEffectEnd /*end*/,
	                   openblack::audio::AnimEffectMeasure /*measure*/) override
	{
		_calls.push_back("EndAnimEffect " + bankName + " " + std::to_string(keys.size()));
	}
	entt::entity StartSoundEffect(entt::id_type id, const openblack::audio::SoundEffectOptions& /*options*/) override
	{
		_calls.push_back("StartSoundEffect " + std::to_string(id));
		return static_cast<entt::entity>(id);
	}

private:
	std::vector<std::string>& _calls;
};

} // namespace

TEST(MainThreadQueue, APostOnTheMainThreadRunsAtOnceInItsPlace)
{
	MainThreadQueue queue;
	std::vector<int> order;
	queue.Post(MainThreadApi::Graphics, [&order] { order.push_back(1); });
	order.push_back(2);
	queue.Post(MainThreadApi::Audio, [&order] { order.push_back(3); });
	EXPECT_EQ(order, (std::vector<int> {1, 2, 3}));
	EXPECT_EQ(queue.GetPendingCount(), 0U);
	EXPECT_EQ(queue.RunPending(), 0U);
}

TEST(MainThreadQueue, ATurnOnTheMainThreadRunsItsPostsAtOnce)
{
	MainThreadQueue queue;
	std::vector<int> order;
	{
		const MainThreadQueue::TurnScope turn(queue);
		EXPECT_TRUE(queue.RunsAtOnce());
		queue.Post(MainThreadApi::Renderer, [&order] { order.push_back(1); });
		order.push_back(2);
	}
	EXPECT_EQ(order, (std::vector<int> {1, 2}));
	EXPECT_EQ(queue.GetPendingCount(), 0U);
}

TEST(MainThreadQueue, ADeferralKeepsTheWorkInTheOrderOfThePosts)
{
	MainThreadQueue queue;
	std::vector<int> order;
	{
		const MainThreadQueue::Deferral deferral(queue);
		EXPECT_FALSE(queue.RunsAtOnce());
		queue.Post(MainThreadApi::Graphics, [&order] { order.push_back(1); });
		queue.Post(MainThreadApi::Audio, [&order] { order.push_back(2); });
		queue.Post(MainThreadApi::Audio, [&order] { order.push_back(3); });
		EXPECT_TRUE(order.empty());
		const auto byApi = queue.GetPendingByApi();
		EXPECT_EQ(byApi.at(static_cast<size_t>(MainThreadApi::Graphics)), 1U);
		EXPECT_EQ(byApi.at(static_cast<size_t>(MainThreadApi::Renderer)), 0U);
		EXPECT_EQ(byApi.at(static_cast<size_t>(MainThreadApi::Audio)), 2U);
	}
	EXPECT_TRUE(queue.RunsAtOnce());
	EXPECT_EQ(queue.RunPending(), 3U);
	EXPECT_EQ(order, (std::vector<int> {1, 2, 3}));
	EXPECT_EQ(queue.GetPendingCount(), 0U);
}

TEST(MainThreadQueue, WorkThatPostsMoreWorkIsDoneInTheSameRun)
{
	MainThreadQueue queue;
	std::vector<int> order;
	{
		const MainThreadQueue::Deferral deferral(queue);
		queue.Post(MainThreadApi::Graphics, [&queue, &order] {
			order.push_back(1);
			// The run takes the work again, so this waits for its turn after the work posted before it
			const MainThreadQueue::Deferral inner(queue);
			queue.Post(MainThreadApi::Graphics, [&order] { order.push_back(3); });
		});
		queue.Post(MainThreadApi::Graphics, [&order] { order.push_back(2); });
	}
	EXPECT_EQ(queue.RunPending(), 3U);
	EXPECT_EQ(order, (std::vector<int> {1, 2, 3}));
}

TEST(MainThreadQueue, ATurnOnAnotherThreadWaitsForTheMainThread)
{
	MainThreadQueue queue;
	std::vector<int> order;
	std::vector<std::thread::id> threads;
	std::thread worker([&] {
		const MainThreadQueue::TurnScope turn(queue);
		EXPECT_FALSE(queue.IsMainThread());
		EXPECT_FALSE(queue.RunsAtOnce());
		for (int i = 1; i <= 3; ++i)
		{
			queue.Post(MainThreadApi::Audio, [&order, &threads, i] {
				order.push_back(i);
				threads.push_back(std::this_thread::get_id());
			});
		}
	});
	worker.join();
	EXPECT_TRUE(order.empty());
	EXPECT_EQ(queue.GetPendingCount(), 3U);
	EXPECT_EQ(queue.RunPending(), 3U);
	EXPECT_EQ(order, (std::vector<int> {1, 2, 3}));
	for (const auto& thread : threads)
	{
		EXPECT_EQ(thread, std::this_thread::get_id());
	}
}

TEST(MainThreadQueue, AnotherThreadOutsideATurnKeepsItsDirectCalls)
{
	MainThreadQueue queue;
	int done = 0;
	std::thread other([&] {
		EXPECT_TRUE(queue.RunsAtOnce());
		queue.Post(MainThreadApi::Audio, [&done] { ++done; });
	});
	other.join();
	EXPECT_EQ(done, 1);
	EXPECT_EQ(queue.GetPendingCount(), 0U);
}

TEST(MainThreadQueue, TheCheckCountsOnlyTheDirectCallsOfTurnCode)
{
#if !defined(OPENBLACK_DEVELOPER_TOOLS)
	GTEST_SKIP() << "The check is only in builds with the developer tools";
#endif
	MainThreadQueue queue;
	queue.SetCheck(TurnApiCheck::Report);
	// Frame code may use the services
	MainThreadQueue::CheckDirect(MainThreadApi::Audio, "PlaySound");
	EXPECT_TRUE(queue.GetDirectCalls().empty());
	{
		const MainThreadQueue::TurnScope turn(queue);
		queue.SetTurnStep(7);
		EXPECT_TRUE(queue.InTurnCode());
		for (int i = 0; i < 2; ++i)
		{
			MainThreadQueue::CheckDirect(MainThreadApi::Audio, "GetStatus");
		}
		// The work from the queue may use the services, also in a turn
		queue.Post(MainThreadApi::Renderer, [&queue] {
			EXPECT_FALSE(queue.InTurnCode());
			MainThreadQueue::CheckDirect(MainThreadApi::Renderer, "RequestScreenPicture");
		});
		queue.SetTurnStep(9);
		MainThreadQueue::CheckDirect(MainThreadApi::Graphics, "Texture2D::Update");
	}
	EXPECT_FALSE(queue.InTurnCode());
	const auto calls = queue.GetDirectCalls();
	ASSERT_EQ(calls.size(), 2U);
	EXPECT_EQ(calls[0].api, MainThreadApi::Audio);
	EXPECT_EQ(calls[0].what, "GetStatus");
	EXPECT_EQ(calls[0].turnStep, 7U);
	EXPECT_EQ(calls[0].count, 2U);
	EXPECT_EQ(calls[1].api, MainThreadApi::Graphics);
	EXPECT_EQ(calls[1].turnStep, 9U);
	EXPECT_EQ(calls[1].count, 1U);
	queue.ClearDirectCalls();
	EXPECT_TRUE(queue.GetDirectCalls().empty());
}

TEST(MainThreadQueue, TheCheckIsOffUntilItIsAskedFor)
{
	MainThreadQueue queue;
	EXPECT_EQ(queue.GetCheck(), TurnApiCheck::Off);
	const MainThreadQueue::TurnScope turn(queue);
	MainThreadQueue::CheckDirect(MainThreadApi::Audio, "GetStatus");
	EXPECT_TRUE(queue.GetDirectCalls().empty());
}

TEST(MainThreadQueue, TheFailCheckCallsItsHandlerAtTheFirstCallOfAPlace)
{
#if !defined(OPENBLACK_DEVELOPER_TOOLS)
	GTEST_SKIP() << "The check is only in builds with the developer tools";
#endif
	MainThreadQueue queue;
	queue.SetCheck(TurnApiCheck::Fail);
	std::vector<std::string> failed;
	queue.SetFailHandler([&failed](const DirectCall& call) { failed.push_back(call.what); });
	const MainThreadQueue::TurnScope turn(queue);
	MainThreadQueue::CheckDirect(MainThreadApi::Renderer, "Renderer::Frame");
	MainThreadQueue::CheckDirect(MainThreadApi::Renderer, "Renderer::Frame");
	EXPECT_EQ(failed, (std::vector<std::string> {"Renderer::Frame"}));
}

TEST(MainThreadQueue, TheCheckSeesATurnOnAnotherThread)
{
#if !defined(OPENBLACK_DEVELOPER_TOOLS)
	GTEST_SKIP() << "The check is only in builds with the developer tools";
#endif
	MainThreadQueue queue;
	queue.SetCheck(TurnApiCheck::Report);
	std::thread worker([&queue] {
		const MainThreadQueue::TurnScope turn(queue);
		MainThreadQueue::CheckDirect(MainThreadApi::Audio, "EmitterExists");
	});
	worker.join();
	// The main thread is in no turn
	MainThreadQueue::CheckDirect(MainThreadApi::Audio, "EmitterExists");
	const auto calls = queue.GetDirectCalls();
	ASSERT_EQ(calls.size(), 1U);
	EXPECT_EQ(calls[0].count, 1U);
	EXPECT_FALSE(calls[0].turnStep.has_value());
}

TEST(QueuedAudioManager, TheCallsReachTheAudioInTheirOrder)
{
	MainThreadQueue queue;
	std::vector<std::string> calls;
	openblack::audio::QueuedAudioManager audio(std::make_unique<RecordingAudio>(calls), queue);
	const std::vector<int32_t> keys {4, 5};
	{
		const MainThreadQueue::TurnScope turn(queue);
		audio.PlaySound(11, openblack::audio::PlayType::Once);
		EXPECT_EQ(audio.StartSoundEffect(12, {}), static_cast<entt::entity>(12));
		audio.EndAnimEffect("bank", keys, entt::null, glm::vec3(0.0f), openblack::audio::AnimEffectEnd {},
		                    openblack::audio::AnimEffectMeasure::Straight);
		audio.StopEmitter(static_cast<entt::entity>(13));
	}
	EXPECT_EQ(calls,
	          (std::vector<std::string> {"PlaySound 11", "StartSoundEffect 12", "EndAnimEffect bank 2", "StopEmitter 13"}));
}

TEST(QueuedAudioManager, TheCallsThatGiveNothingBackWaitWhileTheQueueDefers)
{
	MainThreadQueue queue;
	std::vector<std::string> calls;
	openblack::audio::QueuedAudioManager audio(std::make_unique<RecordingAudio>(calls), queue);
	{
		const MainThreadQueue::Deferral deferral(queue);
		std::vector<int32_t> keys {1, 2, 3};
		audio.EndAnimEffect("bank", keys, entt::null, glm::vec3(0.0f), openblack::audio::AnimEffectEnd {},
		                    openblack::audio::AnimEffectMeasure::Straight);
		// The post keeps its own copy of the keys
		keys.clear();
		audio.PlaySound(21, openblack::audio::PlayType::Once);
		EXPECT_TRUE(calls.empty());
	}
	EXPECT_EQ(queue.RunPending(), 2U);
	EXPECT_EQ(calls, (std::vector<std::string> {"EndAnimEffect bank 3", "PlaySound 21"}));
}

TEST(QueuedAudioManager, TheCheckCountsTheCallsThatGiveBackAValue)
{
#if !defined(OPENBLACK_DEVELOPER_TOOLS)
	GTEST_SKIP() << "The check is only in builds with the developer tools";
#endif
	MainThreadQueue queue;
	queue.SetCheck(TurnApiCheck::Report);
	std::vector<std::string> calls;
	openblack::audio::QueuedAudioManager audio(std::make_unique<RecordingAudio>(calls), queue);
	{
		const MainThreadQueue::TurnScope turn(queue);
		audio.PlaySound(31, openblack::audio::PlayType::Once);
		audio.StopEmitter(static_cast<entt::entity>(32));
		[[maybe_unused]] const auto emitter = audio.StartSoundEffect(33, {});
	}
	const auto direct = queue.GetDirectCalls();
	ASSERT_EQ(direct.size(), 1U);
	EXPECT_EQ(direct[0].api, MainThreadApi::Audio);
	EXPECT_EQ(direct[0].what, "StartSoundEffect");
}
