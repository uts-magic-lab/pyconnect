# PyConnect
A lightweight framework to integrate C++ programs with Python

## Introduction
**PyConnect** is a **very simple** and lightweight development framework to integrate existing C/C++ programs with Python. By re-declaring the existing functions/methods and variables using PYCONNCECT macros, you can expose your program's functionality to a local or remotely running Python interpreter as a normal extension module.

PyConnect is composed of two major components:

1. A wrapper library to be integrated (and compile) with your existing C++ code. It provides PYCONNECT macros to expose your program's variables and methods.

1. A PyConnect extension module that act as a conduit between Python intepretor and the PyConnect wrapped program. You need to ```import PyConnect``` in Python intepretor.

## Key features of PyConnect

1. Minimum modifications is required to transform a generic C++ program into a Python scriptable program. Normally, you only need to add about few lines of code into your existing C++ source code. See sample code in ```testing``` subdirectory.

1. No impact on the program's existing behaviour. Your program should still operate as usual. Your existing ways of controlling the program should remain intact.

1. An uniform Pythonic interface. If you have wrapped multiple programs with PyConnect. All your programs will be listed under PyConnect module in the Python interpreter. You can use same programming scheme to write scripts for your programs.

1. Dynamic binding. Your program still runs as an standalone process. Any program failure/crash will be handled gracefully without crashing the Python engine. A Python callback function is invoked when your program gets an exception and/or goes offline.

1. Autodiscovery. PyConnect employs a very simple auto-discovery scheme. Currently, with a PyConnect enabled local network setup, the Python extension module and PyConnect wrapped programs will establish their communication automagically. See section PyConnect enabled network setup.

## Supported compilers and environments
All major C++ compilers that support C++14 standard. That is,

* GCC 4.9 or newer
* Clang 3.4 or newer
* Visual Studio 2015 C/C++ compiler.

PyConnect works under Linux, OS X (powerpc and intel), Solaris and Windows.

### Limitations of PyConnect

1. Only basic C/C++ data types are supported. That is, ```void```, ```bool```, ```int```, ```float```, ```double``` and ```std::string```. Yes, this is very primitive compared with what [pybind11](https://github.com/pybind/pybind11) supports. In fact, I will recommend to use pybind11 by default **unless** you are looking for dynamic binding and remote control your program over network.

1. Not suitable for any programs that has soft/hard realtime requirement.

## Documentation
Check ```pyconnect_intro.pdf``` file under ```doc``` directory.

### Compile PyConnect extension module
#### Linux (debian)

1. You need to install Python and Python-dev for C++ package first (i.e. `aptitude install python python-dev`)

2. ```python pyconnect_ext_setup.py build```

#### OS X

1. ```python pyconnect_ext_setup.py build```

#### Windows

1. You need to first install latest Python (currently 2.7.15+) for Windows package. You then need to have Visual Studio 2015 installed. If you have VS2015 express version, you would also need to have Windows SDK installed.

2. Open a Visual Studio 2015 command prompt and do `set MSSdk=1` and `set DISTUTILS_USE_SDK=1`

3. now you can do `python pyconnect_ext_setup.py build`

### Compile you PyConnect wrapped program

You can do this in two ways:

1. First compile the wrapper library then link it with you program
To compile the wrapper library, just do ```mkdir build;cmake ..;make```. You need to link in ```libpyconnect_wrapper.a``` library file under ```lib``` directory and make sure all PyConnect wrapper header files are available to your program (see below).

2. Put the PyConnect wrapper library related source files with your source code together and modify your program ```Makefile``` or ```CMakeLists.txt``` to include PyConnect code with your compile. You need following PyConnect source files:
```
        PyConnectCommon.h
        PyConnectCommon.cpp
        PyConnectObjcomm.h
        PyConnectObjcomm.cpp
        PyConnectWrapper.h
        PyConnectWrapper.cpp
        PyConnectNetComm.h (network communication layer)
        PyConnectNetComm.cpp (network communication layer)
```

### PyConnect enabled network setup

In order to have PyConnect auto discovery work correctly, you need open TCP and UDP port 37251 on your host computer firewall.

## Example programs
Two very simple programs are included under testing subdirectory. Use them as an important supplements to currently very limited documentation. You can compile them with `mkdir build;cmake ..;make`

## Program License

PyConnect is released under GNU General Public License Version 3. A copy of the license is in LICENSE file. If PyConnect really works out nicely for you and you want to integrate it into your commercial product, please contact me directly.

## Contact details
I can be reached at wang.xun@gmail.com.
