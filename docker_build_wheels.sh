#!/bin/bash

set -exuo pipefail

python3 setup.py sdist
docker run -it -v `pwd`:/src -e PLAT=manylinux2014_x86_64 quay.io/pypa/manylinux2014_x86_64 /src/build_wheels.sh