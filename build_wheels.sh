#!/bin/bash -ex

for PYBIN in /opt/python/*/bin; do
  if [[ "$PYBIN" == *cp311* ]] || [[ "$PYBIN" == *pypy* ]]; then
    continue
  fi

  $PYBIN/pip wheel /src --no-deps -w /src/dist
done

for whl in /src/dist/*.whl; do
    auditwheel repair "$whl" --plat "$PLAT" -w /src/dist
done