/*
 *  PyConnectNetComm.cpp
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
#pragma warning( disable: 4309 )
#endif

#ifdef PYTHON_SERVER
#include "Python.h"
#endif
#ifndef WIN_32
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <dirent.h>
#include <fnmatch.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef LINUX
#include <stddef.h>
#endif
#endif
#include "PyConnectNetComm.h"

#ifndef WIN_32
#define max( a, b ) (a > b) ? a : b
#endif

namespace pyconnect {

static int kTCPMSS = 1024;

PyConnectNetComm * PyConnectNetComm::s_pPyConnectNetComm = NULL;

PyConnectNetComm * PyConnectNetComm::instance()
{
  if (!s_pPyConnectNetComm)
    s_pPyConnectNetComm = new PyConnectNetComm();
  return s_pPyConnectNetComm;
}

PyConnectNetComm::PyConnectNetComm() :
  ObjectComm(),
  pMP_( NULL ),
  udpSocket_( INVALID_SOCKET ),
  tcpSocket_( INVALID_SOCKET ),
  domainSocket_( INVALID_SOCKET ),
  dgramBuffer_( NULL ),
  clientDataBuffer_( NULL ),
  dispatchDataBuffer_( NULL ),
  clientFDList_( NULL ),
  maxFD_( 0 ),
  netCommEnabled_( false ),
  IPCCommEnabled_( false ),
  invalidUDPSock_( false ),
  keepRunning_( true ),
  portInUse_( PYCONNECT_NETCOMM_PORT )
{
}

PyConnectNetComm::~PyConnectNetComm()
{
#ifdef WIN_32
  WSACleanup();
#endif
#ifdef MULTI_THREAD
#ifdef WIN_32
  DeleteCriticalSection( &g_criticalSection );
#else
  pthread_mutex_destroy( &g_mutex );
#endif
#endif
}

void PyConnectNetComm::init( MessageProcessor * pMP, FDSetOwner * fdOwner )
{
  endecryptInit();
  
  pMP_ = pMP;
  setMP( pMP );
  pMP_->addCommObject( this );
  pFDOwner_ = fdOwner;
  updateMPID();
  FD_ZERO( &masterFDSet_ );
#ifdef PYTHON_SERVER
  enableNetComm();
#ifndef WIN_32
  enableIPCComm();
#endif // !WIN_32
#endif
}

bool PyConnectNetComm::initUDPListener()
{
  if ((udpSocket_ = socket( AF_INET, SOCK_DGRAM, 0 ) ) == INVALID_SOCKET) {
    ERROR_MSG( "PyConnectNetComm::initUDPListener: unable to create UDP socket.\n" );
    invalidUDPSock_ = true;
    return false;
  }
  // setup broadcast option
  int turnon = 1;
#ifdef USE_MULTICAST
  char ttl = 30;
  if (setsockopt( udpSocket_, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl, sizeof( ttl ) ) < 0) {
    ERROR_MSG( "PyConnectNetComm::initUDPListener: failed to set multicast TTL on UDP socket.\n" );
#ifdef WIN_32
    closesocket( udpSocket_ );
#else
    close( udpSocket_ );
#endif
    invalidUDPSock_ = true;
    return false;
  }
  if (setsockopt( udpSocket_, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&multiCastReq_, sizeof( multiCastReq_ ) ) < 0) {
    ERROR_MSG( "PyConnectNetComm::initUDPListener: failed to join multicast group on UDP socket.\n" );
#ifdef WIN_32
    closesocket( udpSocket_ );
#else
    close( udpSocket_ );
#endif
    invalidUDPSock_ = true;
    return false;
  }
#else // !USE_MULTICAST
  if (setsockopt( udpSocket_, SOL_SOCKET, SO_BROADCAST, (char *)&turnon, sizeof( turnon ) ) < 0) {
    ERROR_MSG( "PyConnectNetComm::initUDPListener: failed to enable broadcast on UDP socket.\n" );
#ifdef WIN_32
    closesocket( udpSocket_ );
#else
    close( udpSocket_ );
#endif
    invalidUDPSock_ = true;
    return false;
  }
#endif

  if (setsockopt( udpSocket_, SOL_SOCKET, SO_REUSEADDR, (char *)&turnon, sizeof( turnon ) ) < 0) {
    ERROR_MSG( "PyConnectNetComm::initUDPListener: failed to enable reuse address on UDP socket.\n" );
#ifdef WIN_32
    closesocket( udpSocket_ );
#else
    close( udpSocket_ );
#endif
    invalidUDPSock_ = true;
    return false;
  }

#ifdef SO_REUSEPORT
  setsockopt( udpSocket_, SOL_SOCKET, SO_REUSEPORT, (char *)&turnon, sizeof( turnon ) );
#endif

  sAddr_.sin_port = htons( PYCONNECT_NETCOMM_PORT );
  
  if (bind( udpSocket_, (struct sockaddr *)&sAddr_, sizeof( sAddr_ ) ) < 0) {
    ERROR_MSG( "PyConnectNetComm::initUDPListener: unable to bind to network interface.\n" );
#ifdef WIN_32
    closesocket( udpSocket_ );
#else
    close( udpSocket_ );
#endif
    invalidUDPSock_ = true;
    return false;
  }

#ifdef WIN_32
  maxFD_++;
#else
  maxFD_ = max( maxFD_, udpSocket_ );
#endif
  FD_SET( udpSocket_, &masterFDSet_ );
  if (pFDOwner_)
    pFDOwner_->setFD( udpSocket_ );

  if (dgramBuffer_ == NULL)
    dgramBuffer_ = new unsigned char[PYCONNECT_UDP_BUFFER_SIZE];

  return true;
}

bool PyConnectNetComm::initTCPListener()
{
  if ((tcpSocket_ = socket( AF_INET, SOCK_STREAM, 0 )) == INVALID_SOCKET) {
    ERROR_MSG( "PyConnectNetComm::initTCPListener: unable to create TCP socket.\n" );
    return false;
  }

#ifndef WIN_32
  int turnon = 1;
  if (setsockopt( tcpSocket_, SOL_SOCKET, SO_REUSEADDR, (char *)&turnon, sizeof( turnon ) ) < 0) {
    ERROR_MSG( "PyConnectNetComm::initTCPListener: failed to enable reuse addr option on TCP socket.\n" );
    close( tcpSocket_ );
    return false;
  }
#endif

  int triedPorts = 0;
  sAddr_.sin_port = htons( portInUse_ );
  int bindRet = bind( tcpSocket_, (struct sockaddr *)&sAddr_, sizeof( sAddr_ ) );
  while (bindRet < 0 && triedPorts < PYCONNECT_COMMPORT_RANGE) {
    triedPorts++;
    sAddr_.sin_port = htons( portInUse_ + triedPorts );
    bindRet = bind( tcpSocket_, (struct sockaddr *)&sAddr_, sizeof( sAddr_ ) );
  }
  
  if (bindRet < 0) {
    ERROR_MSG( "PyConnectNetComm::initTCPListener: unable to bind to network interface.\n" );
#ifdef WIN_32
    closesocket( tcpSocket_ );
#else
    close( tcpSocket_ );
#endif
    return false;
  }

  portInUse_ = sAddr_.sin_port; // now in network byte order
  
  if (listen( tcpSocket_, 5 ) < 0) {
    ERROR_MSG( "PyConnectNetComm::initTCPListener: unable to listen for incoming data.\n" );
#ifdef WIN_32
    closesocket( tcpSocket_ );
#else
    close( tcpSocket_ );
#endif
    return false;
  }

  INFO_MSG( "Listening on TCP port %d\n", ntohs( portInUse_ ) & 0xffff );
#ifdef WIN_32
  maxFD_++;
#else
  maxFD_ = max( maxFD_, tcpSocket_ );
#endif
  FD_SET( tcpSocket_, &masterFDSet_ );
  if (pFDOwner_)
    pFDOwner_->setFD( tcpSocket_ );

  if (clientDataBuffer_ == NULL)
    clientDataBuffer_ = new unsigned char[PYCONNECT_TCP_BUFFER_SIZE];

  if (dispatchDataBuffer_ == NULL)
    dispatchDataBuffer_ = new unsigned char[PYCONNECT_MSG_BUFFER_SIZE];

  return true;
}

void PyConnectNetComm::processIncomingData( fd_set * readyFDSet )
{
  struct sockaddr_in cAddr;
  int cLen = sizeof( cAddr );
  SOCKET_T fd = INVALID_SOCKET;

  if (netCommEnabled_) {
    if (FD_ISSET( udpSocket_, readyFDSet )) {
      int readLen = (int)recvfrom( udpSocket_, dgramBuffer_, PYCONNECT_UDP_BUFFER_SIZE,
        0, (sockaddr *)&cAddr, (socklen_t *)&cLen );
      if (readLen <= 0) {
        ERROR_MSG( "PyConnectNetComm::continuousProcessing: error accepting "
          "incoming UDP packet. error %d\n", errno );
      }
      else {
        processUDPInput( dgramBuffer_, readLen, cAddr );
      }
    }
    if (FD_ISSET( tcpSocket_, readyFDSet )) {
      // accept incoming TCP connection and read the stream
      fd = accept( tcpSocket_, (sockaddr *)&cAddr, (socklen_t *)&cLen );
      if (fd != INVALID_SOCKET) {
        addFdToClientList( fd, PyConnectNetComm::NETWORK, &cAddr );
      }
#ifdef WIN_32
      else if (errno != WSAECONNABORTED) {
#else
      else if (errno != ECONNABORTED) {
#endif
        ERROR_MSG( "PyConnectNetComm::continuousProcessing: error accepting "
               "incoming TCP connection error = %d\n", errno );
      }
    }
  }
  if (IPCCommEnabled_ && FD_ISSET( domainSocket_, readyFDSet )) {
    // accept incoming local socket connection and read the stream
    fd = accept( domainSocket_, (sockaddr *)&cAddr, (socklen_t *)&cLen );
    if (fd != INVALID_SOCKET) {
      addFdToClientList( fd, PyConnectNetComm::LOCALIPC, &cAddr );
    }
#ifdef WIN_32
    else if (errno != WSAECONNABORTED) {
#else
    else if (errno != ECONNABORTED) {
#endif
      ERROR_MSG( "PyConnectNetComm::continuousProcessing: error accepting "
             "incoming local IPC connection error = %d\n", errno );
    }
  }

  ClientFD * FDPtr = clientFDList_;
  ClientFD * prevFDPtr = FDPtr;
  while (FDPtr) {
    fd = FDPtr->fd;
    if (FD_ISSET( fd, readyFDSet )) {
#ifdef WIN_32
      int readLen = recv( fd, clientDataBuffer_, kTCPMSS, 0 );
#else
      int readLen = (int)read( fd, clientDataBuffer_, kTCPMSS );
#endif
      if (readLen <= 0) {
        if (readLen == 0) {
          INFO_MSG( "Socket connection %d closed.\n", fd );
        }
        else {
          ERROR_MSG( "PyConnectNetComm::continuousProcessing: "
                    "error reading data stream on %d error = %d.\n", fd, errno );
        }

        objCommChannelShutdown( fd, true );
        destroyCurrentClient( FDPtr, prevFDPtr );
        continue;
      }
      else {
#ifdef MULTI_THREAD
#ifdef WIN_32
        EnterCriticalSection( &g_criticalSection );
#else
        pthread_mutex_lock( &g_mutex );
#endif
#endif
        setLastUsedCommChannel( fd );
        //DEBUG_MSG( "receive data from fd %d\n", fd );
#ifdef MULTI_THREAD
#ifdef WIN_32
        LeaveCriticalSection( &g_criticalSection );
#else
        pthread_mutex_unlock( &g_mutex );
#endif
#endif
        MesgProcessResult procResult = MESG_PROCESSED_OK;
        unsigned char * dataPtr = clientDataBuffer_;
        
        do {
          if (*dataPtr == PYCONNECT_MSG_INIT && FDPtr->dataInfo.expectedDataLength == 0) { // new message
            if (readLen < 4) {
              ERROR_MSG( "PyConnectNetComm::continuousProcessing: "
                        "invalid data packet in stream on %d (too small).\n", fd );
              break;
            }
            dataPtr++;
            short dataCount = 0;
            memcpy( &dataCount, dataPtr, sizeof( short ) ); dataPtr += sizeof( short );
            readLen -= 3;
            if (dataCount < 0 || dataCount > PYCONNECT_MSG_BUFFER_SIZE) { // encryption buffer size
              ERROR_MSG( "PyConnectNetComm::continuousProcessing: "
                        "invalid data size in stream on %d.\n", fd );
              break;
            }
            else if (dataCount > (readLen - 1)) { // the message needs multiple reads
              FDPtr->dataInfo.bufferedDataLength = readLen;
              memcpy( FDPtr->dataInfo.bufferedData, dataPtr, FDPtr->dataInfo.bufferedDataLength );
              FDPtr->dataInfo.expectedDataLength = dataCount - FDPtr->dataInfo.bufferedDataLength;
              readLen = 0;
            }
            else if (*(dataPtr + dataCount) == PYCONNECT_MSG_END) { // valid message
              pMP_->processInput( dataPtr, dataCount, FDPtr->cAddr );
              readLen -= (dataCount + 1);
              dataPtr += (dataCount + 1);
            }
            else {
              ERROR_MSG( "PyConnectNetComm::continuousProcessing: "
                        "invalid data packet in stream on %d.\n", fd );
              break;
            }
          }
          else if (FDPtr->dataInfo.expectedDataLength > 0) { // patch up cached data
            unsigned char * cachedPtr = FDPtr->dataInfo.bufferedData + FDPtr->dataInfo.bufferedDataLength;
            if (FDPtr->dataInfo.expectedDataLength > (readLen - 1)) { // continue to cache
              memcpy( cachedPtr, dataPtr, readLen );
              FDPtr->dataInfo.bufferedDataLength += readLen;
              FDPtr->dataInfo.expectedDataLength -= readLen;
              readLen = 0;
            }
            else if (*(dataPtr + FDPtr->dataInfo.expectedDataLength) == PYCONNECT_MSG_END) { // valid message
              memcpy( cachedPtr, dataPtr, FDPtr->dataInfo.expectedDataLength );
              pMP_->processInput( FDPtr->dataInfo.bufferedData,
                                 FDPtr->dataInfo.bufferedDataLength + FDPtr->dataInfo.expectedDataLength,
                                 FDPtr->cAddr );

              readLen -= (FDPtr->dataInfo.expectedDataLength + 1);
              if (readLen > 0) {
                dataPtr += (FDPtr->dataInfo.expectedDataLength + 1);
              }
              FDPtr->dataInfo.bufferedDataLength = 0;
              FDPtr->dataInfo.expectedDataLength = 0;
            }
            else {
              ERROR_MSG( "PyConnectNetComm::continuousProcessing: "
                        "unexpected data fragment in data stream on %d.\n", fd );
              FDPtr->dataInfo.bufferedDataLength = 0;
              FDPtr->dataInfo.expectedDataLength = 0;
              break;
            }
          }
          else {
            ERROR_MSG( "PyRideNetComm::continuousProcessing: "
                      "invalid data stream on %d.\n", fd );
            FDPtr->dataInfo.bufferedDataLength = 0;
            FDPtr->dataInfo.expectedDataLength = 0;
            break;
          }
        } while (readLen > 0);

        if (procResult == MESG_TO_SHUTDOWN) {
          objCommChannelShutdown( fd );
          destroyCurrentClient( FDPtr, prevFDPtr );
          continue;
        }
      }
    }
    prevFDPtr = FDPtr;
    FDPtr = FDPtr->pNext;
  }
}

void PyConnectNetComm::continuousProcessing()
{
  int maxFD = 0;
  fd_set readyFDSet;

  while (keepRunning_) {
    FD_ZERO( &readyFDSet );
    memcpy( &readyFDSet, &masterFDSet_, sizeof( masterFDSet_ ) );
    maxFD = maxFD_;

#ifdef MULTI_THREAD
    struct timeval timeout; 
    timeout.tv_sec = 0;
    timeout.tv_usec = 200000; // 200ms
#endif

    // we must be able to interrupt select since masterFDSet_ might
    // be updated by the main thread (i.e. calling PyConnect.connect).
    // For this we use timeout as a solution. NOTE: an alternative
    // solution for POSIX system might be that still using block select
    // but send a signal from main thread to interrupt it when connect
    // is completed.
#if defined( MULTI_THREAD )
    select( maxFD + 1, &readyFDSet, NULL, NULL, &timeout );
#else
    select( maxFD + 1, &readyFDSet, NULL, NULL, NULL ); // blocking select
#endif

    this->processIncomingData( &readyFDSet );
  }
}

void PyConnectNetComm::dataPacketSender( const unsigned char * data, int size, bool broadcast )
{
  if (broadcast) {
    // a hack job to pass on TCP port in use in declaration messages
    int msgType = verifyNegotiationMsg( data, size );
    if (msgType == pyconnect::MODULE_DISCOVERY ||
        msgType == pyconnect::MODULE_DECLARE)
    {
      if (netCommEnabled_) {
        unsigned char * modData = new unsigned char[size + sizeof( short )];
        memcpy( modData, data, size - 1 ); // exclude message end char
        unsigned char * modDataPtr = modData + size - 1;
        memcpy( modDataPtr, (unsigned char *)&portInUse_, sizeof( short ) );
        modDataPtr += sizeof( short );
        *modDataPtr = pyconnect::PYCONNECT_MSG_END;
        netBroadcastSend( modData, size + sizeof( short ) );
        delete [] modData;
      }
      if (IPCCommEnabled_) {
        localBroadcastSend( data, size );
      }    
    }
    else {
      if (netCommEnabled_)
        netBroadcastSend( data, size );
      if (IPCCommEnabled_)
        localBroadcastSend( data, size );
    }
  }
  else {
    clientDataSend( data, size );
  }
}

void PyConnectNetComm::netBroadcastSend( const unsigned char * data, int size )
{
  if (size <= 0) return;

  unsigned char * outputData = NULL;
  int outputLength = 0;
  
  if (encryptMessage( data, size, &outputData, &outputLength ) != 1) {
    return;
  }

#ifdef MULTI_THREAD
#ifdef WIN_32
  EnterCriticalSection( &g_criticalSection );
#else
  pthread_mutex_lock( &g_mutex );
#endif
#endif

  if (netCommEnabled_) {
    if (sendto( udpSocket_, outputData, outputLength, 0, (struct sockaddr *)&bcAddr_, sizeof( bcAddr_ ) ) < 0) {
      ERROR_MSG( "PyConnectNetComm::broadcastSend: Error sending UDP broadcast packet.\n" );
    }
  }

#ifdef MULTI_THREAD
#ifdef WIN_32
  LeaveCriticalSection( &g_criticalSection );
#else
  pthread_mutex_unlock( &g_mutex );
#endif
#endif
}

void PyConnectNetComm::localBroadcastSend( const unsigned char * data, int size )
{
#ifndef WIN_32
  if (size <= 0) return;

  unsigned char * outputData = NULL;
  int outputLength = 0;
  
  if (encryptMessage( data, size, &outputData, &outputLength ) != 1) {
    return;
  }

#ifdef MULTI_THREAD
  pthread_mutex_lock( &g_mutex );
#endif
  
  if (IPCCommEnabled_) {
    SOCKET_T fd = INVALID_SOCKET;
    // make sure connections to all available servers are established
    consolidateIPCSockets();
    
    unsigned char * dataPtr = dispatchDataBuffer_;
    *dataPtr++ = PYCONNECT_MSG_INIT;
    short opl = (short) outputLength;
    memcpy( dataPtr, &opl, sizeof( short ) );
    dataPtr += sizeof( short );
    memcpy( dataPtr, outputData, outputLength );
    dataPtr += outputLength;
    *dataPtr = PYCONNECT_MSG_END;
    outputLength += (2 + sizeof( short ));
    
    for (ClientSocketList::const_iterator iter = liveServerSocketList_.begin();
         iter != liveServerSocketList_.end(); ++iter)
    {
      fd = findOrCreateIPCTalker( *iter );
      if (fd != INVALID_SOCKET)
        send( fd, dispatchDataBuffer_, outputLength, 0 );
    }
  }
  
#ifdef MULTI_THREAD
  pthread_mutex_unlock( &g_mutex );
#endif
#endif // !WIN_32
}

void PyConnectNetComm::clientDataSend( const unsigned char * data, int size )
{
  if (size <= 0) return;

  unsigned char * outputData = NULL;
  int outputLength = 0;
  
  if (encryptMessage( data, size, &outputData, &outputLength ) != 1) {
    return;
  }

#ifdef MULTI_THREAD
#ifdef WIN_32
  EnterCriticalSection( &g_criticalSection );
#else
  pthread_mutex_lock( &g_mutex );
#endif
#endif
  
  SOCKET_T mysock = findOrAddCommChanByMsgID( data );

  if (mysock != INVALID_SOCKET)  {
    //DEBUG_MSG( "dispatch data to fd %d\n", mysock );
    unsigned char * dataPtr = dispatchDataBuffer_;
    *dataPtr++ = PYCONNECT_MSG_INIT;
    short opl = (short) outputLength;
    memcpy( dataPtr, &opl, sizeof( short ) );
    dataPtr += sizeof( short );
    memcpy( dataPtr, outputData, outputLength );
    dataPtr += outputLength;
    *dataPtr = PYCONNECT_MSG_END;
    outputLength += (2 + sizeof( short ));

#ifdef WIN_32
    send( mysock, dispatchDataBuffer_, outputLength, 0 );
#else
    write( mysock, dispatchDataBuffer_, outputLength );
#endif
  }

#ifdef MULTI_THREAD
#ifdef WIN_32
  LeaveCriticalSection( &g_criticalSection );
#else
  pthread_mutex_unlock( &g_mutex );
#endif
#endif
}

void PyConnectNetComm::processUDPInput( unsigned char * recBuffer, int recBytes, struct sockaddr_in & cAddr )
{
  unsigned char * message = NULL;
  int messageSize = 0;
  
  if (decryptMessage( recBuffer, (int)recBytes, &message, (int *)&messageSize ) != 1) {
    WARNING_MSG( "Unable to decrypt incoming messasge.\n" );
    return;
  }

  // initialise a new TCP connection
  switch (verifyNegotiationMsg( message, messageSize )) {
#ifdef PYTHON_SERVER
    case pyconnect::MODULE_DISCOVERY:
      break;
    case pyconnect::PEER_SERVER_MSG:
    {
      if (pMP_)
        pMP_->processInput( message, messageSize, cAddr );
    }
      break;
    case pyconnect::MODULE_DECLARE:
    case pyconnect::PEER_SERVER_DISCOVERY:
#else
    case pyconnect::MODULE_DECLARE:
    case pyconnect::PEER_SERVER_DISCOVERY:
    case pyconnect::PEER_SERVER_MSG:
      break;
    case pyconnect::MODULE_DISCOVERY:
#endif
    {
      short port = 0;
      // extract TCP port and restore message
      unsigned char * portStrPtr = &message[messageSize - sizeof( short ) - 1];
      memcpy( (unsigned char *)&port, portStrPtr, sizeof( short ) );
      //DEBUG_MSG( "incoming port %d\n", ntohs( port ) & 0xffff );
      cAddr.sin_port = port;
      message[messageSize - sizeof( short ) - 1] = pyconnect::PYCONNECT_MSG_END;
      if (createTCPTalker( cAddr )) { // process udp data
        MesgProcessResult procResult = pMP_->processInput( message, messageSize - sizeof( short ), cAddr, true );
        if (procResult == MESG_TO_SHUTDOWN) {
          SOCKET_T fd = getLastUsedCommChannel();
          objCommChannelShutdown( fd );
          destroyCurrentClient( fd );
        }
      }
      else {
#ifdef WIN_32
        ERROR_MSG( "PyConnectNetComm::processUDPInput: Unable to "
          "create connection to %s:%d\n", inet_ntoa( cAddr.sin_addr ),
          ntohs( cAddr.sin_port ) );
#else
        char cAddrStr[INET_ADDRSTRLEN];
        inet_ntop( AF_INET, &cAddr.sin_addr.s_addr, cAddrStr, INET_ADDRSTRLEN );
        ERROR_MSG( "PyConnectNetComm::processUDPInput: Unable to "
          "create connection to %s:%d\n", cAddrStr, ntohs( cAddr.sin_port ) );
#endif
      }
    }
      break;
    default:
      ERROR_MSG( "PyConnectNetComm::processUDPInput()"
        "Unknown negotiation message\n" );
  }
}

bool PyConnectNetComm::createTCPTalker( struct sockaddr_in & cAddr )
{
  SOCKET_T mySocket = socket( AF_INET, SOCK_STREAM, 0 );

  if (mySocket == INVALID_SOCKET)
    return false;

  if (connect( mySocket, (struct sockaddr *)&cAddr, sizeof( cAddr ) ) < 0) {
#ifdef WIN_32
    closesocket( mySocket );
#else
    close( mySocket );
#endif
    return false;
  }
  addFdToClientList( mySocket, PyConnectNetComm::NETWORK, &cAddr );
  return true;
}

#ifndef WIN_32
SOCKET_T PyConnectNetComm::findOrCreateIPCTalker( int procID )
{
  SOCKET_T mySocket = findFdFromClientListByProcID( procID );
  
  if (mySocket != INVALID_SOCKET)
    return mySocket;

  mySocket = socket( AF_UNIX, SOCK_STREAM, 0 );

  if (mySocket != INVALID_SOCKET) {
    struct sockaddr_un dAddr;
    memset( &dAddr, 0, sizeof( dAddr ) );
    dAddr.sun_family = AF_UNIX;
    sprintf( dAddr.sun_path, "%s/%s.%05d", PYCONNECT_DOMAINSOCKET_PATH,
            PYCONNECT_CLTSOCKET_PREFIX, procID );
    
    int connLen = int(offsetof( struct sockaddr_un, sun_path ) + strlen( dAddr.sun_path ));
    
    if (connect( mySocket, (struct sockaddr *)&dAddr, connLen ) < 0) {
      close( mySocket );
      mySocket = INVALID_SOCKET;
    }
    
    INFO_MSG( "connect to %s\n", dAddr.sun_path );
    struct sockaddr_in dummy;
    memset( &dummy, 0, sizeof( dummy ) );
    addFdToClientList( mySocket, PyConnectNetComm::LOCALIPC, &dummy, procID );
  }
  return mySocket;
}

SOCKET_T PyConnectNetComm::findFdFromClientListByProcID( int procID ) // for IPC only
{
  bool found = false;

  ClientFD * fdPtr = clientFDList_;
  while (fdPtr) {
    if (fdPtr->domain == PyConnectNetComm::LOCALIPC) {
      found = (fdPtr->localProcID == procID);
      if (found) break;
    }
    fdPtr = fdPtr->pNext;
  }
  if (found) {
    setLastUsedCommChannel( fdPtr->fd );
    return fdPtr->fd;
  }
  return INVALID_SOCKET;
}

void PyConnectNetComm::consolidateIPCSockets()
{
  DIR * socketDir = opendir( PYCONNECT_DOMAINSOCKET_PATH );
  if (!socketDir) {
    ERROR_MSG( "Unable to access IPC path %s\n", PYCONNECT_DOMAINSOCKET_PATH );
    return;
  }
  char socketPattern[10];
  sprintf( socketPattern, "%s.*", PYCONNECT_CLTSOCKET_PREFIX );
  struct dirent * dp = NULL;
  ClientSocketList deadSocketList;
  liveServerSocketList_.clear();
  
  while ((dp = readdir( socketDir )) != NULL) {
    if (fnmatch( socketPattern, dp->d_name, 0 ) == 0) {
      // find corresponding process that create the socket
      int procID = (int)strtol( strlen( PYCONNECT_CLTSOCKET_PREFIX ) + 1 + dp->d_name, (char **)NULL, 10 );
      if (procID == 0) {
        ERROR_MSG( "socket file name %s contains invalid process number\n", dp->d_name );
        continue;
      }
      bool foundProc = false;
#ifdef BSD_COMPAT
      // check whether the process is still running
      int kernQuery[4];
      struct kinfo_proc kp;
      size_t kplen = sizeof( kp );
      
      kernQuery[0] = CTL_KERN;
      kernQuery[1] = KERN_PROC;
      kernQuery[2] = KERN_PROC_PID;
      kernQuery[3] = procID;
      foundProc = ((sysctl( kernQuery, 4, &kp, &kplen, NULL, 0 ) == 0) && kplen > 0);
#else // linux / solaris like
      char procIDStr[6];
      sprintf( procIDStr, "%d", procID );
      DIR * procDir = opendir( "/proc" );
      struct dirent * procdp = NULL;
      
      while ((procdp = readdir( procDir )) != NULL && !foundProc) {
        foundProc = (strcmp( procIDStr, procdp->d_name ) == 0);
      }
      closedir( procDir );
#endif
      if (foundProc) { //push onto a existing client list
        liveServerSocketList_.push_back( procID );
      }
      else { // push onto a dead socket list
        deadSocketList.push_back( procID );
      }
    }
  }
  closedir( socketDir );
  
  char deadSocket[20];
  for (ClientSocketList::const_iterator iter = deadSocketList.begin();
       iter != deadSocketList.end(); ++iter)
  {
    sprintf( deadSocket, "%s/%s.%05d", PYCONNECT_DOMAINSOCKET_PATH,
            PYCONNECT_CLTSOCKET_PREFIX, *iter );
    INFO_MSG( "Removing dead socket %s\n", deadSocket );
    unlink( deadSocket );
  }
  deadSocketList.clear();
}
#endif // !WIN_32

void PyConnectNetComm::enableNetComm()
{
  if (netCommEnabled_)
    return;

  sAddr_.sin_family = AF_INET;
  sAddr_.sin_addr.s_addr = INADDR_ANY;
  bcAddr_.sin_family = AF_INET;
#ifdef WIN_32
  bcAddr_.sin_addr.s_addr = inet_addr( PYCONNECT_BROADCAST_IP );
#else
  inet_pton( AF_INET, PYCONNECT_BROADCAST_IP, &bcAddr_.sin_addr.s_addr );
#endif
  bcAddr_.sin_port = htons( PYCONNECT_NETCOMM_PORT );
  
#ifdef USE_MULTICAST
  multiCastReq_.imr_multiaddr.s_addr = bcAddr_.sin_addr.s_addr;
  multiCastReq_.imr_interface.s_addr = INADDR_ANY;
#endif
  
  netCommEnabled_ = initUDPListener();
  netCommEnabled_ &= initTCPListener();
  
  if (!netCommEnabled_) {
    ERROR_MSG( "PyConnectNetComm::enableNetComm: Unable to initialise network "
              "communication layer!\n" );
#ifdef WIN_32
    WSACleanup();
#endif
  }
}

void PyConnectNetComm::disableNetComm( bool onExit )
{
  if (!netCommEnabled_)
    return;

#ifdef MULTI_THREAD // maybe little over zealous here
#ifdef WIN_32
  EnterCriticalSection( &g_criticalSection );
#else
  pthread_mutex_lock( &g_mutex );
#endif
#endif
  ClientFD * fdPtr = clientFDList_;
  while (fdPtr) {
    if (fdPtr->domain != PyConnectNetComm::NETWORK) {
      fdPtr = fdPtr->pNext;
      continue;
    }

    objCommChannelShutdown( fdPtr->fd, !onExit );

#ifdef WIN_32
    shutdown( fdPtr->fd, SD_SEND );
    closesocket( fdPtr->fd );
#else
    close( fdPtr->fd );
#endif
    if (pFDOwner_) {
      pFDOwner_->clearFD( fdPtr->fd );
    }
    else {
      FD_CLR( fdPtr->fd, &masterFDSet_ );
#ifdef WIN_32
      maxFD_--; // not really used
#endif
    }

    ClientFD * tmpPtr = fdPtr;
    fdPtr = fdPtr->pNext;
    delete tmpPtr;
  }

#ifdef USE_MULTICAST
  setsockopt( udpSocket_, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *)&multiCastReq_, sizeof( multiCastReq_ ) );
#endif
#ifdef WIN_32
  shutdown( udpSocket_, SD_SEND );
  closesocket( udpSocket_ );
  shutdown( tcpSocket_, SD_SEND );
  closesocket( tcpSocket_ );
#else
  close( udpSocket_ );
  close( tcpSocket_ );
#endif
  if (pFDOwner_) {
    pFDOwner_->clearFD( udpSocket_ );
    pFDOwner_->clearFD( tcpSocket_ );
  }
  
  delete [] dgramBuffer_;
  dgramBuffer_ = NULL;

#ifdef MULTI_THREAD
#ifdef WIN_32
  LeaveCriticalSection( &g_criticalSection );
#else
  pthread_mutex_unlock( &g_mutex );
#endif
#endif

#ifdef WIN_32
  WSACleanup();
#endif
  netCommEnabled_ = false;
}

#ifndef WIN_32
void PyConnectNetComm::enableIPCComm()
{
  if (IPCCommEnabled_)
    return;
  
  if ((domainSocket_ = socket( AF_UNIX, SOCK_STREAM, 0 )) == INVALID_SOCKET) {
    ERROR_MSG( "PyConnectNetComm::enableIPCComm: unable to create unix domain socket.\n" );
    return;
  }
  
  struct sockaddr_un dAddr;
  memset( &dAddr, 0, sizeof( dAddr ) );
  dAddr.sun_family = AF_UNIX;
  sprintf( dAddr.sun_path, "%s/%s.%05d", PYCONNECT_DOMAINSOCKET_PATH,
          PYCONNECT_SVRSOCKET_PREFIX, getpid() );
  
  unlink( dAddr.sun_path );
  
  consolidateIPCSockets();
  
  int bindLen = int(offsetof( struct sockaddr_un, sun_path ) + strlen( dAddr.sun_path ));
  
  if (bind( domainSocket_, (struct sockaddr *)&dAddr, bindLen ) < 0) {
    ERROR_MSG( "PyConnectNetComm::enableIPCComm: unable to bind to local domain socket.\n" );
#ifdef WIN_32
#else
    close( domainSocket_ );
#endif
    return;
  }
  
  if (listen( domainSocket_, 5 ) < 0) {
    ERROR_MSG( "PyConnectNetComm::enableIPCComm: unable to listen on local socket for incoming data.\n" );
#ifdef WIN_32
#else
    close( domainSocket_ );
#endif
    return;
  }
  
  // make socket user/group read and write
  chmod( dAddr.sun_path, S_IRWXU | S_IRGRP | S_IWGRP );
  
  INFO_MSG( "Listening on local socket %s\n", dAddr.sun_path );
#ifdef WIN_32
  maxFD_++;
#else
  maxFD_ = max( maxFD_, domainSocket_ );
#endif
  FD_SET( domainSocket_, &masterFDSet_ );
  if (pFDOwner_)
    pFDOwner_->setFD( domainSocket_ );
  
  if (clientDataBuffer_ == NULL)
    clientDataBuffer_ = new unsigned char[PYCONNECT_TCP_BUFFER_SIZE];
  
  if (dispatchDataBuffer_ == NULL)
    dispatchDataBuffer_ = new unsigned char[PYCONNECT_MSG_BUFFER_SIZE];

  IPCCommEnabled_ = true;
}

void PyConnectNetComm::disableIPCComm( bool onExit )
{
  if (!IPCCommEnabled_)
    return;

#ifdef MULTI_THREAD // maybe little over zealous here
  pthread_mutex_lock( &g_mutex );
#endif
  // TODO: Bug further cleanup on Python engine exit
  // seems to hit an assert at
  // Assertion failed: (autoInterpreterState), function PyGILState_Ensure, file Python/pystate.c, line 563
  ClientFD * fdPtr = clientFDList_;
  while (fdPtr) {
    if (fdPtr->domain != PyConnectNetComm::LOCALIPC) {
      fdPtr = fdPtr->pNext;
      continue;
    }

    objCommChannelShutdown( fdPtr->fd, !onExit );

    close( fdPtr->fd );

    if (pFDOwner_) {
      pFDOwner_->clearFD( fdPtr->fd );
    }
    else {
      FD_CLR( fdPtr->fd, &masterFDSet_ );
    }

    ClientFD * tmpPtr = fdPtr;
    fdPtr = fdPtr->pNext;
    delete tmpPtr;
  }

  close( domainSocket_ );

  if (pFDOwner_) {
    pFDOwner_->clearFD( domainSocket_ );
  }
  
  char mySockPath[50];
  sprintf( mySockPath, "%s/%s.%05d", PYCONNECT_DOMAINSOCKET_PATH,
          PYCONNECT_SVRSOCKET_PREFIX, getpid() );
  INFO_MSG( "Cleanup our IPC socket %s\n", mySockPath );
  unlink( mySockPath );
  
#ifdef MULTI_THREAD
  pthread_mutex_unlock( &g_mutex );
#endif
  IPCCommEnabled_ = false;
}
#endif  // !WIN_32

void PyConnectNetComm::fini()
{
  keepRunning_ = false;
  
  if (netCommEnabled_)
    disableNetComm( true );
#ifndef WIN_32
  if (IPCCommEnabled_)
    disableIPCComm( true );
#endif

  if (clientDataBuffer_) {
    delete [] clientDataBuffer_;
    clientDataBuffer_ = NULL;
  }

  if (dispatchDataBuffer_) {
    delete [] dispatchDataBuffer_;
    dispatchDataBuffer_ = NULL;
  }

  endecryptFini();
}

CommObjectStat PyConnectNetComm::setBroadcastAddr( char * addr )
{
  if (!netCommEnabled_)
    return NOT_SUPPORTED;

#ifdef USE_MULTICAST
  return NOT_SUPPORTED;
#else
  unsigned long saddr;
#ifdef WIN_32
  saddr = inet_addr( addr );
  if (saddr == INADDR_NONE &&
    strcmp( addr, "255.255.255.255" ))
  {
#else
  if (inet_pton( AF_INET, addr, &saddr ) != 1) {
#endif
    return INVALID_ADDR_PORT;
  }

  bcAddr_.sin_addr.s_addr = (in_addr_t)saddr;
  INFO_MSG( "PyConnectNetComm::setBroadcastAddr %s\n", addr );
  return COMM_SUCCESS;
#endif
}

CommObjectStat PyConnectNetComm::createTalker( char * host, int port )
{
  unsigned long saddr = 0;
  struct hostent * hostInfo = gethostbyname( host ); // try resolve name first
  if (!hostInfo) {
#ifdef WIN_32
    saddr = inet_addr( host );
    if (saddr == INADDR_NONE) {
#else
    if (inet_pton( AF_INET, host, &saddr ) != 1) {
#endif
      return INVALID_ADDR_PORT;
    }
  }
  if (port == 0)
    port = PYCONNECT_NETCOMM_PORT;
  else if (port < 1000 || port > PYCONNECT_NETCOMM_PORT + PYCONNECT_COMMPORT_RANGE)
    return INVALID_ADDR_PORT;

  struct sockaddr_in hostAddr;
  hostAddr.sin_family = AF_INET;
  if (hostInfo) {
    memcpy( (char *)&hostAddr.sin_addr, hostInfo->h_addr, hostInfo->h_length );
  }
  else {
    hostAddr.sin_addr.s_addr = (in_addr_t)saddr;
  }
  hostAddr.sin_port = htons( port );
  if (!createTCPTalker( hostAddr ))
    return COMM_FAILED;

  return COMM_SUCCESS;
}

//helper method to manage TCP fd list
void PyConnectNetComm::addFdToClientList( const SOCKET_T & fd, FDDomain domain, struct sockaddr_in * cAddr, int procID )
{
#ifdef MULTI_THREAD
#ifdef WIN_32
  EnterCriticalSection( &g_criticalSection );
#else
  pthread_mutex_lock( &g_mutex );
#endif
#endif
  //DEBUG_MSG( "addFdToClientList: fd %d, domain %d procID %d\n", fd, domain, procID );
  ClientFD * newFD = new ClientFD;
  newFD->fd = fd;
  newFD->domain = domain;
  newFD->localProcID = procID;
  newFD->cAddr = *cAddr;
  newFD->dataInfo.bufferedData = new unsigned char[PYCONNECT_MSG_BUFFER_SIZE];
  newFD->dataInfo.bufferedDataLength = 0;
  newFD->dataInfo.expectedDataLength = 0;

  newFD->pNext = NULL;
  if (clientFDList_) {
    ClientFD * fdPtr = clientFDList_;
    while (fdPtr->pNext) fdPtr = fdPtr->pNext;
    fdPtr->pNext = newFD;
  }
  else {
    clientFDList_ = newFD;
  }
  FD_SET( fd, &masterFDSet_ );
#ifdef WIN_32
  maxFD_++;
#else
  maxFD_ = max( fd, maxFD_ );
#endif
  if (pFDOwner_)
    pFDOwner_->setFD( fd );

  setLastUsedCommChannel( fd );

#ifdef MULTI_THREAD
#ifdef WIN_32
    LeaveCriticalSection( &g_criticalSection );
#else
    pthread_mutex_unlock( &g_mutex );
#endif
#endif
  if (domain == PyConnectNetComm::NETWORK) {
    int turnon = 1;
    if (setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, (char *) &turnon, sizeof(int)) < 0) {
      ERROR_MSG( "Unable to set TCP_NODELAY to socket %d\n", fd );
    }
    turnon = kTCPMSS;
    if (setsockopt( fd, IPPROTO_TCP, TCP_MAXSEG, (char *) &turnon, sizeof(int)) < 0) {
      ERROR_MSG( "Unable to set TCP MSS to socket %d\n", fd );
    }
  }
}

void PyConnectNetComm::destroyCurrentClient( ClientFD * & FDPtr, ClientFD * & prevFDPtr )
{
  if (FDPtr == NULL || prevFDPtr == NULL)
    return;
  
  SOCKET_T fd = FDPtr->fd;
  
#ifdef MULTI_THREAD
#ifdef WIN_32
  EnterCriticalSection( &g_criticalSection );
#else
  pthread_mutex_lock( &g_mutex );
#endif
#endif
  if (FDPtr->dataInfo.bufferedData) {
    delete [] FDPtr->dataInfo.bufferedData;
    FDPtr->dataInfo.bufferedData = NULL;
    FDPtr->dataInfo.bufferedDataLength = FDPtr->dataInfo.expectedDataLength = 0;
  }

  // clear from the FDList
  if (FDPtr == clientFDList_) { // deletion of first node
    clientFDList_ = clientFDList_->pNext;
    prevFDPtr = clientFDList_;
    delete FDPtr;
    FDPtr = clientFDList_;
  }
  else {
    prevFDPtr->pNext = FDPtr->pNext;
    delete FDPtr;
    FDPtr = prevFDPtr->pNext;
  }

  if (fd != INVALID_SOCKET) {
#ifdef WIN_32
    shutdown( fd, SD_SEND );
    closesocket( fd );
#else
    close( fd );
#endif
    FD_CLR( fd, &masterFDSet_ );
    
    if (pFDOwner_)
      pFDOwner_->clearFD( fd );
  }
#ifdef MULTI_THREAD
#ifdef WIN_32
  LeaveCriticalSection( &g_criticalSection );
#else
  pthread_mutex_unlock( &g_mutex );
#endif
#endif

}

void PyConnectNetComm::destroyCurrentClient( SOCKET_T fd )
{
#ifdef MULTI_THREAD
#ifdef WIN_32
  EnterCriticalSection( &g_criticalSection );
#else
  pthread_mutex_lock( &g_mutex );
#endif
#endif

  if (fd != INVALID_SOCKET) {
    //DEBUG_MSG( "PyConnectNetComm:: destroy unwanted TCP connection %d\n", fd );
    ClientFD * FDPtr = clientFDList_;
    ClientFD * prevFDPtr = FDPtr;
    while (FDPtr && FDPtr->fd != fd) {
      prevFDPtr = FDPtr;
      FDPtr = FDPtr->pNext;
    }
    
    if (FDPtr) { // found the client in the client list
      FD_CLR( fd, &masterFDSet_ );
      if (pFDOwner_)
        pFDOwner_->clearFD( fd );
      
      if (FDPtr == clientFDList_) {
        clientFDList_ = clientFDList_->pNext;
      }
      else {
        prevFDPtr->pNext = FDPtr->pNext;
      }
      
      if (FDPtr->dataInfo.bufferedData) {
        delete [] FDPtr->dataInfo.bufferedData;
        FDPtr->dataInfo.bufferedData = NULL;
        FDPtr->dataInfo.bufferedDataLength = FDPtr->dataInfo.expectedDataLength = 0;
      }

      delete FDPtr;
#ifdef WIN_32
      shutdown( fd, SD_SEND );
      closesocket( fd );
#else
      close( fd );
#endif
      
    }
  }

#ifdef MULTI_THREAD
#ifdef WIN_32
  LeaveCriticalSection( &g_criticalSection );
#else
  pthread_mutex_unlock( &g_mutex );
#endif
#endif
}

void PyConnectNetComm::updateMPID()
{
  int commAddr = 0;
  if (this->getIDFromIP( commAddr )) {
#ifdef PYTHON_SERVER
    INFO_MSG( "PythonServer: set server id to %d\n", commAddr );
    pMP_->updateMPID( commAddr );
#endif
  }
}

#ifdef WIN_32
bool PyConnectNetComm::getIDFromIP( int & addr )
{
  char * buf = NULL;
  DWORD dwBytesRet = 0;
  bool stat = false;

  WSAData wsaData;
  int nStart = 0;
  if ((nStart = WSAStartup( 0x202,&wsaData )) != 0) {
    ERROR_MSG( "Winsock 2 DLL initialization failed\n" );
    WSASetLastError( nStart );
    return stat;
  }
  SOCKET_T mySock = socket( AF_INET, SOCK_STREAM, 0 );

  if (mySock == INVALID_SOCKET)
    return stat;

  int rc = WSAIoctl( mySock, SIO_ADDRESS_LIST_QUERY, NULL,
    0, NULL, 0, &dwBytesRet, NULL, NULL );

  if (rc == SOCKET_ERROR && GetLastError() == WSAEFAULT) { // retrieve output buffer size
    char addrbuf[INET6_ADDRSTRLEN] = {'\0'};

    // Allocate the necessary size
    buf = (char *)HeapAlloc( GetProcessHeap(), 0, dwBytesRet );
    if (buf == NULL) {
      closesocket( mySock );
      return stat;
    }
    rc = WSAIoctl( mySock, SIO_ADDRESS_LIST_QUERY, NULL, 0, 
      buf, dwBytesRet, &dwBytesRet, NULL, NULL );
    if (rc != SOCKET_ERROR) {
      SOCKET_ADDRESS_LIST * slist = (SOCKET_ADDRESS_LIST *)buf;
      if (slist->iAddressCount > 0) {
        sockaddr_in localAddr;
        memcpy( (char *)&localAddr, slist->Address[0].lpSockaddr,
          slist->Address[0].iSockaddrLength );
        addr = htonl(localAddr.sin_addr.s_addr) & 0xff;
        stat = true;
      }
    }
  }

  if (buf) HeapFree( GetProcessHeap(), 0, buf );   
  closesocket( mySock ); 
  return stat;
}
#elif defined( SOLARIS )
bool PyConnectNetComm::getIDFromIP( int & addr )
{
  int nofinf = 4;
  struct lifconf mylifconf;

  mylifconf.lifc_family = AF_UNSPEC; // only retrieve IPv4 interface
  mylifconf.lifc_flags = 0;
  mylifconf.lifc_len = nofinf * sizeof( struct lifreq );
  mylifconf.lifc_req = new struct lifreq[nofinf];
  SOCKET_T mySock = socket( AF_INET, SOCK_STREAM, 0 );
  if (ioctl( mySock, SIOCGLIFCONF, &mylifconf ) == -1) {
    ERROR_MSG( "PyConnectNetComm::getIDFromIP: ioctl SIOCGLIFCONF "
      "failed\n" );
    delete [] mylifconf.lifc_req;
    close( mySock ); 
    return false;
  }

  struct lifreq * lifr = mylifconf.lifc_req;
  for (;(char *)lifr < (char *)mylifconf.lifc_req + mylifconf.lifc_len; ++lifr) {
    if (ioctl( mySock, SIOCGLIFFLAGS, lifr ) == -1) {
      continue;  /* failed to get flags, skip it */
    }
    if ((lifr->lifr_flags & IFF_UP) &&
        !(lifr->lifr_flags & IFF_LOOPBACK))
    { // found an live non-loopback interface
      ioctl( mySock, SIOCGLIFADDR, lifr );
      sockaddr_in localAddr;
      memcpy( &localAddr, (struct sockaddr_in *)&lifr->lifr_addr,
        sizeof( struct sockaddr_in ) );
      addr = htonl(localAddr.sin_addr.s_addr) & 0xff;
      delete [] mylifconf.lifc_req;
      close( mySock ); 
      return true;
    }
  }
  delete [] mylifconf.lifc_req;
  close( mySock ); 
  return false;
}
#elif defined( BSD_COMPAT )
#include <ifaddrs.h>
bool PyConnectNetComm::getIDFromIP( int & addr )
{
  bool stat = false;

  struct ifaddrs * ifAddrList = NULL;
  struct ifaddrs * ifAddrPtr;
  getifaddrs( &ifAddrList );
  ifAddrPtr = ifAddrList;
  
  while (ifAddrPtr) {
    if (ifAddrPtr->ifa_addr->sa_family == AF_INET &&
        (ifAddrPtr->ifa_flags & IFF_UP) &&
        !(ifAddrPtr->ifa_flags & IFF_LOOPBACK))
    {
      sockaddr_in localAddr;
      memcpy( &localAddr, (struct sockaddr_in *)ifAddrPtr->ifa_addr,
             sizeof( struct sockaddr_in ) );
      addr = htonl(localAddr.sin_addr.s_addr) & 0xff;
      stat = true;
      break;
    }
    ifAddrPtr = ifAddrPtr->ifa_next;
  }
  freeifaddrs( ifAddrList );
  return stat;
}
#else // linux etc
bool PyConnectNetComm::getIDFromIP( int & addr )
{
  int nofinf = 4;
  struct ifconf myifconf;

  myifconf.ifc_len = nofinf * sizeof( struct ifreq );
  myifconf.ifc_req = new struct ifreq[nofinf];
  SOCKET_T mySock = socket( AF_INET, SOCK_STREAM, 0 );
  if (ioctl( mySock, SIOCGIFCONF, &myifconf ) == -1) {
    ERROR_MSG( "PyConnectNetComm::getIDFromIP: ioctl SIOCGIFCONF "
      "failed\n" );
    delete [] myifconf.ifc_req;
    close( mySock ); 
    return false;
  }

  struct ifreq * ifr = myifconf.ifc_req;
  for (;(char *)ifr < (char *)myifconf.ifc_req + myifconf.ifc_len; ++ifr) {
    if (ioctl( mySock, SIOCGIFFLAGS, ifr ) == -1) {
      continue;  /* failed to get flags, skip it */
    }
    if ((ifr->ifr_flags & IFF_UP) &&
        !(ifr->ifr_flags & IFF_LOOPBACK))
    { // found an live non-loopback interface
      ioctl( mySock, SIOCGIFADDR, ifr );
      sockaddr_in localAddr;
      memcpy( &localAddr, (struct sockaddr_in *)&ifr->ifr_addr,
        sizeof( struct sockaddr_in ) );
      addr = htonl(localAddr.sin_addr.s_addr) & 0xff;
      delete [] myifconf.ifc_req;
      close( mySock ); 
      return true;
    }
  }
  delete [] myifconf.ifc_req;
  close( mySock );
  return false;
}
#endif

} // namespace pyconnect
