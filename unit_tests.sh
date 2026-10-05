#!/bin/bash

verify() {
  if [[ $1 -ne 0 ]]; then
    exit $1
  fi
}

make
eval ./test.sh build/test/*
verify $?

eval test/printf/printf_test.sh
verify $?

echo "All test cases complete"