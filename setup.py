#!/usr/bin/env/python

import os
import sys

from setuptools import setup
from setuptools.extension import Extension

source_files = [os.path.join("src", x) for x in os.listdir("src") if x.lower().endswith(".cpp")]

setup(name="ccscript",
      version="1.338",
      description="ccscript",
      url="http://starmen.net/pkhack/ccscript",
      ext_modules=[
          Extension("ccscript",
                    source_files,
                    language="c++"
                    )
      ])