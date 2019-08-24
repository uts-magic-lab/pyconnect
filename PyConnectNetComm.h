/*
 *  PyConnectNetComm.h
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

#ifndef PyConnectNetComm_h_DEFINED
#define PyConnectNetComm_h_DEFINED

#include <sys/types.h>
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif
#include "PyConnectObjComm.h"

// critical section/mutex
#ifdef MULTI_THREAD
#ifdef WIN32
#include <winbase.h>
extern CRITICAL_SECTION g_criticalSection;
#else
#include <pthread.h>
extern pthread_mutex_t g_mutex;
#endif
#endif

#ifndef WIN32
#define PYCONNECT_DOMAINSOCKET_PATH  "/tmp"
#ifdef PYTHON_SERVER
#define PYCONNECT_SVRSOCKET_PREFIX  "pysvr"
#define PYCONNECT_CLTSOCKET_PREFIX  "pyclt"
#else
#define PYCONNECT_SVRSOCKET_PREFIX  "pyclt"
#define PYCONNECT_CLTSOCKET_PREFIX  "pysvr"
#endif
#endif //!WIN32

#define PYCONNECT_NETCOMM_PORT 37251
#ifndef PYCONNECT_BROADCAST_IP
#ifdef USE_MULTICAST
#define PYCONNECT_BROADCAST_IP  "230.15.20.20"
#else
#define PYCONNECT_BROADCAST_IP  "255.255.255.255" // need to be modified
#endif
#endif
#define PYCONNECT_UDP_BUFFER_SIZE  2048
#define PYCONNECT_TCP_BUFFER_SIZE  4096
#define PYCONNECT_MAX_TCP_SESSION  50
#define PYCONNECT_COMMPORT_RANGE   100  // this basically limits number of pythonised objects running on same machine/interface

#ifdef HAS_OWN_MAIN_LOOP
#define PYCONNECT_NETCOMM_INIT  \
  PyConnectNetComm::instance()->init( this->getMP(), this )

#define PYCONNECT_NETCOMM_DECLARE \
  virtual void setFD( const SOCKET_T & fd );  \
  virtual void clearFD( const SOCKET_T & fd )

#define PYCONNECT_NETCOMM_PROCESS_DATA( READYFDSET ) \
  PyConnectNetComm::instance()->processIncomingData( READYFDSET )

#else
#define PYCONNECT_NETCOMM_INIT  \
  PyConnectNetComm::instance()->init( this->getMP() )

#define PYCONNECT_NETCOMM_DECLARE

#define PYCONNECT_NETCOMM_PROCESS_DATA \
  PyConnectNetComm::instance()->continuousProcessing()

#endif

#define PYCONNECT_NETCOMM_FINI  \
  PyConnectNetComm::instance()->fini()

#ifndef PYTHON_SERVER
#define PYCONNECT_NETCOMM_ENABLE_NET \
  PyConnectNetComm::instance()->enableNetComm()

#define PYCONNECT_NETCOMM_DISABLE_NET \
  PyConnectNetComm::instance()->disableNetComm()

#ifdef WIN32
#define PYCONNECT_NETCOMM_ENABLE_IPC \
  static_assert( 0, "PYCONNECT_NETCOMM_ENABLE_IPC is not supported under Windows." )
#define PYCONNECT_NETCOMM_DISABLE_IPC \
  static_assert( 0,"PYCONNECT_NETCOMM_DISABLE_IPC is not supported under Windows." )
#else
#define PYCONNECT_NETCOMM_ENABLE_IPC \
  PyConnectNetComm::instance()->enableIPCComm()
#define PYCONNECT_NETCOMM_DISABLE_IPC \
  PyConnectNetComm::instance()->disableIPCComm()
#endif
#endif // HAS_OWN_MAIN_LOOP

namespace pyconnect {

class FDSetOwner {
public:
  virtual void setFD( const SOCKET_T & fd ) = 0;
  virtual void clearFD( const SOCKET_T & fd ) = 0;
  virtual ~FDSetOwner() {}
};

class PyConnectNetComm : public ObjectComm
{
public:
  static PyConnectNetComm * instance();
  virtual ~PyConnectNetComm();

  void processIncomingData( fd_set * readyFDSet );
  void init( MessageProcessor * pMP, FDSetOwner * fdOwner = NULL );
  void continuousProcessing();

  void dataPacketSender( const unsigned char * data, int size, bool broadcast = false );
  CommObjectStat createTalker( char * host, int port = 0 );
  CommObjectStat setBroadcastAddr( char * addr );

  void fini();

  void enableNetComm();
  void disableNetComm( bool onExit = false );
#ifndef WIN32
  void enableIPCComm();
  void disableIPCComm( bool onExit = false );
#endif //!WIN32

private:
  enum FDDomain {
    UNKNOWN,
    NETWORK,
    LOCALIPC
  };

  struct SocketDataBufferInfo {
    unsigned char * bufferedData;
    int expectedDataLength;
    int bufferedDataLength;
  };
  
  typedef struct sClientFD {
    SOCKET_T fd;
    FDDomain domain;
    struct sockaddr_in cAddr; // client address, NETWORK only
    int localProcID; // server process id, IPC only
    struct SocketDataBufferInfo dataInfo;
    sClientFD * pNext;
  } ClientFD;

  PyConnectNetComm();
  bool initUDPListener();
  bool initTCPListener();
  
  void clientDataSend( const unsigned char * data, int size );
  void netBroadcastSend( const unsigned char * data, int size );
  void localBroadcastSend( const unsigned char * data, int size );

  void processUDPInput( unsigned char * recBuffer, int recBytes, struct sockaddr_in & cAddr );
  bool createTCPTalker( struct sockaddr_in & cAddr );
#ifndef WIN32
  SOCKET_T findOrCreateIPCTalker( int procID );
  SOCKET_T findFdFromClientListByProcID( int procID );
  void consolidateIPCSockets();
#endif // !WIN32
  void addFdToClientList( const SOCKET_T & fd, FDDomain domain,
                         struct sockaddr_in * cAddr, int procID = 0 );
  void destroyCurrentClient( SOCKET_T fd );
  void destroyCurrentClient( ClientFD * & FDPtr, ClientFD * & prevFDPtr );
  void updateMPID();
  bool getIDFromIP( int & addr );

  static PyConnectNetComm *  s_pPyConnectNetComm;

  MessageProcessor * pMP_;

  struct sockaddr_in  sAddr_;
  struct sockaddr_in  bcAddr_;

  SOCKET_T  udpSocket_;
  SOCKET_T  tcpSocket_;
  SOCKET_T  domainSocket_;

  unsigned char * dgramBuffer_;
  unsigned char * clientDataBuffer_;
  unsigned char * dispatchDataBuffer_;

  ClientFD * clientFDList_;

  // only used in own main loop
  FDSetOwner * pFDOwner_;
  int     maxFD_;
  fd_set  masterFDSet_;  

  bool  netCommEnabled_;
  bool  IPCCommEnabled_;
  bool  invalidUDPSock_;
  bool  keepRunning_;
  unsigned short portInUse_;
#ifdef USE_MULTICAST
  struct ip_mreq multiCastReq_;
#endif
  typedef std::vector<int> ClientSocketList; // server process id
  ClientSocketList liveServerSocketList_;
};

} // namespace pyconnect
#endif  // PyConnectNetComm_h_DEFINED
