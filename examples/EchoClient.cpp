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

#include "ActiveSocket.h"       // Include header for active socket object definition

static constexpr auto MAX_PACKET = 4096;
static constexpr auto TEST_PACKET = "Test Packet";

int main( int argc, char **argv )
{
   CActiveSocket client;

   client.Initialize(); // Initialize our socket object

   if( client.Open( "127.0.0.1", 6789 ) ) // Connect to echo server
   {
      if( client.Send( (uint8 *)TEST_PACKET, strlen( TEST_PACKET ) ) )
      {
         auto numBytes = 0;
         if( ( numBytes = client.Receive( MAX_PACKET ) ) > 0 )
         {
            printf( "received %d bytes: '%s'\n", numBytes, client.GetData().c_str() );
         }
         else
         {
            printf( "Connection disconnected...\n" );
         }
      }
   }

   client.Close();

   return 1;
}
