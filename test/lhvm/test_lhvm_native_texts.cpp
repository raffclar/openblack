/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdlib>

#include <string>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>

using namespace openblack::lhvm;

namespace
{

/// A program whose data holds one text of its own, as a compiled script's texts are kept
LHVMFile ProgramWithOneText()
{
	const std::vector<VMInstruction> code {{Opcode::End, VMMode::Immediate, DataType::Int, VMValue(uint32_t {0}), 0}};
	const std::vector<char> data {'L', 'a', 'n', 'd', '1', '\0'};
	return {LHVMVersion::BlackAndWhite, {}, code, {}, {}, data};
}

/// A machine with two natives that read texts as the game's do: through the place a call pushes for each
class LhvmNativeTexts: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_vm.Initialise(&_natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		ASSERT_EQ(_vm.LoadBinary(ProgramWithOneText()), EXIT_SUCCESS);
	}

	std::vector<std::string> _read;
	LHVM _vm;
	std::vector<NativeFunction> _natives {
	    NativeFunction(nullptr, 0, 0, "NONE"),
	    // Takes one text
	    NativeFunction([this] { _read.push_back(_vm.GetString(_vm.Pop().uintVal)); }, 1, 0, "READ_TEXT"),
	    // Takes a number then a text, and gives back the text's length
	    NativeFunction(
	        [this] {
		        const auto text = _vm.GetString(_vm.Pop().uintVal);
		        const auto number = _vm.Popf();
		        _read.push_back(text);
		        _vm.Pushf(number + static_cast<float>(text.size()));
	        },
	        2, 1, "COUNT_TEXT"),
	};
};

} // namespace

TEST_F(LhvmNativeTexts, ANativeReadsATextGivenToItsCall)
{
	const std::vector<NativeArgument> arguments {std::string("Land1Zone2.exc")};
	std::vector<std::pair<VMValue, DataType>> results;
	ASSERT_TRUE(_vm.CallNative(1, arguments, results));
	ASSERT_EQ(_read.size(), 1u);
	EXPECT_EQ(_read[0], "Land1Zone2.exc");
	EXPECT_TRUE(results.empty());
}

TEST_F(LhvmNativeTexts, TextsAndValuesGoInTheOrderGiven)
{
	const std::vector<NativeArgument> arguments {std::pair(VMValue(2.0f), DataType::Float), std::string("abc")};
	std::vector<std::pair<VMValue, DataType>> results;
	ASSERT_TRUE(_vm.CallNative(2, arguments, results));
	ASSERT_EQ(_read.size(), 1u);
	EXPECT_EQ(_read[0], "abc");
	ASSERT_EQ(results.size(), 1u);
	EXPECT_FLOAT_EQ(results[0].first.floatVal, 5.0f);
}

// The scripts' own texts are untouched, and the call's texts are gone after it, so calls don't make the data grow
TEST_F(LhvmNativeTexts, TheScriptsDataIsAsItWasAfterTheCall)
{
	const auto before = _vm.GetData();
	std::vector<std::pair<VMValue, DataType>> results;
	for (int call = 0; call < 3; ++call)
	{
		const std::vector<NativeArgument> arguments {std::string("a fairly long file name.exc")};
		ASSERT_TRUE(_vm.CallNative(1, arguments, results));
	}
	EXPECT_EQ(_vm.GetData(), before);
	EXPECT_EQ(_vm.GetString(0), "Land1");
	EXPECT_EQ(_read, std::vector<std::string>(3, "a fairly long file name.exc"));
}

// An empty text is a text too: the native reads nothing, not the scripts' first text
TEST_F(LhvmNativeTexts, AnEmptyTextReadsAsEmpty)
{
	const std::vector<NativeArgument> arguments {std::string()};
	std::vector<std::pair<VMValue, DataType>> results;
	ASSERT_TRUE(_vm.CallNative(1, arguments, results));
	ASSERT_EQ(_read.size(), 1u);
	EXPECT_EQ(_read[0], "");
}
