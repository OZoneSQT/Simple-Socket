/*---------------------------------------------------------------------------*/
/*                                                                           */
/* PassiveSocket.cpp - Passive Socket Implementation                         */
/*                                                                           */
/* Author : Mark Carrier (mark@carrierlabs.com)                              */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/* Copyright (c) 2007-2009 CarrierLabs, LLC.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * 4. The name "CarrierLabs" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    mark@carrierlabs.com.
 *
 * THIS SOFTWARE IS PROVIDED BY MARK CARRIER ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL MARK CARRIER OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *----------------------------------------------------------------------------*/

#include "PassiveSocket.h"

CPassiveSocket::CPassiveSocket( CSocketType nType ) : CSimpleSocket( nType )
{
}

CPassiveSocket::~CPassiveSocket()
{
   CSimpleSocket::Close();
}

//------------------------------------------------------------------------------
//
// Listen() -
//
//------------------------------------------------------------------------------
bool CPassiveSocket::Listen( const char *pAddr, uint16 nPort, int32 nConnectionBacklog )
{
   bool           bRetVal = false;

#ifdef _LINUX
   int32          nReuse;
   nReuse = IPTOS_LOWDELAY;

   //--------------------------------------------------------------------------
   // Set the following socket option SO_REUSEADDR.  This will allow the file
   // descriptor to be reused immediately after the socket is closed instead
   // of setting in a TIMED_WAIT state.
   //--------------------------------------------------------------------------
   SETSOCKOPT( m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&nReuse, sizeof( int32 ) );
   SETSOCKOPT( m_socket, IPPROTO_TCP, IP_TOS, &nReuse, sizeof( int32 ) );
#endif

   memset( &m_stServerSockaddr, 0, sizeof( m_stServerSockaddr ) );
   m_stServerSockaddr.sin_family = AF_INET;
   m_stServerSockaddr.sin_port = htons( nPort );

   //--------------------------------------------------------------------------
   // If no IP Address (interface ethn) is supplied, then bind to all interface
   // else bind to specified interface.
   //--------------------------------------------------------------------------
   if( ( pAddr == NULL ) || ( !strlen( pAddr ) ) )
   {
      m_stServerSockaddr.sin_addr.s_addr = htonl( INADDR_ANY );
   }
   else
   {
      switch( inet_pton( m_nSocketDomain, pAddr, &m_stServerSockaddr.sin_addr ) )
      {
      case -1: TranslateSocketError();                 return false;
      case 0:  SetSocketError( SocketInvalidAddress ); return false;
      case 1:  break; // Success
      default:
         SetSocketError( SocketEunknown );
         return false;
      }
   }

   m_timer.Initialize();
   m_timer.SetStartTime();

   //--------------------------------------------------------------------------
   // Bind to the specified port
   //--------------------------------------------------------------------------
   if( bind( m_socket, ( struct sockaddr * )&m_stServerSockaddr, sizeof( m_stServerSockaddr ) ) != CSimpleSocket::SocketError )
   {
      if( m_nSocketType == CSimpleSocket::SocketTypeTcp )
      {
         if( listen( m_socket, nConnectionBacklog ) != CSimpleSocket::SocketError )
         {
            bRetVal = true;
         }
      }
      else
      {
         bRetVal = true;
      }
   }

   m_timer.SetEndTime();

   //--------------------------------------------------------------------------
   // If there was a socket error then close the socket to clean out the
   // connection in the backlog.
   //--------------------------------------------------------------------------
   TranslateSocketError();

   if( !bRetVal )
   {
      CSocketError err = GetSocketError();
      Close();
      SetSocketError( err );
   }
   else
   {
      socklen_t nSockLen = sizeof( struct sockaddr );

      memset( &m_stServerSockaddr, 0, nSockLen );
      getsockname( m_socket, ( struct sockaddr * )&m_stServerSockaddr, &nSockLen );
   }

   return bRetVal;
}

//------------------------------------------------------------------------------
//
// Accept() -
//
//------------------------------------------------------------------------------
std::unique_ptr<CActiveSocket> CPassiveSocket::Accept()
{
   uint32         nSockLen;
   std::unique_ptr<CActiveSocket> pClientSocket = nullptr;
   SOCKET         socket = CSimpleSocket::SocketError;

   if( m_nSocketType != CSimpleSocket::SocketTypeTcp )
   {
      SetSocketError( CSimpleSocket::SocketProtocolError );
      return pClientSocket;
   }

   pClientSocket = std::make_unique<CActiveSocket>();

   //--------------------------------------------------------------------------
   // Wait for incoming connection.
   //--------------------------------------------------------------------------
   if( pClientSocket != nullptr )
   {
      CSocketError socketErrno = SocketSuccess;

      m_timer.Initialize();
      m_timer.SetStartTime();

      nSockLen = sizeof( m_stClientSockaddr );

      do
      {
         errno = 0;
         socket = accept( m_socket, ( struct sockaddr * )&m_stClientSockaddr, (socklen_t *)&nSockLen );

         if( socket != INVALID_SOCKET )
         {
            pClientSocket->SetSocketHandle( socket );
            pClientSocket->TranslateSocketError();
            socketErrno = pClientSocket->GetSocketError();
            socklen_t nSockLen = sizeof( struct sockaddr );

            //-------------------------------------------------------------
            // Store client and server IP and port information for this
            // connection.
            //-------------------------------------------------------------
            getpeername( m_socket, ( struct sockaddr * )&pClientSocket->m_stClientSockaddr, &nSockLen );
            memcpy( (void *)&pClientSocket->m_stClientSockaddr, (void *)&m_stClientSockaddr, nSockLen );

            memset( &pClientSocket->m_stServerSockaddr, 0, nSockLen );
            getsockname( m_socket, ( struct sockaddr * )&pClientSocket->m_stServerSockaddr, &nSockLen );
         }
         else
         {
            TranslateSocketError();
            socketErrno = GetSocketError();
         }

      } while( socketErrno == CSimpleSocket::SocketInterrupted );

      m_timer.SetEndTime();

      if( socketErrno != CSimpleSocket::SocketSuccess )
      {
         pClientSocket.reset( nullptr );
      }
   }

   return pClientSocket;
}
