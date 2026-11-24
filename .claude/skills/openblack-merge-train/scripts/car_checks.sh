# The checks, the PR and the merge of one car. batch.sh and car_finish.sh source this file in the car's worktree,
# with N, NAME, WT, IN and OUT set.
[ ${#IN[@]} -gt 0 ] || { echo "STOP: nothing merged"; exit 1; }
if [ -f src/Debug/TestbedScenarioRegistry.h ]; then
  python "$HERE/fix_kind_names.py" . || { echo "STOP: testbed kinds and names differ"; exit 3; }
  [ -n "$(git status --porcelain src/Debug/TestbedScenarioRegistry.cpp)" ] && git commit -qam "Testbed: the command names array holds every command after the merge" -m "$OB_TRAIN_TRAILER"
fi
echo "--- format (clang-format 17, every changed C++ file)"; bash "$HERE/format_check.sh" "$WT" </dev/null || { echo "STOP: format: a changed C++ file is not clang-format 17 clean, or clang-format 17 is missing (see the lines above)"; exit 21; }
extra_check start "$WT" "$NAME"
if [ -f tools/build/check_draw_order.py ]; then echo "--- draw order"; python tools/build/check_draw_order.py || { echo "STOP: two random draws in no fixed order (tools/build/check_draw_order.py)"; exit 13; }; fi
echo "--- configure"; $OB configure --fresh > car-cfg.log 2>&1 || { tail -20 car-cfg.log; echo "STOP: configure"; exit 4; }
echo "--- tidy (clang-tidy 17, changed lines)"; bash "$HERE/tidy_check.sh" "$WT" "$($OB dir)" </dev/null || { s=$?; [ $s = 23 ] && echo "STOP: tidy: a changed line has a clang-tidy 17 finding, or a NOLINT without a check name or a reason (see the lines above)" || echo "STOP: tidy: the check cannot run (clang-tidy 17 or compile_commands.json is missing, or a file timed out)"; exit $s; }
echo "--- debug"; $OB build > car-dbg.log 2>&1 || { grep -E "error|warning" car-dbg.log | head; echo "STOP: debug build"; exit 5; }
dw=$(warnings car-dbg.log); echo "debug warnings: $dw"
echo "--- relwdi"; $OB build --config RelWithDebInfo > car-rel.log 2>&1 || { grep -E "error|warning" car-rel.log | head; echo "STOP: relwdi build"; exit 6; }
rw=$(warnings car-rel.log); echo "relwdi warnings: $rw"
echo "--- ship build (developer tools OFF): openblack and openblack-server"; SHIP=$OB_BUILDS/prm-wtb$NAME-ship
OB_CMAKE_ARGS="-DOPENBLACK_DEVELOPER_TOOLS=OFF" $OB configure --fresh --build-dir "$SHIP" > car-ship-cfg.log 2>&1 || { tail -20 car-ship-cfg.log; echo "STOP: ship configure"; exit 17; }
OB_CMAKE_ARGS="-DOPENBLACK_DEVELOPER_TOOLS=OFF" $OB build --build-dir "$SHIP" openblack openblack-server > car-ship.log 2>&1 || { grep -E "error|warning" car-ship.log | head; echo "STOP: ship build (developer tools OFF)"; exit 17; }
echo "ship warnings: $(warnings car-ship.log)"
echo "--- linux gcc 13 server build (WSL; train.linuxGccServer, off by default)"
bash "$HERE/linux_gcc_server.sh" "$WT" > car-gcc.log 2>&1 </dev/null || { tail -30 car-gcc.log; echo "STOP: openblack-server does not build on Linux GCC 13 (car-gcc.log)"; exit 19; }
grep -E "^gcc: (skipped|openblack-server builds|[0-9]+ after)" car-gcc.log
echo "--- test"; $OB test > car-test.log 2>&1
tests=$(grep -m1 -E 'tests passed, [0-9]+ tests failed out of [0-9]+' car-test.log); echo "$tests"
flaky=""
if ! echo "$tests" | grep -q '100% tests passed'; then
  failed=$(grep -E '^\s+[0-9]+ - test_[A-Za-z0-9_]+ \(Failed\)' car-test.log | sed -E 's/.* - (test_[A-Za-z0-9_]+) .*/\1/')
  ok=1
  # A test that failed under machine load must pass 3 of 3 alone, else the car stops
  for t in $failed; do for i in 1 2 3; do $OB test -R "^$t\$" 2>&1 | grep -q '100% tests passed' || ok=0; done; done
  if [ -n "$failed" ] && [ $ok = 1 ]; then flaky=$(echo $failed | tr ' ' ','); else echo "STOP: tests failed: $failed"; exit 7; fi
fi
echo "--- full test program in one process, then shuffled (test order leaks)"; bd=$($OB dir); tp=$(test_program "$bd")
(cd "$bd/test" && "$tp" > "$WT/car-full.log" 2>&1 && "$tp" --gtest_shuffle --gtest_random_seed=1 > "$WT/car-shuffle.log" 2>&1) || { grep -E "FAILED  \]|Assertion" "$WT/car-full.log" "$WT/car-shuffle.log" | head; echo "STOP: the full openblack_tests run (plain or shuffled) failed: a test leaks state into a later test"; exit 14; }
echo "--- relwdi oracle and bit tests"; $OB test -C RelWithDebInfo -R "oracle|x87|sweep|bit" > car-reltest.log 2>&1; grep -m1 -E "tests passed, [0-9]+ tests failed" car-reltest.log; grep -q "100% tests passed" car-reltest.log || { echo "STOP: relwdi oracle/bit tests failed"; exit 12; }
echo "--- net_check mpm_3p_1, 3 clients"; bash "$HERE/nc3_check.sh" "$WT" "$($OB dir)" > car-nc3.log 2>&1 </dev/null || { cat car-nc3.log; echo "STOP: mpm_3p_1 3-client net_check failed (a slot parted, a client resumed or an expect was missed)"; exit 15; }
echo "--- web check (one Release web build, Land 1 on WebGPU in headless Chrome)"; WEB=$OB_BUILDS/prm-wtb$NAME-web
bash "$HERE/web_check.sh" "$WT" "$WEB" "$P/carlogs/batch$NAME-web" > car-web.log 2>&1 </dev/null || { tail -25 car-web.log; echo "STOP: web check failed: the land did not draw or the console has a fault on WebGPU ($P/carlogs/batch$NAME-web, $WEB stays for a look)"; exit 18; }
grep -E "^web (build|gate|check):" car-web.log; rm -rf "$WEB"
if [ -n "$OB_EXTRA_CHECK" ]; then echo "--- extra check (train.extraCheck)"; extra_check report "$WT" "$NAME" || { echo "STOP: extra check failed"; exit 15; }; fi
[ "$dw" = 0 ] && [ "$rw" = 0 ] || { echo "STOP: warnings"; exit 8; }
count=$(echo "$tests" | sed -E 's/.*out of ([0-9]+).*/\1/')
for f in car-*.log; do mv "$f" "$P/carlogs/batch$NAME-$f"; done
OPENBLACK_UPDATE_INSPECTOR_QUERIES=1 $OB test -R '^test_inspector_systems$' 2>&1 | grep "tests passed"
python tools/inspector/generate_component_fields.py >/dev/null 2>&1
if [ -n "$(git status --porcelain)" ]; then
  git add -u && git commit -q -m "Inspector: regenerate the catalogue and component fields" -m "$OB_TRAIN_TRAILER"
fi
cd "$OB_MAIN_CHECKOUT" && git fetch -q $R
git -C "$WT" merge-base --is-ancestor $R/$M HEAD || { echo "STOP: $M moved during the build"; exit 9; }
git -C "$WT" push -q $R HEAD:refs/heads/batch-$NAME-pr
car=$(git -C "$WT" rev-parse --short=9 HEAD)
body=$P/prs/batch-$NAME.md
{
  echo "Merge train $N: batch $NAME, ${#IN[@]} lanes merged together into one car ($car), built and tested once."
  echo
  echo "Train car (local checks): Debug and RelWithDebInfo builds with 0 warnings; full ctest $count/$count${flaky:+ ($flaky failed once under machine load in the full run and passed 3 of 3 alone)}."
  [ ${#OUT[@]} -gt 0 ] && { echo; echo "Left for the next batch (conflicts): ${OUT[*]}"; }
  for c in "${IN[@]}"; do
    s=${c%%:*}
    echo; echo "---"; echo
    if [ -f "$P/prs/$s.md" ]; then
      sed -e "s/^Merge train {N}: /### /" -e "s/{CAR}/$car/" -e "s#{TESTS}#$count/$count (in this batch)#" -e '/Generated with \[Claude Code\]/d' "$P/prs/$s.md"
    else
      echo "### ${c#*:} (no PR body in prs/$s.md)"
    fi
  done
  echo; echo "🤖 Generated with [Claude Code](https://claude.com/claude-code)"
} > "$body"
titles=$(for c in "${IN[@]}"; do echo -n "${c#*:}, "; done | sed 's/, $//')
url=$(gh pr create -R "$OB_GITHUB_REPO" --base $M --head batch-$NAME-pr --title "Merge train $N: batch $NAME ($titles)" --body-file "$body") || { echo "STOP: pr create failed"; exit 10; }
sleep 5
gh pr merge "$url" -R "$OB_GITHUB_REPO" --merge || { echo "STOP: merge failed ($url)"; exit 11; }
git fetch -q $R && git merge --ff-only -q $R/$M
# The game of the new main branch goes into the master store (ob-build store), so the next car's before and after
# check takes it from there and does not build the old main branch again. The merge commit has the tree of the car but
# its own build id, and the server refuses a client with another build id. So the car's worktree moves to the merge
# commit and the game and the server build again first: only the build id compiles, then the two programs link.
sl="$P/carlogs/batch$NAME-store.log"
{ git -C "$WT" merge --ff-only -q $R/$M && $OB build --worktree "$WT" --config RelWithDebInfo openblack openblack-server &&
  $OB store --worktree "$WT" --config RelWithDebInfo $R/$M && $OB store-check --config RelWithDebInfo $R/$M; } > "$sl" 2>&1 &&
  echo "STORED $(git rev-parse --short=9 $R/$M) in the master store, with its own build id" || echo "note: the new $M is not in the master store ($sl)"
echo "MERGED $N batch $NAME $url car $car ($count tests): ${IN[*]}"
[ -n "$OB_LEAD_LOG" ] && echo "- MERGED $url (train $N, batch $NAME: ${IN[*]}; car $car $count/$count${flaky:+, flaky $flaky}; left out: ${OUT[*]:-none}), main ff'd $(git rev-parse --short=9 $R/$M)." >> "$OB_LEAD_LOG"
[ ${#OUT[@]} -gt 0 ] && echo "LEFT OUT: ${OUT[*]}"
echo "BATCH DONE"
