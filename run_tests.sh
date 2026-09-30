#!/bin/sh
# Build riscl with `make`, then compile and run every tests/*.riscl, comparing standard output with
# its .out and the exit code with its .code (0 when there is none). Standard input is its .in, or empty.
# A signal shows as 128 + its number, as the shell reports it.
make -s || exit 1
pass=0; fail=0; tmp=$(mktemp -d)
for src in tests/*.riscl; do
    base=${src%.riscl}; name=$(basename "$base")
    want_code=0; [ -f "$base.code" ] && want_code=$(cat "$base.code")
    ./riscl "$src" "$tmp/prog"; built=$?
    if [ $built -ne 0 ]; then echo "FAIL $name: riscl exited $built"; fail=$((fail + 1)); continue; fi
    if [ -f "$base.in" ]; then "$tmp/prog" < "$base.in" > "$tmp/out" 2>/dev/null; else "$tmp/prog" < /dev/null > "$tmp/out" 2>/dev/null; fi
    code=$?
    if cmp -s "$tmp/out" "$base.out" && [ "$code" -eq "$want_code" ]; then pass=$((pass + 1))
    else echo "FAIL $name: exit $code (want $want_code), output: $(head -c 80 "$tmp/out" | od -An -c | tr -s ' ' | head -2)"; fail=$((fail + 1)); fi
done
rm -rf "$tmp"
echo "$pass passed, $fail failed"
[ $fail -eq 0 ]
