#!/bin/sh
cmake --build build && ./build/endorsement_score $@
