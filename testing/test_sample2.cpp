/*
 *  test_sample2.cpp
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

#include "test_sample2.hpp"

#ifdef WIN_32
#define FACTOR 0x19db1ded53e8000
int gettimeofday( struct timeval * tp,void * tz )
{
  FILETIME f;
  ULARGE_INTEGER ifreq;
  LONGLONG res; 
  GetSystemTimeAsFileTime(&f);
  ifreq.HighPart = f.dwHighDateTime;
  ifreq.LowPart = f.dwLowDateTime;

  res = ifreq.QuadPart - FACTOR;
  tp->tv_sec = (long)((LONGLONG)res/10000000);
  tp->tv_usec = (long)((LONGLONG)res% 10000000000); // Micro Seonds

  return 0;
}
#else
#define max( a, b ) (a) > (b) ? (a) : (b)
#endif

PYCONNECT_LOGGING_DECLARE( "testing.log" );

int main( int argc, char ** argv )
{
  PYCONNECT_LOGGING_INIT;
  TestSample2 tp;

  tp.processContinously();
  INFO_MSG( "quiting the timer program..." );
  PYCONNECT_LOGGING_FINI;
}

TestSample2::TestSample2() :
  timeout( 500 ),
  timerTriggerNo( 0 ),
  breakLoop_( false ),
  maxFD_( 0 ),
  timerEnabled_( false )
{
  PYCONNECT_DECLARE_MODULE( TestSample2, "A simple timer program that uses PyConnect framework." );
  PYCONNECT_RO_ATTRIBUTE_DECLARE( timeout, "timeout period for the timer" );
  PYCONNECT_RO_ATTRIBUTE_DECLARE( timerTriggerNo, "timer trigger" );

  PYCONNECT_METHOD_DECLARE( enableTimer, "enables the timer with specific timeout period",
    OPT_ARG( period, int, "timeout period in milliseconds" ) );
  PYCONNECT_METHOD_DECLARE( disableTimer, "disable the timer" );
  PYCONNECT_METHOD_DECLARE( quit, "stop program", );

  FD_ZERO( &masterFDSet_ );
  PYCONNECT_NETCOMM_INIT;
  PYCONNECT_NETCOMM_ENABLE_NET;
  PYCONNECT_MODULE_INIT;  
}

TestSample2::~TestSample2()
{
  PYCONNECT_MODULE_FINI;  
  PYCONNECT_NETCOMM_FINI;
}

bool TestSample2::enableTimer( int period )
{
  if (period < 100) {
    ERROR_MSG( "TestSample2::enableTimer: only support timeout"
      " period > 100 milliseconds" );
    return false;
  }
  if (timerEnabled_) {
    WARNING_MSG( "TestSample2::enableTimer: timer already enabled." );
    return true;
  }
  printf( "TestSample2 timer enabled.\n" );
  gettimeofday( &nextTime_, NULL );
  TestSample2::addTimeInMS( nextTime_, period );
  timeout = period;
  timerEnabled_ = true;
  PYCONNECT_ATTRIBUTE_UPDATE( timeout );
  return true;
}

void TestSample2::disableTimer()
{
  if (!timerEnabled_) {
    return;
  }
  timerEnabled_ = false;
  timerTriggerNo = 0; // reset
  printf( "TestSample2 timer disabled.\n" );
}

void TestSample2::processContinously()
{
  fd_set readyFDSet;

  while (!breakLoop_) {
    struct timeval timeout, timeStamp;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // 100ms

    FD_ZERO( &readyFDSet );
    memcpy( &readyFDSet, &masterFDSet_, sizeof( masterFDSet_ ) );
    int maxFD = maxFD_;
    select( maxFD+1, &readyFDSet, NULL, NULL, &timeout ); // non-blocking select
    // get current timestamp
    gettimeofday( &timeStamp, NULL );

    // check timer
    this->checkTimeout( timeStamp );

    PYCONNECT_NETCOMM_PROCESS_DATA( &readyFDSet );
  }
}

void TestSample2::checkTimeout( timeval & curTime )
{
  if (!timerEnabled_)
    return;
  if (TestSample2::compareTimeInMS( nextTime_, curTime ) >= 0) {
    timerTriggerNo++;
    PYCONNECT_ATTRIBUTE_UPDATE( timerTriggerNo );
    TestSample2::addTimeInMS( nextTime_, timeout );
  }
}

int TestSample2::compareTimeInMS( struct timeval & timeA, struct timeval & timeB )
{
  // a > b: -1; a = b: 0; a < b: 1
  if (timeA.tv_sec > timeB.tv_sec)
    return -1;
  else if (timeA.tv_sec < timeB.tv_sec)
    return 1;
  else {
    if (timeA.tv_usec > timeB.tv_usec)
      return -1;
    else if (timeA.tv_usec < timeB.tv_usec)
      return 1;
    else
      return 0;
  }
}

void TestSample2::addTimeInMS( struct timeval & time, long msec )
{
  time.tv_sec += msec / 1000;
  time.tv_usec += (msec % 1000) * 1000;
}


void TestSample2::setFD( const SOCKET_T & fd )
{
  FD_SET( fd, &masterFDSet_ );
#ifdef WIN_32
  maxFD_++; // not really used
#else
  maxFD_ = max( fd, maxFD_ );
#endif
}

void TestSample2::clearFD( const SOCKET_T & fd )
{
  FD_CLR( fd, &masterFDSet_ );
#ifdef WIN_32
  maxFD_--; // not really used
#else
  maxFD_ = max( fd, maxFD_ );
#endif
}
