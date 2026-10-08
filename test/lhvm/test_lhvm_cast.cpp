/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The script machine's cast, one target type at a time: a value pushed with one type, cast, and read back by a native
// function with the type it came out with. The cast converts for integer and float targets and only retags for the
// others; the unknown type 5 leaves the value alone, and every other type (none, the unknown type 7, vector, object,
// boolean) keeps the bits under the new type.

#include <cstdlib>

#include <bit>
#include <optional>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>

using namespace openblack;
using lhvm::DataType;
using lhvm::Opcode;
using lhvm::VMInstruction;
using lhvm::VMMode;
using lhvm::VMScript;
using lhvm::VMValue;

namespace
{
struct Cast
{
	VMValue value;
	DataType type;
};

/// Runs PUSH value, CAST target, then a native that takes the top of the stack and reports it
std::optional<Cast> RunCast(VMValue value, DataType pushedType, DataType target)
{
	const std::vector<VMInstruction> code {
	    {Opcode::Push, VMMode::Immediate, pushedType, value, 0},
	    {Opcode::Cast, VMMode::Cast, target, VMValue(uint32_t {0}), 0},
	    {Opcode::Sys, VMMode::Immediate, DataType::Int, VMValue(uint32_t {1}), 0},
	    {Opcode::End, VMMode::Immediate, DataType::Int, VMValue(uint32_t {0}), 0},
	};
	const std::vector<VMScript> scripts {VMScript("Cast", "test.txt", lhvm::ScriptType::Script, 1, {}, 0, 0, 1)};

	std::optional<Cast> seen;
	lhvm::LHVM vm;
	const std::vector<lhvm::NativeFunction> natives {
	    lhvm::NativeFunction(nullptr, 0, 0, "NONE"),
	    lhvm::NativeFunction(
	        [&]() {
		        DataType type {};
		        const auto popped = vm.Pop(type);
		        seen = Cast {.value = popped, .type = type};
	        },
	        1, 0, "READ"),
	};
	vm.Initialise(&natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	EXPECT_EQ(vm.LoadBinary(lhvm::LHVMFile(lhvm::LHVMVersion::BlackAndWhite, {"unused"}, code, {}, scripts, {})), EXIT_SUCCESS);
	EXPECT_NE(vm.StartScript("Cast", lhvm::ScriptType::All), 0u);
	vm.LookIn(lhvm::ScriptType::All);
	return seen;
}
} // namespace

TEST(LhvmCast, ToIntTruncatesTheFloat)
{
	const auto cast = RunCast(VMValue(-2.75f), DataType::Float, DataType::Int);
	ASSERT_TRUE(cast.has_value());
	EXPECT_EQ(cast->type, DataType::Int);
	EXPECT_EQ(cast->value.intVal, -2);
}

TEST(LhvmCast, ToFloatReadsTheBitsAsAnUnsignedInteger)
{
	const auto cast = RunCast(VMValue(uint32_t {7}), DataType::Int, DataType::Float);
	ASSERT_TRUE(cast.has_value());
	EXPECT_EQ(cast->type, DataType::Float);
	EXPECT_EQ(cast->value.floatVal, 7.0f);
}

TEST(LhvmCast, VectorObjectAndBooleanKeepTheBits)
{
	for (const auto target : {DataType::Vector, DataType::Object, DataType::Boolean})
	{
		const auto cast = RunCast(VMValue(1.5f), DataType::Float, target);
		ASSERT_TRUE(cast.has_value());
		EXPECT_EQ(cast->type, target);
		EXPECT_EQ(cast->value.uintVal, std::bit_cast<uint32_t>(1.5f));
	}
}

TEST(LhvmCast, TypeFiveLeavesTheValueAlone)
{
	const auto cast = RunCast(VMValue(1.5f), DataType::Float, DataType::Unk5);
	ASSERT_TRUE(cast.has_value());
	EXPECT_EQ(cast->type, DataType::Float);
	EXPECT_EQ(cast->value.floatVal, 1.5f);
}

// Pinned as the machine does it today: the other target types fall to the default branch, which retags. Another
// reading of the original's switch would leave the value alone for these two, so a change here must be deliberate.
TEST(LhvmCast, NoneAndTypeSevenRetagWithTheSameBits)
{
	for (const auto target : {DataType::None, DataType::Unk7})
	{
		const auto cast = RunCast(VMValue(1.5f), DataType::Float, target);
		ASSERT_TRUE(cast.has_value());
		EXPECT_EQ(cast->type, target);
		EXPECT_EQ(cast->value.uintVal, std::bit_cast<uint32_t>(1.5f));
	}
}
