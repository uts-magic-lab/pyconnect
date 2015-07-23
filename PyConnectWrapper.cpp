/*
 *  PyConnectWrapper.cpp
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

#include <iterator>
#include "PyConnectWrapper.h"

namespace pyconnect {

Attribute::Attribute( const char * desc, PyConnectType::Type type, int (*getrawfn)( unsigned char * & ),
            void (*getfn)( int, int ), void (*setfn)( int, unsigned char * &, int &, int ) )
{
  this->desc = std::string( desc );
  this->type = type;
  this->attrGetFn = getfn;
  this->attrSetFn = setfn;
  this->getRawValueFn = getrawfn;
}

Method::Method( const char * desc, PyConnectType::Type type,
        void (*accessFn)( int, unsigned char * &, int &, int ),
        Arguments & args )
{
  this->desc = std::string( desc );
  this->type = type;
  this->args_ = args;
  this->accessFn_ = accessFn;
}

Argument::Argument( const char * name, const char * desc,
                   PyConnectType::Type type, bool isOptional )
{
#ifdef WIN32
  strncpy_s( this->name, 256, name, _TRUNCATE );
#else
  strncpy( this->name, name, 255 );
  this->name[255] = '\0';
#endif
  this->desc = std::string( desc );
  this->type = type;
  this->isOptional_ = isOptional;
}

Method::~Method()
{
  for (Arguments::iterator aiter = args_.begin();
     aiter!= args_.end(); aiter++ )
  {
    delete *aiter;
  }
  args_.clear();
}

PyConnectModule::PyConnectModule( const char * name, const char * desc, OObject * oobject )
{
  this->name = std::string( name );
  this->desc = std::string( desc );
  this->oobject_ = oobject;
}

PyConnectModule::PyConnectModule( const std::string & name, const std::string & desc )
{
  this->name = name;
  this->desc = desc;
}

PyConnectModule::~PyConnectModule()
{
  for (Attributes::iterator aiter = attributes.begin();
     aiter!= attributes.end(); aiter++ )
  {
    delete aiter->second;
  }
  attributes.clear();
  
  for (Methods::iterator miter = methods.begin();
     miter != methods.end(); miter++ )
  {
    delete miter->second;
  }
  methods.clear();
}

PyConnectWrapper * PyConnectWrapper::s_pPyConnectWrapper = NULL;

void PyConnectWrapper::init( PyConnectModule * pModule )
{
  if (s_pPyConnectWrapper) {
    if (!s_pPyConnectWrapper->pPyConnectModule_)
      delete s_pPyConnectWrapper->pPyConnectModule_;
    s_pPyConnectWrapper->pPyConnectModule_ = pModule;
  }
  else {
    s_pPyConnectWrapper = new PyConnectWrapper( pModule );
  }
}

void PyConnectWrapper::declarePyConnectModule( int serverId, bool toBroadcast )
{
  unsigned char * dataBuffer = NULL;
  
  if (!pPyConnectModule_)
    return;

  int nameLen = pPyConnectModule_->name.length();
  int descLen = pPyConnectModule_->desc.length();
  int dsl = packedIntLen( descLen );
  int dataLength = nameLen + descLen + dsl;
  int totalMsgSize = 4 + dataLength;
  dataBuffer = new unsigned char[totalMsgSize];

  unsigned char * bufPtr = dataBuffer;
  
  *bufPtr = (unsigned char)(MODULE_DECLARE << 4 | (serverId & 0xf)); bufPtr++;

#ifdef __clang__ // notify the other end that we evaluate method argument in a "reversed" order
  *bufPtr++ = 1;
#else
  *bufPtr++ = 0;
#endif

  packString( (unsigned char*)pPyConnectModule_->name.data(), nameLen, bufPtr );
  packString( (unsigned char*)pPyConnectModule_->desc.data(), descLen, bufPtr, true );

  *bufPtr = PYCONNECT_MSG_END;
  
  this->dispatchMessage( dataBuffer, totalMsgSize, toBroadcast );
  
  delete [] dataBuffer;
}

void PyConnectWrapper::declareModuleAttrMetd( int serverId )
{
  unsigned char * dataBuffer = NULL;
  
  if (!pPyConnectModule_)
    return;
  
  // calculate required buffer size
  int attrlens = 0;
  int metdlens = 0;
  
  int attrValueLens = 0;
  AttrValuePack * attrValuePacks = new AttrValuePack[pPyConnectModule_->attributes.size()];
  int attId = 0;
  for (Attributes::iterator aiter = pPyConnectModule_->attributes.begin();
    aiter != pPyConnectModule_->attributes.end(); aiter++)
  {
    attrlens += aiter->first.length();
    attrValuePacks[attId].len = aiter->second->getRawValue( attrValuePacks[attId].buf );
    attrValueLens += attrValuePacks[attId].len;
    attId++;
  }

  int nofattrs = pPyConnectModule_->attributes.size();
  int totalAttrSize = attrlens + attrValueLens + nofattrs * 2;  // 2 == type + name length

  for (Methods::iterator miter = pPyConnectModule_->methods.begin();
    miter != pPyConnectModule_->methods.end(); miter++)
  {
    metdlens += miter->first.length();
    Method * curMetd = miter->second;
    metdlens += curMetd->args().size(); // argument type 1 byte per argument
  }
  int nofmethods = pPyConnectModule_->methods.size();
  int totalMetdSize = metdlens + nofmethods * 3;  // 3 == type + name length + nofargs

  int asl = packedIntLen( nofattrs );
  int msl = packedIntLen( nofmethods );

  int totalMsgSize = 3 + asl + msl + totalAttrSize + totalMetdSize;

  //DEBUG_MSG( "attrValueLens %d total msg size %d\n", attrValueLens, totalMsgSize );

  dataBuffer = new unsigned char[totalMsgSize];
  
  unsigned char * bufPtr = dataBuffer;
  
  *bufPtr = (unsigned char) (ATTR_METD_EXPOSE << 4 | (serverId & 0xf)); bufPtr++;
  *bufPtr = (unsigned char) serverMap_[serverId].assignedModuleID; bufPtr++;

  // pack available attributes information
  packIntToStr( nofattrs, bufPtr );
  attId = 0;
  for (Attributes::iterator aiter = pPyConnectModule_->attributes.begin();
    aiter != pPyConnectModule_->attributes.end(); aiter++)
  {
    Attribute * attr = aiter->second;
    char flag = (attr->isWritable() ? 1 : 0) << 6;
    flag |= attr->type & 0x3f;
    *bufPtr = flag; bufPtr++;
    packString( (unsigned char*)aiter->first.data(), aiter->first.length(), bufPtr );
    memcpy( bufPtr, attrValuePacks[attId].buf, attrValuePacks[attId].len );
    bufPtr += attrValuePacks[attId].len;
    // clean up attrValuePacks
    attrValuePacks[attId].len = 0;
    delete [] attrValuePacks[attId].buf;
    attId++;
  }
  // pack available method information  
  packIntToStr( nofmethods, bufPtr );
  for (Methods::iterator miter = pPyConnectModule_->methods.begin();
    miter != pPyConnectModule_->methods.end(); miter++)
  {
    Method * metd = miter->second;
    *bufPtr = (unsigned char)(metd->type & 0x3f); bufPtr++;
    packString( (unsigned char*)miter->first.data(), miter->first.length(), bufPtr );
    *bufPtr = (unsigned char) metd->args().size(); bufPtr++;
    for (Arguments::iterator iter = metd->args().begin();
      iter != metd->args().end();iter++)
    {
      char flag = ((*iter)->isOptional() ? 1 : 0) << 6;
      flag |= (*iter)->type & 0x3f;
      *bufPtr = flag; bufPtr++;
    }
  }

  *bufPtr = PYCONNECT_MSG_END;

  this->dispatchMessage( dataBuffer, totalMsgSize );
  delete [] dataBuffer;
}

MesgProcessResult PyConnectWrapper::processInput( unsigned char * recData, int bytesReceived, struct sockaddr_in & cAddr, bool skipdecrypt )
{
  unsigned char * message = NULL;
  int messageSize = 0;

  if (bytesReceived < 1) {
    ERROR_MSG( "PyConnectWrapper::processInput received empty data? Ignore.\n" );
    return MESG_PROCESSED_FAILED;
  }

  if (skipdecrypt) {
    message = recData;
    messageSize = bytesReceived;
  }
  else {
    if (decryptMessage( recData, (int)bytesReceived, &message, (int *)&messageSize ) != 1) {
      WARNING_MSG( "Unable to decrypt incoming messasge.\n" );
      return MESG_PROCESSED_FAILED;
    }
  }

  int msgType = (message[0] >> 4) & 0xf;
  int serverId = message[0] & 0xf;
  if (serverId == -1 || msgType == 0) {
    ERROR_MSG( "PyConnectWrapper::processInput invalid message header! "
      "serverId = %d, msgType = %d. Ignore.\n", serverId, msgType );
    return MESG_PROCESSED_FAILED;
  }

  message++;
  ServerMap::iterator siter = serverMap_.find( serverId );
  if (msgType == MODULE_DISCOVERY) {
    // check server/module mapping
    if (siter == serverMap_.end()) {
      ServerInfo sinfo;
      sinfo.assignedModuleID = -1;
      sinfo.attributeUpdate = true;
      sinfo.sAddr = cAddr;
      serverMap_[serverId] = sinfo;
      declarePyConnectModule( serverId, false );
      return MESG_PROCESSED_OK;
    }
    else {
      WARNING_MSG( "PyConnectWrapper::processInput server %d is already registerd! Ignore.\n",
        serverId );
      return MESG_TO_SHUTDOWN;
    }
  }
  else if (msgType == SERVER_SHUTDOWN) {
    if (siter != serverMap_.end()) {
      INFO_MSG( "Remove server %d\n", serverId );
#ifndef OPENR_OBJECT
      pPyConnectModule_->oobject()->onClientDisconnected( serverId );
#endif
      serverMap_.erase( serverId );
    }
    return MESG_TO_SHUTDOWN;
  }
  else if (msgType == MODULE_ASSIGN_ID) {
    int modId = (int)*message++;
    int dummyLen = 0;
    std::string mName = unpackString( message, dummyLen );
    if (mName == pPyConnectModule_->name) {
      if (siter == serverMap_.end()) { // new server
        ServerInfo sinfo;
        sinfo.assignedModuleID = modId;
        sinfo.attributeUpdate = true;
        sinfo.sAddr = cAddr;
        serverMap_[serverId] = sinfo;
      }
      else {
        siter->second.assignedModuleID = modId;
      }
#ifndef OPENR_OBJECT
      pPyConnectModule_->oobject()->onClientConnected( serverId );
#endif
      declareModuleAttrMetd( serverId );
    }
    return MESG_PROCESSED_OK;
  }
  else {
    if (siter == serverMap_.end()) {
      WARNING_MSG( "PyConnectWrapper::processInput server %d is not registerd! Ignore.\n",
                   serverId );
      return MESG_PROCESSED_FAILED;
    }
    if (siter->second.assignedModuleID != (int)*message++) {
      // not for us. sliently ignore
      return MESG_PROCESSED_FAILED;
    }
  }
  switch (msgType) {
    case CALL_ATTR_METD:
    case CALL_ATTR_METD_NOCB:
    {
      this->noResponse_ = (msgType == CALL_ATTR_METD_NOCB);
      int rDataSize = 0;
      int dummyLen = 0;
      unpackLENumber( rDataSize, message, dummyLen );
      int attrId = unpackStrToInt( message, rDataSize );
      if (attrId < (int)pPyConnectModule_->attributes.size()) {
        Attributes::iterator iter = pPyConnectModule_->attributes.begin();
        advance( iter , attrId );
        iter->second->setAttrValue( attrId, message, rDataSize, serverId );
      }
      else {
        int metdId = attrId - pPyConnectModule_->attributes.size();
        Methods::iterator iter = pPyConnectModule_->methods.begin();
        advance( iter, metdId );
        iter->second->methodCall( metdId, message, rDataSize, serverId );
      }
      this->noResponse_ = false; //reset
    }
      break;
    case ATTR_VALUE_UPDATE:
    {
      int rDataSize = 0;
      int dummyLen = 0;
      unpackLENumber( rDataSize, message, dummyLen );
      int attrId = unpackStrToInt( message, rDataSize );
      if (attrId < (int)pPyConnectModule_->attributes.size()) {
        Attributes::iterator iter = pPyConnectModule_->attributes.begin();
        advance( iter , attrId );
        iter->second->getAttrValue( attrId, serverId );
      }
      else {
        ERROR_MSG( "PyConnectWrapper::processInput invalid attribute id %d! Ignore.\n",
              attrId );
      }
    }
      break;
    case GET_ATTR_METD_DESC:
    {
      int rDataSize = 1;
      int attrId = unpackStrToInt( message, rDataSize );
      if (attrId < (int)pPyConnectModule_->attributes.size()) {
        Attributes::iterator iter = pPyConnectModule_->attributes.begin();
        advance( iter , attrId );
        sendAttrMetdResponse( NO_ERRORS, attrId, iter->second->desc.length(),
            (unsigned char*)iter->second->desc.data(), serverId );
      }
      else {
        int metdId = attrId - pPyConnectModule_->attributes.size();
        Methods::iterator iter = pPyConnectModule_->methods.begin();
        advance( iter, metdId );
        sendAttrMetdResponse( NO_ERRORS, metdId, iter->second->desc.length(),
                           (unsigned char*)iter->second->desc.data(), serverId );
      }
    }
      break;
    default:
      ERROR_MSG( "PyConnectWrapper::processInput invalid message header! Ignore.\n" );
      return MESG_PROCESSED_FAILED;
  }
  if (*message++ != pyconnect::PYCONNECT_MSG_END) {
    WARNING_MSG( "PythonServer::processInput: possible message corruption. msg id %d\n",
                  msgType );
  }

  return MESG_PROCESSED_OK;
}

PyConnectMsgStatus PyConnectWrapper::validateAttribute( int attrId, const char * attrName )
{
  PyConnectMsgStatus status = NO_ERRORS;
  Attributes::iterator oiter = pPyConnectModule_->attributes.find( std::string( attrName ) );
  Attributes::iterator iter = pPyConnectModule_->attributes.begin();
  advance( iter, attrId );

  if (oiter != iter) {
    ERROR_MSG( "PyConnectWrapper::validateAttribute attribute %s index mismatch."
      "Provided index %d\n", attrName, attrId );
    status = INDEX_MISMATCH;
  }
    
  return status;
}

PyConnectMsgStatus PyConnectWrapper::validateMethod( int metdId, const char * metdName )
{
  PyConnectMsgStatus status = NO_ERRORS;
  Methods::iterator oiter = pPyConnectModule_->methods.find( std::string( metdName ) );
  Methods::iterator iter = pPyConnectModule_->methods.begin();
  advance( iter, metdId );

  if (oiter != iter) {
    ERROR_MSG( "PyConnectWrapper::validateMethod method %s index mismatch."
      "Provided index %d\n", metdName, metdId );
    status = INDEX_MISMATCH;
  }
  return status;
}

void PyConnectWrapper::sendAttrMetdResponse( int err, int index, int length, 
  unsigned char * data, int serverId, PyConnectMsg msgType )
{
  unsigned char * dataBuffer = NULL;

  int al = packedIntLen( index );
  int dataLength = length + al;
  int totalMsgSize = 4+sizeof(int)+dataLength;

  dataBuffer = new unsigned char[totalMsgSize];

  unsigned char * bufPtr = dataBuffer;
  
  *bufPtr = (unsigned char) (msgType << 4 | (serverId & 0xf)); bufPtr++;
  *bufPtr = (unsigned char) serverMap_[serverId].assignedModuleID; bufPtr++;
  *bufPtr = (unsigned char) err; bufPtr++;
  packToLENumber( dataLength, bufPtr );

  packIntToStr( index, bufPtr );
  if (length) {
    memcpy( bufPtr, data, length );
    bufPtr += length;
  }
  *bufPtr = PYCONNECT_MSG_END;
  
  this->dispatchMessage( dataBuffer, totalMsgSize );
  
  delete [] dataBuffer;
}

void PyConnectWrapper::addNewAttribute( const char * attrName, const char * desc,
  PyConnectType::Type type, int (*getrawfn)( unsigned char * & ), void (*getfn)( int, int ),
  void (*setfn)( int, unsigned char * &, int &, int ) )
{
  if (strlen( attrName ) > 255) {
    ERROR_MSG( "PyConnectWrapper::addNewAttribute attribute %s name length is"
        " too long. Igore\n", attrName );
    return;
  }
  
  Attributes::iterator iter = pPyConnectModule_->attributes.find( std::string( attrName ) );

  if (iter != pPyConnectModule_->attributes.end()) {
    ERROR_MSG( "PyConnectWrapper::addNewAttribute duplicate attribute %s.\n",
      attrName );
    return;
  }

  pPyConnectModule_->attributes[std::string( attrName )] =
    new Attribute( desc, type, getrawfn, getfn, setfn );
}

void PyConnectWrapper::addNewMethod( const char * metdName, const char * desc,
  PyConnectType::Type type, void (*accessFn)( int, unsigned char * &, int &, int ),
  Arguments & args )
{
  if (strlen( metdName ) > 255) {
    ERROR_MSG( "PyConnectWrapper::addNewMethod method %s name length is"
          " too long. Igore\n", metdName );
    return;
  }
  
  Methods::iterator iter = pPyConnectModule_->methods.find( std::string( metdName ) );

  if (iter != pPyConnectModule_->methods.end()) {
    ERROR_MSG( "PyConnectWrapper::addNewMethod duplicate method %s.\n",
      metdName );
    return;
  }

  pPyConnectModule_->methods[std::string( metdName )] =
    new Method( desc, type, accessFn, args );
}

void PyConnectWrapper::updateMethodAccessFn( const char * metdName,
  void (*accessFn)( int, unsigned char * &, int &, int ) )
{
  Methods::iterator iter = pPyConnectModule_->methods.find( std::string( metdName ) );

  if (iter == pPyConnectModule_->methods.end()) {
    ERROR_MSG( "PyConnectWrapper::updateMethodAccessFn unable to method %s.\n",
      metdName );
    return;
  }

  iter->second->methodCall( accessFn );
}

void PyConnectWrapper::moduleShutdown()
{
  int totalMsgSize = 3;
  unsigned char dataBuffer[3];
  dataBuffer[2] = PYCONNECT_MSG_END;
  
  for (ServerMap::iterator iter = serverMap_.begin();
     iter != serverMap_.end(); iter++)
  {
    dataBuffer[0] = (unsigned char) (MODULE_SHUTDOWN << 4 | (iter->first & 0xf));
    dataBuffer[1] = (unsigned char) iter->second.assignedModuleID;
    this->dispatchMessage( dataBuffer, totalMsgSize );
  }

  serverMap_.clear();
  if (pPyConnectModule_) {
    delete pPyConnectModule_;
    pPyConnectModule_ = NULL;
  }  
}

}  // namespace pyconnect

