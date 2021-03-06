/*
 *  test_sample1.hpp
 *  A simple example program uses PyConnect framework
 *
 *  Copyright 2006, 2007 Xun Wang.
 *  This file is part of PyConnect.
 *
 *  PyConnect is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  PyConnect is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef WIN_32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>
#include <unistd.h>
#endif
#include <time.h>

#include <string>

#include "PyConnectWrapper.h"
#include "PyConnectNetComm.h"

using namespace pyconnect;

#define PYCONNECT_MODULE_NAME TestSample1

class TestSample1 : public OObject
{
public:
  TestSample1();
  ~TestSample1();
  
  void helloWorld();
  void printThisText( const std::string& text );
  int doAddition( int a, int b );
  double doMultiply( float a, float b );
  float doDivision( float a, float b );
  std::string concateString( const std::string & a, const std::string & b );
  void testBoolean( bool good );

private:
  int methodCalls; //number of times methods in TestSample1 have been called
  std::string myString;

public:
  PYCONNECT_NETCOMM_DECLARE;
  PYCONNECT_WRAPPER_DECLARE;

  PYCONNECT_MODULE_DESCRIPTION( "A simple test program that uses PyConnect framework." );

  PYCONNECT_METHOD( helloWorld, "hello world method" );
  PYCONNECT_METHOD( testBoolean, "test boolean" );
  PYCONNECT_METHOD( printThisText, "prints a input text" );
  PYCONNECT_METHOD( doAddition, "add two integers" );
  PYCONNECT_METHOD( doMultiply, "multiple two float numbers" );
  PYCONNECT_METHOD( doDivision, "divide two float number" );
  PYCONNECT_METHOD( concateString, "concatated string" );

  PYCONNECT_RO_ATTRIBUTE( methodCalls, "number of method calls so far" );
  PYCONNECT_RO_ATTRIBUTE( myString, "saved concated string" );
};
