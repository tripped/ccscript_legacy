#!/usr/bin/env/python

import os
import sys

from setuptools import setup
from setuptools.extension import Extension


WINDOWS_BOOST_VERSION = "1_55"
WINDOWS_BOOST_DIRECTORY = "c:\\local\\boost_{}_0".format(WINDOWS_BOOST_VERSION)
WINDOWS_VISUAL_STUDIO_VERSION = "9.0"

source_files = [os.path.join("src", x) for x in os.listdir("src") if x.lower().endswith(".cpp")]
boost_libraries = ["boost_python", "boost_filesystem"]
if sys.platform == "win32":
    boost_libraries = ["{}-vc{}-{}".format(x, WINDOWS_VISUAL_STUDIO_VERSION.replace(".", ""), WINDOWS_BOOST_VERSION)
                       for x in boost_libraries]
    include_dirs = [WINDOWS_BOOST_DIRECTORY]
    library_dirs = ["{}\\lib32-msvc-{}".format(WINDOWS_BOOST_DIRECTORY, WINDOWS_VISUAL_STUDIO_VERSION)]
else:
    include_dirs = None
    library_dirs = None

setup(name="ccscript",
      version="1.337",
      description="ccscript",
      url="http://starmen.net/pkhack/ccscript",
      ext_modules=[
          Extension("ccscript",
                    source_files,
                    libraries=boost_libraries,
                    include_dirs=include_dirs,
                    library_dirs=library_dirs,
                    language="c++")
      ])