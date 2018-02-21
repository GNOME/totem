#!/usr/bin/env python3

import glob
import os
import subprocess
import sys

for file in glob.glob(os.path.join(sys.argv[2], '**/*.py')):
  subprocess.call([sys.argv[1], '-d', 'C0111', '-d', 'W0511', '-d', 'F0401', file])
