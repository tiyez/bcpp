#!/bin/bash

file="$1"
echo $file
../bcpp $file > $file.reflong
tail -n 25 $file.reflong > $file.ref
rm -rf $file.reflong
