/*
 *  PyConnectObjComm.cpp
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

#include "PyConnectObjComm.h"
#include <assert.h>

namespace pyconnect {

void MessageProcessor::addCommObject( ObjectComm * pCommObj )
{
  CommObjectList::const_iterator citer = commObjList_.begin();
  while (citer!=commObjList_.end()&& pCommObj != *citer)
    citer++;

  if (citer==commObjList_.end()) { // commObj has not been added to list before
    commObjList_.push_back( pCommObj );
  }
}

void MessageProcessor::dispatchMessage( const unsigned char * data, int size, bool broadcast )
{
  for (CommObjectList::const_iterator citer = commObjList_.begin();
    citer != commObjList_.end(); citer++ )
  {
    (*citer)->dataPacketSender( data, size, broadcast );
  }
}

CommObjectStat MessageProcessor::connectTo( char * host, int port )
{
  CommObjectStat retVal = NOT_SUPPORTED;
  for (CommObjectList::const_iterator citer = commObjList_.begin();
    citer != commObjList_.end(); citer++ )
  {
    CommObjectStat val = (*citer)->createTalker( host, port );
    if (val == COMM_SUCCESS) { // we assume one comm object support host/port scheme
      return val;
    }
    else {
      retVal = (CommObjectStat)(retVal | val);
    }
  }
  return retVal;
}

CommObjectStat MessageProcessor::setBroadcastAddr( char * addr )
{
  CommObjectStat retVal = NOT_SUPPORTED;
  for (CommObjectList::const_iterator citer = commObjList_.begin();
    citer != commObjList_.end(); citer++ )
  {
    CommObjectStat val = (*citer)->setBroadcastAddr( addr );
    switch (val) {
      case COMM_SUCCESS:
      case COMM_FAILED:
        return val;
        break;
      default:
        retVal = (CommObjectStat)(retVal | val);
    }
  }
  return retVal;
}

MessageProcessor::~MessageProcessor()
{
  // just clear the commObject list
  // No need to delete comm objects.
  commObjList_.clear();
}

ObjectComm::ObjectComm()
{
  activeCommChannel_ = INVALID_SOCKET;
}

SOCKET_T ObjectComm::getLastUsedCommChannel()
{
  if (activeCommChannel_ == INVALID_SOCKET) {
    ERROR_MSG( "ObjectComm::getLastUsedCommChannel: "
           "No active Comm channel!" );
  }
  SOCKET_T retSock = activeCommChannel_;
  activeCommChannel_ = INVALID_SOCKET;
  return retSock;
}

int ObjectComm::verifyNegotiationMsg( const unsigned char * recBuffer, int receivedBytes )
{
  if (recBuffer[receivedBytes-1] != PYCONNECT_MSG_END)
    return -1;
  else
    return ((recBuffer[0] >> 4) & 0xf);
}

void ObjectComm::setLastUsedCommChannel( SOCKET_T index )
{
  if (!(activeCommChannel_ == INVALID_SOCKET ||
        activeCommChannel_ == index))
  {
    WARNING_MSG( "ObjectComm::setLastUsedCommChannel: "
      "active TCP channel is already set to %d! new channel %d\n",
      activeCommChannel_, index );
  }
  if (index == INVALID_SOCKET) {
    ERROR_MSG( "ObjectComm::setLastUsedCommChannel: "
           "invalid active channel index!\n" );  
    abort();
    return;
  }
  activeCommChannel_ = index;
}

SOCKET_T ObjectComm::findOrAddCommChanByMsgID( const unsigned char * data )
{
  SOCKET_T index = INVALID_SOCKET;
  int objID = -1;

  unsigned char header = *data >> 4 & 0xf;
#ifdef PYTHON_SERVER
  objID = (int)data[1] & 0xff; // Module ID
  if (header == pyconnect::MODULE_ASSIGN_ID) {
#else
  objID = *data & 0xf; // server ID
  if (header == pyconnect::ATTR_METD_EXPOSE ||
      header == pyconnect::MODULE_DECLARE)
  { // map server socket to serverID
#endif
    index = getLastUsedCommChannel();
    if (index != INVALID_SOCKET)
      objCommMap_[objID] = index;
  }
#ifdef PYTHON_SERVER
  else if (header == pyconnect::MODULE_DISCOVERY) { // this is for direct connection.
    index = getLastUsedCommChannel();
  }
#endif
  else {
    // check module / index mapping
    ObjectCommChannelMap::const_iterator mapIter = objCommMap_.find( objID );
    //assert( mapIter != objCommMap_.end());
    if (mapIter != objCommMap_.end())
      index = mapIter->second;
  }
  if (index == INVALID_SOCKET) {
    ERROR_MSG( "findOrAddCommChanByMsgID: invalid socket found.\n" );
  }
  return index;
}

void ObjectComm::resetObjCommChanMap()
{
  objCommMap_.clear();
  activeCommChannel_ = INVALID_SOCKET;
}

void ObjectComm::objCommChannelShutdown( SOCKET_T chanID, bool notifyMesgProc )
{
  ObjectCommChannelMap::iterator mapIter;

  for (mapIter=objCommMap_.begin(); 
     mapIter!=objCommMap_.end(); mapIter++)
  {
    if (mapIter->second == chanID) {
      break;
    }
  }
  if (mapIter != objCommMap_.end()) { // found a module index mapping
    if (notifyMesgProc) { // construct an artficial message to shut module down
      unsigned char dataBuffer[3];
#ifdef PYTHON_SERVER  
      dataBuffer[0] = (char) (pyconnect::MODULE_SHUTDOWN << 4 | 0);
      dataBuffer[1] = (char) mapIter->first;
#else
      dataBuffer[0] = (char) (pyconnect::SERVER_SHUTDOWN << 4 | (mapIter->first & 0xf));
      dataBuffer[1] = (char) 0;
#endif
      dataBuffer[2] = pyconnect::PYCONNECT_MSG_END;
      struct sockaddr_in dummy;
      memset( &dummy, 0, sizeof( sockaddr_in ) );
      if (pMP_)
        pMP_->processInput( dataBuffer, 3, dummy, true );
    }
    objCommMap_.erase( mapIter );
  }
}

} // namespace pyconnect
