#!/usr/bin/env python3

import os
import subprocess

installdir = os.environ['MESON_INSTALL_PREFIX']

if not os.environ.get('DESTDIR'):
  print('Byte-compiling python modules...')
  subprocess.call(['python', '-m', 'compileall', '-f', '-q', installdir])

  print('Byte-compiling python modules (optimized versions) ...')
  subprocess.call(['python', '-O', '-m', 'compileall', '-f', '-q', installdir])
