#!/bin/bash
#
# (c) 2018, Linaro Limited
#
# SPDX-License-Identifier:     BSD-3-Clause

echo "Running server"

../fdserver &

echo "Running writer"
./share_pipe_writer &

echo "Running reader"
./share_pipe_reader
