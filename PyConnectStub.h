/*
 *  PyConnectStub.h
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

#ifndef PyConnectStub_h_DEFINED
#define PyConnectStub_h_DEFINED

#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif

#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#endif

#include <Python.h>
#include <vector>

#include "structmember.h"
#include "PyConnectCommon.h"
#include "PyConnectObjComm.h"

// critical section/mutex
#ifdef MULTI_THREAD
#ifdef WIN32
#include <Winbase.h>
extern CRITICAL_SECTION g_criticalSection;
#else
#include <pthread.h>
#include <signal.h>
extern pthread_mutex_t g_mutex;
extern pthread_t g_iothread;
#endif
#endif

#ifndef PYCONNECT_DEFAULT_SERVER_ID
#define PYCONNECT_DEFAULT_SERVER_ID  1
#endif

namespace pyconnect {

class PyOutputWriter {
public:
  virtual void write( const char * msg ) = 0;
  virtual PyObject * mainScript() = 0;
  virtual ~PyOutputWriter() {}
};

class PyConnectObject;

class PyConnectArgument { // no description for argument yet
public:
  PyConnectArgument( std::string & name, PyConnectType::Type type,
    bool optional = false );
  PyConnectArgument( PyConnectType::Type type, bool optional = false );

  std::string & name() { return this->name_; }
  PyConnectType::Type type() const { return type_; }
  bool isOptional() const { return isOptional_; }

protected:
  std::string name_;
  PyConnectType::Type type_;
  bool isOptional_;
};

class PyConnectAttribute : public PyConnectArgument {
public:
  PyConnectAttribute( std::string & name, PyConnectType::Type type,
    bool readonly, PyObject * initValue );
  ~PyConnectAttribute();
  bool isReadOnly() const { return readonly_; }
  PyObject * getValue() { Py_INCREF( value_ ); return value_; }
  void setValue( PyObject * newValue );
  void setDescription( const std::string & desc ) { desc_ = desc; }

private:
  std::string desc_;
  bool readonly_;
  PyObject * value_;
};

typedef std::vector<PyConnectAttribute *> pyAttributes;
typedef std::vector<PyConnectArgument *> pyArguments;

class PyConnectMethod : public PyObject {
public:
  PyConnectMethod( PyConnectObject * owner, std::string & name,
      PyConnectType::Type type, pyArguments & args, int optArgs = 0 );
  ~PyConnectMethod();
  
  int id() const { return id_; }
  std::string & name() { return name_; }
  void setDescription( const std::string & desc ) { desc_ = desc; myMetdDef_.ml_doc = desc_.c_str(); }
  PyConnectType::Type retType() const { return type_; }
  PyObject * methodObj() { Py_INCREF( metdObj_ ); return metdObj_; }

  static PyObject * pyCallRouter( PyObject * self, PyObject * args );
  static void dealloc( PyConnectMethod * self ) { delete self; }

private:
  PyConnectObject * owner_;
  std::string name_;
  std::string desc_;
  PyConnectType::Type type_;
  pyArguments args_;
  int optArgs_;
  int id_;
  PyObject * pyFunction_;
  PyObject * ownerClassObj_;
  PyObject * metdObj_;
  struct PyMethodDef myMetdDef_;
  
  PyObject * pyCall( PyObject * args );
};

typedef std::vector<PyConnectMethod *> pyMethods;

class PyConnectObject : public PyObject {
public:
  PyConnectObject();
  PyConnectObject( const char * name, int id = -1, const char * desc = NULL, char options = 0 );
  void init( char * name, int id = -1, char * desc = NULL );
  ~PyConnectObject();

  static PyObject * getAttr( PyObject * pObject, PyObject * attrName )
    { return static_cast<PyConnectObject *>(pObject)->getAttribute( PyString_AS_STRING( attrName ) ); }
  static int setAttr( PyObject * pObject, PyObject * attrName, PyObject * value )
    { return static_cast<PyConnectObject *>(pObject)->setAttribute( PyString_AS_STRING( attrName ), value ); }

  static PyObject * pyNew( PyTypeObject * type, PyObject * args,
               PyObject * kwds );
  static int pyInit( PyConnectObject * self, PyObject * args,
              PyObject * kwds );
  static void dealloc( PyConnectObject * self ) { delete self; }
  
  int id() { return id_; }
  std::string & name() { return name_; }

  void onSetAttrMetdResp( int index, int err, unsigned char * & data, int & remainingLength );
  void onGetAttrResp( int index, int err, unsigned char * & data, int & remainingLength );
  void onSetAttrMetdDesc( unsigned char * & data, int & remainingLength );
  void addNewAttribute( std::string & name, PyConnectType::Type type,
    bool readOnly, PyObject * initValue );
  void addNewMethod( std::string & metdName, PyConnectType::Type type,
    pyArguments & args, int optArgs );

  void setNetworkAddress( struct sockaddr_in & cAddr );

private:
  int id_;
  bool noCallback_;
  bool argEvalReversed_;
  std::string name_;
  std::string desc_;
  pyAttributes pPyAttrs_;
  pyMethods pPyMetds_;
  PyObject * myDict_;

  PyObject * getAttribute( char * name );
  int setAttribute( char * name, PyObject * value );
  
  friend class PyConnectMethod;
};

class PyConnectStub : public MessageProcessor {
public:
  static  PyObject * PyConnect_discover( PyObject * self );
  static  PyObject * PyConnect_write( PyObject * self, PyObject * args );
  static  PyObject * PyConnect_connect( PyObject * self, PyObject * args );
  static  PyObject * PyConnect_disconnect( PyObject * self, PyObject * args );
  static  PyObject * PyConnect_set_broadcast( PyObject * self, PyObject * args );
  static  PyObject * PyConnect_send_peer_msg( PyObject * self, PyObject * args );
  
  static  PyConnectStub * init( PyOutputWriter * pow = NULL );
  static  PyConnectStub * instance() { return s_pPyConnectStub; }
  static  void fini();
  static  void invokeCallback( PyObject * module, const char * fnName, PyObject * arg );
  
  PyObject * getPyConnect() { return pPyConnect_; }

  void remoteAttrMethodCall( PyConnectObject * pObject, int index,
    unsigned char * argStr = NULL, int argLength = 0,
    PyConnectMsg msgType = CALL_ATTR_METD );

  MesgProcessResult processInput( unsigned char * recData, int bytesReceived, struct sockaddr_in & cAddr, bool skipdecrypt = false );

  void updateMPID( int id );
  void sendDiscoveryMsg( bool broadcast = true );
  void sendPeerMessage( char * msg );

private:
  typedef std::vector< PyConnectObject * > PyModules;

  PyOutputWriter *  pow_;
  PyObject *        pPyConnect_;
  int               nextObjId_;
  PyModules         modules_;
  int               serverID_;

  static PyConnectStub *  s_pPyConnectStub;

  void addNewModule( std::string & name, std::string & desc, char options, struct sockaddr_in & cAddr );
  void assignModuleID( std::string & name, int id );
  PyConnectObject * findModuleByID( int id );
  PyConnectObject * findModuleByRef( PyObject * obj );
  PyConnectObject * findModuleByName( std::string & name );
  void deleteModuleByID( int id );
  void shutdownModuleByRef( PyConnectObject * obj );
  PyConnectStub( PyOutputWriter * pow, PyObject * pyConnect );
  ~PyConnectStub();
};

} // namespace pyconnect

#endif // PyConnectStub_h_DEFINED
