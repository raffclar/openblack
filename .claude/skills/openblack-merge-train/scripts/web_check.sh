# usage: web_check.sh <worktree> <web build folder> <out folder>
# The web check of one car: one Release web build of the worktree (ob-web, beside ob-build), then the web part of the
# pre-deploy gate (skill openblack-deploy-check) on WebGPU only. The gate starts the page as a player gets it in headless
# Chrome: the name box, the first question of Land 1 and the land after it. It tests that the land drew, that the
# console has no graphics device error (WebGPU validation), no uncaught exception and no worker load error, and that
# every page file answered. The exit code is 0 only when every item passes.
# Web 551d052b4 built and passed every desktop check, but its land textures did not bind on WebGPU and the screen
# stayed black (web-sp-black-1). This check stops such a car.
set -u
WT=$1; BUILD=$2; OUT=$3
OB_WEB=${OB_WEB_TOOL:-$(dirname "${OB_BUILD_TOOL:?}")/ob-web}
GATE="$WT/.claude/skills/openblack-deploy-check/scripts/pre_deploy_gate.py"
[ -f "$OB_WEB" ] || { echo "web check: no ob-web at $OB_WEB (set OB_WEB_TOOL)"; exit 2; }
[ -f "$GATE" ] || { echo "web check: no $GATE in the car"; exit 2; }
t0=$SECONDS
echo "--- web build (ob-web, Release): $BUILD"
bash "$OB_WEB" build --worktree "$WT" --build-dir "$BUILD" --config Release >"$OUT-build.log" 2>&1 ||
  { grep -E "error|FAILED" "$OUT-build.log" | head; echo "web check: FAIL: the web build failed ($OUT-build.log)"; exit 1; }
echo "web build: $((SECONDS - t0)) s"
t1=$SECONDS
rm -rf "$OUT"
"${OB_PYTHON:-python}" -I "$GATE" --web-page "$BUILD/bin/Release" --renderers webgpu --work "$OUT" </dev/null
status=$?
# The Chrome profile holds a copy of the game files (about 200 MB): the screenshots and the logs stay
for profile in "$OUT"/*/chrome-profile; do [ -d "$profile" ] && rm -rf "$profile"; done
echo "web gate: $((SECONDS - t1)) s"
echo "web check: $([ $status = 0 ] && echo PASS || echo FAIL) in $((SECONDS - t0)) s (screenshots and console in $OUT)"
exit $status
