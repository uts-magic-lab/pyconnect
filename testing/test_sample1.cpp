/*
 *  test_sample1.cpp
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
#include <unistd.h>
#include "test_sample1.hpp"

PYCONNECT_LOGGING_DECLARE( "testing.log" );

int main( int argc, char ** argv )
{
  PYCONNECT_LOGGING_INIT;
  TestSample1 tp;

  PYCONNECT_NETCOMM_PROCESS_DATA;
}

TestSample1::TestSample1() :
  methodCalls( 0 ),
  myString( "my test" )
{
  PYCONNECT_DECLARE_MODULE( TestSample1, "A simple test program that uses PyConnect framework." );

  PYCONNECT_RO_ATTRIBUTE_DECLARE( methodCalls, "number of method calls so far" );
  PYCONNECT_RO_ATTRIBUTE_DECLARE( myString, "saved concated string" );

  PYCONNECT_METHOD_DECLARE( helloWorld, void, "hello world method" );
  PYCONNECT_METHOD_DECLARE( testBoolean, void, "test boolean", ARG( good, bool, "boolean" ));
  PYCONNECT_METHOD_DECLARE( printThisText, void, "prints a input text", ARG( text, std::string, "text string" ) );
  PYCONNECT_METHOD_DECLARE( doAddition, int, "add two integers", ARG( a, int, "integer a" ) \
                         ARG( b, int, "integer b" ) );
  PYCONNECT_METHOD_DECLARE( doMultiply, double, "multiple two float numbers", ARG( a, float, "float a" ) \
                         ARG( b, float, "float b" ) );
  PYCONNECT_METHOD_DECLARE( doDivision, float, "divide two float number", ARG( a, float, "float a" ) \
                         ARG( b, float, "float b" ) );
  PYCONNECT_METHOD_DECLARE( concateString, std::string, "concatated string", ARG( a, std:string, "string a" ) \
                           ARG( b, std::string, "string b" ) );

  PYCONNECT_NETCOMM_INIT;
  PYCONNECT_NETCOMM_ENABLE_NET;
  PYCONNECT_MODULE_INIT;
}

TestSample1::~TestSample1()
{
  PYCONNECT_MODULE_FINI;  
  PYCONNECT_NETCOMM_FINI;
}

void TestSample1::helloWorld()
{
  printf( "Hello World ;)\n" );
  methodCalls++;
  PYCONNECT_ATTRIBUTE_UPDATE( methodCalls );
}

void TestSample1::printThisText( const std::string & text )
{
  printf( "Incoming test message %s\n", text.c_str() );
  methodCalls++;
  PYCONNECT_ATTRIBUTE_UPDATE( methodCalls );
}

int TestSample1::doAddition( int a, int b )
{
  int c = a + b;
  printf( "%d + %d = %d\n", a, b, c );
  methodCalls++;
  PYCONNECT_ATTRIBUTE_UPDATE( methodCalls );
  return c;
}

double TestSample1::doMultiply( float a, float b )
{
  double c = a * b;
  printf( "%f * %f = %f\n", a, b, c );
  methodCalls++;
  PYCONNECT_ATTRIBUTE_UPDATE( methodCalls );
  return c;
}

void TestSample1::testBoolean( bool good )
{
  printf( "received boolean is %s\n", good ? "true" : "false" );
  methodCalls++;
  PYCONNECT_ATTRIBUTE_UPDATE( methodCalls );
}

std::string TestSample1::concateString( const std::string & a, const std::string & b )
{
  myString = a + b;
  printf( "%s + %s = %s\n", a.c_str(), b.c_str(), myString.c_str() );
  methodCalls++;
  PYCONNECT_ATTRIBUTE_UPDATE( methodCalls );
  PYCONNECT_ATTRIBUTE_UPDATE( myString );
  return myString;
}

float TestSample1::doDivision( float a, float b )
{
  methodCalls++;
  PYCONNECT_ATTRIBUTE_UPDATE( methodCalls );

  float c = 0.0;
  try {
    c = a / b;
  }
  catch (...) {
    printf( "failed division. divide by zero\n" );
    throw;
  }
  printf( "%f / %f = %f\n", a, b, c );
  return c;
}
