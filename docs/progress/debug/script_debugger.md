# Script debugger

The editor's Scripts and Log tabs: a reader and debugger for the game's compiled challenge scripts, as they run in
openblack's virtual machine. The scripts and their functions are in ../story/ and the virtual machine in ../engine/.

**Progress: 19/21 done, 1 partial — 93%**

## Browsing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Lists the loaded program's scripts by kind, searched, with how many tasks run each | done | `src/Editor/Panels/ScriptsPanel.cpp`, `src/Editor/Scripts/ScriptModel.cpp`; tests `EditorScripts.*` |
| Opens another compiled program, or goes back to the game's own | done | "Open program...", "Game's program" |
| A script's details, and starting it as the game starts scripts | done | "Script" side tab |

## Reading code

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The code coloured, every name filled in | done | `ScriptModel` |
| Clicking a jump or a call follows it, with Back and Next | done | |
| A search in the code | done | |
| The source beside the code, decompiled into the scripts' own language | done | `src/Editor/Scripts/Decompiler.cpp` with the decompiler component |
| Editing the source and compiling it into the running program, other scripts keeping their places | done | `Decompiler.h` compile; the compiler component |
| The original source files of the scripts, when they are there | partial | statements can be put on their recorded source lines (`lhvmtool --source-lines`); the panel shows decompiled source |

## Debugging

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Breakpoints set in the margin | done | |
| The running tasks with their stacks, variables and what they wait for | done | "Line", tasks list |
| Holding, stepping and continuing a task, or all of them; stopping a task or a script's tasks | done | |
| The code view following the picked task's next instruction | done | "Follow task" |
| Global variables changed in place | done | "Globals" |
| The native functions the scripts call, and which openblack has written | done | "Natives" ("Used", "Unwritten") |
| The program's data | done | "Data" |
| Exception handlers of a task | done | "Handlers" |

## Log

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scripts' output and errors from when the editor starts, kept in a ring | done | `src/Editor/Panels/LogPanel.cpp`, `src/Editor/Scripts/LogRing.h` |
| Levels to show, a search, and following the newest lines | done | |
| The number of errors in the tab's title | done | |

## Land scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's own setup script (its features and map commands) shown and stepped | todo | land scripts run once on load; only the console can run their commands (`src/Debug/Console.cpp`) |
