/*
 *  PyConnectStub.cpp
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

#include <arpa/inet.h>
#include "PyConnectStub.h"

#ifdef WIN_32
#pragma warning( disable: 4018 )
#endif

namespace pyconnect {
// static variables
PyDoc_STRVAR( PyConnectObject_doc, "PyConnect Object. Not documented." );
PyDoc_STRVAR( PyConnect_doc, \
              "PyConnect module is a module that act as an central interface to all available Pythonised OPEN-R objects." );

static PyTypeObject PyConnectObjectType = {
  PyObject_HEAD_INIT(&PyType_Type)
  0,            /*ob_size*/
  "PyConnect.PyConnectObject",  /*tp_name*/
  sizeof(PyConnectObject),  /*tp_basicsize*/
  0,            /*tp_itemsize*/
  (destructor)PyConnectObject::dealloc,  /*tp_dealloc*/
  0,            /*tp_print*/
  0,            /*tp_getattr*/
  0,            /*tp_setattr*/
  0,            /*tp_compare*/
  0,            /*tp_repr*/
  0,            /*tp_as_number*/
  0,            /*tp_as_sequence*/
  0,            /*tp_as_mapping*/
  0,            /*tp_hash */
  0,            /*tp_call*/
  0,            /*tp_str*/
  PyConnectObject::getAttr,  /*tp_getattro*/
  PyConnectObject::setAttr,  /*tp_setattro*/
  0,            /*tp_as_buffer*/
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,    /*tp_flags*/
  PyConnectObject_doc,    /* tp_doc */
  0,            /* tp_traverse */
  0,            /* tp_clear */
  0,            /* tp_richcompare */
  0,            /* tp_weaklistoffset */
  0,            /* tp_iter */
  0,            /* tp_iternext */
  0,            /* tp_methods */
  0,            /* tp_members */
  0,            /* tp_getset */
  &PyBaseObject_Type,            /* tp_base */
  0,            /* tp_dict */
  0,            /* tp_descr_get */
  0,            /* tp_descr_set */
  0,            /* tp_dictoffset */
  (initproc)PyConnectObject::pyInit,  /* tp_init */
  0,            /* tp_alloc */
  PyConnectObject::pyNew    /* tp_new */
};

static PyTypeObject PyConnectMethodType = {
  PyObject_HEAD_INIT(NULL)
  0,                         /*ob_size*/
  "PyConnect.PyConnectMethod",     /*tp_name*/
  sizeof(PyConnectMethod),      /*tp_basicsize*/
  0,                         /*tp_itemsize*/
  (destructor)PyConnectMethod::dealloc, /*tp_dealloc*/
  0,                        /*tp_print*/
  0,                        /*tp_getattr*/
  0,                        /*tp_setattr*/
  0,                        /*tp_compare*/
  0,                        /*tp_repr*/
  0,                        /*tp_as_number*/
  0,                        /*tp_as_sequence*/
  0,                        /*tp_as_mapping*/
  0,                        /*tp_hash */
  0,                        /*tp_call*/
  0,                        /*tp_str*/
  0,                        /*tp_getattro*/
  0,                        /*tp_setattro*/
  0,                        /*tp_as_buffer*/
  Py_TPFLAGS_DEFAULT,       /*tp_flags*/
  "PyConnectMethod objects",   /* tp_doc */
  0,                        /* tp_traverse */
  0,                        /* tp_clear */
  0,                        /* tp_richcompare */
  0,                        /* tp_weaklistoffset */
  0,                        /* tp_iter */
  0,                        /* tp_iternext */
  0,                        /* tp_methods */
  0,                        /* tp_members */
  0,                        /* tp_getset */
  0,                        /* tp_base */
  0,                        /* tp_dict */
  0,                        /* tp_descr_get */
  0,                        /* tp_descr_set */
  0,                        /* tp_dictoffset */
  0,                        /* tp_init */
  0,                        /* tp_alloc */
  0,                        /* tp_new */
};

/**
*  Static method to initiate PyConnect extension module
 *
 */
static PyMethodDef PyConnect_methods[] = {
  { "write", PyConnectStub::PyConnect_write, METH_VARARGS,
    "standard output for PyConnect server console" },
  { "discover", (PyCFunction)PyConnectStub::PyConnect_discover, METH_NOARGS,
    "search for available PyConnect objects" },
  { "connect", (PyCFunction)PyConnectStub::PyConnect_connect, METH_VARARGS,
    "connect to a specific remote PyConnect component" },
  { "disconnect", (PyCFunction)PyConnectStub::PyConnect_disconnect, METH_VARARGS,
    "disconnect a specific remote PyConnect component" },
  { "set_broadcast", (PyCFunction)PyConnectStub::PyConnect_set_broadcast, METH_VARARGS,
    "set new broadcast address" },
  { "send_peer_message", (PyCFunction)PyConnectStub::PyConnect_send_peer_msg, METH_VARARGS,
    "send peer servers a message" },
  { NULL, NULL, 0, NULL }           /* sentinel */
};

PyConnectArgument::PyConnectArgument( std::string & name, PyConnectType::Type type,
                               bool optional ) :
  name_( name ),
  type_( type ),
  isOptional_( optional )
{}

PyConnectArgument::PyConnectArgument( PyConnectType::Type type, bool optional ) :
  name_( "" ),
  type_ ( type ),
  isOptional_( optional )
{}

PyConnectAttribute::PyConnectAttribute( std::string & name, PyConnectType::Type type,
  bool readonly, PyObject * initValue ) :
  PyConnectArgument( name, type ),
  readonly_( readonly ),
  value_( initValue )  // we steal reference
{}

PyConnectAttribute::~PyConnectAttribute()
{
  Py_DECREF( value_ );
}

void PyConnectAttribute::setValue( PyObject * newValue )
{
  // assume we get a value with new reference
  Py_DECREF( value_ );
  value_ = newValue;
}

PyConnectObject::PyConnectObject() :
  noCallback_( false ),
  argEvalReversed_( false )
{
  PyConnectObject( "Generic PyConnect object", -1, 
    "Undocumented PyConnect object" );
}

PyConnectObject::PyConnectObject( const char * name, int id, const char * desc, char options ) :
  noCallback_( false ),
  argEvalReversed_( false )
{
  PyObject_INIT( this, &PyConnectObjectType );

  if (name)
    this->name_ = std::string( name );
  else
    this->name_ = std::string( "Generic PyConnect object" );

  if (desc) {
    this->desc_ = std::string( desc );
  }
  else {
    this->desc_ = "Undocumented PyConnect object";
  }
  this->id_ = id;
  
  if (options == 1) {
    argEvalReversed_ = true;
  }

  this->myDict_ = PyDict_New();
  PyObject * idObj = PyInt_FromLong( id );
  PyDict_SetItemString( this->myDict_, "id", idObj );
  Py_DECREF( idObj );
  PyObject * nameObj = PyString_FromString( name );
  PyDict_SetItemString( this->myDict_, "__name__", nameObj );
  Py_DECREF( nameObj );
}

void PyConnectObject::init( char * name, int id, char * desc )
{
  if (name)
    this->name_ = std::string( name );

  if (desc) {
    this->desc_ = std::string( desc );
  }

  this->id_ = id;
  PyObject * idObj = PyInt_FromLong( id );
  PyDict_SetItemString( this->myDict_, "id", idObj );
  Py_DECREF( idObj );
  PyObject * nameObj = PyString_FromString( name );
  PyDict_SetItemString( this->myDict_, "__name__", nameObj );
  Py_DECREF( nameObj );
}

void PyConnectObject::setNetworkAddress( struct sockaddr_in & cAddr )
{
  if (cAddr.sin_family != AF_INET)
    return;
    
  PyObject * portObj = PyInt_FromLong( cAddr.sin_port );
  PyDict_SetItemString( this->myDict_, "__port__", portObj );
  Py_DECREF( portObj );
  
  char cAddrStr[INET_ADDRSTRLEN];
  inet_ntop( AF_INET, &cAddr.sin_addr.s_addr, cAddrStr, INET_ADDRSTRLEN );

  PyObject * ipObj = PyString_FromString( cAddrStr );
  PyDict_SetItemString( this->myDict_, "__ip__", ipObj );
  Py_DECREF( ipObj );
}

PyConnectObject::~PyConnectObject()
{
  for (pyAttributes::iterator iter = pPyAttrs_.begin();
     iter != pPyAttrs_.end(); iter++)
  {
    delete *iter;
  }
  pPyAttrs_.clear();
  
  for (pyMethods::iterator iter = pPyMetds_.begin();
     iter != pPyMetds_.end(); iter++)
  {
    delete *iter;
  }
  pPyMetds_.clear();
  PyDict_Clear( this->myDict_ );
  Py_XDECREF( this->myDict_ );
}

PyObject * PyConnectObject::pyNew( PyTypeObject * type, PyObject * args,
    PyObject * kwds )
{
  // NOTE: this pyNew should only be called on the server side
  // PyConnectObject created this way does not correspond to actual 
  // Pythonised OPEN-R object by itself (Implication for this is 
  // that it does not have an ID and not in  the internal live 
  // PyConnect object list.
  //DEBUG_MSG( "PyConnectObject::pyNew\n" );
  static const char *kwlist[] = { "name", "id", "doc", NULL };

  PyObject * py_name = NULL;
  PyObject * py_doc = NULL;
  int id = 0;

  if (!PyArg_ParseTupleAndKeywords( args, kwds, "O|iO", (char **)kwlist, 
    &py_name, &id, &py_doc ))
  {
    return NULL;
  }
  return (PyObject *) new PyConnectObject( PyString_AS_STRING( py_name ),
    id, py_doc ? PyString_AS_STRING( py_doc ) : NULL );
}

int PyConnectObject::pyInit( PyConnectObject * self, PyObject * args,
    PyObject * kwds )
{
  return 0;
}

void PyConnectObject::addNewAttribute( std::string & name,
    PyConnectType::Type type, bool readOnly, PyObject * initValue )
{
  INFO_MSG( "PyConnectObject:: add new %s attribute: %s, type %d\n",
         readOnly ? "readonly" : "", name.c_str(), (int)type );
  pPyAttrs_.push_back( new PyConnectAttribute( name, type, readOnly, initValue ) );
  PyDict_SetItemString( this->myDict_, name.c_str(), initValue );
}

void PyConnectObject::addNewMethod( std::string & metdName, 
    PyConnectType::Type type, pyArguments & args, int optArgs )
{
  INFO_MSG( "PyConnectObject:: add a new method: %s rettype %d "
    "args %d, optional args %d\n", metdName.c_str(), (int)type,
    (int)args.size(), optArgs );
  INFO_MSG( "Arguments are:\n" );
  for (unsigned int i=0; i<args.size(); i++) {
    INFO_MSG( "\ttype %d\n", (int) args[i]->type() );
  }
  PyConnectMethod * pMetd = new PyConnectMethod( this, metdName, type, args, optArgs );
  pPyMetds_.push_back( pMetd );
  PyDict_SetItemString( this->myDict_, metdName.c_str(), pMetd );
}

PyObject * PyConnectObject::getAttribute( char * name )
{
  if (!strcmp( name, "__doc__" )) {
    return PyString_FromString( this->desc_.c_str() );
  }

  if (!strcmp( name, "__nocallback__" )) {
    if (this->noCallback_)
      Py_RETURN_TRUE;
    else
      Py_RETURN_FALSE;
  }

  if (!strcmp( name, "__dict__" )) {
    Py_INCREF( this->myDict_ );
    return this->myDict_;
  }
  bool found = false;
  pyAttributes::iterator aiter;
  for (aiter = pPyAttrs_.begin();aiter != pPyAttrs_.end();aiter++) {
    if (!strcmp( name, (*aiter)->name().c_str() )) {
      found = true;
      break;
    }
  }

  if (found) {
    return (*aiter)->getValue();
  }
  pyMethods::iterator miter;
  for (miter = pPyMetds_.begin();miter != pPyMetds_.end();miter++) {
    if (!strcmp( name, (*miter)->name().c_str() )) {
      found = true;
      break;
    }
  }
  if (found) {
    PyObject * metdObj = (*miter)->methodObj();
    if (!PyCallable_Check( metdObj )) {
      PyErr_Format( PyExc_TypeError, "%s is not callable object", name );
      return NULL;
    }
    else {
      return metdObj;
    }
  }
  // check dictionary
  PyObject * otherAttr = PyDict_GetItemString( this->myDict_, name );
  if (otherAttr) {
    Py_INCREF( otherAttr );
    return otherAttr;
  }
  else { // let python generic get attribute have a go
    PyObject * pName = PyString_FromString( name );
    PyObject * pResult = PyObject_GenericGetAttr( this, pName );
    Py_DECREF( pName );
    return pResult;
  }
}

int PyConnectObject::setAttribute( char * name, PyObject * value )
{
  if (!value)
    return -1;
  
  std::string attName( name );

  if (!strcmp( name, "__nocallback__" )) {
    if (PyBool_Check( value )) {
      this->noCallback_ = (value == Py_True);
    }
    else {
      PyErr_Format( PyExc_AttributeError, 
        "%s must take a boolean value.", name );
      return -1;
    }
  }

  // check buildin readonly attributes
  if (!strcmp( name, "__name__" ) ||
      !strcmp( name, "id" ) )
  {
    PyErr_Format( PyExc_AttributeError, 
      "%s is a read-only build-in attribute.", name );
    return -1;
  }
  if (!strcmp( name, "__doc__" )) {
    this->desc_ = std::string( PyString_AsString( value ) );
    return 0;
  }

  bool found = false;
  pyAttributes::iterator aiter;
  for (aiter = pPyAttrs_.begin();aiter != pPyAttrs_.end();aiter++) {
    if (attName == (*aiter)->name()) {
      found = true;
      break;
    }
  }
  
  if (found) {
    if ((*aiter)->isReadOnly()) {
      PyErr_Format( PyExc_AttributeError, "attribute %s is read-only.", name );
      return -1;
    }
    int aind = int(aiter - pPyAttrs_.begin());
    int valSize = PyConnectType::validateTypeAndSize( value, (*aiter)->type() );
    if (!valSize ) {
      PyErr_Format( PyExc_ValueError, "value for attribute %s is not a %s",
        (*aiter)->name().c_str(), PyConnectType::typeName( (*aiter)->type() ).c_str() );
      return -1;
    }
    unsigned char * valBuf = new unsigned char[valSize];
    unsigned char * dataPtr = valBuf;
    PyConnectType::packToStr( value, (*aiter)->type(), dataPtr );
    if (this->noCallback_) {
      Py_INCREF( value );
      (*aiter)->setValue( value );
      PyConnectStub::instance()->remoteAttrMethodCall( this, aind, valBuf, valSize, CALL_ATTR_METD_NOCB );
    }
    else {
      PyConnectStub::instance()->remoteAttrMethodCall( this, aind, valBuf, valSize );
    }
    delete [] valBuf;
    return 0;
  }
  pyMethods::iterator miter;
  for (miter = pPyMetds_.begin();miter != pPyMetds_.end();miter++) {
    if (!strcmp( name, (*miter)->name().c_str() )) {
      found = true;
      break;
    }
  }
  if (found) {
    PyErr_Format( PyExc_TypeError, "%s is a built-in method provided by "
      "%s PyConnect Object. You cannot override it!", name, this->name_.c_str() );
    return -1;
  }
  // check our dictionary
  PyObject * otherAttr = PyDict_GetItemString( this->myDict_, name );
  if (otherAttr) {
    if (value == Py_None) // delete
      PyDict_DelItemString( this->myDict_, name );
    else
      PyDict_SetItemString( this->myDict_, name, value );
  }
  else {
    PyDict_SetItemString( this->myDict_, name, value );
  }
  return 0;
}

void PyConnectObject::onSetAttrMetdResp( int index, int err, unsigned char * & data, int & remainingLength )
{
  std::string fname;
  PyObject * arg = NULL;

  if (index < (int)pPyAttrs_.size()) {
    pyAttributes::iterator aiter = pPyAttrs_.begin() + index;
    fname = (*aiter)->name();
    if (err) {  // onSetAttFailed
      fname = "onSet" + fname + "Failed";
      arg = Py_BuildValue( "(i)", err );
    }
    else {  //onSetAtt
      fname = "onSet" + fname;
      PyObject * retVal = 
        PyConnectType::unpackStr( (*aiter)->type(), data, remainingLength );
      (*aiter)->setValue( retVal );
      arg = Py_BuildValue( "(O)", retVal );
    }
  }
  else {
    int mindex = index - (int)pPyAttrs_.size();
    if (mindex < (int)pPyMetds_.size()) {
      pyMethods::iterator miter = pPyMetds_.begin() + mindex;
      fname = (*miter)->name();
      if (err) {  // onMetdFailed
        fname = "on" + fname + "Failed";
        arg = Py_BuildValue( "(i)", err );
      }
      else {  //onMetdComplete
        fname = "on" + fname + "Completed";
        PyObject * retVal =
          PyConnectType::unpackStr( (*miter)->retType(), data, remainingLength );
        arg = Py_BuildValue( "(O)", retVal );
        Py_DECREF( retVal );
      }
    }
    else {
      fname = "onUnknownAttrMetdError";
      arg = Py_BuildValue( "(i)", index );
    }
  }
  PyConnectStub::invokeCallback( this, fname.c_str(), arg );
  Py_DECREF( arg );
}

void PyConnectObject::onGetAttrResp( int index, int err, unsigned char * & data, int & remainingLength )
{
  std::string fname;
  PyObject * arg = NULL;
  
  //DEBUG_MSG( "onGetAttrResp, id %d, err %d, len %d, data %.*s\n", index, err, remainingLength,
  //           remainingLength, data );
  
  if (index < (int)pPyAttrs_.size()) {
    pyAttributes::iterator aiter = pPyAttrs_.begin() + index;
    fname = (*aiter)->name();
    if (err) {  // onAttrUpdateFailed
      fname = "on" + fname + "UpdateFailed";
      arg = Py_BuildValue( "(i)", err );
    }
    else {  //onAttrUpdate
      fname = "on" + fname + "Update";
      PyObject * retVal = 
        PyConnectType::unpackStr( (*aiter)->type(), data, remainingLength );
      (*aiter)->setValue( retVal );
      arg = Py_BuildValue( "(O)", retVal );
    }
  }
  else {
    fname = "onUnknownAttrbuteError";
    arg = Py_BuildValue( "(i)", index );
  }
  PyConnectStub::invokeCallback( this, fname.c_str(), arg );
  Py_DECREF( arg );
}

void PyConnectObject::setAttrMetdDesc( int index, int err, unsigned char * & data, int & remainingLength )
{
  // pending further investigation
  /*
  PyObject * arg = NULL;
  
  if (index < pPyAttrs_.size()) {
    pyAttributes::iterator aiter = pPyAttrs_.begin() + index;
  }
  else {
    index -= pPyAttrs_.size();
    if (index < pPyMetds_.size()) {
      pyMethods::iterator miter = pPyMetds_.begin() + index;
    }
  }
  */
  return;
}

/*
 *====================
 * PyConnectStub section
 *====================
 */

PyConnectStub * PyConnectStub::s_pPyConnectStub = NULL;

PyConnectStub::PyConnectStub( PyOutputWriter * pow, PyObject * pyConnect ) :
  pow_( pow ),
  pPyConnect_( pyConnect ),
  nextObjId_( 1 ),
  serverID_( PYCONNECT_DEFAULT_SERVER_ID )
{
}

PyConnectStub::~PyConnectStub()
{
  unsigned char dataBuffer[3];

  for (PyModules::const_iterator miter = modules_.begin();
       miter != modules_.end(); miter++)
  {
    dataBuffer[0] = (unsigned char) (SERVER_SHUTDOWN << 4 | (this->serverID_ & 0xf));
    dataBuffer[1] = (unsigned char) (*miter)->id();
    dataBuffer[2] = PYCONNECT_MSG_END;
    this->dispatchMessage( dataBuffer, 3 );
    Py_DECREF( *miter );
  }
  modules_.clear();
  Py_DECREF( pPyConnect_ );
}

void PyConnectStub::remoteAttrMethodCall( PyConnectObject * pObject, int index,
  unsigned char * argStr, int argLen, PyConnectMsg msgType )
{
  int objID = pObject->id();
  if (objID < 0)
    return;

  int asl = packedIntLen( index );
  int totalMsgSize = 3 + sizeof( int ) + asl + argLen;
  unsigned char * dataBuffer = new unsigned char[totalMsgSize];
  
  unsigned char * bufPtr = dataBuffer;
  
  *bufPtr = (unsigned char) (msgType << 4 | (this->serverID_ & 0xf)); bufPtr++;
  *bufPtr = (unsigned char) objID; bufPtr++;
  int dataLength = asl + argLen;
  packToLENumber( dataLength, bufPtr );

  packIntToStr( index, bufPtr );
  if (!(argLen == 0 || argStr == NULL)) {
    memcpy( bufPtr, argStr, argLen );
    bufPtr += argLen;
  }
  *bufPtr = PYCONNECT_MSG_END;
  this->dispatchMessage( dataBuffer, totalMsgSize );
  delete [] dataBuffer;
}

void PyConnectStub::assignModuleID( std::string & name, int id )
{
  int totalMsgSize = 4 + (int)name.length();
  unsigned char * dataBuffer = new unsigned char[totalMsgSize];
  
  unsigned char * bufPtr = dataBuffer;
  
  *bufPtr = (unsigned char) (MODULE_ASSIGN_ID << 4 | (this->serverID_ & 0xf)); bufPtr++;
  *bufPtr = (unsigned char) id; bufPtr++;
  packString( (unsigned char*)name.data(), (int)name.length(), bufPtr );
  *bufPtr = PYCONNECT_MSG_END;

  this->dispatchMessage( dataBuffer, totalMsgSize );
  delete [] dataBuffer;
}

PyConnectStub * PyConnectStub::init( PyOutputWriter * pow )
{
  if (!s_pPyConnectStub) {
    assert( PyType_Ready(&PyConnectObjectType) >= 0 );
    assert( PyType_Ready(&PyConnectMethodType) >= 0 );

    PyObject * pyConnect = 
      Py_InitModule3( "PyConnect", PyConnect_methods, PyConnect_doc );

    Py_INCREF( &PyConnectObjectType );
    Py_INCREF( &PyConnectMethodType );

    //PyModule_AddObject( pyConnect, "PyConnectObject", (PyObject *)&PyConnectObjectType );

    s_pPyConnectStub = new PyConnectStub( pow, pyConnect );
  }
  return s_pPyConnectStub;
}

void PyConnectStub::fini()
{
  if (!s_pPyConnectStub) {
    delete s_pPyConnectStub;
    s_pPyConnectStub = NULL;
  }
}

void PyConnectStub::updateMPID( int id )
{
  if (serverID_ != PYCONNECT_DEFAULT_SERVER_ID) { // this means the server id has been set before
    WARNING_MSG( "PythonServer: server ID is being changed from %d to %d\n",
                serverID_, id );
  }
  serverID_ = id;
  PyObject_SetAttrString( pPyConnect_, "ServerID", PyInt_FromLong( id ) );
}

void PyConnectStub::addNewModule( std::string & name, std::string & desc, char options, struct sockaddr_in & cAddr )
{
  INFO_MSG( "Creating new PyConnectObject: %s\n", name.c_str() );

  if (name == "") {
    ERROR_MSG( "PyConnectStub::addNewModule: Module name is empty.\n" );
    return;
  }
  if (nextObjId_ > 0xff) {
    ERROR_MSG( "PyConnectStub::addNewModule: maximum number registered "
           "module reached Ignore.\n" );
    return;
  }

  // threadsafe lock
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  
  // trying to load a module script contains partial implementation
  // for the PyConnect module
/* Not use this for the moment.
#ifdef DOS_FILENAME_LENGTH
  std::string filename = name.substr( 0, 8 );
  INFO_MSG( "Attempt to import corresponding %s script%s\n", filename.c_str(),
    name.length() > 8 ? "[Name truncated]" : "" );
#else
  std::string filename = name;
  INFO_MSG( "Attempt to import corresponding %s script\n", filename.c_str() );
#endif

  PyObject * pModule = PyImport_ImportModule( const_cast<char *>(filename.c_str()) );
  PyObject * pClass = NULL;
*/
  PyConnectObject * py_newObj = NULL;
/*
  if (pModule == NULL) {
    if (PyErr_Occurred() == PyExc_ImportError) {
      ERROR_MSG( "Error importing %s script\n", filename.c_str() );
      PyErr_Print();
    }
    PyErr_Clear();
  }
  else {
    pClass = PyObject_GetAttrString( pModule, const_cast<char*>(name.c_str()) );
  }

  if (pClass) {
    // check if the imported class is a sub class of PyConnectObject
    if (PyObject_TypeCheck( pClass, &PyConnectObjectType )) { 

      //py_newObj = static_cast<PyConnectObject *>(_PyObject_New( (PyTypeObject *)pClass ));
      if (!py_newObj) {
        ERROR_MSG( "Unable to create new type object\n" );
      }
      else {
        py_newObj->init( name.c_str(), nextObjId_, 
          (desc != "" ? desc.c_str() : NULL) );
      }
    }
    else {
      ERROR_MSG( "PyConnectStub::addNewModule: Imported class %s is not a "
                 "sub type of PyConnectObject. Ignore.\n", name.c_str() );
    }
  }
  else {
    // clear only earlier errors
    PyErr_Clear(); 
    */
    py_newObj = new PyConnectObject( name.c_str(), nextObjId_, 
                                  (desc != "" ? desc.c_str() : NULL), options );
//  }
//  Py_XDECREF( pClass );
//  Py_XDECREF( pModule );

  assert( py_newObj != NULL );
  py_newObj->setNetworkAddress( cAddr );
  PyObject_SetAttrString( pPyConnect_, const_cast<char*>(name.c_str()), py_newObj );
  
  modules_.push_back( py_newObj );
  PyGILState_Release( gstate );

  assignModuleID( name, nextObjId_++ );
}

PyConnectObject * PyConnectStub::findModuleByID( int id )
{
  PyModules::iterator miter = modules_.begin();
  
  while (!(miter==modules_.end() || (*miter)->id() == id)) miter++;
  
  if (miter == modules_.end())
    return NULL;
  else
    return *miter;
}

void PyConnectStub::shutdownModuleByRef( PyConnectObject * obj )
{
  //DEBUG_MSG( "PyConnectStub::deleteModuleByID %d\n", id );
  PyModules::iterator miter = modules_.begin();
  
  while (!(miter == modules_.end() || (*miter) == obj)) miter++;
  
  if (miter != modules_.end()) {
    unsigned char dataBuffer[3];
    
    dataBuffer[0] = (unsigned char) (SERVER_SHUTDOWN << 4 | (this->serverID_ & 0xf));
    dataBuffer[1] = (unsigned char) obj->id();
    dataBuffer[2] = PYCONNECT_MSG_END;
    this->dispatchMessage( dataBuffer, 3 );
  
    // threadsafe lock
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();
    
    PyObject * arg = Py_BuildValue( "(si)", obj->name().c_str(), obj->id() );
    if (pow_) {
      invokeCallback( pow_->mainScript(), "onModuleDestroyed", arg );
    }
    else {
      invokeCallback( pPyConnect_, "onModuleDestroyed", arg );
    }
    Py_DECREF( arg );
    PyObject_DelAttrString( pPyConnect_, const_cast<char *>(obj->name().c_str()) );
    Py_DECREF( *miter );
    modules_.erase( miter );
    PyGILState_Release( gstate );
  }
}

void PyConnectStub::deleteModuleByID( int id )
{
  //DEBUG_MSG( "PyConnectStub::deleteModuleByID %d\n", id );
  PyModules::iterator miter = modules_.begin();

  while (!(miter == modules_.end() || (*miter)->id() == id)) miter++;

  if (miter != modules_.end()) {
    // threadsafe lock
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    PyObject * arg = Py_BuildValue( "(si)", (*miter)->name().c_str(), id );
    if (pow_) {
      invokeCallback( pow_->mainScript(), "onModuleDestroyed", arg );
    }
    else {
      invokeCallback( pPyConnect_, "onModuleDestroyed", arg );
    }
    Py_DECREF( arg );
    PyObject_DelAttrString( pPyConnect_, const_cast<char *>((*miter)->name().c_str()) );
    Py_DECREF( *miter );
    modules_.erase( miter );
    PyGILState_Release( gstate );
  }
}

PyConnectObject * PyConnectStub::findModuleByName( std::string & name )
{
  PyModules::iterator miter = modules_.begin();

  while (!(miter==modules_.end() || (*miter)->name() == name)) miter++;

  if (miter==modules_.end())
    return NULL;
  else
    return *miter;  
}

PyConnectObject * PyConnectStub::findModuleByRef( PyObject * obj )
{
  PyConnectObject * searchObj = static_cast<PyConnectObject *> (obj);
  PyModules::iterator miter = modules_.begin();

  while (!(miter==modules_.end() || (*miter) == searchObj)) miter++;

  if (miter == modules_.end())
    return NULL;
  else
    return *miter;  
}

MesgProcessResult PyConnectStub::processInput( unsigned char * recData, int bytesReceived, struct sockaddr_in & cAddr, bool skipdecrypt )
{
  if (bytesReceived < 1) {
    ERROR_MSG( "PythonServer::processInput received empty data? Ignore.\n" );
    return MESG_PROCESSED_FAILED;
  }

  unsigned char * message = NULL;
  int messageSize = 0;
  
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

  int msgType = message[0] >> 4 & 0xf;
  if (msgType == 0) {
    ERROR_MSG( "PythonServer::processInput invalid message header! Ignore.\n" );
    return MESG_PROCESSED_FAILED;
  }
  //DEBUG_MSG( "PyConnectStub::processInput(): msgType %d\n", msgType );

  message++;
  PyConnectObject * pPyModule = NULL;
  int dummyLen = 0;
  int moduleId = 0;
  if (msgType == MODULE_DECLARE) {
    char modOpt = *message++;
    std::string mName = unpackString( message, dummyLen );
    pPyModule = findModuleByName( mName );
    if (pPyModule == NULL) {
      std::string mDesc = unpackString( message, dummyLen, true );
      addNewModule( mName, mDesc, modOpt, cAddr );
      return MESG_PROCESSED_OK;
    }
    else {
      WARNING_MSG( "PythonServer::processInput module %s already registered! Ignore.\n",
            mName.c_str() );
      return MESG_TO_SHUTDOWN; // we should reject the connection
    }
  }
  else if (msgType == MODULE_SHUTDOWN) {
    // find appropriate module
    moduleId = (int)(*message++ & 0xf);
    //DEBUG_MSG( "PyConnectStub:processInput: MODULE_SHUTDOWN id %d\n",
    //  moduleId );
    deleteModuleByID( moduleId );
    return MESG_TO_SHUTDOWN;
  }
  else if (msgType == PEER_SERVER_MSG) {
    int peerServerID = *(message - 1) & 0xf;
    std::string peerMsg = unpackString( message, dummyLen );
    // threadsafe lock
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    PyObject * arg = Py_BuildValue( "(is)", peerServerID, peerMsg.c_str() );
    if (pow_) {
      invokeCallback( pow_->mainScript(), "onPeerMessage", arg );
    }
    else {
      invokeCallback( pPyConnect_, "onPeerMessage", arg );
    }
    Py_DECREF( arg );

    PyGILState_Release( gstate );
    return MESG_PROCESSED_OK;
  }
  else if (msgType == PEER_SERVER_DISCOVERY) {
    // TODO:: to be implemented
    return MESG_PROCESSED_OK;
  }
  else {
    // find appropriate module
    moduleId = (int)(*message++ & 0xf);
    pPyModule = findModuleByID( moduleId );
    if (pPyModule == NULL) {
      WARNING_MSG( "PythonServer::processInput: unable to find module with ID %d.\n",
        moduleId );
      return MESG_PROCESSED_FAILED;
    }
  }
  // threadsafe lock
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

  switch (msgType) {
    case ATTR_METD_EXPOSE:
    {
      //DEBUG_MSG( "PyConnectStub:processInput: ATTR_METD_EXPOSE\n" );
      // unpacking attributes
      int nofattrs = unpackStrToInt( message, dummyLen );
      for (int i = 0;i < nofattrs;i++) {
        bool readOnly = !(*message >> 6);
        PyConnectType::Type type =
          (PyConnectType::Type)(*message++ & 0x3f);
        std::string attrName = unpackString( message, dummyLen );
        PyObject * initValue = PyConnectType::unpackStr( type, message, dummyLen );
        pPyModule->addNewAttribute( attrName, type, readOnly, initValue );
      }

      // unpacking methods
      int nofmetds = unpackStrToInt( message, dummyLen );
      for (int i = 0;i < nofmetds;i++) {
        PyConnectType::Type type = (PyConnectType::Type)(*message++ & 0x3f);
        std::string metdName = unpackString( message, dummyLen );
        int nofargs =(int)(*message++ & 0xf);
        pyArguments args;
        int optArgs = 0;
        for (int j = 0;j < nofargs;j++) {
          bool optional = !!(*message >> 6);
          if (optional) optArgs++;
          PyConnectType::Type type = 
            (PyConnectType::Type)(*message++ & 0x3f);
          args.push_back( new PyConnectArgument( type, optional ) );
        }
        pPyModule->addNewMethod( metdName, type, args, optArgs );
      }
      PyObject * arg = Py_BuildValue( "(O)", pPyModule );
      if (pow_) {
        invokeCallback( pow_->mainScript(), "onModuleCreated", arg );
      }
      else {
        invokeCallback( pPyConnect_, "onModuleCreated", arg );
      }
      Py_DECREF( arg );
    }
      break;
    case ATTR_METD_RESP:
    {
      //DEBUG_MSG( "PyConnectStub:processInput: ATTR_METD_RESP\n" );
      int err = (int) (*message++ & 0xf);
      int datalen = 0;
      int dummyLen = 0;
      unpackLENumber( datalen, message, dummyLen );  // assume both server and client conform to same interger definition
      int amind = unpackStrToInt( message, datalen );
      pPyModule->onSetAttrMetdResp( amind, err, message, datalen );
    }
      break;
    case ATTR_VALUE_UPDATE:
    {
      //DEBUG_MSG( "PyConnectStub:processInput: ATTR_VALUE_UPDATE\n" );
      int err = (int)(*message++ & 0xf);
      int datalen = 0;
      int dummyLen = 0;
      unpackLENumber( datalen, message, dummyLen );  // assume both server and client conform to same interger definition
      int amind = unpackStrToInt( message, datalen );
      pPyModule->onGetAttrResp( amind, err, message, datalen );
    }
      break;
    case ATTR_METD_DESC_RESP:
    {
      //DEBUG_MSG( "PyConnectStub:processInput: ATTR_METD_DESC_RESP\n" );
      int err = (int)(*message++ & 0xf);
      int datalen = 0;
      int dummyLen = 0;
      unpackLENumber( datalen, message, dummyLen );  // assume both server and client conform to same interger definition
      int amind = unpackStrToInt( message, datalen );
      pPyModule->setAttrMetdDesc( amind, err, message, datalen );
    }
      break;
    default:
      ERROR_MSG( "PythonServer::processInput invalid message header! Ignore.\n" );
      return MESG_PROCESSED_FAILED;
  }
  PyGILState_Release( gstate );
  
  if (*message++ != PYCONNECT_MSG_END) {
    WARNING_MSG( "PythonServer::processInput: possible message corruption. msg id %d\n",
        msgType );
  }

  return MESG_PROCESSED_OK;
}

PyConnectMethod::PyConnectMethod( PyConnectObject * owner, std::string & name,
  PyConnectType::Type type, pyArguments & args, int optArgs ) :
  owner_( owner ),
  name_( name ),
  type_( type ),
  args_( args ),
  optArgs_( optArgs )
{
  PyObject_INIT( this, &PyConnectMethodType );

  id_ = (int)owner_->pPyMetds_.size();
  myMetdDef_.ml_name = const_cast<char *>(name_.c_str());
  myMetdDef_.ml_meth = (PyCFunction)&(PyConnectMethod::pyCallRouter);
  myMetdDef_.ml_flags = METH_VARARGS;
  myMetdDef_.ml_doc = 0;
  pyFunction_ = PyCFunction_New( &myMetdDef_, this );
  ownerClassObj_ = PyObject_GetAttrString( owner_, "__class__" );
  metdObj_ = PyMethod_New( pyFunction_, owner_, ownerClassObj_ );
}

PyObject * PyConnectMethod::pyCallRouter( PyObject * self, PyObject * args )
{
  return static_cast<PyConnectMethod *>(self)->pyCall( args );
}

PyObject * PyConnectMethod::pyCall( PyObject * args )
{
  int argSize = (int)PyTuple_Size( args ) - 1; // remove first self argument
  int minReqArgs = (int)args_.size() - optArgs_;

  if (argSize < minReqArgs ||
      argSize > minReqArgs + optArgs_)
  {
    PyErr_Format( PyExc_TypeError, "%s() requires %s%d arguments, "
      "total %d arguments.", name_.c_str(), 
      (optArgs_) ? "at least " : "", minReqArgs, 
      minReqArgs + optArgs_ );
    return NULL;
  }

  //DEBUG_MSG( "Method id %d. nof real args %d minimum req args %d, total args %d\n",
  //  id_, argSize, minReqArgs, minReqArgs + optArgs_ );
  
  PyObject * pItem = NULL;
  int totalArgSize = 0;
  unsigned char * argsBuf = NULL;
  if (argSize > 0) {
    PyObject ** argList = new PyObject *[argSize];
    for (int i = 0;i < argSize;i++) {
      pItem = PyTuple_GetItem( args, i+1 );
      int sizeReq = PyConnectType::validateTypeAndSize( pItem, args_[i]->type() );
      if (!sizeReq) {
        PyErr_Format( PyExc_ValueError, "%s(): argument %d is not a %s",
              name_.c_str(), i+1, PyConnectType::typeName( args_[i]->type() ).c_str() );
        delete [] argList;
        return NULL;
      }
      argList[i] = pItem;
      Py_INCREF( pItem );
      totalArgSize += sizeReq;
    }

    argsBuf = new unsigned  char[totalArgSize];
    unsigned char * dataPtr = argsBuf;
    if (owner_->argEvalReversed_) {
      for (int i = 0;i < argSize; i++) {
        PyConnectType::packToStr( argList[i], args_[i]->type(), dataPtr );
        Py_XDECREF( argList[i] );
      }
    }
    else {
      for (int i = argSize-1;i >= 0; i--) {
        PyConnectType::packToStr( argList[i], args_[i]->type(), dataPtr );
        Py_XDECREF( argList[i] );
      }
    }
    delete [] argList;
  }
  int mindex = (int)owner_->pPyAttrs_.size() + id_;
  if (owner_->noCallback_) {
    PyConnectStub::instance()->remoteAttrMethodCall( owner_, mindex, argsBuf, totalArgSize, CALL_ATTR_METD_NOCB );
  }
  else {
    PyConnectStub::instance()->remoteAttrMethodCall( owner_, mindex, argsBuf, totalArgSize );
  }
  if (argsBuf)
    delete [] argsBuf;
  Py_RETURN_NONE;
}

PyConnectMethod::~PyConnectMethod()
{
  for (pyArguments::iterator iter = args_.begin();
     iter != args_.end(); iter++)
  {
    delete *iter;
  }
  args_.clear();
}

void PyConnectStub::sendDiscoveryMsg( bool broadcast )
{
  if (!s_pPyConnectStub)
    return;

  unsigned char dataBuffer[2];
  
  dataBuffer[0] = (unsigned char) (MODULE_DISCOVERY << 4 | (this->serverID_ & 0xf));
  dataBuffer[1] = PYCONNECT_MSG_END;

  s_pPyConnectStub->dispatchMessage( dataBuffer, 2, broadcast );
}

void PyConnectStub::sendPeerMessage( char * mesg )
{
  if (!s_pPyConnectStub)
    return;

  int msgLen = (int)strlen( mesg );
  int dsl = packedIntLen( msgLen );
  int dataLength = msgLen + dsl + 2;
  unsigned char * dataBuffer = new unsigned char[dataLength];

  unsigned char * bufPtr = dataBuffer;

  *bufPtr = (unsigned char) (PEER_SERVER_MSG << 4 | (this->serverID_ & 0xf)); bufPtr++;
  packString( (unsigned char *)mesg, msgLen, bufPtr, true );
  *bufPtr = PYCONNECT_MSG_END;

  s_pPyConnectStub->dispatchMessage( dataBuffer, dataLength, true );
  delete [] dataBuffer;
}

PyObject * PyConnectStub::PyConnect_discover( PyObject * self )
{
  s_pPyConnectStub->sendDiscoveryMsg();
  Py_RETURN_NONE;
}

PyObject * PyConnectStub::PyConnect_write( PyObject *self, PyObject * args )
{
  char * msg;
  std::string outputMsg;

  if (!s_pPyConnectStub || !s_pPyConnectStub->pow_)
    Py_RETURN_NONE;

  if (!PyArg_ParseTuple( args, "s", &msg )) {
    // PyArg_ParseTuple will set the error status.
    return NULL;
  }
  
  // Next send it to all (active) clients.
  while (*msg) {
    if (*msg == '\n')
      outputMsg += "\r\n";
    else
      outputMsg += *msg;
    msg++;
  }

  s_pPyConnectStub->pow_->write( outputMsg.c_str() );
  
  Py_RETURN_NONE;
}

PyObject * PyConnectStub::PyConnect_disconnect( PyObject *self, PyObject * args )
{
  if (!s_pPyConnectStub)
    Py_RETURN_NONE;
  
  PyObject * obj;
  PyConnectObject * pyConnectbj = NULL;

  if (!PyArg_ParseTuple( args, "O", &obj )) {
    // PyArg_ParseTuple will set the error status.
    return NULL;
  }
  if (PyString_Check( obj )) {
    std::string objName = PyString_AsString( obj );
    pyConnectbj = PyConnectStub::instance()->findModuleByName( objName );
  }
  else if (PyObject_IsInstance(obj, (PyObject *) &PyConnectObjectType)) {
    pyConnectbj = PyConnectStub::instance()->findModuleByRef( obj );
  }
  else {
    PyErr_Format( PyExc_TypeError, "Input argument is not string object or a PyConnect object." );
    return NULL;
  }

  if (!pyConnectbj) {
    PyErr_Format( PyExc_LookupError, "Unable to find a live PyConnect object correspoding to the input." );
    return NULL;
  }
  INFO_MSG( "Attempt to delete module %s id %d\n", pyConnectbj->name().c_str(), pyConnectbj->id() );
  PyConnectStub::instance()->shutdownModuleByRef( pyConnectbj );

  Py_RETURN_NONE;  
}

PyObject * PyConnectStub::PyConnect_connect( PyObject *self, PyObject * args )
{
  if (!s_pPyConnectStub)
    Py_RETURN_NONE;

  char * addr;
  int port = 0;
  if (!PyArg_ParseTuple( args, "s|i", &addr, &port )) {
    // PyArg_ParseTuple will set the error status.
    return NULL;
  }

  bool commFailed = true;
  CommObjectStat ret = s_pPyConnectStub->connectTo( addr, port );
  if (ret & COMM_FAILED) {
    PyErr_Format( PyExc_IOError,
      "PyConnect.connect: unable to connect PyConnect component on %s!", addr );
  }
  else if (ret & INVALID_ADDR_PORT) {
    PyErr_Format( PyExc_ValueError,
      "PyConnect.connect: invalid PyConnect component address:port!" );
  }
  else if (ret & NOT_SUPPORTED) {
    PyErr_Format( PyExc_NotImplementedError,
      "PyConnect.connect: No underlying comm object support!" );
  }
  else { // COMM_SUCCESS
    s_pPyConnectStub->sendDiscoveryMsg( false );
    commFailed = false;
  }
  if (commFailed)
    return NULL;
  else
    Py_RETURN_NONE;
}

PyObject * PyConnectStub::PyConnect_set_broadcast( PyObject * self, PyObject * args )
{
  if (!s_pPyConnectStub)
    Py_RETURN_NONE;

  char * msg;
  if (!PyArg_ParseTuple( args, "s", &msg )) {
    // PyArg_ParseTuple will set the error status.
    return NULL;
  }
  CommObjectStat ret = s_pPyConnectStub->setBroadcastAddr( msg );
  if (ret & COMM_FAILED) {
    PyErr_Format( PyExc_IOError,
      "PyConnect.set_broadcast: unable to set broadcast address to %s!", msg );
    return NULL;
  }
  else if (ret & INVALID_ADDR_PORT) {
    PyErr_Format( PyExc_ValueError,
      "PyConnect.set_broadcast: invalid broadcast address!" );
    return NULL;
  }
  else if (ret & NOT_SUPPORTED) {
    PyErr_Format( PyExc_NotImplementedError,
      "PyConnect.set_broadcast: No underlying comm object support!" );
    return NULL;
  }
  s_pPyConnectStub->sendDiscoveryMsg();
  Py_RETURN_NONE;
}

PyObject * PyConnectStub::PyConnect_send_peer_msg( PyObject * self, PyObject * args )
{
  if (!s_pPyConnectStub)
    Py_RETURN_NONE;

  char * msg;
  
  if (!PyArg_ParseTuple( args, "s", &msg )) {
    // PyArg_ParseTuple will set the error status.
    return NULL;
  }
  
  s_pPyConnectStub->sendPeerMessage( msg );

  Py_RETURN_NONE;
}
// helper method to call python function or object method
void PyConnectStub::invokeCallback( PyObject * module, const char * fnName, PyObject * arg )
{
  if (!module)
    return;

  //DEBUG_MSG( "Attempt get callback function %s\n", fnName );

  PyObject * callbackFn = PyObject_GetAttrString( module, const_cast<char *>(fnName) ); 
  if (!callbackFn) {
    PyErr_Clear();
    return;
  }
  else if (!PyCallable_Check( callbackFn )) {
    PyErr_Format( PyExc_TypeError, "%s is not callable object", fnName );
  }
  else {
    PyObject * pResult = PyObject_CallObject( callbackFn, arg );
    if (PyErr_Occurred()) {
      PyErr_Print();
    }
    Py_XDECREF( pResult );
  }
  Py_DECREF( callbackFn );
}
  
} // namespace pyconnect
