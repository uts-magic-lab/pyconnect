/*
 *  PyConnectObjComm.h
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

#ifndef PyConnectObjComm_h_DEFINED
#define PyConnectObjComm_h_DEFINED

#ifdef OPENR_OBJECT
#include <OPENR/OObject.h>
#include <OPENR/OSubject.h>
#include <OPENR/OObserver.h>
#endif

#ifdef WIN_32
#include <winsock2.h>
#define SOCKET_T  SOCKET
#else
#define SOCKET_T  int
#define INVALID_SOCKET -1
#include <netinet/in.h>
#endif

#include <vector>
#include <map>
#include "PyConnectCommon.h"

namespace pyconnect {

class ObjectComm;

typedef enum {
  NOT_SUPPORTED     = 1,
  INVALID_ADDR_PORT = 2,
  COMM_FAILED       = 4,
  COMM_SUCCESS      = 8
} CommObjectStat;

typedef enum {
  MESG_PROCESSED_OK = 0,
  MESG_PROCESSED_FAILED = 1,
  MESG_TO_SHUTDOWN = 2
} MesgProcessResult;

class MessageProcessor
{
public:
  MessageProcessor() {}
  virtual ~MessageProcessor();
  virtual MesgProcessResult processInput( unsigned char * recData, int bytesReceived, struct sockaddr_in & cAddr, bool skipdecrypt = false ) = 0;
  void addCommObject( ObjectComm * pCommObj );
  virtual void updateMPID( int id ) {}

protected:
  typedef std::vector<ObjectComm *> CommObjectList;
  CommObjectList  commObjList_;

  void dispatchMessage( const unsigned char * data, int size, bool broadcast = false );
  CommObjectStat connectTo( char * host, int port = 0 );
  CommObjectStat setBroadcastAddr( char * addr );
};

class ObjectComm
{
public:
  ObjectComm();
  virtual ~ObjectComm() {}
  virtual void dataPacketSender( const unsigned char * data, int size, bool broadcast = false ) = 0;
  virtual CommObjectStat setBroadcastAddr( char * addr ) { return NOT_SUPPORTED; }
  virtual CommObjectStat createTalker( char * host, int port = 0 ) { return NOT_SUPPORTED; }
  virtual void fini() {}

protected:
  MessageProcessor * pMP_;

  void setMP( MessageProcessor * pMP ) { pMP_ = pMP; }
  int verifyNegotiationMsg( const unsigned char * recBuffer, int receivedBytes );
  SOCKET_T findOrAddCommChanByMsgID( const unsigned char * data );
  SOCKET_T getLastUsedCommChannel();
  void resetLastUsedCommChannel() { activeCommChannel_ = INVALID_SOCKET; }
  void setLastUsedCommChannel( SOCKET_T index );
  void resetObjCommChanMap();
  void objCommChannelShutdown( SOCKET_T chanID, bool notifyMesgProc = false );
  
private:
  typedef std::map<int, SOCKET_T>  ObjectCommChannelMap; // < remote host object (module or server) ID , channel index/socket >

  ObjectCommChannelMap objCommMap_;
  SOCKET_T  activeCommChannel_;
};

} // namespace pyconnect

#endif  // PyConnectObjComm_h_DEFINED
