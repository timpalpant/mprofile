#!/bin/bash -ex

cd /src

eval "$(pyenv init -)";

for PYVERSION in $(pyenv versions --bare); do
  pyenv shell $PYVERSION
  pip install setuptools wheel
  python setup.py test
  # NB: This is not actually manylinux2010-compatible, but ABSL and c++11
  # have ABI requirements that won't compile on the manylinux2010 images.
  # Tensorflow images are also built on Ubuntu 14.04, this seems better
  # than forcing everyone compile it. manylinux2014 should be coming soon (tm).
  python setup.py sdist bdist_wheel --plat-name manylinux2010_x86_64
done
