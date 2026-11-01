/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LHVM.h"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

#include "LHVMFile.h"

namespace openblack::lhvm
{
// Adapted from https://stackoverflow.com/a/13059195/10604387
//          and https://stackoverflow.com/a/46069245/10604387
struct membuf: std::streambuf
{
	membuf(char const* base, size_t size)
	{
		char* p(const_cast<char*>(base));
		this->setg(p, p, p + size);
	}
	std::streampos seekoff(off_type off, std::ios_base::seekdir way, [[maybe_unused]] std::ios_base::openmode which) override
	{
		if (way == std::ios_base::cur)
		{
			gbump(static_cast<int>(off));
		}
		else if (way == std::ios_base::end)
		{
			setg(eback(), egptr() + off, egptr());
		}
		else if (way == std::ios_base::beg)
		{
			setg(eback(), eback() + off, egptr());
		}
		return gptr() - eback();
	}

	std::streampos seekpos([[maybe_unused]] pos_type pos, [[maybe_unused]] std::ios_base::openmode which) override
	{
		return seekoff(pos - static_cast<off_type>(0), std::ios_base::beg, which);
	}
};
struct imemstream: virtual membuf, std::istream
{
	imemstream(char const* base, size_t size)
	    : membuf(base, size)
	    , std::istream(dynamic_cast<std::streambuf*>(this))
	{
	}
};

namespace
{

/// The game's conversion of a float to an integer: it truncates to 64 bits, and out of range and NaN give the lowest value
[[nodiscard]] int64_t Ftol(float value)
{
	constexpr auto k_Limit = 9.2233720368547758e18f; // 2^63
	if (!std::isfinite(value) || std::fabs(value) >= k_Limit)
	{
		return std::numeric_limits<int64_t>::min();
	}
	return static_cast<int64_t>(value);
}

/// The game's check of a divisor: its compare finds zero, and also NaN, which it cannot order
[[nodiscard]] bool IsZeroForTheGame(float value)
{
	return value == 0.0f || std::isnan(value);
}

} // namespace

LHVM::LHVM()
{
	_currentStack = &_mainStack;

	_opcodesImpl[0] = &LHVM::Opcode00End;
	_opcodesImpl[1] = &LHVM::Opcode01Jz;
	_opcodesImpl[2] = &LHVM::Opcode02Push;
	_opcodesImpl[3] = &LHVM::Opcode03Pop;
	_opcodesImpl[4] = &LHVM::Opcode04Add;
	_opcodesImpl[5] = &LHVM::Opcode05Sys;
	_opcodesImpl[6] = &LHVM::Opcode06Sub;
	_opcodesImpl[7] = &LHVM::Opcode07Neg;
	_opcodesImpl[8] = &LHVM::Opcode08Mul;
	_opcodesImpl[9] = &LHVM::Opcode09Div;

	_opcodesImpl[10] = &LHVM::Opcode10Mod;
	_opcodesImpl[11] = &LHVM::Opcode11Not;
	_opcodesImpl[12] = &LHVM::Opcode12And;
	_opcodesImpl[13] = &LHVM::Opcode13Or;
	_opcodesImpl[14] = &LHVM::Opcode14Eq;
	_opcodesImpl[15] = &LHVM::Opcode15Neq;
	_opcodesImpl[16] = &LHVM::Opcode16Geq;
	_opcodesImpl[17] = &LHVM::Opcode17Leq;
	_opcodesImpl[18] = &LHVM::Opcode18Gt;
	_opcodesImpl[19] = &LHVM::Opcode19Lt;
	_opcodesImpl[20] = &LHVM::Opcode20Jmp;
	_opcodesImpl[21] = &LHVM::Opcode21Sleep;
	_opcodesImpl[22] = &LHVM::Opcode22Except;
	_opcodesImpl[23] = &LHVM::Opcode23Cast;
	_opcodesImpl[24] = &LHVM::Opcode24Call;
	_opcodesImpl[25] = &LHVM::Opcode25EndExcept;
	_opcodesImpl[26] = &LHVM::Opcode26RetExcept;
	_opcodesImpl[27] = &LHVM::Opcode27IterExcept;
	_opcodesImpl[28] = &LHVM::Opcode28BrkExcept;
	_opcodesImpl[29] = &LHVM::Opcode29Swap;
	_opcodesImpl[30] = &LHVM::Opcode30Line;
}

LHVM::~LHVM() = default;

void LHVM::Initialise(const std::vector<NativeFunction>* functions, std::function<void(uint32_t func)> nativeCallEnterCallback,
                      std::function<void(uint32_t func)> nativeCallExitCallback,
                      std::function<void(uint32_t taskNumber)> stopTaskCallback,
                      std::function<void(ErrorCode code, const std::string v0, uint32_t v1)> errorCallback,
                      std::function<void(uint32_t objId)> addReference, std::function<void(uint32_t objId)> removeReference)
{
	_functions = functions;
	_nativeCallEnterCallback = nativeCallEnterCallback;
	_nativeCallExitCallback = nativeCallExitCallback;
	_stopTaskCallback = stopTaskCallback;
	_errorCallback = errorCallback;
	_addReference = addReference;
	_removeReference = removeReference;
	Reboot();
}

int LHVM::LoadBinary(const std::filesystem::path& filepath)
{
	LHVMFile file;
	file.Open(filepath);
	return LoadBinary(file);
}

int LHVM::LoadBinary(const std::vector<uint8_t>& buffer)
{
	LHVMFile file;
	file.Open(buffer);
	return LoadBinary(file);
}

int LHVM::LoadBinary(const LHVMFile& file)
{
	if (!file.IsLoaded() || file.HasStatus())
	{
		return EXIT_FAILURE;
	}

	StopAllTasks();

	_programFile.reset();
	_instructions = file.GetInstructions();
	_scripts = file.GetScripts();
	_data = file.GetData();
	_mainStack.count = 0;
	_mainStack.pushCount = 0;
	_mainStack.popCount = 0;
	_currentStack = &_mainStack;

	_variablesNames = file.GetVariablesNames();
	_variables.clear();
	_variables.reserve(_variablesNames.size() + 1);
	_variables.emplace_back(DataType::Float, VMValue(0.0f), "Null variable");
	for (auto& name : _variablesNames)
	{
		_variables.emplace_back(DataType::Float, VMValue(0.0f), name);
	}

	_tasks.clear();
	_stoppedWhileRunning.reset();
	_resumesInsideNative.clear();
	_heldTasks.clear();
	_resumeFrom.clear();
	_breakpoints.clear();
	_ticks = 0;
	_currentLineNumber = 0;
	_highestTaskId = 0;
	_highestScriptId = _scripts.size();
	_executedInstructions = 0;

	_auto = file.GetAutostart();
	// The game starts each one by its place in the list of scripts, of any type
	for (const auto scriptId : _auto)
	{
		StartScript(scriptId);
	}
	// The copy for saves is made now, while the game loads, and not at the first save
	[[maybe_unused]] const auto program = ProgramFile();

	return EXIT_SUCCESS;
}

int LHVM::UpdateProgram(const LHVMFile& file)
{
	if (!file.IsLoaded() || file.GetInstructions().size() < _instructions.size() ||
	    file.GetVariablesNames().size() < _variablesNames.size() || file.GetScripts().size() < _scripts.size())
	{
		return EXIT_FAILURE;
	}
	_programFile.reset();
	_instructions = file.GetInstructions();
	_scripts = file.GetScripts();
	_data = file.GetData();
	const auto& names = file.GetVariablesNames();
	for (auto i = _variablesNames.size(); i < names.size(); ++i)
	{
		_variables.emplace_back(DataType::Float, VMValue(0.0f), names[i]);
	}
	_variablesNames = names;
	_highestScriptId = static_cast<uint32_t>(_scripts.size());
	// The copy for saves is made now, while the game loads, and not at the first save
	[[maybe_unused]] const auto program = ProgramFile();
	return EXIT_SUCCESS;
}

int LHVM::RestoreState(const std::filesystem::path& filepath)
{
	auto file = LHVMFile();
	file.Open(filepath);
	return RestoreState(file);
}

int LHVM::RestoreState(const std::vector<uint8_t>& buffer)
{
	auto file = LHVMFile();
	file.Open(buffer);
	return RestoreState(file);
}

int LHVM::RestoreState(const LHVMFile& file)
{
	if (!file.IsLoaded() || !file.HasStatus())
	{
		return EXIT_FAILURE;
	}

	StopAllTasks();

	_programFile.reset();
	_instructions = file.GetInstructions();
	_scripts = file.GetScripts();
	_data = file.GetData();
	_mainStack = file.GetStack();
	_currentStack = &_mainStack;
	_variablesNames = file.GetVariablesNames();
	_variables = file.GetVariablesValues();

	_auto = file.GetAutostart();

	_tasks.clear();
	_stoppedWhileRunning.reset();
	_heldTasks.clear();
	_resumeFrom.clear();
	_breakpoints.clear();
	_resumesInsideNative.clear();
	for (const auto& task : file.GetTasks())
	{
		_tasks.emplace(task.id, task);
		// A save from inside a native function keeps the task at the call, with fewer values on its stack than the
		// native takes. Between two instructions, a task never stops at a call that does not have its arguments.
		if (task.instructionAddress < _instructions.size())
		{
			const auto& instruction = _instructions.at(task.instructionAddress);
			const auto id = instruction.data.intVal;
			if (instruction.code == Opcode::Sys && _functions != nullptr && id > 0 && id < _functions->size() &&
			    static_cast<int64_t>(_functions->at(id).stackIn) > static_cast<int64_t>(task.stack.count))
			{
				_resumesInsideNative.emplace(task.id, task.instructionAddress);
			}
		}
	}

	_ticks = file.GetTicks();
	_currentLineNumber = file.GetCurrentLineNumber();
	_highestTaskId = file.GetHighestTaskId();
	_highestScriptId = file.GetHighestScriptId();
	_executedInstructions = file.GetExecutedInstructions();
	// The copy for saves is made now, while the game loads, and not at the first save
	[[maybe_unused]] const auto program = ProgramFile();

	return EXIT_SUCCESS;
}

VMValue LHVM::Pop(DataType& type)
{
	_currentStack->popCount++;
	if (_currentStack->count > 0)
	{
		_currentStack->count--;
		type = _currentStack->types.at(_currentStack->count);
		return _currentStack->values.at(_currentStack->count);
	}
	type = DataType::None;
	// The game gives a zero and reports nothing. A native that a save stopped takes its arguments again after the load,
	// and these pops are expected.
	if (!_repeatsCutNative)
	{
		SignalError(ErrorCode::ErrStackEmpty);
	}
	return VMValue(0u);
}

VMValue LHVM::Pop()
{
	DataType tmp;
	return Pop(tmp);
}

float LHVM::Popf()
{
	DataType tmp;
	return Pop(tmp).floatVal;
}

void LHVM::Push(VMValue value, DataType type)
{
	_currentStack->pushCount++;
	if (_currentStack->count < VMStack::k_Capacity)
	{
		_currentStack->values.at(_currentStack->count) = value;
		_currentStack->types.at(_currentStack->count) = type;
		_currentStack->count++;
	}
	else
	{
		SignalError(ErrorCode::ErrStackFull);
	}
}

void LHVM::Pushf(float value)
{
	Push(VMValue(value), DataType::Float);
}

void LHVM::Pushv(float value)
{
	Push(VMValue(value), DataType::Vector);
}

void LHVM::Pushi(int32_t value)
{
	Push(VMValue(value), DataType::Int);
}

void LHVM::Pusho(uint32_t value)
{
	Push(VMValue(value), DataType::Object);
}

void LHVM::Pushb(bool value)
{
	Push(VMValue(value ? 1 : 0), DataType::Boolean);
}

void LHVM::Reboot()
{
	StopAllTasks();
	_variables.clear();
	_programFile.reset();
	_variablesNames.clear();
	_scripts.clear();
	_auto.clear();
	_instructions.clear();
	_data.clear();
	_breakpoints.clear();

	_ticks = 0;
	_highestTaskId = 0;
	_highestScriptId = 0;
	_currentLineNumber = 0;
	_executedInstructions = 0;

	_mainStack.popCount += _mainStack.count;
	_mainStack.count = 0;

	_currentTask = nullptr;
	_currentStack = &_mainStack;
}

int LHVM::SaveBinary(const std::filesystem::path& filepath)
{
	LHVMFile file(LHVMVersion::BlackAndWhite, _variablesNames, _instructions, _auto, _scripts, _data);
	file.Write(filepath);
	return EXIT_SUCCESS;
}

LHVMFile LHVM::StateFile() const
{
	return StateFile(*ProgramFile(), CopyRunningState());
}

std::shared_ptr<const LHVMFile> LHVM::ProgramFile() const
{
	if (!_programFile)
	{
		_programFile = std::make_shared<const LHVMFile>(LHVMVersion::BlackAndWhite, _variablesNames, _instructions, _auto,
		                                                _scripts, _data);
	}
	return _programFile;
}

LHVM::RunningState LHVM::CopyRunningState() const
{
	RunningState state {.stack = _mainStack,
	                    .variables = _variables,
	                    .tasks = {},
	                    .ticks = _ticks,
	                    .currentLineNumber = _currentLineNumber,
	                    .highestTaskId = _highestTaskId,
	                    .highestScriptId = _highestScriptId,
	                    .executedInstructions = _executedInstructions};
	state.tasks.reserve(_tasks.size());
	for (const auto& [id, task] : _tasks)
	{
		state.tasks.emplace_back(task);
	}
	return state;
}

LHVMFile LHVM::StateFile(const LHVMFile& program, const RunningState& state)
{
	return {program.GetVersion(),
	        program.GetVariablesNames(),
	        program.GetInstructions(),
	        program.GetAutostart(),
	        program.GetScripts(),
	        program.GetData(),
	        state.stack,
	        state.variables,
	        state.tasks,
	        state.ticks,
	        state.currentLineNumber,
	        state.highestTaskId,
	        state.highestScriptId,
	        state.executedInstructions};
}

int LHVM::SaveState(const std::filesystem::path& filepath)
{
	StateFile().Write(filepath);
	return EXIT_SUCCESS;
}

std::vector<uint8_t> LHVM::SaveState() const
{
	std::ostringstream stream(std::ios::binary);
	StateFile().Write(stream);
	const auto text = stream.str();
	return {text.begin(), text.end()};
}

void LHVM::LookIn(const ScriptType allowedScriptTypesMask)
{
	// First, the exception handlers of each task
	for (auto iter = _tasks.begin(); iter != _tasks.end(); ++iter)
	{
		auto& [id, task] = *iter;
		if (!(task.type & allowedScriptTypesMask) || IsParked(id))
		{
			continue;
		}
		_currentStack = &task.stack;
		if (!task.inExceptionHandler && task.waitingTaskId == 0)
		{
			task.currentExceptionHandlerIndex = 0;
			// The conditions that end the task's loops (until) and the handlers of its blocks
			if (GetExceptionHandlersCount(task) > 0)
			{
				task.pevInstructionAddress = task.instructionAddress;
				task.instructionAddress = GetCurrentExceptionHandlerIp(task, task.currentExceptionHandlerIndex);
				task.inExceptionHandler = true;
			}
		}
		if (task.inExceptionHandler)
		{
			CpuLoop(task);
			// The game cannot find the next task after a task that stopped itself. The tasks after it do not get this step.
			if (EraseTaskStoppedWhileRunning())
			{
				break;
			}
		}
	}

	// Then, the normal code of each task
	for (auto iter = _tasks.begin(); iter != _tasks.end(); ++iter)
	{
		auto& [id, task] = *iter;
		if ((task.type & allowedScriptTypesMask) && !IsParked(id) && !task.inExceptionHandler)
		{
			_currentStack = &task.stack;
			CpuLoop(task);
			if (EraseTaskStoppedWhileRunning())
			{
				break;
			}
		}
	}

	// Last, each task counts the turn and a task that waits on a task that ended continues. A task that has stopped
	// goes, and the count starts again from the first task. Thus the tasks before it count this turn two times.
	for (auto iter = _tasks.begin(); iter != _tasks.end();)
	{
		auto& [id, task] = *iter;
		if (task.stop)
		{
			StopTask(id);
			iter = _tasks.begin();
			continue;
		}
		if ((task.type & allowedScriptTypesMask) && !IsParked(id))
		{
			task.ticks++;
			if (task.waitingTaskId != 0 && !TaskExists(task.waitingTaskId))
			{
				task.waitingTaskId = 0;
				task.instructionAddress++;
			}
		}
		++iter;
	}

	_ticks++;
	_currentStack = &_mainStack;
}

bool LHVM::EraseTaskStoppedWhileRunning()
{
	if (!_stoppedWhileRunning.has_value())
	{
		return false;
	}
	const auto taskNumber = *_stoppedWhileRunning;
	_stoppedWhileRunning.reset();
	_tasks.erase(taskNumber);
	_heldTasks.erase(taskNumber);
	_resumeFrom.erase(taskNumber);
	_currentStack = &_mainStack;
	return true;
}

uint32_t LHVM::StartScript(const std::string& name, const ScriptType allowedScriptTypesMask)
{
	const auto* const script = GetScript(name);
	if (script != nullptr)
	{
		if (script->type & allowedScriptTypesMask)
		{
			return StartScript(*script);
		}
		SignalError(ErrorCode::ErrNoScriptOfType, name);
	}
	else
	{
		SignalError(ErrorCode::ErrScriptNameNotFound, name);
	}
	return 0;
}

uint32_t LHVM::StartScriptAt(const std::string& name, const ScriptType allowedScriptTypesMask, const uint32_t address)
{
	const auto* const script = GetScript(name);
	if (script == nullptr || !(script->type & allowedScriptTypesMask) || address < script->instructionAddress ||
	    address >= _instructions.size())
	{
		return 0;
	}
	// The code of the script stops at its first end instruction
	const auto code = std::span(_instructions).subspan(script->instructionAddress);
	const auto end = std::ranges::find(code, Opcode::End, &VMInstruction::code);
	if (address >= script->instructionAddress + static_cast<uint32_t>(std::distance(code.begin(), end)))
	{
		return 0;
	}
	const auto taskNumber = StartScript(*script);
	auto& task = _tasks.at(taskNumber);
	if (code.front().code == Opcode::Except)
	{
		task.exceptionHandlerIps.emplace_back(code.front().data.uintVal);
	}
	task.instructionAddress = address;
	return taskNumber;
}

std::optional<uint32_t> LHVM::FindRunOfScript(const std::string& caller, const std::string& callee) const
{
	const auto byName = [this](const std::string& name) { return std::ranges::find(_scripts, name, &VMScript::name); };
	const auto callerScript = byName(caller);
	const auto calleeScript = byName(callee);
	if (callerScript == _scripts.end() || calleeScript == _scripts.end() ||
	    callerScript->instructionAddress >= _instructions.size())
	{
		return std::nullopt;
	}
	for (auto address = callerScript->instructionAddress; address < _instructions.size(); ++address)
	{
		const auto& instruction = _instructions[address];
		if (instruction.code == Opcode::End)
		{
			break;
		}
		if (instruction.code == Opcode::Run && instruction.data.uintVal == calleeScript->scriptId)
		{
			return address;
		}
	}
	return std::nullopt;
}

uint32_t LHVM::StartScript(const uint32_t id)
{
	if (id > 0 && id <= _scripts.size())
	{
		return StartScript(_scripts.at(id - 1));
	}
	SignalError(ErrorCode::ErrScriptIdNotFound, id);
	return 0;
}

uint32_t LHVM::StartScript(const VMScript& script)
{
	const auto taskNumber = ++_highestTaskId;

	// copy values from current stack to new stack
	VMStack stack {};
	for (unsigned int i = 0; i < script.parameterCount; i++)
	{
		DataType type;
		const auto& value = Pop(type);

		stack.pushCount++;
		if (stack.count < VMStack::k_Capacity)
		{
			stack.values.at(stack.count) = value;
			stack.types.at(stack.count) = type;
			stack.count++;
		}
	}

	// allocate local variables with default values
	const auto& scriptVariables = script.variables;
	std::vector<VMVar> taskVariables;
	taskVariables.reserve(scriptVariables.size());
	for (const auto& name : scriptVariables)
	{
		taskVariables.emplace_back(DataType::Float, VMValue(0.0f), name);
	}

	const auto& task = VMTask(taskVariables, script.scriptId, taskNumber, script.instructionAddress, script.variablesOffset,
	                          stack, script.name, script.filename, script.type);

	_tasks.emplace(taskNumber, task);

	return taskNumber;
}

void LHVM::StopAllTasks()
{
	std::vector<uint32_t> ids;
	ids.reserve(_tasks.size());
	for (const auto& [id, task] : _tasks)
	{
		ids.emplace_back(id);
	}
	for (const auto id : ids)
	{
		StopTask(id);
	}
}

void LHVM::StopScripts(std::function<bool(const std::string& name, const std::string& filename)> filter)
{
	std::vector<uint32_t> ids;
	for (const auto& [id, task] : _tasks)
	{
		if (filter(task.name, task.filename))
		{
			ids.emplace_back(id);
		}
	}

	for (const auto id : ids)
	{
		StopTask(id);
	}
}

void LHVM::StopTask(uint32_t taskNumber)
{
	if (_stoppedWhileRunning == taskNumber)
	{
		return;
	}
	if (TaskExists(taskNumber))
	{
		InvokeStopTaskCallback(taskNumber);
		auto& task = _tasks.at(taskNumber);
		for (auto& var : task.localVars)
		{
			if (var.type == DataType::Object)
			{
				RemoveReference(var.value.uintVal);
			}
		}

		if (_currentTask == &task)
		{
			// The task stops itself through a native function. The game frees it at once but continues its code, which
			// is not safe to copy. Here the task does no more instructions and goes when its run ends.
			_currentStack = &_mainStack;
			task.localVars.clear();
			task.stop = true;
			_stoppedWhileRunning = taskNumber;
			return;
		}

		_tasks.erase(taskNumber);
		_heldTasks.erase(taskNumber);
		_resumeFrom.erase(taskNumber);
	}
	else
	{
		SignalError(ErrorCode::ErrTaskIdNotFound, taskNumber);
	}
}

void LHVM::StopTasksOfType(const ScriptType typesMask)
{
	std::vector<uint32_t> ids;
	for (const auto& [id, task] : _tasks)
	{
		if (task.type & typesMask)
		{
			ids.emplace_back(task.id);
		}
	}

	for (const auto id : ids)
	{
		StopTask(id);
	}
}

std::string LHVM::GetString(uint32_t offset)
{
	if (offset < _data.size())
	{
		return {_data.data() + offset};
	}
	return "";
}

void LHVM::InvokeStopTaskCallback(const uint32_t taskNumber)
{
	if (_stopTaskCallback != nullptr)
	{
		_stopTaskCallback(taskNumber);
	}
}

void LHVM::AddReference(const uint32_t objectId)
{
	// A variable that holds no object gives no reference to the game
	if (objectId != 0 && _addReference != nullptr)
	{
		_addReference(objectId);
	}
}

void LHVM::RemoveReference(const uint32_t objectId)
{
	// A variable that held no object has no reference to give back
	if (objectId != 0 && _removeReference != nullptr)
	{
		_removeReference(objectId);
	}
}

void LHVM::InvokeNativeCallEnterCallback(const uint32_t funcId)
{
	if (_nativeCallEnterCallback != nullptr)
	{
		_nativeCallEnterCallback(funcId);
	}
}

void LHVM::InvokeNativeCallExitCallback(const uint32_t funcId)
{
	if (_nativeCallExitCallback != nullptr)
	{
		_nativeCallExitCallback(funcId);
	}
}

void LHVM::SignalError(const ErrorCode code)
{
	if (_errorCallback != nullptr)
	{
		_errorCallback(code, "", 0);
	}
}

void LHVM::SignalError(const ErrorCode code, const uint32_t data)
{
	if (_errorCallback != nullptr)
	{
		_errorCallback(code, "", data);
	}
}

void LHVM::SignalError(const ErrorCode code, const std::string& data)
{
	if (_errorCallback != nullptr)
	{
		_errorCallback(code, data, 0);
	}
}

const VMScript* LHVM::GetScript(const std::string& name)
{
	for (const auto& script : _scripts)
	{
		if (script.name == name)
		{
			return &script;
		}
	}
	return nullptr;
}

bool LHVM::TaskExists(const uint32_t taskId)
{
	return _tasks.contains(taskId);
}

uint32_t LHVM::GetTicksCount()
{
	return _ticks;
}

void LHVM::PushElaspedTime()
{
	// The game's script time is the tick count times 0.1f, rounded once to a float by its 24-bit FPU
	const auto time = static_cast<float>(static_cast<double>(GetTicksCount()) * static_cast<double>(0.1f));
	Pushf(time);
}

VMVar& LHVM::GetVar(VMTask& task, const uint32_t id)
{
	const auto offset = task.variablesOffset;
	return (id > offset) ? task.localVars.at(id - offset - 1) : _variables.at(id);
}

uint32_t LHVM::GetExceptionHandlersCount(const VMTask& task)
{
	return static_cast<uint32_t>(task.exceptionHandlerIps.size());
}

uint32_t LHVM::GetCurrentExceptionHandlerIp(const VMTask& task, const uint32_t index)
{
	return task.exceptionHandlerIps.at(task.exceptionHandlerIps.size() - index - 1);
}

void LHVM::PrintInstruction(const VMTask& task, const VMInstruction& instruction)
{
	// TODO(Daniels118): improve this
	std::string opcode = k_OpcodeNames.at(static_cast<int>(instruction.code));
	std::string arg;
	if (instruction.code == Opcode::Run)
	{
		if (instruction.mode == VMMode::Async)
		{
			arg = "async ";
		}
		arg += _scripts.at(instruction.data.intVal - 1).name;
	}
	else if (instruction.code == Opcode::Sys)
	{
		arg += _functions->at(instruction.data.intVal).name;
	}
	else
	{
		if (instruction.code == Opcode::Push || instruction.code == Opcode::Pop || instruction.code == Opcode::Cast ||
		    instruction.code == Opcode::Add || instruction.code == Opcode::Sub || instruction.code == Opcode::Mul ||
		    instruction.code == Opcode::Div || instruction.code == Opcode::Mod)
		{
			opcode += k_DataTypeChars.at(static_cast<int>(instruction.type));
		}
		if ((instruction.code == Opcode::Push || instruction.code == Opcode::Pop) && instruction.mode == VMMode::Reference)
		{
			if (instruction.data.intVal > task.variablesOffset)
			{
				arg = task.localVars[instruction.data.intVal - task.variablesOffset - 1].name;
			}
			else
			{
				arg = _variables.at(instruction.data.intVal).name;
			}
		}
		else if (instruction.code == Opcode::Push && instruction.mode == VMMode::Immediate)
		{
			if (instruction.type == DataType::Float || instruction.type == DataType::Vector)
			{
				arg = std::to_string(instruction.data.floatVal);
			}
			else
			{
				arg = std::to_string(instruction.data.intVal);
			}
		}
		else if (instruction.code == Opcode::Jmp || instruction.code == Opcode::Wait || instruction.code == Opcode::Swap)
		{
			arg = std::to_string(instruction.data.intVal);
		}
		if (instruction.code == Opcode::Wait)
		{
			const bool val = task.stack.values.at(task.stack.count - 1).intVal != 0;
			arg += val ? " [true] -> continue" : " [false] -> JUMP";
		}
	}
	printf("%s:%d %s[%d] %s %s\n", task.filename.c_str(), instruction.line, task.name.c_str(), task.id, opcode.c_str(),
	       arg.c_str());
}

void LHVM::CpuLoop(VMTask& task)
{
	const auto wasExceptionHandler = task.inExceptionHandler;
	task.iield = false;
	// A task that a save stopped inside a native function starts again at that call. Its exception handler can run
	// first, from another place, and the task comes back to the call after it.
	bool repeatsCutNative = false;
	if (const auto cut = _resumesInsideNative.find(task.id); cut != _resumesInsideNative.end())
	{
		repeatsCutNative = cut->second == task.instructionAddress;
		if (repeatsCutNative || !task.inExceptionHandler)
		{
			_resumesInsideNative.erase(cut);
		}
	}
	while (task.waitingTaskId == 0)
	{
		if ((!_breakpoints.empty() || !_heldTasks.empty() || !_resumeFrom.empty()) && DebuggerStops(task))
		{
			break;
		}
		_currentTask = &task;
		_executedInstructions++;
		const auto& instruction = _instructions.at(task.instructionAddress);

		// PrintInstruction(task, instruction); // just for debug purposes

		_repeatsCutNative = std::exchange(repeatsCutNative, false);
		(this->*_opcodesImpl.at(static_cast<int>(instruction.code)))(task, instruction);
		_repeatsCutNative = false;

		if (task.stop || task.iield || task.waitingTaskId != 0 || task.inExceptionHandler != wasExceptionHandler)
		{
			break;
		}
		task.instructionAddress++;
	}
	_currentTask = nullptr;
}

std::vector<LHVM::CutNative> LHVM::NativesCutBySave() const
{
	std::vector<CutNative> cut;
	cut.reserve(_resumesInsideNative.size());
	for (const auto& [task, address] : _resumesInsideNative)
	{
		cut.push_back({.task = task, .native = _instructions.at(address).data.uintVal});
	}
	return cut;
}

bool LHVM::DebuggerStops(VMTask& task)
{
	const auto address = task.instructionAddress;
	if (auto held = _heldTasks.find(task.id); held != _heldTasks.end())
	{
		if (held->second == 0)
		{
			return true;
		}
		--held->second;
		return false;
	}
	bool resuming = false;
	if (auto resume = _resumeFrom.find(task.id); resume != _resumeFrom.end())
	{
		resuming = resume->second == address;
		_resumeFrom.erase(resume);
	}
	if (!resuming && _breakpoints.contains(address))
	{
		_heldTasks[task.id] = 0;
		return true;
	}
	return false;
}

bool LHVM::IsParked(uint32_t taskNumber) const
{
	const auto held = _heldTasks.find(taskNumber);
	return held != _heldTasks.end() && held->second == 0;
}

void LHVM::SetVariable(uint32_t id, VMValue value)
{
	if (id < _variables.size())
	{
		_variables.at(id).value = value;
	}
}

void LHVM::SetTaskVariable(uint32_t taskNumber, size_t index, VMValue value)
{
	if (auto task = _tasks.find(taskNumber); task != _tasks.end() && index < task->second.localVars.size())
	{
		task->second.localVars.at(index).value = value;
	}
}

void LHVM::SetBreakpoint(uint32_t address, bool enabled)
{
	if (enabled)
	{
		_breakpoints.insert(address);
	}
	else
	{
		_breakpoints.erase(address);
	}
}

void LHVM::HoldTask(uint32_t taskNumber)
{
	if (TaskExists(taskNumber))
	{
		_heldTasks[taskNumber] = 0;
	}
}

void LHVM::StepTask(uint32_t taskNumber)
{
	if (auto held = _heldTasks.find(taskNumber); held != _heldTasks.end())
	{
		++held->second;
	}
}

void LHVM::ContinueTask(uint32_t taskNumber)
{
	if (auto task = _tasks.find(taskNumber); task != _tasks.end() && _heldTasks.erase(taskNumber) > 0)
	{
		_resumeFrom[taskNumber] = task->second.instructionAddress;
	}
}

float LHVM::Fmod(float a, float b)
{
	// The game truncates both values to integers first and takes the whole multiple of the divisor from the value
	const auto divisor = static_cast<int32_t>(Ftol(b));
	if (divisor == 0)
	{
		return 0.0f;
	}
	const auto dividend = static_cast<int32_t>(Ftol(a));
	const auto multiple = static_cast<int32_t>(static_cast<uint32_t>((static_cast<int64_t>(dividend) / divisor) * divisor));
	// The difference is exact as a double, so the conversion rounds it to a float one time, as the game does
	return static_cast<float>(static_cast<double>(a) - static_cast<double>(multiple));
}

void LHVM::Opcode00End(VMTask& task, const VMInstruction& /*instruction*/)
{
	task.stop = true;
}

void LHVM::Opcode01Jz(VMTask& task, const VMInstruction& instruction)
{
	const bool val = Pop().intVal != 0;
	if (val)
	{
		task.ticks = 1;
	}
	else // jump if popped value isn't zero
	{
		if (instruction.mode == VMMode::Forward)
		{
			task.instructionAddress = instruction.data.intVal - 1;
		}
		else // Mode::BACKWARD
		{
			task.instructionAddress = instruction.data.intVal;
			task.iield = true;
		}
	}
}

void LHVM::Opcode02Push(VMTask& task, const VMInstruction& instruction)
{
	if (instruction.mode == VMMode::Immediate)
	{
		Push(instruction.data, instruction.type);
	}
	else // Mode::REFERENCE
	{
		const auto& var = GetVar(task, instruction.data.uintVal);
		Push(var.value, var.type);
	}
}

void LHVM::Opcode03Pop(VMTask& task, const VMInstruction& instruction)
{
	if (instruction.mode == VMMode::Reference)
	{
		PopToVariable(task, instruction.data.uintVal);
	}
	else // Mode::IMMEDIATE
	{
		Pop(); // cannot POP to immediate value, just discard the value
	}
}

void LHVM::PopToVariable(VMTask& task, uint32_t id)
{
	auto& var = GetVar(task, id);
	DataType type;
	const auto newVal = Pop(type);
	if (type == DataType::Object)
	{
		AddReference(newVal.uintVal);
	}
	if (var.type == DataType::Object)
	{
		// the object the variable held until now
		RemoveReference(var.value.uintVal);
	}
	var.value = newVal;
	var.type = type;
}

void LHVM::Opcode04Add(VMTask& /*task*/, const VMInstruction& instruction)
{
	VMValue a0;
	VMValue a1;
	VMValue a2;
	VMValue b0;
	VMValue b1;
	VMValue b2;
	switch (instruction.type)
	{
	case DataType::Int:
		a0 = Pop();
		b0 = Pop();
		Push(VMValue(b0.uintVal + a0.uintVal), DataType::Int);
		break;
	case DataType::Float:
		a0 = Pop();
		b0 = Pop();
		Pushf(b0.floatVal + a0.floatVal);
		break;
	case DataType::Vector:
		a0 = Pop();
		a1 = Pop();
		a2 = Pop();
		b0 = Pop();
		b1 = Pop();
		b2 = Pop();
		Pushv(b2.floatVal + a2.floatVal);
		Pushv(b1.floatVal + a1.floatVal);
		Pushv(b0.floatVal + a0.floatVal);
		break;
	default:
		// The game takes nothing off the stack and puts a zero on it
		Pushf(0.0f);
		SignalError(ErrorCode::ErrInvalidType);
	}
}

void LHVM::Opcode05Sys(VMTask& /*task*/, const VMInstruction& instruction)
{
	const auto id = instruction.data.intVal;
	if (id > 0 && id < _functions->size())
	{
		const auto& func = _functions->at(id);
		if (func.impl != nullptr)
		{
			_currentStack->pushCount = 0;
			_currentStack->popCount = 0;
			InvokeNativeCallEnterCallback(id);
			func.impl();
			// The game's functions always take their arguments off the stack. One that hasn't been written yet may leave
			// them, which would shift every argument after it, so they are taken from under whatever it pushed.
			auto& stack = *_currentStack;
			const auto in = static_cast<uint32_t>(std::max(func.stackIn, 0));
			if (stack.popCount == 0 && in > 0 && stack.count >= in + stack.pushCount)
			{
				const auto top = stack.count - stack.pushCount;
				for (uint32_t i = 0; i < stack.pushCount; ++i)
				{
					stack.values.at(top - in + i) = stack.values.at(top + i);
					stack.types.at(top - in + i) = stack.types.at(top + i);
				}
				stack.count -= in;
			}
			InvokeNativeCallExitCallback(id);
		}
		else // if impl not provided, then just adjust the stack
		{
			for (int i = 0; i < func.stackIn; i++)
			{
				Pop();
			}
			for (unsigned int i = 0; i < func.stackOut; i++)
			{
				Pushf(0.0f);
			}
		}
	}
	else
	{
		SignalError(ErrorCode::ErrNativeFuncNotFound, id);
	}
}

void LHVM::Opcode06Sub(VMTask& /*task*/, const VMInstruction& instruction)
{
	VMValue a0;
	VMValue a1;
	VMValue a2;
	VMValue b0;
	VMValue b1;
	VMValue b2;
	switch (instruction.type)
	{
	case DataType::Int:
		a0 = Pop();
		b0 = Pop();
		Push(VMValue(b0.uintVal - a0.uintVal), DataType::Int);
		break;
	case DataType::Float:
		a0 = Pop();
		b0 = Pop();
		Pushf(b0.floatVal - a0.floatVal);
		break;
	case DataType::Vector:
		a0 = Pop();
		a1 = Pop();
		a2 = Pop();
		b0 = Pop();
		b1 = Pop();
		b2 = Pop();
		Pushv(b2.floatVal - a2.floatVal);
		Pushv(b1.floatVal - a1.floatVal);
		Pushv(b0.floatVal - a0.floatVal);
		break;
	default:
		// The game takes nothing off the stack and puts a zero on it
		Pushf(0.0f);
		SignalError(ErrorCode::ErrInvalidType);
	}
}

void LHVM::Opcode07Neg(VMTask& /*task*/, const VMInstruction& instruction)
{
	VMValue a0;
	VMValue a1;
	VMValue a2;
	switch (instruction.type)
	{
	case DataType::Int:
		a0 = Pop();
		Push(VMValue(0u - a0.uintVal), DataType::Int);
		break;
	case DataType::Float:
		a0 = Pop();
		Pushf(-a0.floatVal);
		break;
	case DataType::Vector:
		a0 = Pop();
		a1 = Pop();
		a2 = Pop();
		Pushv(-a2.floatVal);
		Pushv(-a1.floatVal);
		Pushv(-a0.floatVal);
		break;
	default:
		// The game takes nothing off the stack and puts a zero on it
		Pushf(0.0f);
		SignalError(ErrorCode::ErrInvalidType);
	}
}

void LHVM::Opcode08Mul(VMTask& /*task*/, const VMInstruction& instruction)
{
	VMValue a0;
	VMValue a1;
	VMValue a2;
	VMValue b0;
	switch (instruction.type)
	{
	case DataType::Int:
		a0 = Pop();
		b0 = Pop();
		Push(VMValue(b0.uintVal * a0.uintVal), DataType::Int);
		break;
	case DataType::Float:
		a0 = Pop();
		b0 = Pop();
		Pushf(b0.floatVal * a0.floatVal);
		break;
	case DataType::Vector:
		// The vector is on top of the number
		a0 = Pop();
		a1 = Pop();
		a2 = Pop();
		b0 = Pop();
		Pushv(b0.floatVal * a2.floatVal);
		Pushv(b0.floatVal * a1.floatVal);
		Pushv(b0.floatVal * a0.floatVal);
		break;
	default:
		// The game takes nothing off the stack and puts a zero on it
		Pushf(0.0f);
		SignalError(ErrorCode::ErrInvalidType);
	}
}

void LHVM::Opcode09Div(VMTask& /*task*/, const VMInstruction& instruction)
{
	// When the divisor is zero, the game puts a zero on the stack and keeps the dividend under it
	switch (instruction.type)
	{
	case DataType::Int:
	{
		// The game divides integers as unsigned values
		const auto divisor = Pop().uintVal;
		if (divisor == 0)
		{
			Pushi(0);
			SignalError(ErrorCode::ErrDivByZero);
			break;
		}
		const auto dividend = Pop().uintVal;
		Push(VMValue(dividend / divisor), DataType::Int);
		break;
	}
	case DataType::Float:
	{
		const auto divisor = Popf();
		if (IsZeroForTheGame(divisor))
		{
			Pushf(0.0f);
			SignalError(ErrorCode::ErrDivByZero);
			break;
		}
		const auto dividend = Popf();
		Pushf(dividend / divisor);
		break;
	}
	case DataType::Vector:
	{
		// The number is on top of the vector. The game takes all four values off before it does the check.
		const auto divisor = Popf();
		const auto b0 = Popf();
		const auto b1 = Popf();
		const auto b2 = Popf();
		if (IsZeroForTheGame(divisor))
		{
			Pushv(0.0f);
			Pushv(0.0f);
			Pushv(0.0f);
			SignalError(ErrorCode::ErrDivByZero);
			break;
		}
		Pushv(b2 / divisor);
		Pushv(b1 / divisor);
		Pushv(b0 / divisor);
		break;
	}
	default:
		// The game takes nothing off the stack and puts a zero on it
		Pushf(0.0f);
		SignalError(ErrorCode::ErrInvalidType);
	}
}

void LHVM::Opcode10Mod(VMTask& /*task*/, const VMInstruction& instruction)
{
	// When the divisor is zero, the game puts a zero on the stack and keeps the dividend under it
	switch (instruction.type)
	{
	case DataType::Int:
	{
		// The game divides integers as unsigned values
		const auto divisor = Pop().uintVal;
		if (divisor == 0)
		{
			Pushi(0);
			SignalError(ErrorCode::ErrDivByZero);
			break;
		}
		const auto dividend = Pop().uintVal;
		Push(VMValue(dividend % divisor), DataType::Int);
		break;
	}
	case DataType::Float:
	{
		const auto divisor = Popf();
		if (IsZeroForTheGame(divisor))
		{
			Pushf(0.0f);
			SignalError(ErrorCode::ErrDivByZero);
			break;
		}
		const auto dividend = Popf();
		Pushf(Fmod(dividend, divisor));
		break;
	}
	case DataType::Vector:
		// The game takes nothing off the stack and puts a zero vector on it
		Pushv(0.0f);
		Pushv(0.0f);
		Pushv(0.0f);
		break;
	default:
		// The game takes nothing off the stack and puts a zero on it
		Pushf(0.0f);
		SignalError(ErrorCode::ErrInvalidType);
	}
}

void LHVM::Opcode11Not(VMTask& /*task*/, const VMInstruction& /*instruction*/)
{
	const bool a = Pop().intVal != 0;
	Pushb(!a);
}

void LHVM::Opcode12And(VMTask& /*task*/, const VMInstruction& /*instruction*/)
{
	const bool b = Pop().intVal != 0;
	const bool a = Pop().intVal != 0;
	Pushb(a && b);
}

void LHVM::Opcode13Or(VMTask& /*task*/, const VMInstruction& /*instruction*/)
{
	const bool b = Pop().intVal != 0;
	const bool a = Pop().intVal != 0;
	Pushb(a || b);
}

void LHVM::Opcode14Eq(VMTask& /*task*/, const VMInstruction& instruction)
{
	VMValue a0;
	VMValue b0;
	switch (instruction.type)
	{
	case DataType::Int:
	case DataType::Boolean:
		b0 = Pop();
		a0 = Pop();
		Pushb(a0.uintVal == b0.uintVal);
		break;
	case DataType::Float:
		b0 = Pop();
		a0 = Pop();
		Pushb(a0.floatVal == b0.floatVal);
		break;
	default:
		// The game compares only numbers and truth values. For another type it takes nothing off the stack and puts false.
		Pushb(false);
		SignalError(ErrorCode::ErrInvalidType);
	}
}

void LHVM::Opcode15Neq(VMTask& /*task*/, const VMInstruction& instruction)
{
	VMValue a0;
	VMValue b0;
	switch (instruction.type)
	{
	case DataType::Int:
	case DataType::Boolean:
		b0 = Pop();
		a0 = Pop();
		Pushb(a0.uintVal != b0.uintVal);
		break;
	case DataType::Float:
		b0 = Pop();
		a0 = Pop();
		Pushb(a0.floatVal != b0.floatVal);
		break;
	default:
		// The game compares only numbers and truth values. For another type it takes nothing off the stack and puts true.
		Pushb(true);
		SignalError(ErrorCode::ErrInvalidType);
	}
}

void LHVM::Opcode16Geq(VMTask& /*task*/, const VMInstruction& instruction)
{
	VMValue a0;
	VMValue b0;
	switch (instruction.type)
	{
	case DataType::Int:
		// The game compares integers as unsigned values
		b0 = Pop();
		a0 = Pop();
		Pushb(a0.uintVal >= b0.uintVal);
		break;
	case DataType::Float:
		b0 = Pop();
		a0 = Pop();
		Pushb(a0.floatVal >= b0.floatVal);
		break;
	default:
		Pushb(false);
		SignalError(ErrorCode::ErrInvalidType);
	}
}

void LHVM::Opcode17Leq(VMTask& /*task*/, const VMInstruction& instruction)
{
	VMValue a0;
	VMValue b0;
	switch (instruction.type)
	{
	case DataType::Int:
		// The game compares integers as unsigned values
		b0 = Pop();
		a0 = Pop();
		Pushb(a0.uintVal <= b0.uintVal);
		break;
	case DataType::Float:
		b0 = Pop();
		a0 = Pop();
		Pushb(a0.floatVal <= b0.floatVal);
		break;
	default:
		Pushb(false);
		SignalError(ErrorCode::ErrInvalidType);
	}
}

void LHVM::Opcode18Gt(VMTask& /*task*/, const VMInstruction& instruction)
{
	VMValue a0;
	VMValue b0;
	switch (instruction.type)
	{
	case DataType::Int:
		// The game compares integers as unsigned values
		b0 = Pop();
		a0 = Pop();
		Pushb(a0.uintVal > b0.uintVal);
		break;
	case DataType::Float:
		b0 = Pop();
		a0 = Pop();
		Pushb(a0.floatVal > b0.floatVal);
		break;
	default:
		Pushb(false);
		SignalError(ErrorCode::ErrInvalidType);
	}
}

void LHVM::Opcode19Lt(VMTask& /*task*/, const VMInstruction& instruction)
{
	VMValue a0;
	VMValue b0;
	switch (instruction.type)
	{
	case DataType::Int:
		// The game compares integers as unsigned values
		b0 = Pop();
		a0 = Pop();
		Pushb(a0.uintVal < b0.uintVal);
		break;
	case DataType::Float:
		b0 = Pop();
		a0 = Pop();
		Pushb(a0.floatVal < b0.floatVal);
		break;
	default:
		Pushb(false);
		SignalError(ErrorCode::ErrInvalidType);
	}
}

void LHVM::Opcode20Jmp(VMTask& task, const VMInstruction& instruction)
{
	if (instruction.mode == VMMode::Forward)
	{
		task.instructionAddress = instruction.data.intVal - 1;
	}
	else // Mode::BACKWARD
	{
		task.instructionAddress = instruction.data.intVal;
		task.iield = true;
	}
}

void LHVM::Opcode21Sleep(VMTask& task, const VMInstruction& /*instruction*/)
{
	// The time in tenths of a second, truncated through a 64-bit integer. The low 32 bits are an unsigned count.
	const auto seconds = Popf();
	const auto tenths = static_cast<uint32_t>(Ftol(seconds * 10.0f));
	if (task.ticks <= tenths)
	{
		task.sleeping = true;
		Pushb(false);
	}
	else
	{
		task.sleeping = false;
		Pushb(true);
	}
}

void LHVM::Opcode22Except(VMTask& /*task*/, const VMInstruction& instruction)
{
	if (_currentTask != nullptr)
	{
		_currentTask->exceptionHandlerIps.emplace_back(instruction.data.uintVal);
	}
}

void LHVM::Opcode23Cast(VMTask& task, const VMInstruction& instruction)
{
	if (instruction.mode == VMMode::Zero)
	{
		// The game clears a variable only for a float. It puts a zero on the stack and takes it off into the variable.
		if (instruction.type == DataType::Float)
		{
			Pushf(0.0f);
			PopToVariable(task, instruction.data.uintVal);
		}
	}
	else // Mode::CAST
	{
		// The game's cast converts the value for two types and only retags it for the others
		switch (instruction.type)
		{
		case DataType::Int:
			// The bits read as a float, truncated through a 64-bit integer and its low 32 bits kept: out of range and NaN
			// give 0
			Push(VMValue(static_cast<int32_t>(Ftol(Pop().floatVal))), DataType::Int);
			break;
		case DataType::Float:
			// The bits read as an unsigned 32-bit integer
			Push(VMValue(static_cast<float>(Pop().uintVal)), DataType::Float);
			break;
		case DataType::Vector:
		case DataType::Object:
		case DataType::Boolean:
			// The same bits, the new type
			Push(Pop(), instruction.type);
			break;
		default:
			// Nothing: the value stays as it was
			break;
		}
	}
}

void LHVM::Opcode24Call(VMTask& task, const VMInstruction& instruction)
{
	const auto newTaskId = StartScript(instruction.data.uintVal);
	if (instruction.mode == VMMode::Sync)
	{
		task.waitingTaskId = newTaskId;
	}
}

void LHVM::Opcode25EndExcept(VMTask& task, const VMInstruction& instruction)
{
	if (instruction.mode == VMMode::EndExcept)
	{
		if (_currentTask != nullptr && !_currentTask->exceptionHandlerIps.empty())
		{
			_currentTask->exceptionHandlerIps.pop_back();
		}
	}
	else // Mode::Yield
	{
		task.iield = true;
		task.instructionAddress++;
	}
}

void LHVM::Opcode26RetExcept(VMTask& task, const VMInstruction& /*instruction*/)
{
	task.instructionAddress = task.pevInstructionAddress;
	task.pevInstructionAddress = 0;
	task.inExceptionHandler = false;
}

void LHVM::Opcode27IterExcept(VMTask& task, const VMInstruction& /*instruction*/)
{
	task.currentExceptionHandlerIndex++;
	if (task.currentExceptionHandlerIndex < task.exceptionHandlerIps.size())
	{
		task.instructionAddress = GetCurrentExceptionHandlerIp(task, task.currentExceptionHandlerIndex) - 1;
	}
	else
	{
		task.instructionAddress = task.pevInstructionAddress;
		task.pevInstructionAddress = 0;
		task.inExceptionHandler = false;
	}
}

void LHVM::Opcode28BrkExcept(VMTask& task, const VMInstruction& /*instruction*/)
{
	// The handler that runs and each handler of a block inside it go. The handlers of the blocks around it stay.
	for (uint32_t i = 0; i <= task.currentExceptionHandlerIndex && !task.exceptionHandlerIps.empty(); ++i)
	{
		task.exceptionHandlerIps.pop_back();
	}

	task.pevInstructionAddress = 0;
	task.instructionAddress++;
	task.inExceptionHandler = false;
}

void LHVM::Opcode29Swap(VMTask& /*task*/, const VMInstruction& instruction)
{
	if (instruction.type == DataType::Int) // swap the 2 topmost values on the stack
	{
		DataType t0;
		DataType t1;
		const VMValue v0 = Pop(t0);
		const VMValue v1 = Pop(t1);
		Push(v0, t0);
		Push(v1, t1);
		return;
	}
	const auto count = instruction.data.uintVal;
	if (count == 0)
	{
		SignalError(ErrorCode::ErrInvalidOperand);
		return;
	}
	// The game keeps at most 31 of the values that it takes off. For the others, it puts back zeros of the last type.
	std::array<VMValue, VMStack::k_Capacity> kept {};
	std::array<DataType, VMStack::k_Capacity> keptTypes {};
	size_t keptCount = 0;
	auto lastType = DataType::None;
	const auto keep = [&](VMValue value, DataType type) {
		if (keptCount < kept.size())
		{
			kept.at(keptCount) = value;
			keptTypes.at(keptCount) = type;
			++keptCount;
		}
	};
	const auto putBack = [&]() {
		if (keptCount == 0)
		{
			Push(VMValue(0u), lastType);
			return;
		}
		--keptCount;
		lastType = keptTypes.at(keptCount);
		Push(kept.at(keptCount), lastType);
	};
	VMValue chosen;
	auto chosenType = DataType::None;
	if (instruction.mode == VMMode::CopyFrom)
	{
		// The value N places down moves to the top, and the values above it move down one place
		for (uint32_t i = 0; i < count; ++i)
		{
			const auto value = Pop(lastType);
			if (i == count - 1)
			{
				chosen = value;
				chosenType = lastType;
			}
			else
			{
				keep(value, lastType);
			}
		}
		for (uint32_t i = 0; i + 1 < count; ++i)
		{
			putBack();
		}
		Push(chosen, chosenType);
	}
	else // Mode::CopyTo: a copy of the topmost value goes N places down
	{
		for (uint32_t i = 0; i < count; ++i)
		{
			const auto value = Pop(lastType);
			if (i == 0)
			{
				chosen = value;
				chosenType = lastType;
			}
			keep(value, lastType);
		}
		Push(chosen, chosenType);
		for (uint32_t i = 0; i < count; ++i)
		{
			putBack();
		}
	}
}

void LHVM::Opcode30Line(VMTask& /*task*/, const VMInstruction& /*instruction*/) {}

bool LHVM::CallNative(uint32_t id, std::span<const NativeArgument> arguments,
                      std::vector<std::pair<VMValue, DataType>>& results)
{
	if (_functions == nullptr || id == 0 || id >= _functions->size() || _functions->at(id).impl == nullptr ||
	    _currentTask != nullptr)
	{
		return false;
	}
	_currentStack = &_mainStack;
	const auto before = _mainStack.count;
	// The call's texts go after the scripts' own data, where the native's lookup of a text's place finds them, and are
	// taken off again after it: the scripts' data is as it was, and repeated calls don't make it grow
	const auto scriptsData = _data.size();
	for (const auto& argument : arguments)
	{
		if (const auto* text = std::get_if<std::string>(&argument))
		{
			const auto offset = static_cast<uint32_t>(_data.size());
			_programFile.reset();
			_data.insert(_data.end(), text->begin(), text->end());
			_data.push_back('\0');
			Push(VMValue(offset), DataType::Int);
		}
		else
		{
			const auto& [value, type] = std::get<std::pair<VMValue, DataType>>(argument);
			Push(value, type);
		}
	}
	const auto withTexts = _data.size();
	_mainStack.pushCount = 0;
	_mainStack.popCount = 0;
	InvokeNativeCallEnterCallback(id);
	_functions->at(id).impl();
	InvokeNativeCallExitCallback(id);
	// A native that loaded other scripts has replaced the data, texts and all: only the call's own texts are taken off
	if (_data.size() == withTexts && withTexts != scriptsData)
	{
		_programFile.reset();
		_data.resize(scriptsData);
	}
	// What it gave back sits on top of what was there before
	results.clear();
	const auto pushed = std::min<uint32_t>(_mainStack.pushCount, _mainStack.count);
	const auto first = _mainStack.count - pushed;
	for (uint32_t i = first; i < _mainStack.count; ++i)
	{
		results.emplace_back(_mainStack.values.at(i), _mainStack.types.at(i));
	}
	_mainStack.count = std::min(_mainStack.count, before);
	return true;
}

} // namespace openblack::lhvm
