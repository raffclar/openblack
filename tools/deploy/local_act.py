#!/usr/bin/env python3
"""The local act changes of the deploy workflows of a bwgame-service clone, for a deploy that runs on this computer.

The hosted runner has swap, a big disk and an artifact store. act has none of these, so a local run needs four changes
to a clone of raffclar/bwgame-service. This script makes them in the clone's files only. It never commits, never
starts a workflow and never touches the live site.

  build-game.yml       the tools step installs what the image lacks, the swap step is gone, and the build tar AND the
                       debug file of the web build go to the host folder /out (act cannot take upload-artifact v7)
  deploy-game.yml      the build jobs run alone with act -j, so their plan test is gone
  deploy-game-local.yml  a new file: the deploy job takes the tars and the debug file from /out, and its builds have the
                       commit that --sha names

The debug file is kept in /out as web-openblack.debug.wasm, beside web-build.tar. Without it the deploy cannot give the
web build's function names to Sentry (the step also needs the secret SENTRY_AUTH_TOKEN in the act secrets file).

Usage: local_act.py <clone of bwgame-service> --sha <the commit of openblack that /out holds>
"""
import argparse
import re
import sys
from pathlib import Path

APT_OLD = "sudo apt-get install -y --no-install-recommends ninja-build"
APT_NEW = ("sudo apt-get install -y --no-install-recommends cmake build-essential curl zip unzip tar git python3 "
           "python3-venv brotli rclone ninja-build")
JOBS_OLD = "BW_BUILD_JOBS: ${{ inputs.kind == 'server' && '2' || '' }}"
JOBS_NEW = "BW_BUILD_JOBS: ${{ inputs.kind == 'server' && '2' || '3' }}"
SWAP = re.compile(r"\n((?: {6}# GCC 13 needs[^\n]*\n)(?: {6}#[^\n]*\n)*)( {6}- name: Add swap\n(?: {8}[^\n]*\n| *\n)+?)(?= {6}# The caches)")
DEBUG_UPLOAD = re.compile(r" {6}- name: Keep the debug file of the web build\n(?: {8}[^\n]*\n)+")
BUILD_UPLOAD = re.compile(r" {6}- uses: actions/upload-artifact@[^\n]*\n(?: {8}[^\n]*\n)+")

KEEP_DEBUG = (
    "      # LOCAL ADAPTATION: act's artifact server does not take upload-artifact v7; keep the debug file on the host\n"
    "      - name: Keep the debug file of the web build\n"
    "        if: steps.build.outputs.debug_artifact != ''\n"
    "        run: |\n"
    "          mkdir -p /out\n"
    "          cp \"$BW_WORK_DIR/web-debug/openblack.debug.wasm\" /out/web-openblack.debug.wasm\n"
    "\n")
KEEP_BUILD = (
    "      # LOCAL ADAPTATION: act's artifact server does not take upload-artifact v7; keep the build.tar on the host\n"
    "      - name: Keep the build\n"
    "        run: |\n"
    "          mkdir -p /out\n"
    "          cp \"$RUNNER_TEMP/build.tar\" \"/out/${{ inputs.kind }}-build.tar\"\n"
    "          ls -l /out\n")


def lf(text):
    return text.replace("\r\n", "\n")


def patch_build_game(text):
    """build-game.yml for act. Raises ValueError when a part of the file is not as expected."""
    text = lf(text)
    for old, new in ((APT_OLD, APT_NEW), (JOBS_OLD, JOBS_NEW)):
        if old not in text:
            raise ValueError(f"build-game.yml has no '{old}'")
        text = text.replace(old, new, 1)
    text, swaps = SWAP.subn("\n      # LOCAL ADAPTATION: the swap step is removed; the Docker VM has 30 GB of memory\n", text, count=1)
    if not swaps:
        raise ValueError("build-game.yml has no swap step")
    # The debug upload comes before the build upload in the file; each has its own pattern
    text, debug = DEBUG_UPLOAD.subn(lambda _: KEEP_DEBUG, text, count=1)
    text, build = BUILD_UPLOAD.subn(lambda _: KEEP_BUILD, text, count=1)
    if not build:
        raise ValueError("build-game.yml has no build upload step")
    if "web-openblack.debug.wasm" not in text and debug == 0 and "debug_artifact" in text:
        raise ValueError("build-game.yml keeps a debug file, but its upload step is not as expected")
    return text


BUILD_IF = ("    if: needs.plan.outputs.build_web == 'true'\n", "    if: needs.plan.outputs.build_server == 'true'\n")


def patch_deploy_game_for_builds(text):
    """deploy-game.yml for act -j build-web / build-server: the plan outputs do not exist there, so the tests go"""
    text = lf(text)
    for line in BUILD_IF:
        if line not in text:
            raise ValueError(f"deploy-game.yml has no '{line.strip()}'")
        text = text.replace(line, "", 1)
    return text


DEPLOY_HEAD = re.compile(r"    needs: \[plan, build-web, build-server, test-site\]\n    if: >-\n(?:      [^\n]*\n)+")
DOWNLOADS = re.compile(r"      - if: needs\.plan\.outputs\.build_web == 'true'\n        uses: actions/download-artifact@[^\n]*\n"
                       r"(?: {8}[^\n]*\n)+"
                       r"      - if: needs\.plan\.outputs\.build_web == 'true' && needs\.build-web\.outputs\.debug_artifact != ''\n"
                       r"        uses: actions/download-artifact@[^\n]*\n(?: {8}[^\n]*\n)+"
                       r"      - if: needs\.plan\.outputs\.build_server == 'true'\n        uses: actions/download-artifact@[^\n]*\n"
                       r"(?: {8}[^\n]*\n)+")
LOCAL_TAKE = (
    "      # LOCAL DEPLOY: the builds and the debug file come from the host folder /out (made by act build-game runs)\n"
    "      - name: Take the builds from the host\n"
    "        id: host\n"
    "        run: |\n"
    "          mkdir -p \"$RUNNER_TEMP/web\" \"$RUNNER_TEMP/server\" \"$RUNNER_TEMP/web-debug\"\n"
    "          cp /out/web-build.tar \"$RUNNER_TEMP/web/build.tar\"\n"
    "          cp /out/server-build.tar \"$RUNNER_TEMP/server/build.tar\"\n"
    "          if [ -f /out/web-openblack.debug.wasm ]; then\n"
    "            cp /out/web-openblack.debug.wasm \"$RUNNER_TEMP/web-debug/openblack.debug.wasm\"\n"
    "            echo found=true >> \"$GITHUB_OUTPUT\"\n"
    "          else\n"
    "            echo \"::warning::no web-openblack.debug.wasm in /out: Sentry gets no debug file\"\n"
    "            echo found=false >> \"$GITHUB_OUTPUT\"\n"
    "          fi\n")
SENTRY_IF_OLD = ("        if: env.WEB == 'true' && needs.plan.outputs.build_web == 'true' && "
                 "needs.build-web.outputs.debug_artifact != ''\n")
SENTRY_IF_NEW = "        if: env.WEB == 'true' && steps.host.outputs.found == 'true'\n"


def make_local_deploy(text, sha):
    """deploy-game-local.yml from the pristine deploy-game.yml"""
    if not re.fullmatch(r"[0-9a-f]{7,40}", sha):
        raise ValueError(f"'{sha}' is not a commit")
    text = lf(text)
    text = text.replace("name: Deploy game\n", "name: Deploy game local\n", 1)
    text = patch_deploy_game_for_builds(text)
    text, heads = DEPLOY_HEAD.subn("    needs: [plan, test-site]\n    if: needs.plan.result == 'success' && "
                                   "needs.test-site.result == 'success'\n", text, count=1)
    text, takes = DOWNLOADS.subn(lambda _: LOCAL_TAKE, text, count=1)
    if SENTRY_IF_OLD not in text:
        raise ValueError("deploy-game.yml has no Sentry debug step as expected")
    text = text.replace(SENTRY_IF_OLD, SENTRY_IF_NEW, 1)
    for name in ("WEB_SHA: ${{ needs.build-web.outputs.openblack_sha }}", "SERVER_SHA: ${{ needs.build-server.outputs.openblack_sha }}"):
        if name not in text:
            raise ValueError(f"deploy-game.yml has no '{name}'")
        text = text.replace(name, name.split(":")[0] + ": " + sha, 1)
    if not heads or not takes:
        raise ValueError("deploy-game.yml deploy job is not as expected")
    if "needs.build-web" in text.split("  deploy:\n", 1)[1]:
        raise ValueError("the local deploy job still reads a build job")
    return text


def apply(root, sha):
    workflows = Path(root) / ".github" / "workflows"
    deploy_text = (workflows / "deploy-game.yml").read_text(encoding="utf-8")
    build_text = (workflows / "build-game.yml").read_text(encoding="utf-8")
    local = make_local_deploy(deploy_text, sha)
    build = patch_build_game(build_text)
    builds = patch_deploy_game_for_builds(deploy_text)
    (workflows / "build-game.yml").write_text(build, encoding="utf-8", newline="\n")
    (workflows / "deploy-game.yml").write_text(builds, encoding="utf-8", newline="\n")
    (workflows / "deploy-game-local.yml").write_text(local, encoding="utf-8", newline="\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("clone", help="a clone of bwgame-service")
    parser.add_argument("--sha", required=True, help="the commit of openblack that the tars in /out were built from")
    options = parser.parse_args()
    try:
        apply(options.clone, options.sha)
    except ValueError as error:
        print(f"stop: {error}", file=sys.stderr)
        return 1
    print("applied: build-game.yml, deploy-game.yml (build jobs), deploy-game-local.yml")
    return 0


if __name__ == "__main__":
    sys.exit(main())
