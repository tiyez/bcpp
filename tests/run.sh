#!/bin/bash

function run_test () {
	local name="$1"
	../bcpp "$name" > output 2> /dev/null
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

