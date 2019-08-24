#
#  pyconnect_ext_setup.py
#  A PyConnect extension module build script
#
#  Copyright 2006 - 2010 Xun Wang.
#  This file is part of PyConnect.
#
#  PyConnect is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3 of the License, or
#  (at your option) any later version.
#
#  PyConnect is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import os
from distutils.core import setup, Extension
#from setuptools import setup, find_packages, Extension
#import distutilscross

BIG_ENDIAN_ARCH = [ 'sparc', 'powerpc', 'ppc' ]

macro = [('PYTHON_SERVER', None), ('PYCONNECT_DEFAULT_SERVER_ID', '2'), 
         ('MULTI_THREAD', None), ('RELEASE', None)] #, ('USE_MULTICAST', None)]

lib = []
inc_dirs = []
lib_dirs = []

osname = os.name
if osname == 'nt':
    macro = macro + [('WIN32', None), ('WIN32_LEAN_AND_MEAN', None), ('NO_WINCOM', None)]
    lib = lib + ['ws2_32', 'Kernel32', 'libeay32', 'advapi32', 'oleaut32', 'user32', 'gdi32']
    inc_dirs = ['Windows/include']
    lib_dirs = ['Windows/lib']
elif osname == 'posix':
    lib = ['crypto', 'pthread']
    f = os.popen('uname -ms')
    (myos, myarch) = f.readline().split(' ')
    f.close()
    if myos == 'Darwin' or myos.endswith( 'BSD' ):
      macro.append(('BSD_COMPAT', None))
      inc_dirs = ['/usr/local/opt/openssl/include']
      lib_dirs = ['/usr/local/opt/openssl/lib']
    elif myos == 'SunOS':
      macro.append(('SOLARIS', None))
    elif myos == 'Linux':
      macro.append(('LINUX', None))

    for arch in BIG_ENDIAN_ARCH:
       if arch in myarch:
          macro.append(('WITH_BIG_ENDIAN', None))
          break
else:
    print "unknow platform. quit"
    exit( -1 )

module1 = Extension('PyConnect',
                    define_macros = macro,
                    include_dirs = inc_dirs,
                    library_dirs = lib_dirs,
                    libraries = lib,
                    sources = ['PyConnectPyModule.cpp','PyConnectObjComm.cpp',
                    'PyConnectNetComm.cpp','PyConnectStub.cpp','PyConnectCommon.cpp'])

setup (name = 'PyConnect',
       version = '0.2.3',
       description = 'This is a Python extension module for PyConnect',
       author = 'Xun Wang',
       author_email = 'Wang.Xun@gmail.com',
       license = 'GPL V3',
       platforms = 'Win32, Linux, OS X, Solaris',
       ext_modules = [module1])
