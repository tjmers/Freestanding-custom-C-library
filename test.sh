#!/bin/bash


for arg in "$@"; do
	echo "Running $arg"
	eval "$arg"
	rc=$?

	echo "$arg returns with exit code $rc"

	# Exit early if it doesn't return 0
	if [[ $rc -ne 0 ]]; then
		exit 1
	fi
done

exit 0