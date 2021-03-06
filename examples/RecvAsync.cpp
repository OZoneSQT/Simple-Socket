/*

MIT License

Copyright (c) 2018 Chris McArthur, prince.chrismc(at)gmail(dot)com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include "PassiveSocket.h"   // Include header for both passive and active socket object definition

#include <future>
#include <string>
#include <utility>

using namespace std::chrono_literals;

static constexpr const int32_t NEXT_BYTE = 1;
static constexpr const char* TEST_PACKET = "Test Packet";
static constexpr const char* LOCAL_HOST = "127.0.0.1";

// ---------------------------------------------------------------------------------------------
// Message Utility Code
// ---------------------------------------------------------------------------------------------
class AsyncMessage final
{
   friend class AsyncMessageBuilder;

public:
   AsyncMessage( const std::string& sMessage ) : m_sMessage( std::to_string( sMessage.size() ) + "\n" + sMessage ) {}
   AsyncMessage( const AsyncMessage& oNewMessage )  = default;
   AsyncMessage( AsyncMessage&& oNewMessage ) noexcept { std::swap( m_sMessage, oNewMessage.m_sMessage ); }
   ~AsyncMessage() = default;

   AsyncMessage& operator=( const AsyncMessage& oNewMessage ) = default;
   AsyncMessage& operator=( AsyncMessage&& oNewMessage ) noexcept
   {
      std::swap( m_sMessage, oNewMessage.m_sMessage );
      return *this;
   }

   [[nodiscard]] constexpr const std::string& ToString() const { return m_sMessage; }

   [[nodiscard]] const uint8_t* GetWireFormat() const { return reinterpret_cast<const uint8_t*>( m_sMessage.c_str() ); }
   [[nodiscard]] size_t GetWireFormatSize() const { return m_sMessage.size(); }

private:
   std::string m_sMessage;
};

class AsyncMessageBuilder final
{
public:
   AsyncMessageBuilder() : m_oMessage( "" ), m_iExpectedSize( incomplete ) { m_oMessage.m_sMessage.clear(); }
   explicit AsyncMessageBuilder( AsyncMessage  oMessage ) : m_oMessage(std::move( oMessage )), m_iExpectedSize( incomplete )
   {
      _ParseMessage();
   }
   AsyncMessageBuilder( const AsyncMessageBuilder& ) = delete;
   AsyncMessageBuilder( const AsyncMessageBuilder&& ) = delete;
   ~AsyncMessageBuilder() = default;

   const AsyncMessageBuilder& operator=( const AsyncMessageBuilder& ) = delete;
   const AsyncMessageBuilder& operator=( const AsyncMessageBuilder&& ) = delete;

   static constexpr size_t incomplete = -1;

   [[nodiscard]] bool IsComplete() const { return m_iExpectedSize == m_oMessage.m_sMessage.length(); }

   size_t Append( const std::string& sData )
   {
      m_oMessage.m_sMessage += sData;
      if ( !IsComplete() ) _ParseMessage();
      return m_iExpectedSize;
   }

   const AsyncMessage&& ExtractMessage()
   {
      m_oMessage.m_sMessage = std::to_string( m_iExpectedSize ) + "\n" + m_oMessage.m_sMessage;
      m_iExpectedSize = incomplete;
      return std::move( m_oMessage );
   }

private:
   AsyncMessage m_oMessage;
   size_t m_iExpectedSize;

   void _ParseMessage()
   {
      const size_t iNewLineIndex = m_oMessage.m_sMessage.find_first_of( '\n' );
      if ( iNewLineIndex != std::string::npos )
      {
         m_iExpectedSize = std::stoull( m_oMessage.m_sMessage.substr( 0, iNewLineIndex ) );
         m_oMessage.m_sMessage = m_oMessage.m_sMessage.substr( iNewLineIndex + 1 );
      }
   }
};

int main()
{
   std::promise<void> oExitSignal;

   std::promise<uint16_t> oPortEvent;
   auto oPortRetval = oPortEvent.get_future();

   // ---------------------------------------------------------------------------------------------
   // Server Code
   // ---------------------------------------------------------------------------------------------
   auto oRetval = std::async( std::launch::async, [oExitEvent = oExitSignal.get_future(), &oPortEvent]() {
      CPassiveSocket oSocket;

      oSocket.SetNonblocking();          // Configure this socket to be non-blocking
      oSocket.Listen( LOCAL_HOST, 0 );   // Bind to local host on port any port

      oPortEvent.set_value( oSocket.GetServerPort() );

      while ( oExitEvent.wait_for( 10ms ) == std::future_status::timeout )
      {
         std::unique_ptr<CActiveSocket> pClient = nullptr;
         if ( ( pClient = oSocket.Accept() ) != nullptr )   // Wait for an incomming connection
         {
            pClient->SetNonblocking();   // Configure new client connection to be non-blocking

            AsyncMessageBuilder oBuilder;

            do
            {
               pClient->Receive( NEXT_BYTE * 3 );       // Receive next bytes of request from the client.
               oBuilder.Append( pClient->GetData() );   // Gather Message in a buffer
            } while ( !oBuilder.IsComplete() );

            AsyncMessage oEchoMessage( oBuilder.ExtractMessage() );
            pClient->Send(
                oEchoMessage.GetWireFormat(),
                oEchoMessage.GetWireFormatSize() );   // Send response to client and close connection to the client.
            pClient.reset();                          // Close socket since we have completed transmission
         }
      }
   } );

   const uint16_t nPort = oPortRetval.get();

   // ---------------------------------------------------------------------------------------------
   // Client Code
   // ---------------------------------------------------------------------------------------------
   CActiveSocket oClient;

   oClient.SetNonblocking();   // Configure this socket to be non-blocking

   if ( oClient.Open( LOCAL_HOST, nPort ) )   // Attempt connection to local server and reported port
   {
      AsyncMessage oMessage( TEST_PACKET );
      if ( oClient.Send( oMessage.GetWireFormat(), oMessage.GetWireFormatSize() ) )   // Send a message the server
      {
         oClient.Select();

         size_t iTotalBytes = 0;
         while ( iTotalBytes != oMessage.GetWireFormatSize() )
         {
            const int iBytesReceived = oClient.Receive( NEXT_BYTE );   // Receive one byte of response from the server.

            if ( iBytesReceived > 0 )
            {
               iTotalBytes += iBytesReceived;
               std::string sResult = oClient.GetData();
               printf( "received %d bytes: '%s'\n", iBytesReceived, sResult.c_str() );
            }
         }
      }
   }

   oExitSignal.set_value();
   oRetval.get();

   return 1;
}
