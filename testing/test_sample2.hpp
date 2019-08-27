/*
 *  test_sample2.hpp
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

#define PYCONNECT_MODULE_NAME TestSample2

class TestSample2 : public OObject, FDSetOwner
{
public:
  TestSample2();
  ~TestSample2();
  
  bool enableTimer( int period = 500 ); // period in milliseconds
  void disableTimer();
  void processContinously();
  void quit() { this->breakLoop_ = true; } 

private:
  bool breakLoop_;
  int maxFD_;
  fd_set masterFDSet_;

  int timeout; //time out period for the timer
  int timerTriggerNo; //number of time timer got triggered.

  // timer handling
  bool timerEnabled_;
  struct timeval nextTime_;
  void checkTimeout( struct timeval & curTime );

  static int compareTimeInMS( struct timeval & timeA, struct timeval & timeB );
  static void addTimeInMS( struct timeval & time, long msec );

public:
  PYCONNECT_NETCOMM_DECLARE;
  PYCONNECT_WRAPPER_DECLARE;
  
  PYCONNECT_METHOD( enableTimer, "enables the timer with specific timeout period" );
  PYCONNECT_METHOD( disableTimer, "disable the timer" );
  PYCONNECT_METHOD( quit, "stop program" );
  PYCONNECT_RO_ATTRIBUTE( timeout, "timeout period for the timer" );
  PYCONNECT_RO_ATTRIBUTE( timerTriggerNo, "timer trigger" );
};
