#!/bin/bash
set -eu

# Build the kernel and the shell, then boot them on QEMU's loongarch
# "virt" machine.  See the Makefile for the individual build steps.
exec make run
