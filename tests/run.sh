#!/bin/bash

function run_test () {
	local name="$1"
	../bcpp "$name" > output.long 2> /dev/null
	tail -n 25 output.long > output
	rm -rf output.long
	local result="$(diff "$name.ref" output)"
	if [[ "$result" != "" ]]; then
		echo "testcase $name: failed"
		echo "$result"
	else
		echo "testcase $name: success"
	fi
}

for i in $(ls t*.c); do
	run_test "$i"
done

