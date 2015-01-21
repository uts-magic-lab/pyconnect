/*
 *  PyConnectPyModule.cpp
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
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#endif
#include "PyConnectNetComm.h"
#include "PyConnectStub.h"

// critical section/mutex
#ifdef WIN_32
CRITICAL_SECTION g_criticalSection;
#else
pthread_mutex_t g_mutex;
pthread_mutexattr_t mta;
pthread_t g_iothread;
#endif

using namespace pyconnect;

PYCONNECT_LOGGING_DECLARE( "pyconnect.log" );

/* Initialization function for the module (*must* be called initxx) */
#ifndef PyMODINIT_FUNC  /* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif

#ifdef WIN_32
void ioprocess_thread( void * arg )
#else
void * ioprocess_thread( void * arg )
#endif
{
  PyConnectNetComm::instance()->continuousProcessing();
#ifndef WIN_32
  return NULL;
#endif
}

static void finiPyConnect()
{
  // stop the thread first
  PyConnectNetComm::instance()->fini();
  PyConnectStub::instance()->fini();
}

PyMODINIT_FUNC initPyConnect( void )
{
#ifdef WIN_32
  InitializeCriticalSection( &g_criticalSection );
#else
  pthread_mutexattr_init( &mta );
  pthread_mutexattr_settype( &mta, PTHREAD_MUTEX_RECURSIVE );
  pthread_mutex_init( &g_mutex, &mta );
#endif

  PYCONNECT_LOGGING_INIT;
  PyEval_InitThreads();

  PyConnectStub * pPyConnectStub = PyConnectStub::init();
  PyObject * pMod = pPyConnectStub->getPyConnect();
  Py_INCREF( pMod );
  PyConnectNetComm::instance()->init( pPyConnectStub );
#ifdef WIN_32
  _beginthread( ioprocess_thread, 0, NULL );
#else
  if (pthread_create( &g_iothread, NULL, ioprocess_thread, NULL)) {
    ERROR_MSG( "Unable to create thread to handle PyConnect network data\n" );
    abort();
  }
#endif
  atexit( finiPyConnect );
}
