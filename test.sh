#!/bin/bash


for arg in "$@"; do
	echo "Running $arg"
	echo "$arg returns with $( eval "$arg" )"
done