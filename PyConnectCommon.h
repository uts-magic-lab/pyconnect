/*
 *  PyConnectCommon.h
 *  
 *  Copyright 2006, 2007, 2013, 2014 Xun Wang.
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

#ifndef PyConnectCommon_h_DEFINED
#define PyConnectCommon_h_DEFINED

#ifndef _MSC_VER
#include <cxxabi.h>
#endif

#include <iostream>

#ifdef PYTHON_SERVER
#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif

#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#endif

#include "Python.h"
#else
#include <tuple>
#include <functional>
#include <vector>
#endif
#include <string>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define PYCONNECT_MSG_BUFFER_SIZE 10240

#ifdef RELEASE
#define PYCONNECT_LOGGING_INIT
#define PYCONNECT_LOGGING_DECLARE( LOGNAME )
#if defined( WIN32 ) || defined( SUN_COMPILER )
#define DEBUG_MSG( ... )
#define ERROR_MSG( ... )
#define WARNING_MSG( ... )
#define INFO_MSG( ... )
#else
#define DEBUG_MSG( MSG... )
#define ERROR_MSG( MSG... )
#define WARNING_MSG( MSG... )
#define INFO_MSG( MSG... )
#endif
#define PYCONNECT_LOGGING_FINI
#else
#ifdef OPENR_LOG
#define PYCONNECT_LOGGING_INIT
#define DEBUG_MSG( MSG... ) OSYSDEBUG(( MSG ))
#define ERROR_MSG( MSG... ) OSYSLOG1(( osyslogERROR, MSG ))
#define WARNING_MSG( MSG... ) OSYSLOG1(( osyslogWARNING, MSG ))
#define INFO_MSG( MSG... ) OSYSLOG1(( osyslogINFO, MSG ))
#else
#define PYCONNECT_LOGGING_DECLARE( LOGNAME ) \
  FILE * s_pyconnectlog = NULL; \
  char * logFileName = (char *)LOGNAME

extern FILE * s_pyconnectlog;
#define PYCONNECT_LOGGING_INIT \
  s_pyconnectlog = fopen( logFileName, "a" )

//#define s_pyconnectlog stdout

#if defined( WIN32 ) || defined( SUN_COMPILER )
#define DEBUG_MSG(...) \
fprintf( s_pyconnectlog, "DEBUG: " ); \
fprintf( s_pyconnectlog, __VA_ARGS__ ); \
fflush( s_pyconnectlog )

#define INFO_MSG(...) \
fprintf( s_pyconnectlog, "INFO: " ); \
fprintf( s_pyconnectlog, __VA_ARGS__ ); \
fflush( s_pyconnectlog )

#define WARNING_MSG(...) \
fprintf( s_pyconnectlog, "WARNING: " ); \
fprintf( s_pyconnectlog, __VA_ARGS__ ); \
fflush( s_pyconnectlog )

#define ERROR_MSG(...) \
fprintf( s_pyconnectlog, "ERROR: " ); \
fprintf( s_pyconnectlog, __VA_ARGS__ ); \
fflush( s_pyconnectlog )
#else // !WIN32
#define DEBUG_MSG( MSG... ) \
fprintf( s_pyconnectlog, "DEBUG: " ); \
fprintf( s_pyconnectlog, MSG ); \
fflush( s_pyconnectlog )

#define INFO_MSG( MSG... ) \
fprintf( s_pyconnectlog, "INFO: " ); \
fprintf( s_pyconnectlog, MSG ); \
fflush( s_pyconnectlog )

#define WARNING_MSG( MSG... ) \
fprintf( s_pyconnectlog, "WARNING: " ); \
fprintf( s_pyconnectlog, MSG ); \
fflush( s_pyconnectlog )

#define ERROR_MSG( MSG... ) \
fprintf( s_pyconnectlog, "ERROR: " ); \
fprintf( s_pyconnectlog, MSG ); \
fflush( s_pyconnectlog )
#endif

#define PYCONNECT_LOGGING_FINI fclose( s_pyconnectlog )
#endif
#endif // define RELEASE

#ifndef DEFINED_ENCRYPTION_KEY
#pragma message ( "Make sure you change the encryption key for network communication. \
                   To disable this warning, set DEFINED_ENCRYPTION_KEY macro to true." )
#endif

namespace pyconnect {

const int MAX_STR_LENGTH = 32767;
const char PYCONNECT_MSG_INIT  = '#';
const char PYCONNECT_MSG_END   = '@';

typedef enum {
  MODULE_DISCOVERY      = 0x1,
  MODULE_DECLARE        = 0x2,
  MODULE_ASSIGN_ID      = 0x3,
  ATTR_METD_EXPOSE      = 0x4,
  CALL_ATTR_METD        = 0x5,
  CALL_ATTR_METD_NOCB   = 0x6,
  ATTR_METD_RESP        = 0x7,
  ATTR_VALUE_UPDATE     = 0x8,
  GET_ATTR_METD_DESC    = 0x9,
  ATTR_METD_DESC        = 0xa,
  MODULE_SHUTDOWN       = 0xb,
  SERVER_SHUTDOWN       = 0xc,
  PEER_SERVER_DISCOVERY = 0xd, // TODO: to be implemented.
  PEER_SERVER_MSG       = 0xe
} PyConnectMsg;

typedef enum {
  NO_ERRORS             = 0x0,
  NO_ATTR_METD          = 0x1,
  STR_TOO_LONG          = 0x2,
  INDEX_MISMATCH        = 0x4,
  NO_PYCONNECT_OBJECT   = 0x8,
  METD_EXCEPTION        = 0xa,
  MSG_CORRUPTED         = 0xf
} PyConnectMsgStatus;

struct PyConnectType {
  typedef enum {
    INT  = 0,
    FLOAT  = 1,
    DOUBLE  = 2, // same as float for Python in fact.
    STRING  = 3,
    BOOL  = 4,
    COMPOSITE = 5,
    PyVOID  = 6 // silly window compile breaks if defined as VOID
  } Type;

  static const std::string typeName( const Type type );
  static const Type typeName( const char * type );

#ifdef PYTHON_SERVER
  static int validateTypeAndSize( PyObject * obj, Type type );
  static PyObject * defaultValue( Type type );
  static void packToStr( PyObject * arg, Type type, unsigned char * & dataPtr );
  static PyObject * unpackStr( Type type, unsigned char * & dataPtr, int & remainingLength );
#endif
};

#ifndef PYTHON_SERVER
template <typename T>
struct is_string
{
  static constexpr bool value = false;
};
template <class T, class Traits, class Alloc>
struct is_string<std::basic_string<T, Traits, Alloc>>
{
  static constexpr bool value = true;
};

template <typename T>
struct is_supported : std::false_type {};

// Specialisations for supported types:
template <> struct is_supported<int> : std::true_type {};
template <> struct is_supported<float> : std::true_type {};
template <> struct is_supported<double> : std::true_type {};
template <> struct is_supported<bool> : std::true_type {};
template <> struct is_supported<void> : std::true_type {};
template <> struct is_supported<std::string> : std::true_type {};

template <typename T, typename std::enable_if<is_supported<T>{}, int>::type = 0>
struct pyconnect_type
{
  static constexpr PyConnectType::Type value = PyConnectType::COMPOSITE;
};

template <> struct pyconnect_type<void> { static constexpr  PyConnectType::Type value = PyConnectType::PyVOID; };
template <> struct pyconnect_type<bool> { static constexpr PyConnectType::Type value = PyConnectType::BOOL; };
template <> struct pyconnect_type<int> { static constexpr PyConnectType::Type value = PyConnectType::INT; };
template <> struct pyconnect_type<float> { static constexpr PyConnectType::Type value = PyConnectType::FLOAT; };
template <> struct pyconnect_type<double> { static constexpr PyConnectType::Type value = PyConnectType::DOUBLE; };
template <> struct pyconnect_type<std::string> { static constexpr PyConnectType::Type value = PyConnectType::STRING; };

template <typename T> static const PyConnectType::Type getVarType( const T& val )
{
#ifndef _MSC_VER
  int status = 0;
  char * demangled = abi::__cxa_demangle( typeid(val).name(), 0, 0, &status );
#else
  char * demangled = typeid(val).name();
#endif
  ERROR_MSG( "PyConnect::getVarType: c++ data type %s is not supported", demangled );
  return PyConnectType::COMPOSITE;
}
template <typename T> static const char * getVarTypeName( const T& val )
{
#ifndef _MSC_VER
  int status = 0;
  char * demangled = abi::__cxa_demangle( typeid(val).name(), 0, 0, &status );
#else
  char * demangled = typeid(val).name();
#endif
  ERROR_MSG( "PyConnect::getVarTypeName: c++ data type %s is not supported", demangled );
  return "";
}

template <> const PyConnectType::Type getVarType( const int& )
{
  return PyConnectType::INT;
}
template <> const PyConnectType::Type getVarType( const float& )
{
  return PyConnectType::FLOAT;
}
template <> const PyConnectType::Type getVarType( const double& )
{
  return PyConnectType::DOUBLE;
}
template <> const PyConnectType::Type getVarType( const bool& )
{
  return PyConnectType::BOOL;
}
template <> const PyConnectType::Type getVarType( const std::string & )
{
  return PyConnectType::STRING;
}

template <> const char * getVarTypeName( const int& )
{
  return "integer";
}
template <> const char * getVarTypeName( const float& )
{
  return "float";
}
template <> const char * getVarTypeName( const double& )
{
  return "double";
}
template <> const char * getVarTypeName( const bool& )
{
  return "boolean";
}
template <> const char * getVarTypeName( const std::string& )
{
  return "string";
}

template<class F>
struct function_traits;
 
// function pointer
template<class R, typename... Args>
struct function_traits<std::function<R(*)(Args...)>> : public function_traits<std::function<R(Args...)>>
{};
 
template<class R, class... Args>
struct function_traits<std::function<R(Args...)> >
{
  using return_type = R;

  static constexpr std::size_t arity = sizeof...(Args);

  template <std::size_t N>
  struct argument
  {
    static_assert(N < arity, "error: invalid parameter index.");
    using type = typename std::tuple_element<N,std::tuple<Args...>>::type;
  };
};

// member function pointer
template<class C, class R, class... Args>
struct function_traits<std::function<R(C::*)(Args...)>> : public function_traits<R(C&,Args...)>
{};

// const member function pointer
template<class C, class R, class... Args>
struct function_traits<std::function<R(C::*)(Args...)> const> : public function_traits<R(C&,Args...)>
{};

template<class C, class R, class... Args>
struct function_traits<R(C&,Args...)>
{
  using return_type = R;
  using class_type = C;

  static constexpr std::size_t arity = sizeof...(Args);

  template <std::size_t N>
  struct argument
  {
    static_assert(N < arity, "error: invalid parameter index.");
    using type = typename std::tuple_element<N,std::tuple<Args...>>::type;
  };
};

template <size_t ArgIdx, typename R, typename... Args>
auto get_arg_types( std::vector<int> & atlist ) 
    -> typename std::enable_if<ArgIdx == 1>::type
{
  using fun = function_traits<std::function<R(Args...)>>; 
  atlist.push_back(pyconnect::pyconnect_type<typename std::decay<typename fun::template argument<0>::type>::type>::value);
}

template <size_t ArgIdx, typename R, typename... Args>
auto get_arg_types( std::vector<int> & atlist ) 
    -> typename std::enable_if<(ArgIdx != 1)>::type
{
  using fun = function_traits<std::function<R(Args...)>>;
  atlist.push_back(pyconnect::pyconnect_type<typename std::decay<typename fun::template argument<ArgIdx-1>::type>::type>::value);

  get_arg_types<ArgIdx - 1, R, Args...>(atlist);
}

template <typename F, typename std::enable_if<(F::arity > 0), int>::type = 0, std::size_t... Is>
void get_args_type_list(std::index_sequence<Is...>, std::vector<int> & rtl)
{
  get_arg_types<F::arity, typename F::return_type, typename F::template argument<Is>::type...>(rtl);
}

template <typename F, typename std::enable_if<(F::arity == 0), int>::type = 0, std::size_t... Is>
void get_args_type_list(std::index_sequence<Is...>, std::vector<int> & rtl)
{
}

#endif

template <typename DataType> static int packToLENumber( const DataType & v, unsigned char * & dataPtr )
{
  // TODO: optimise this operation
  int dataLength = sizeof( DataType );
#ifdef WITH_BIG_ENDIAN
  char * valPtr = (char *)&v;
  for (int i = dataLength-1;i >= 0;i--) {
//  for (int i = 0;i < dataLength;i++) {
//    *dataPtr = (char)( (v >> (8*(dataLength - i - 1))) & 0xff );
    *dataPtr = (*(valPtr+i) & 0xFF);
    dataPtr++;
  }
#else
  memcpy( dataPtr, (char *)&v, dataLength );
  dataPtr += dataLength;
#endif
  return dataLength;
}

template <typename DataType> static void unpackLENumber( DataType & val, unsigned char * & dataPtr, int & remainingLength )
{
  // TODO: optimise this operation
  int dataLength = sizeof( DataType );
#ifdef WITH_BIG_ENDIAN
  // convert to Big Endian
  char * valPtr = (char *)&val;
  for (int i = dataLength-1;i >= 0;i--) {
    *(valPtr+i) = (*dataPtr & 0xFF);
    dataPtr++;
  }
#else
  memcpy( (char *)&val, dataPtr, dataLength );
  dataPtr += dataLength;
#endif
  remainingLength -= dataLength;
}

void packString( unsigned char * str, int length, unsigned char * & dataBufPtr,
        bool extendSize = false );
std::string unpackString( unsigned char * & dataBufPtr, int & remainingBytes,
               bool extendSize = false );
int unpackStrToInt( unsigned char * & str, int & remainingBytes );
int packIntToStr( int num, unsigned char * & str );
int packedIntLen( int num );

#ifdef __cplusplus
extern "C" {
#endif
  unsigned char * decodeBase64( char * input, size_t * outLen );
  char * encodeBase64( const unsigned char * input, size_t length );
  void endecryptInit();
  void endecryptFini();
  int decryptMessage( const unsigned char * origMesg, int origMesgLength, unsigned char ** decryptedMesg, int * decryptedMesgLength );
  int encryptMessage( const unsigned char * origMesg, int origMesgLength, unsigned char ** encryptedMesg, int * encryptedMesgLength );
  int secureSHA256Hash( const unsigned char * password, const int pwlen, unsigned char * code );
  
#ifdef __cplusplus
}
#endif
  
} // namespace pyconnect

#endif  // PyConnectCommon_h_DEFINED
