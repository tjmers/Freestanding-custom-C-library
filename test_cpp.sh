#!/bin/bash

verify() {
  if [[ $1 -ne 0 ]]; then
    exit $1
  fi
}

make tests_cpp
verify $?

eval ./test.sh build/test_cpp/*
verify $?

echo "All C++ test cases complete"
