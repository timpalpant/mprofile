from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import ctypes
import fnmatch
import glob
import io
import os
import re
import sys

from setuptools import Extension, setup

README = os.path.join(os.path.abspath(os.path.dirname(__file__)), "README.md")
with io.open(README, encoding="utf-8") as f:
    long_description = f.read()

def globex(pattern, exclude=[]):
    return [fn for fn in glob.iglob(pattern)
            if not any(fnmatch.fnmatch(fn, pattern) for pattern in exclude)]


pythonapi = ctypes.cdll.LoadLibrary(None)
if not hasattr(pythonapi, 'PyMem_SetAllocator'):
    print(("WARNING: PyMem_SetAllocatorEx: missing, %s has not been patched. " +
           "Heap profiler will attempt runtime patching") % sys.executable)


ext = Extension(
    "mprofile._profiler",
    language="c++",
    sources=globex("src/*.cc", exclude=["*_test.cc", "*_bench.cc"]) + [
        "third_party/google/tcmalloc/sampler.cc",
    ],
    depends=glob.glob("src/*.h"),
    include_dirs=[os.getcwd(), "src"],
    define_macros=[("PY_SSIZE_T_CLEAN", None)],
    extra_compile_args=["-std=c++11"],
    extra_link_args=["-std=c++11", "-static-libstdc++"],
)


def get_version():
  """Read the version from __init__.py."""

  with open("mprofile/__init__.py") as fp:
    # Do not handle exceptions from open() so setup will fail when it cannot
    # open the file
    line = fp.read()
    version = re.search(r"^__version__ = '([0-9]+\.[0-9]+(\.[0-9]+)?-?.*)'",
                        line, re.M)
    if version:
      return version.group(1)

  raise RuntimeError(
      "Cannot determine version from mprofile/__init__.py.")


setup(
    name="mprofile",
    version=get_version(),
    description="A low-overhead memory profiler.",
    long_description=long_description,
    long_description_content_type="text/markdown",
    platforms=["Mac OS X", "POSIX"],
    classifiers=[
        "Development Status :: 2 - Pre-Alpha",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: GNU Lesser General Public License v3 (LGPLv3)",
        "Operating System :: POSIX",
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 3.4",
        "Programming Language :: Python :: 3.5",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: Implementation :: CPython",
        "Topic :: Software Development :: Testing",
        "Topic :: Software Development :: Libraries :: Python Modules"
    ],
    project_urls={
        "Source": "https://github.com/timpalpant/mprofile",
        "Tracker": "https://github.com/timpalpant/mprofile/issues",
    },
    keywords="profiling performance",
    url="http://github.com/timpalpant/mprofile",
    author="Timothy Palpant",
    author_email="tim@palpant.us",
    license="MIT",
    setup_requires=["wheel"],
    packages=["mprofile"],
    ext_modules=[ext],
    test_suite="test",
)
