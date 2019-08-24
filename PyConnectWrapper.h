/*
 *  PyConnectWrapper.h
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

#ifndef PyConnectWrapper_h_DEFINED
#define PyConnectWrapper_h_DEFINED

#ifdef WIN32
#pragma warning( disable: 4267 ) //TODO: need to detailed verification on this usage
#endif

#include <string>
#include <vector>
#include <iterator>
#include <map>
#include <typeinfo>
#include <type_traits>

#ifdef OPENR_OBJECT
#include <OPENR/OObject.h>
#include <OPENR/OSyslog.h>
#endif
#include "PyConnectCommon.h"
#include "PyConnectObjComm.h"

namespace pyconnect {

#ifndef OPENR_OBJECT
class OObject
{
public:
  virtual void onClientConnected( int clientid ) {} // from our module perspective
  virtual void onClientDisconnected( int clientid ) {}
  virtual ~OObject() {}
}; // abstract root class
#endif

typedef struct {
  std::string  desc;
  PyConnectType::Type type;
} ModuleElement;
  
class Argument : public ModuleElement
{
public:
  Argument( const char * name, const char * desc,
    PyConnectType::Type type, bool isOptional = false );
  bool isOptional() const { return isOptional_; }

  char name[256];

private:
  bool isOptional_;
};

class Attribute : public ModuleElement
{
public:
  Attribute( const char * desc,
         PyConnectType::Type type,
         int (*getrawfn)( unsigned char * & ),
         void (*getfn)( int, int ),
         void (*setfn)( int, unsigned char * &, int &, int ) = NULL );
  
  void setAttrValue( int attrId, unsigned char * & data, int & dataLen, int serverId )
    { (this->attrSetFn)( attrId, data, dataLen, serverId ); }
  
  void getAttrValue( int attrId, int serverId )
    { (this->attrGetFn)( attrId, serverId ); }
  
  int getRawValue( unsigned char * & buf ) { return (this->getRawValueFn)( buf ); }
  
  bool isWritable() { return (attrSetFn != NULL); }
  
private:
  void (*attrGetFn)( int, int );
  void (*attrSetFn)( int, unsigned char * &, int &, int );
  int (*getRawValueFn)( unsigned char * & );
};
  
typedef std::vector<Argument *>  Arguments;
typedef std::map<std::string, Attribute *>  Attributes;
  
class Method : public ModuleElement
{
public:
  Method( const char * desc, PyConnectType::Type type,
    void (*accessFn)( int, unsigned char * &, int &, int ),
    Arguments & args );
  ~Method();

  void methodCall( int id, unsigned char * & data, int & dataLength, int serverId )
    { (this->accessFn_)( id, data, dataLength, serverId ); }
  void methodCall( void (*accessFn)( int, unsigned char * &, int &, int ) )
    { this->accessFn_ = accessFn; } 
  Arguments & args() { return this->args_; }
  PyConnectType::Type retType() { return this->type; }

private:
  void (*accessFn_)( int, unsigned char * &, int &, int );
  Arguments args_;
};
  
typedef std::map<std::string, Method *>  Methods;
  
class PyConnectModule : public ModuleElement
{
public:
  std::string  name;
  Attributes  attributes;
  Methods    methods;
  
  PyConnectModule( const char * name, const char * desc, OObject * oobject );
  PyConnectModule( const std::string & name, const std::string & desc );
  ~PyConnectModule();

  OObject * oobject() { return this->oobject_; }
private:
  OObject * oobject_;
};

template <typename DataType> struct PyConnectData {
  // NOTE:: any data types apart from the basic data type are expected to have their
  // own template specialisation.
  static DataType getData( unsigned char * & dataStr, int & remainingBytes, PyConnectMsgStatus & status )
  {
    if (status != NO_ERRORS)
      return (DataType) 0; // not best way to handle error

    if ((remainingBytes - sizeof( DataType )) < 0) {
      status = MSG_CORRUPTED;
      return (DataType) 0; // not best way to handle error
    }
    DataType rData;
    unpackLENumber( rData, dataStr, remainingBytes );
    return rData;
  }
  static DataType getData( unsigned char * & dataStr, int & remainingBytes, PyConnectMsgStatus & status,
    DataType defaultValue )
  {
    if (status != NO_ERRORS)
      return (DataType) defaultValue; // not best way to handle error

    if (remainingBytes == 0) { // no data left
      return (DataType) defaultValue;
    }
    else if ((remainingBytes - sizeof( DataType )) < 0) {
      status = MSG_CORRUPTED;
      return (DataType) defaultValue; // not best way to handle error
    }
    DataType rData;
    unpackLENumber( rData, dataStr, remainingBytes );
    return rData;
  }
  static unsigned char * setData( const DataType & amValue, int & dataLength, PyConnectMsgStatus & status )
  {
    dataLength = 0;
    if (status != NO_ERRORS)
      return NULL;

    dataLength = sizeof( DataType );
    unsigned char * dataBuf = new unsigned char[dataLength];
    unsigned char * dataPtr = dataBuf;
    packToLENumber( amValue, dataPtr );
    return dataBuf;
  }
  static void fini( unsigned char * tmpData )
  {
    if (tmpData)
      delete [] tmpData;
  }
};

template <> struct PyConnectData<std::string>
{
  static std::string getData( unsigned char * & dataStr,
    int & remainingBytes, PyConnectMsgStatus & status )
  {
    if (status != NO_ERRORS)
      return std::string( "" );

    return unpackString( dataStr, remainingBytes, true );
  }
  static std::string getData( unsigned char * & dataStr, int & remainingBytes,
    PyConnectMsgStatus & status, std::string & defaultValue )
  {
    if (status != NO_ERRORS)
      return std::string( "" );

    if (remainingBytes <= 0)
      return defaultValue;

    return unpackString( dataStr, remainingBytes, true );
  }
  static unsigned char * setData( const std::string & amValue, int & dataLength, PyConnectMsgStatus & status )
  {
    dataLength = 0;
    if (status != NO_ERRORS)
      return NULL;

    dataLength = amValue.length();

    if (dataLength > MAX_STR_LENGTH) {
      ERROR_MSG( "PyConnectData::<string>SetData string is too long.\n" );
      status = STR_TOO_LONG;
      dataLength = 0; //reset data length
    }
    int l = packedIntLen( dataLength );
    dataLength += l;
    unsigned char * dataBuf = new unsigned char[dataLength];
    unsigned char * dataPtr = dataBuf;
    packString( (unsigned char*)amValue.data(), amValue.length(), dataPtr, true );
    return dataBuf;
  }
  static void fini( unsigned char * tmpData )
  {
    if (tmpData)
      delete [] tmpData;
  }
};

template <> struct PyConnectData<bool> {
  // NOTE:: any data types apart from the basic data type are expected to have their
  // own template specialisation.
  static bool getData( unsigned char * & dataStr, int & remainingBytes, PyConnectMsgStatus & status )
  {
    if (status != NO_ERRORS)
      return false;

    if ((remainingBytes - 1) < 0) {  // not enough bytes available
      status = MSG_CORRUPTED;
      return false;
    }
    bool rData = !!(*dataStr);
    remainingBytes--;
    dataStr++;
    return rData;
  }
  static bool getData( unsigned char * & dataStr, int & remainingBytes,
    PyConnectMsgStatus & status, bool defaultValue )
  {
    if (status != NO_ERRORS)
      return false;

    if (remainingBytes == 0) {
      return defaultValue;
    }
    else if ((remainingBytes - 1) < 0) {  // not enough bytes available
      status = MSG_CORRUPTED;
      return defaultValue;
    }
    bool rData = !!(*dataStr);
    remainingBytes--;
    dataStr++;
    return rData;
  }
  static unsigned char * setData( const bool & amValue, int & dataLength, PyConnectMsgStatus & status )
  {
    dataLength = 0;
    if (status != NO_ERRORS)
      return NULL;

    dataLength = 1;
    unsigned char * dataBuf = new unsigned char[dataLength];
    *dataBuf = (unsigned char)amValue;
    return dataBuf; // assume little endian
  }
  static void fini( unsigned char * tmpData )
  {
    if (tmpData)
      delete [] tmpData;
  }
};

class PyConnectWrapper : public MessageProcessor
{
public:
  MesgProcessResult processInput( unsigned char * recData, int bytesReceived, struct sockaddr_in & cAddr, bool skipdecrypt = false );

  void declarePyConnectModule( int serverId = 0, bool toBroadcast = true );
  void declareModuleAttrMetd( int serverId = 0 );
  
  void sendAttrMetdResponse( int err, int index, int length, 
    unsigned char * data, int serverId, PyConnectMsg msgType = ATTR_METD_RESP );

  template <class DataType> int packRawAttrData( const DataType & amValue, unsigned char * & dataBuf )
  {
    PyConnectMsgStatus status = NO_ERRORS;
    int retLen = 0;
    dataBuf = PyConnectData<DataType>::setData( amValue, retLen, status );
    return retLen;
  }
  
  template <class DataType> void postAttrMetdData( int amId, const DataType & amValue,
    PyConnectMsgStatus status, int serverId, PyConnectMsg msgType = ATTR_METD_RESP )
  {
    int retLen = 0;
    unsigned char * retStr = PyConnectData<DataType>::setData( amValue, retLen, status );
    this->sendAttrMetdResponse( status, amId, retLen, retStr, serverId, msgType );
    PyConnectData<DataType>::fini( retStr );
  }

  template <class DataType> void setAttrData( int attrId, DataType & attrValue, 
    unsigned char * & dataPtr, int & remainingBytes, PyConnectMsgStatus status, int serverId )
  {
    attrValue = 
      PyConnectData<DataType>::getData( dataPtr, remainingBytes, status );
    if (!this->noResponse_)
      this->sendAttrMetdResponse( status, attrId, remainingBytes, dataPtr, serverId );
  }

  template <class DataType> void updateAttribute( const char * attrName, const DataType & attrValue )
  {
    Attributes::iterator oiter = pPyConnectModule_->attributes.find( std::string( attrName ) );
    
    if (oiter == pPyConnectModule_->attributes.end()) {
      ERROR_MSG( "PyConnectWrapper::updateAttribute attribute %s is not found.\n",
            attrName );
      return;
    }
#ifdef SUN_COMPILER
    int attrId = 0;
    distance( pPyConnectModule_->attributes.begin(), oiter, attrId );
#else
    int attrId = distance( pPyConnectModule_->attributes.begin(), oiter );
#endif
    for (ServerMap::const_iterator siter = serverMap_.begin();
       siter != serverMap_.end(); siter++)
    {
      if (siter->second.attributeUpdate) {
        postAttrMetdData( attrId, attrValue, NO_ERRORS, siter->first, ATTR_VALUE_UPDATE );
      }
    }
  }
  
  void addNewAttribute( const char * attrName, const char * desc, PyConnectType::Type type,
                int (*getrawfn)( unsigned char * & ),
                void (*getfn)( int, int ),
                void (*setfn)( int, unsigned char * &, int &, int ) );

  void addNewMethod( const char * metdName, const char * desc, PyConnectType::Type type,
        void (*accessFn)( int, unsigned char * &, int &, int ), Arguments & args );
  void updateMethodAccessFn( const char * metdName,
                    void (*accessFn)( int, unsigned char * &, int &, int ) );

  PyConnectMsgStatus validateAttribute( int attrId, const char * attrName );
  PyConnectMsgStatus validateMethod( int metdId, const char * metdName );

  bool noResponse() { return noResponse_; }

  PyConnectModule * pyConnectModule() { return pPyConnectModule_; }
  void moduleShutdown();
  static void init( PyConnectModule * pModule );
  static PyConnectWrapper * instance() { return s_pPyConnectWrapper; }

  Arguments s_arglist;

private:
  typedef struct {
    int len;
    unsigned char * buf;
  } AttrValuePack; // only used in declareModuleAttrMetd
  
  typedef struct {
    int assignedModuleID;
    bool attributeUpdate;
    struct sockaddr_in sAddr;
  } ServerInfo;

  typedef std::map<int, ServerInfo> ServerMap; // <serverID, assigned ModuleID>

  ServerMap serverMap_;
  bool noResponse_;
  
  static PyConnectWrapper * s_pPyConnectWrapper;

  PyConnectWrapper( PyConnectModule * pModule ) : noResponse_( false ) { pPyConnectModule_ = pModule; }
  PyConnectModule * pPyConnectModule_;
};

#define PYCONNECT_WRAPPER_DECLARE                    \
  MessageProcessor * getMP() { return pyconnect::PyConnectWrapper::instance(); }  \
  void dummy0()

#define PYCONNECT_DECLARE_MODULE( NAME, DESC )              \
  pyconnect::PyConnectWrapper::init( new pyconnect::PyConnectModule( #NAME, DESC, this ) )

#define PYCONNECT_MODULE_INIT  \
  pyconnect::PyConnectWrapper::instance()->declarePyConnectModule()

#define PYCONNECT_MODULE_FINI  \
  pyconnect::PyConnectWrapper::instance()->moduleShutdown()  \

#define PYCONNECT_RO_ATTRIBUTE( NAME )    \
  decltype(NAME) get##NAME##_value() const  \
  { \
    return this->NAME;  \
  } \
  static int s_get_raw_value_##NAME( unsigned char * & valueBuf )  \
  {                            \
    return pyconnect::PyConnectWrapper::instance()->packRawAttrData( \
      static_cast<PYCONNECT_MODULE_NAME *>(pyconnect::PyConnectWrapper::instance()->pyConnectModule()->oobject())->get##NAME##_value(), \
      valueBuf );                    \
  }                            \
  static void s_get_attr_##NAME( int attrId, int serverId )    \
  {                            \
    if (pyconnect::PyConnectWrapper::instance()->pyConnectModule()->oobject()) {        \
      pyconnect::PyConnectWrapper::instance()->postAttrMetdData( attrId,        \
        static_cast<PYCONNECT_MODULE_NAME *>(pyconnect::PyConnectWrapper::instance()->pyConnectModule()->oobject())->get##NAME##_value(),  \
        pyconnect::PyConnectWrapper::instance()->validateAttribute( attrId, #NAME ),  \
        serverId );            \
    }                    \
    else {                  \
      ERROR_MSG( "PyConnect wrapper: unable to access attribute."  \
        "object does not exist\n" );      \
      pyconnect::PyConnectWrapper::instance()->sendAttrMetdResponse(  \
        pyconnect::NO_PYCONNECT_OBJECT, attrId, 0, NULL, serverId);    \
    }                    \
  }                      \
  void dummy_##NAME()

#define PYCONNECT_RO_ATTRIBUTE_DECLARE( NAME, DESC )    \
  pyconnect::PyConnectWrapper::instance()->addNewAttribute( #NAME, DESC,  \
    pyconnect::getVarType( NAME ), &PYCONNECT_MODULE_NAME::s_get_raw_value_##NAME, \
    &PYCONNECT_MODULE_NAME::s_get_attr_##NAME, NULL );

#define PYCONNECT_RW_ATTRIBUTE( NAME )  \
  decltype(NAME) get_##NAME##_value() const  \
  { \
    return this->NAME; \
  } \
  void set_##NAME##_value( decltype(NAME) value ) \
  { \
    return this->NAME = value;  \
  } \
  static int s_get_raw_value_##NAME( unsigned char * & valueBuf )  \
  {                            \
    return pyconnect::PyConnectWrapper::instance()->packRawAttrData( \
      static_cast<PYCONNECT_MODULE_NAME *>(pyconnect::PyConnectWrapper::instance()->pyConnectModule()->oobject())->get_##NAME##_value(), \
      valueBuf );                    \
  }                            \
  static void s_get_attr_##NAME( int attrId, int serverId )    \
  {                                \
    if (pyconnect::PyConnectWrapper::instance()->pyConnectModule()->oobject()) {        \
      pyconnect::PyConnectWrapper::instance()->postAttrMetdData( attrId,          \
        static_cast<PYCONNECT_MODULE_NAME *>(pyconnect::PyConnectWrapper::instance()->pyConnectModule()->oobject())->get_##NAME##_value(),  \
        pyconnect::PyConnectWrapper::instance()->validateAttribute( attrId, #NAME ),  \
        serverId );            \
    }                    \
    else {                  \
      ERROR_MSG( "PyConnect wrapper: unable to access attribute."  \
        "object does not exist\n" );      \
      pyconnect::PyConnectWrapper::instance()->sendAttrMetdResponse(  \
        pyconnect::NO_PYCONNECT_OBJECT, attrId, 0, NULL, serverId );    \
    }                    \
  };                      \
  static void s_set_attr_##NAME( int attrId, unsigned char * & dataStr, int & rBytes, int serverId )  \
  {                      \
    if (pyconnect::PyConnectWrapper::instance()->pyConnectModule()->oobject()) {        \
      pyconnect::PyConnectWrapper::instance()->setAttrData( attrId,            \
        static_cast<PYCONNECT_MODULE_NAME *>(pyconnect::PyConnectWrapper::instance()->pyConnectModule()->oobject())->set_##NAME##_value( NAME ),  \
        dataStr, rBytes,                      \
        pyconnect::PyConnectWrapper::instance()->validateAttribute( attrId, #NAME ),  \
        serverId );            \
    }                    \
    else {                  \
      ERROR_MSG( "PyConnect wrapper: unable to access attribute."  \
        "object does not exist\n" );    \
      pyconnect::PyConnectWrapper::instance()->sendAttrMetdResponse(  \
        pyconnect::NO_PYCONNECT_OBJECT, attrId, 0, NULL, serverId );    \
    }                    \
  }                      \
  void dummy_##NAME()

#define PYCONNECT_RW_ATTRIBUTE_DECLARE( NAME, DESC )  \
  pyconnect::PyConnectWrapper::instance()->addNewAttribute( #NAME, DESC, \
    pyconnect::getVarType( NAME ), &PYCONNECT_MODULE_NAME::s_get_raw_value_##NAME, \
    &PYCONNECT_MODULE_NAME::s_get_attr_##NAME, \
    &PYCONNECT_MODULE_NAME::s_set_attr_##NAME )

/* example void setHeadYaw( yaw, limit )
  PYCONNECT_METHOD_DECLARE( setHeadYaw, 'set PyConnect head yaw'
      ARG( yaw, float, 'current yaw value' )
      ARG( limit, float, 'current yaw limit' )      
    )
  PYCONNECT_METHOD_ACCESS( setHeadYaw, ARGTYPE( float ), ARGTYPE( HeadMove ), ARGTYPE( float ) )
*/
#define PYCONNECT_ATTRIBUTE_UPDATE( NAME )    \
  pyconnect::PyConnectWrapper::instance()->updateAttribute( #NAME, \
    static_cast<PYCONNECT_MODULE_NAME *>(pyconnect::PyConnectWrapper::instance()->pyConnectModule()->oobject())->NAME );

#define ARGTYPE( DATATYPE ) (pyconnect::PyConnectData<DATATYPE>::getData( dataStr, rBytes, status ))
#define OPT_ARGTYPE( DATATYPE, DEFAULTVALUE ) \
  (pyconnect::PyConnectData<DATATYPE>::getData( dataStr, rBytes, status, DEFAULTVALUE ))

#define ARG( NAME, TYPE, DESC )          \
  pyconnect::PyConnectWrapper::instance()->s_arglist.push_back(            \
    new pyconnect::Argument( #NAME, DESC, pyconnect::PyConnectType::typeName( #TYPE ) ) );
#define OPT_ARG( NAME, TYPE, DESC )        \
  pyconnect::PyConnectWrapper::instance()->s_arglist.push_back(            \
    new pyconnect::Argument( #NAME, DESC, pyconnect::PyConnectType::typeName( #TYPE ), true ) );

template <typename retval, typename T, typename std::enable_if<std::is_void<retval>{}, int>::type = 0> static void invoke_method_call( int metdIndex, int serverId, T fn )
{
  fn();
}

template <typename retval, typename T, typename std::enable_if<!std::is_void<retval>{}, int>::type = 0> static void invoke_method_call( int metdIndex, int serverId, T fn )
{
  pyconnect::PyConnectWrapper::instance()->postAttrMetdData( metdIndex, fn(),
                                                    pyconnect::NO_ERRORS, serverId );
}

#if defined( WIN32 ) || defined( SUN_COMPILER )
#define PYCONNECT_METHOD_ACCESS( NAME, ... )  \
  static void s_call_fn_##NAME( int metdId, unsigned char * & dataStr, int & rBytes, int serverId )        \
  {                      \
    int metdIndex = pyconnect::PyConnectWrapper::instance()->pyConnectModule()->attributes.size() + metdId; \
    if (pyconnect::PyConnectWrapper::instance()->pyConnectModule()->oobject()) {    \
      pyconnect::PyConnectMsgStatus status =  \
        pyconnect::PyConnectWrapper::instance()->validateMethod( metdId, #NAME );  \
      if (status == pyconnect::NO_ERRORS) {  \
        using fntraits = pyconnect::function_traits<std::function<decltype(&PYCONNECT_MODULE_NAME::NAME)>>; \
        try { \
          if (pyconnect::PyConnectWrapper::instance()->noResponse()) { \
            static_cast<PYCONNECT_MODULE_NAME *>(pyconnect::PyConnectWrapper::instance()->pyConnectModule()->oobject())->NAME( __VA_ARGS__ ); \
          } \
          else { \
            auto fnc = std::bind( &PYCONNECT_MODULE_NAME::NAME, \
              static_cast<PYCONNECT_MODULE_NAME *>(pyconnect::PyConnectWrapper::instance()->pyConnectModule()->oobject()), \
                           __VA_ARGS__ ); \
            invoke_method_call<fntraits::return_type>( metdIndex, serverId, fnc ); \
          } \
        } \
        catch (...) { \
          ERROR_MSG( "Caught method %s throwing an exception.\n", #NAME ); \
          pyconnect::PyConnectWrapper::instance()->sendAttrMetdResponse(  \
            pyconnect::METD_EXCEPTION, metdIndex, 0, NULL, serverId );  \
        } \
        if (std::is_void<fntraits::return_type>::value && \
            !(pyconnect::PyConnectWrapper::instance()->noResponse())) \
        { \
          pyconnect::PyConnectWrapper::instance()->sendAttrMetdResponse( pyconnect::NO_ERRORS,        \
            metdIndex, 0, NULL, serverId );  \
        } \
        return;          \
      }                  \
    }                    \
    ERROR_MSG( "PyConnect wrapper: unable to access "  \
      "correct object method.\n" );    \
    pyconnect::PyConnectWrapper::instance()->sendAttrMetdResponse(  \
      pyconnect::NO_PYCONNECT_OBJECT, metdIndex, 0, NULL, serverId );  \
  }

#define PYCONNECT_METHOD_DECLARE( NAME, DESC, ... )  \
  __VA_ARGS__                            \
  { \
    using fntraits = pyconnect::function_traits<decltype(&PYCONNECT_MODULE_NAME::NAME)>; \
    pyconnect::PyConnectWrapper::instance()->addNewMethod( #NAME, DESC,  \
      pyconnect::pyconnect_type<std::function<fntraits::return_type>>::value, &PYCONNECT_MODULE_NAME::s_call_fn_##NAME, \
      pyconnect::PyConnectWrapper::instance()->s_arglist );  \
    pyconnect::PyConnectWrapper::instance()->s_arglist.clear() \
  }

#else // !(WIN32 || SUN_COMPILER)
#define PYCONNECT_METHOD_ACCESS( NAME, ARGLIST... )  \
  static void s_call_fn_##NAME( int metdId, unsigned char * & dataStr, int & rBytes, int serverId )        \
  {                      \
    int metdIndex = pyconnect::PyConnectWrapper::instance()->pyConnectModule()->attributes.size() + metdId; \
    if (pyconnect::PyConnectWrapper::instance()->pyConnectModule()->oobject()) {    \
      pyconnect::PyConnectMsgStatus status =  \
        pyconnect::PyConnectWrapper::instance()->validateMethod( metdId, #NAME );  \
      if (status == pyconnect::NO_ERRORS) {  \
        using fntraits = pyconnect::function_traits<std::function<decltype(&PYCONNECT_MODULE_NAME::NAME)>>; \
        try { \
          if (pyconnect::PyConnectWrapper::instance()->noResponse()) { \
            static_cast<PYCONNECT_MODULE_NAME *>(pyconnect::PyConnectWrapper::instance()->pyConnectModule()->oobject())->NAME( ARGLIST ); \
          } \
          else { \
            auto fnc = std::bind( &PYCONNECT_MODULE_NAME::NAME, \
              static_cast<PYCONNECT_MODULE_NAME *>(pyconnect::PyConnectWrapper::instance()->pyConnectModule()->oobject()), \
                           ##ARGLIST ); \
            invoke_method_call<fntraits::return_type>( metdIndex, serverId, fnc ); \
          } \
        } \
        catch (...) { \
          ERROR_MSG( "Caught method %s throwing an exception.\n", #NAME ); \
          pyconnect::PyConnectWrapper::instance()->sendAttrMetdResponse(  \
            pyconnect::METD_EXCEPTION, metdIndex, 0, NULL, serverId );  \
        } \
        if (std::is_void<fntraits::return_type>::value && \
            !(pyconnect::PyConnectWrapper::instance()->noResponse())) \
        { \
          pyconnect::PyConnectWrapper::instance()->sendAttrMetdResponse( pyconnect::NO_ERRORS,        \
            metdIndex, 0, NULL, serverId );  \
        } \
        return;          \
      }                  \
    }                    \
    ERROR_MSG( "PyConnect wrapper: unable to access "  \
      "correct object method.\n" );    \
    pyconnect::PyConnectWrapper::instance()->sendAttrMetdResponse(  \
      pyconnect::NO_PYCONNECT_OBJECT, metdIndex, 0, NULL, serverId );  \
  }

#define PYCONNECT_METHOD_DECLARE( NAME, DESC )  \
  { \
    using fntraits = pyconnect::function_traits<std::function<decltype(&PYCONNECT_MODULE_NAME::NAME)>>; \
    std::vector<int> argtypelist; \
    get_args_type_list<fntraits>(pyconnect::make_index_sequence<fntraits::arity>{}, argtypelist);  \
    int asize = (int)argtypelist.size(); \
    printf("total number of arguments %d\n", asize); \
    for (int i = 0; i < asize; ++i) { \
      pyconnect::PyConnectWrapper::instance()->s_arglist.push_back(  \
        new pyconnect::Argument( "", "", (pyconnect::PyConnectType::Type)argtypelist[asize-i-1] ) ); \
    } \
    pyconnect::PyConnectWrapper::instance()->addNewMethod( #NAME, DESC,  \
      pyconnect::pyconnect_type<fntraits::return_type>::value, &PYCONNECT_MODULE_NAME::s_call_fn_##NAME, \
      pyconnect::PyConnectWrapper::instance()->s_arglist );  \
    pyconnect::PyConnectWrapper::instance()->s_arglist.clear(); \
  }

#endif // WIN32
} // namespace pyconnect

#endif // PyConnectWrapper_h_DEFINED
