#!/bin/bash

set -exuo pipefail

docker build -t pypprof:latest .
docker run -it -v `pwd`:/src pypprof:latest /src/build_wheels.sh
