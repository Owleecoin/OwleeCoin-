#!/usr/bin/env bash
#
# Copyright (c) 2019-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export CI_IMAGE_NAME_TAG="docker.io/debian:bookworm"
export CONTAINER_NAME=ci_native_fuzz_valgrind
export PACKAGES="clang llvm libclang-rt-dev libevent-dev libboost-dev libsqlite3-dev valgrind"
export NO_DEPENDS=1
export RUN_UNIT_TESTS=false
export RUN_FUNCTIONAL_TESTS=false
export RUN_FUZZ_TESTS=true
export FUZZ_TESTS_CONFIG="--valgrind"
export GOAL="install"
export SYSCOIN_CONFIG="--enable-fuzz --with-sanitizers=fuzzer CC=clang-18 CXX=clang++-18"
