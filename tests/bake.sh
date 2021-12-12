#!/bin/bash

file="$1"
echo $file
../bcpp $file > $file.ref
