#!/bin/bash

docker build -t pypprof:latest .
docker run -v `pwd`:/src pypprof:latest /src/build_wheels.sh
