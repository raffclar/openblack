@echo off
rem Copyright (c) 2018-2026 openblack developers
rem
rem For a complete list of all authors, please refer to contributors.md
rem Interested in contributing? Visit https://github.com/openblack/openblack
rem
rem openblack is licensed under the GNU General Public License version 3.
rem Runs ob-build (the bash script next to this file) from cmd.exe or PowerShell through Git for Windows' bash.
setlocal
set "OB_BASH=%ProgramFiles%\Git\bin\bash.exe"
if not exist "%OB_BASH%" set "OB_BASH=%ProgramW6432%\Git\bin\bash.exe"
if not exist "%OB_BASH%" (
	echo ob-build: error: Git for Windows' bash.exe was not found 1>&2
	exit /b 1
)
"%OB_BASH%" "%~dp0ob-build" %*
exit /b %ERRORLEVEL%
