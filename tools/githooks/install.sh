#!/bin/sh
# Install the openblack git hooks into the common git folder of this repository.
# The common folder is shared by every worktree, so one install covers all of them.
# A different hook that is already there is saved as <name>.bak before it is replaced.
#
# usage: sh tools/githooks/install.sh        (run it from any folder of the repository)
# PowerShell users: run it through Git Bash:  & "C:\Program Files\Git\bin\sh.exe" tools/githooks/install.sh

set -eu

here=$(cd "$(dirname "$0")" && pwd)
common=$(git -C "$here" rev-parse --path-format=absolute --git-common-dir)
dest="$common/hooks"
mkdir -p "$dest"

for hook in pre-commit; do
	src="$here/$hook"
	target="$dest/$hook"
	if [ -f "$target" ] && ! cmp -s "$src" "$target"; then
		backup="$target.bak"
		n=1
		while [ -e "$backup" ]; do
			backup="$target.bak.$n"
			n=$((n + 1))
		done
		cp "$target" "$backup"
		echo "install: saved the old $hook as $backup"
	fi
	cp "$src" "$target"
	chmod +x "$target"
	echo "install: $hook is in $dest"
done
