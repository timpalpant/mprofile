#!/bin/bash

clang-format -sort-includes -style=Google -i src/*.h src/*.cc
