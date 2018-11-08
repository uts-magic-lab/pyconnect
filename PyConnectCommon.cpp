/*
 *  PyConnectCommon.cpp
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

#include <string.h>
#include "PyConnectCommon.h"

#ifdef OPENR_OBJECT
#include <OPENR/OObject.h>
#endif

#include <openssl/crypto.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <openssl/err.h>

#ifndef OPENR_OBJECT
#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
#endif

#define ENCRYPTION_KEY_LENGTH  32
#define PYCONNECT_MSG_ENDECRYPT_BUFFER_SIZE       10240
/* Use the following python code to generate random 32 char key and encode into base64
 *
 * import base64
 * import string
 * import random
 *
 * def id_generator(size=6, chars=string.ascii_uppercase + string.ascii_lowercase + string.digits):
 *     return ''.join(random.choice(chars) for _ in range(size))
 *
 * key = id_generator(32)
 * base64.b64encode( key )
 */
namespace pyconnect {

static const unsigned char encrypt_key_text[] = "Mk80Z2J4TXJ1N0x4Q3RsQXhQNlB0d1NNWWNjd3Nzdjk=";
static const unsigned char encrypt_iv[] = "HrWOZK1H"; // must be 8 bytes
static unsigned char * encrypt_key;
static unsigned char * encryptbuffer = NULL;
static unsigned char * decryptbuffer = NULL;

#ifndef OPENR_OBJECT
#ifdef WIN32
static CRITICAL_SECTION t_criticalSection_1_;
static CRITICAL_SECTION t_criticalSection_2_;
#else
static pthread_mutex_t t_mutex_1;
static pthread_mutex_t t_mutex_2;
static pthread_mutexattr_t t_mta;
#endif
#endif

// helper functions
/* NOTE: encodeBase64 and decodeBase64 are not thread safe!!!! */
unsigned char * decodeBase64( char * input, size_t * outLen )
{
  BIO * bmem, * b64;

  int inLen = (int)strlen( input );
  int maxOutLen=(inLen * 6 + 7) / 8;
  unsigned char * buf = (unsigned char *)malloc( maxOutLen );
  if (buf) {
    memset( buf, 0, maxOutLen );

    b64 = BIO_new( BIO_f_base64() );
    if (b64) {
      BIO_set_flags( b64, BIO_FLAGS_BASE64_NO_NL );
      bmem = BIO_new_mem_buf( (char *) input, inLen );
      b64 = BIO_push( b64, bmem );
      *outLen = BIO_read( b64, buf, maxOutLen );
      BIO_free_all( b64 );
    }
  }
  return buf;
}

char * encodeBase64( const unsigned char * input, size_t length )
{
  BIO * bmem, * b64;
  BUF_MEM * bptr;
  char * buf = NULL;

  b64 = BIO_new( BIO_f_base64() );
  if (b64) {
    BIO_set_flags( b64, BIO_FLAGS_BASE64_NO_NL );
    bmem = BIO_new( BIO_s_mem() );
    b64 = BIO_push( b64, bmem );
    BIO_write( b64, input, (int)length );
    BIO_flush( b64 );
    BIO_get_mem_ptr( b64, &bptr );

    buf = (char *)malloc( bptr->length + 1);
    memcpy( buf, bptr->data, bptr->length );
    buf[bptr->length] = 0;
    BIO_free_all( b64 );
  }
  return buf;
}

void endecryptInit()
{
  INFO_MSG( "Communication encryption enabled.\n" );

#ifndef OPENR_OBJECT
#ifdef WIN32
  InitializeCriticalSection( &t_criticalSection_1_ );
  InitializeCriticalSection( &t_criticalSection_2_ );
#else
  pthread_mutexattr_init( &t_mta );
  pthread_mutexattr_settype( &t_mta, PTHREAD_MUTEX_RECURSIVE );
  pthread_mutex_init( &t_mutex_1, &t_mta );
  pthread_mutex_init( &t_mutex_2, &t_mta );
#endif
#endif

  size_t keyLen = 0;
  if (encrypt_key) {
    free( encrypt_key );
  }
  encrypt_key = decodeBase64( (char *)encrypt_key_text, &keyLen );

  if (keyLen != ENCRYPTION_KEY_LENGTH) {
    ERROR_MSG( "Encryption key decode error.\n" );
  }

  if (!encryptbuffer) {
    encryptbuffer = (unsigned char *)malloc( PYCONNECT_MSG_ENDECRYPT_BUFFER_SIZE );
  }
  if (!decryptbuffer) {
    decryptbuffer = (unsigned char *)malloc( PYCONNECT_MSG_ENDECRYPT_BUFFER_SIZE );
  }
}

void endecryptFini()
{
  if (encryptbuffer) {
    free( encryptbuffer );
    encryptbuffer = NULL;
  }
  if (decryptbuffer) {
    free( decryptbuffer );
    decryptbuffer = NULL;
  }
  if (encrypt_key) {
    free( encrypt_key );
    encrypt_key = NULL;
  }
#ifndef OPENR_OBJECT
#ifdef WIN32
  DeleteCriticalSection( &t_criticalSection_1_ );
  DeleteCriticalSection( &t_criticalSection_2_ );
#else
  pthread_mutex_destroy( &t_mutex_1 );
  pthread_mutex_destroy( &t_mutex_2 );
  pthread_mutexattr_destroy( &t_mta );
#endif
#endif
}

int decryptMessage( const unsigned char * origMesg, int origMesgLength, unsigned char ** decryptedMesg, int * decryptedMesgLength )
{
#ifndef OPENR_OBJECT
#ifdef WIN32
  EnterCriticalSection( &t_criticalSection_1_ );
#else
  pthread_mutex_lock( &t_mutex_1 );
#endif
#endif
  int oLen = 0, tLen = 0;
#if OPENSSL_VERSION_NUMBER < 0x11000000L
  EVP_CIPHER_CTX ectx;
  EVP_CIPHER_CTX * ctx = &ectx;
#else
  EVP_CIPHER_CTX * ctx = EVP_CIPHER_CTX_new();
#endif
  EVP_CIPHER_CTX_init( ctx );
  EVP_DecryptInit( ctx, EVP_bf_cbc(), encrypt_key, encrypt_iv );

  memset( decryptbuffer, 0, PYCONNECT_MSG_ENDECRYPT_BUFFER_SIZE );

  if (EVP_DecryptUpdate( ctx, decryptbuffer, &oLen, origMesg, origMesgLength ) != 1) {
    //ERROR_MSG( "EVP_DecryptUpdate failed.\n" );
#if OPENSSL_VERSION_NUMBER < 0x11000000L
    EVP_CIPHER_CTX_cleanup( ctx );
#else
    EVP_CIPHER_CTX_free( ctx );
#endif
#ifndef OPENR_OBJECT
#ifdef WIN32
    LeaveCriticalSection( &t_criticalSection_1_ );
#else
    pthread_mutex_unlock( &t_mutex_1 );
#endif
#endif
    return 0;
  }
  if (EVP_DecryptFinal( ctx, decryptbuffer+oLen, &tLen ) != 1) {
    //ERROR_MSG( "EVP_DecryptFinal failed.\n" );
#if OPENSSL_VERSION_NUMBER < 0x11000000L
    EVP_CIPHER_CTX_cleanup( ctx );
#else
    EVP_CIPHER_CTX_free( ctx );
#endif
#ifndef OPENR_OBJECT
#ifdef WIN32
    LeaveCriticalSection( &t_criticalSection_1_ );
#else
    pthread_mutex_unlock( &t_mutex_1 );
#endif
#endif
    return 0;
  }

  *decryptedMesgLength = oLen + tLen;
  *decryptedMesg = decryptbuffer;
#if OPENSSL_VERSION_NUMBER < 0x11000000L
  EVP_CIPHER_CTX_cleanup( ctx );
#else
  EVP_CIPHER_CTX_free( ctx );
#endif

#ifndef OPENR_OBJECT
#ifdef WIN32
  LeaveCriticalSection( &t_criticalSection_1_ );
#else
  pthread_mutex_unlock( &t_mutex_1 );
#endif
#endif
  return 1;
}

int encryptMessage( const unsigned char * origMesg, int origMesgLength, unsigned char ** encryptedMesg, int * encryptedMesgLength )
{
#ifndef OPENR_OBJECT
#ifdef WIN32
  EnterCriticalSection( &t_criticalSection_2_ );
#else
  pthread_mutex_lock( &t_mutex_2 );
#endif
#endif
  int oLen = 0, tLen = 0;

#if OPENSSL_VERSION_NUMBER < 0x11000000L
  EVP_CIPHER_CTX ectx;
  EVP_CIPHER_CTX * ctx = &ectx;
#else
  EVP_CIPHER_CTX * ctx = EVP_CIPHER_CTX_new();
#endif
  EVP_CIPHER_CTX_init( ctx );
  EVP_EncryptInit( ctx, EVP_bf_cbc(), encrypt_key, encrypt_iv );

  memset( encryptbuffer, 0, PYCONNECT_MSG_ENDECRYPT_BUFFER_SIZE );

  if (EVP_EncryptUpdate( ctx, encryptbuffer, &oLen, origMesg, origMesgLength ) != 1) {
    //ERROR_MSG( "EVP_EncryptUpdate failed.\n" );
#if OPENSSL_VERSION_NUMBER < 0x11000000L
    EVP_CIPHER_CTX_cleanup( ctx );
#else
    EVP_CIPHER_CTX_free( ctx );
#endif
#ifndef OPENR_OBJECT
#ifdef WIN32
    LeaveCriticalSection( &t_criticalSection_2_ );
#else
    pthread_mutex_unlock( &t_mutex_2 );
#endif
#endif
    return 0;
  }

  if (EVP_EncryptFinal( ctx, encryptbuffer+oLen, &tLen ) != 1) {
    //ERROR_MSG( "EVP_EncryptFinal failed.\n" );
#if OPENSSL_VERSION_NUMBER < 0x11000000L
    EVP_CIPHER_CTX_cleanup( ctx );
#else
    EVP_CIPHER_CTX_free( ctx );
#endif
#ifndef OPENR_OBJECT
#ifdef WIN32
    LeaveCriticalSection( &t_criticalSection_2_ );
#else
    pthread_mutex_unlock( &t_mutex_2 );
#endif
#endif
    return 0;
  }

  *encryptedMesgLength = oLen + tLen;
  *encryptedMesg = encryptbuffer;
#if OPENSSL_VERSION_NUMBER < 0x11000000L
  EVP_CIPHER_CTX_cleanup( ctx );
#else
  EVP_CIPHER_CTX_free( ctx );
#endif
#ifndef OPENR_OBJECT
#ifdef WIN32
  LeaveCriticalSection( &t_criticalSection_2_ );
#else
  pthread_mutex_unlock( &t_mutex_2 );
#endif
#endif
  return 1;
}
} // namespace pyconnect

namespace pyconnect {

int unpackStrToInt( unsigned char * & str, int & remainingBytes )
{
  int num;

  if (str[0] & 0x80) {  // means has extended byte
    num = str[1] << 7 | (str[0] & 0x7f);
    str += 2;
    remainingBytes -= 2;
  }
  else {
    num = str[0];
    str++;
    remainingBytes--;
  }
  return num;
}

int packIntToStr( int num, unsigned char * & str )
{
  if (num < 0) {
    return 0;
  }
  else if (num < 128) {
    str[0] = (char) num;
    str++;
    return 1;
  }
  else if (num < 32768) {
    str[0] = (num & 0x7f) | 0x80;
    str[1] = (char)((num & 0x7f80) >> 7); // little endian
    str += 2;
    return 2;
  }
  else {
    return 0;
  }
}

int packedIntLen( int num )
{
  if (num >= 0 && num < 128)
    return 1;
  else if (num < 32768)
    return 2;
  else
    return 0;
}

void packString( unsigned char * str, int length, unsigned char * & dataBufPtr,
    bool extendSize )
{
  if (!dataBufPtr || !str) return;

  if (extendSize) {
    packIntToStr( length, dataBufPtr );
  }
  else {
    *dataBufPtr = (char) length;
    dataBufPtr++;
  }

  if (length > 0) {
    memcpy( dataBufPtr, str, length );
    dataBufPtr += length;
  }
}

std::string unpackString( unsigned char * & dataBufPtr, int & remainingBytes,
    bool extendSize )
{
  if (!dataBufPtr) return std::string( "" );

  int strLen = 0;
  char * str = NULL;

  if (extendSize) {
    strLen = unpackStrToInt( dataBufPtr, remainingBytes );
  }
  else {
    strLen = (int) *dataBufPtr;
    dataBufPtr++;
  }

  if (strLen == 0)
    return std::string( "" );

  str = new char[strLen+1];
  memcpy( str, dataBufPtr, strLen );
  str[strLen] = '\0';
  dataBufPtr += strLen;
  remainingBytes -= strLen;

  std::string retVal = std::string( str );
  delete [] str;

  return retVal;
}

PyConnectType::Type PyConnectType::typeName( const char * type )
{
  if (strstr( type, "string" ))
    return STRING;
  else if (!strcmp( "int", type ))
    return INT;
  else if (!strcmp( "float", type ))
    return FLOAT;
  else if (!strcmp( "double", type ))
    return DOUBLE;
  else if (!strcmp( "bool", type ))
    return BOOL;
  else if (!strcmp( "void", type ))
    return PyVOID;
  else
    return COMPOSITE;
}

std::string PyConnectType::typeName( Type type )
{
  switch (type) {
    case INT:
      return std::string( "integer" );
      break;
    case FLOAT:
      return std::string( "float" );
      break;
    case DOUBLE:
      return std::string( "double" );
      break;
    case STRING:
      return std::string( "string" );
      break;
    case BOOL:
      return std::string( "boolean" );
      break;
    case COMPOSITE:
      return std::string( "composite object" );
      break;
    default:
      return std::string( "unknown" );
  }
}

#ifdef PYTHON_SERVER
int PyConnectType::validateTypeAndSize( PyObject * obj, Type type )
{
  if (!obj) {
    return 0; //just in case we have a NULL pointer
  }

  switch (type) {
    case INT:
      if (PyInt_Check( obj ))
        return sizeof( int );
      break;
    case FLOAT:
      if (PyFloat_Check( obj ))
        return sizeof( float );
      break;
    case DOUBLE:
      if (PyFloat_Check( obj ))
        return sizeof( double );
      break;
    case STRING:
      if (PyString_Check( obj )) {
        int size = (int)PyString_Size( obj );
        return (size + packedIntLen( size ));
      }
      break;
    case BOOL:
      if (PyBool_Check( obj ))
        return 1;
      break;
    case COMPOSITE:
      return 0;  //TODO: not support yet
    default:
      return 0;
  }
  return 0;
}

PyObject * PyConnectType::defaultValue( Type type )
{
  switch (type) {
    case INT:
      return PyInt_FromLong( 0 );
      break;
    case FLOAT:
    case DOUBLE:
      return PyFloat_FromDouble( 0.0 );
      break;
    case STRING:
      return PyString_FromString( "" );
      break;
    case BOOL:
      Py_RETURN_FALSE;
      break;
    default:
      Py_RETURN_NONE;
  }
}

void PyConnectType::packToStr( PyObject * arg, Type type, unsigned char * & dataPtr )
{
  switch (type) {
    case INT:
    {
      int ret = (int) PyInt_AsLong( arg );
      packToLENumber( ret, dataPtr );
    }
      break;
    case FLOAT:
    {
      float ret = (float) PyFloat_AsDouble( arg );
      packToLENumber( ret, dataPtr );
    }
      break;
    case DOUBLE:
    {
      double ret = PyFloat_AsDouble( arg );
      packToLENumber( ret, dataPtr );
    }
      break;
    case STRING:
    {
      char * ret = PyString_AsString( arg );
      int len = (int)strlen( ret );
      packString( (unsigned char*)ret, len, dataPtr, true );
    }
      break;
    case BOOL:
      (arg == Py_True) ? *dataPtr = 1 : *dataPtr = 0;
      dataPtr++;
      break;
    default:
      return;
  }
}

PyObject * PyConnectType::unpackStr( Type type, unsigned char * & dataPtr, int & remainingLength )
{
  switch (type) {
    case INT:
    {
      int ret = 0;
      unpackLENumber( ret, dataPtr, remainingLength );
      return PyInt_FromLong( ret );
    }
      break;
    case FLOAT:
    {
      float ret = 0.0;
      unpackLENumber( ret, dataPtr, remainingLength );
      return PyFloat_FromDouble( ret );
    }
      break;
    case DOUBLE:
    {
      double ret = 0.0;
      unpackLENumber( ret, dataPtr, remainingLength );
      return PyFloat_FromDouble( ret );
    }
      break;
    case STRING:
    {
      std::string ret = unpackString( dataPtr, remainingLength, true );
      return PyString_FromString( ret.c_str() );
    }
      break;
    case BOOL:
    {
      bool ret = ((*dataPtr++ & 0xf) != 0);
      if (ret)
        Py_RETURN_TRUE;
      else
        Py_RETURN_FALSE;
    }
      break;
    default:
      Py_RETURN_NONE;
  }
}
#endif
} //namespace pyconnect
