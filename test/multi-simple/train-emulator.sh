#!/bin/sh
## ccs, train an emulator on input_model_file.dat using a first order regression
##
## binpath reaches into build, is this a bad idea?
#binpath=../../build/src/interactive_emulator
binpath=interactive_emulator
$binpath estimate_thetas multi-test-input.dat multi_snapshot_file --regression_order=1
