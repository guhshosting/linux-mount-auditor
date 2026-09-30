#!/bin/sh
set -eu
bin=$1
root=$2
n=0
run(){ "$@" >/dev/null; n=$((n+1)); }
run "$bin" help
run "$bin" version
run "$bin" inspect --mountinfo "$root/tests/fixtures/mountinfo.txt" --fstab "$root/tests/fixtures/fstab.txt" --all
run "$bin" inspect --mountinfo "$root/tests/fixtures/mountinfo.txt" --json --all
run "$bin" tree --mountinfo "$root/tests/fixtures/mountinfo.txt"
run "$bin" show /home/file --mountinfo "$root/tests/fixtures/mountinfo.txt"
run "$bin" fstab --fstab "$root/tests/fixtures/fstab.txt" --mountinfo "$root/tests/fixtures/mountinfo.txt"
run "$bin" compare --fstab "$root/tests/fixtures/fstab.txt" --mountinfo "$root/tests/fixtures/mountinfo.txt" --json
run "$bin" audit --fstab "$root/tests/fixtures/fstab.txt" --mountinfo "$root/tests/fixtures/mountinfo.txt"
"$bin" inspect --mountinfo "$root/tests/fixtures/mountinfo.txt" --all | head -n 1 >/dev/null
n=$((n+1))
if "$bin" nope >/dev/null 2>&1; then exit 1; fi; n=$((n+1))
if "$bin" inspect --mountinfo /definitely/missing >/dev/null 2>&1; then exit 1; fi; n=$((n+1))
python3 -c 'import json,subprocess,sys; json.loads(subprocess.check_output([sys.argv[1],"inspect","--mountinfo",sys.argv[2]+"/tests/fixtures/mountinfo.txt","--json","--all"]))' "$bin" "$root"
n=$((n+1))
echo "$n CLI cases passed"
