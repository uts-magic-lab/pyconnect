# pyconnect
A simple framework to integrate C++ programs with Python

### IMPORTANT NOTE:
This software is still considered as experimental. Please, do not use it in an environment that requires high level of reliability. I hope enough people find PyConnect useful and help me to improve it.

## Short Description
PyConnect is a simple development framework to integrate existing C/C++ programs with Python. By re-declaring the existing functions/methods and variables in your program with macros provided by PyConnect, you can expose your program's functionality to Python interpreter, i.e. you now can write Python scripts for your program.

PyConnect consists of three components:

1. a wrapper library that contains macros you need to expose your program's functionality. You need to compile PyConnect wrapper library with your program.

2. a Python extension module that talks to your PyConnect wrapped program.

3. an InterProcess Communication (IPC) layer for communication between the extension module and your PyConnect wrapped program. At this moment, only a TCP/IP based IPC layer is provide. You can write your own IPC layer with, for example, Windows's messaging system.


## Key features of PyConnect

1. Minimum modifications is required to turn your program into a Python scriptable program. For your average programs, you need to add about few dozens lines of code. That's it.

2. No changes to your program's existing behaviour. Your program will still run as it runs before. Your existing ways of controlling the program will remain intact.

3. An uniform Python interface. If you have wrapped multiple programs with PyConnect. All your programs will be listed under PyConnect module in the Python interpreter. You can use same programming scheme to write scripts for your programs.

4. Fault tolerance. Since your program still runs as an individual process. Any program failure/crash can be gracefully handled. A Python callback function is invoked when your program gets an exception and/or goes offline.

5. Autodiscovery. PyConnect employs a very simple auto-discovery scheme. Currently, with a PyConnect enabled local network setup, the Python extension module and PyConnect wrapped programs will establish their communication automatically. See section PyConnect enabled network setup.

## Supported compilers and environments
Main compiler used in the development is GCC 4.2+ (Clang also supported).
I have also used Sun's C/C++ compiler and Visual Studio 2010 C/C++ compiler (still with some issues).
I have PyConnect wrapped programs running under Linux, OS X (powerpc and intel), Solaris and Windows. However, I can't say PyConnect has been extensively tested under these environments.

### Limitations of PyConnect

1. Only basic C/C++ data types are supported at this moment.

2. Only has one TCP/IP based IPC layer implemented.

3. Not suitable for any programs that have soft/hard realtime requirement.

### A brief history of PyConnect
PyConnect was born out of a short project (only a design proposal was required for the project completion) for my master study at the University of Technology, Sydney. The initial goal was to embed a Python engine into Sony AIBO robot for the UTS robot soccer team: UTS Unleashed!. The plan was to take many advantages of Python for further robotic programming (quite few universities' robot soccer teams use Python, in fact). However, no one wants to reprogram many existing tried-and-proven C++ components (besides, there are performance issues for computational intensive tasks). Hence the idea to develop a wrapper for existing code. It soon becomes obvious to me, this idea can go beyond the confines of AIBO. 

NOTE: The embedded Python engine (and related code) for AIBO is not included in the released package.

## Documentation
Check pyconnect_intro.pdf file under doc directory. This document has not been fully updated since PyConnect initial version for AIBO robot. I will improve it over the time. Most stuff is still very relevant.

### Compile PyConnect extension module
#### Linux (debian)

1. You need to install Python and Python-dev for C++ package first (i.e. `aptitude install python2.5 python2.5-dev`)

2. Do `python pyconnect_ext_setup.py build`

#### OS X

1. Do `python pyconnect_ext_setup.py build`

#### Windows

1. You need to first install latest Python (currently 2.7.3) for Windows package. You then need to have Visual Studio 2010 installed. If you have VS2010 express version, you would also need to have Windows SDK installed.

2. Open a Visual Studio 2010 command prompt and do `set MSSdk=1` and `set DISTUTILS_USE_SDK=1`

3. now you can do `python pyconnect_ext_setup.py build`

### Compile you PyConnect wrapped program

You can do this in two ways:

1. First compile the wrapper library then link it with you program
To compile the wrapper library, just do make wrapper. you need to link in `libPyConnectwrapper.a` file and make sure all PyConnect wrapper header files are available to your program (see below)

2. Put the PyConnect wrapper library related source files with your source code together and modify your program `Makefile` to include PyConnect code with your compile. You need following PyConnect source files:

        PyConnectCommon.h
        PyConnectCommon.cpp
        PyConnectObjcomm.h
        PyConnectObjcomm.cpp
        PyConnectWrapper.h
        PyConnectWrapper.cpp
        PyConnectNetComm.h (network communication layer)
        PyConnectNetComm.cpp (network communication layer)


### PyConnect enabled network setup

In order to have PyConnect auto discovery work correctly, you need open TCP and UDP port 37251 on your host computer firewall.

## Example programs
Two very simple programs are included under testing subdirectory. Use them as an important supplements to currently very limited documentation. You can compile them with `make test_one test_two`

## Program License

PyConnect is released under GNU General Public License Version 3. A copy of the license is in LICENSE file. If PyConnect really works out nicely for you and you want to integrate it into your commercial product, please contact me directly.

## Contact details
I can be reached at Wang.Xun@gmail.com.
