#!/bin/sh
# Check that whole C++ files match clang-format 17 (the version of the repository format check).
# Read the changed file names, one a line, from the standard input. Run it from any folder of the work tree.
# Only files in src, apps, components and test, minus .clang-format-ignore, are checked.
#
# Exit 0: every file is clean (or no C++ file is in the list).
# Exit 1: a file is not clean. The names are printed on the standard error.
# Exit 2: clang-format 17 is not found. It never falls back to another version.
#
# Find clang-format 17 in this order (the first folder or program that is version 17 wins):
#   1. the environment variable OB_CLANG_FORMAT_DIR (a folder);
#   2. the git setting openblack.clangFormatDir (a folder);
#   3. the value tools.clangFormat17Dir in .claude/openblack.local.json (a folder);
#   4. clang-format-17 on PATH;
#   5. clang-format on PATH, if it is version 17.

top=$(git rev-parse --show-toplevel) || exit 2
cd "$top" || exit 2

cf_in_dir() {
	[ -n "$1" ] || return 1
	for name in clang-format.exe clang-format; do
		if [ -f "$1/$name" ]; then
			printf '%s\n' "$1/$name"
			return 0
		fi
	done
	return 1
}

is_v17() {
	"$1" --version 2>/dev/null | grep -q -E 'version 17\.'
}

config_dir=
if [ -f .claude/obconfig.py ]; then
	config_dir=$(python .claude/obconfig.py get tools.clangFormat17Dir 2>/dev/null </dev/null || python3 .claude/obconfig.py get tools.clangFormat17Dir 2>/dev/null </dev/null)
fi

CF=
for candidate in \
	"$(cf_in_dir "$OB_CLANG_FORMAT_DIR")" \
	"$(cf_in_dir "$(git config --get openblack.clangFormatDir)")" \
	"$(cf_in_dir "$config_dir")" \
	"$(command -v clang-format-17)" \
	"$(command -v clang-format)"; do
	if [ -n "$candidate" ] && is_v17 "$candidate"; then
		CF=$candidate
		break
	fi
done

if [ -z "$CF" ]; then
	echo "format gate: clang-format 17 is not found (another version is not used)." >&2
	echo "format gate: set OB_CLANG_FORMAT_DIR, git config openblack.clangFormatDir, or tools.clangFormat17Dir." >&2
	exit 2
fi

files=$(grep -E '^(src|apps|components|test)/.*\.(c|cc|cpp|cxx|h|hh|hpp|hxx|inl)$' | tr -d '\r')
if [ -f .clang-format-ignore ] && [ -n "$files" ]; then
	files=$(printf '%s\n' "$files" | grep -v -x -F -f .clang-format-ignore)
fi
# A deleted file is not checked.
existing=
for f in $files; do
	[ -f "$f" ] && existing="$existing
$f"
done
[ -z "$existing" ] && exit 0

# clang-format prints one warning for each place that is wrong, so list each file once.
bad=$(printf '%s\n' "$existing" | sed '/^$/d' | xargs "$CF" --dry-run --Werror 2>&1 |
	grep -E '^[^ ].*:[0-9]+:[0-9]+: (error|warning):' | sed -E 's/:[0-9]+:[0-9]+: .*//' | tr -d '\r' | sort -u)
if [ -n "$bad" ]; then
	echo "format gate: these files do not match clang-format 17:" >&2
	printf '%s\n' "$bad" >&2
	echo "format gate: fix them with: \"$CF\" -i <file>" >&2
	exit 1
fi
exit 0
