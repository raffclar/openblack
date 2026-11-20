#!/bin/bash
# Builds openblack-server of the car commit on Linux with GCC 13 (WSL), as the server nightly and the deploy do:
# preset ninja-multi-vcpkg-shipping, Release, the -Werror of the components, the overlay ports of the repository.
# A Linux-only compiler error stops the car and not the deploy.
#
#   linux_gcc_server.sh <worktree>        exit 0: builds; 19: compile error or no build; 0 with "skipped": switched off
#
# Settings (.claude/openblack.local.json, "train"): linuxGccServer (true or false), linuxGccDistro (the WSL
# distribution, empty for the default one), linuxGccDir (the folder in the WSL file system, default
# /root/openblack-gcc). The build tree must stay in the WSL file system: on /mnt/<drive> the build takes about 75 min.
#
# The warm cache is $linuxGccDir inside WSL: src/ (the car's files; rsync changes only the files that differ, so ninja
# rebuilds only those), src/vcpkg (a vcpkg clone at the commit that the car pins), src/cmake-build-presets (the build
# tree) and vcpkg-binary/ (the vcpkg binary cache). The build is not parallel to the Windows checks; WSL uses all cores.
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
CLAUDE_DIR=$(cd "$HERE/../../.." && pwd)
eval "$(python "$CLAUDE_DIR/obconfig.py" shell)" || { echo "gcc: cannot read the skill settings"; exit 20; }
WT=${1:?usage: linux_gcc_server.sh <worktree>}
case "$(echo "$OB_LINUX_GCC" | tr 'A-Z' 'a-z')" in
  true | 1 | yes | on) ;;
  *) echo "gcc: skipped (train.linuxGccServer is off)"; exit 0 ;;
esac
command -v wsl > /dev/null 2>&1 || { echo "gcc: STOP: wsl is not installed (set train.linuxGccServer to false to skip this step)"; exit 19; }
W=(wsl); [ -n "$OB_LINUX_GCC_DISTRO" ] && W=(wsl -d "$OB_LINUX_GCC_DISTRO")
DIR=${OB_LINUX_GCC_DIR:-/root/openblack-gcc}
export MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*'
t0=$(date +%s)
sha=$(git -C "$WT" rev-parse HEAD) || exit 19
vsha=$(git -C "$WT" ls-tree HEAD vcpkg | awk '{print $3}')
[ -n "$vsha" ] || { echo "gcc: STOP: the commit has no vcpkg pin"; exit 19; }
echo "gcc: openblack-server of ${sha:0:9} on WSL GCC 13 (cache $DIR)"
# The car's files go to a stage folder in the WSL file system; the build script syncs them into src
"${W[@]}" -e bash -c "rm -rf '$DIR/stage' && mkdir -p '$DIR/stage'" || { echo "gcc: STOP: WSL does not run"; exit 19; }
git -C "$WT" archive --format=tar HEAD | "${W[@]}" -e tar -x -C "$DIR/stage" || { echo "gcc: STOP: cannot copy the files into WSL"; exit 19; }
tr -d '\r' < "$HERE/linux_gcc_build.sh" | "${W[@]}" -e bash -c "cat > '$DIR/build.sh'" || { echo "gcc: STOP: cannot copy the build script into WSL"; exit 19; }
"${W[@]}" -e bash "$DIR/build.sh" "$DIR" "$vsha" "${OB_LINUX_GCC_JOBS:-}" < /dev/null 2>&1
rc=$?
echo "gcc: ${rc} after $(( $(date +%s) - t0 )) s"
[ "$rc" = 0 ] || exit 19
exit 0
