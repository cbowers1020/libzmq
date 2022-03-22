/*
    Copyright (c) 2007-2016 Contributors as noted in the AUTHORS file

    This file is part of libzmq, the ZeroMQ core engine in C++.

    libzmq is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License (LGPL) as published
    by the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    As a special exception, the Contributors give you permission to link
    this library with independent modules to produce an executable,
    regardless of the license terms of these independent modules, and to
    copy and distribute the resulting executable under terms of your choice,
    provided that you also meet, for each linked independent module, the
    terms and conditions of the license of that module. An independent
    module is a module which is not derived from or based on this library.
    If you modify this library, you must extend this exception to your
    version of the library.

    libzmq is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
    License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __ZMQ_NORM_LISTENER_HPP_INCLUDED__
#define __ZMQ_NORM_LISTENER_HPP_INCLUDED__

#if defined ZMQ_HAVE_NORM

#include "fd.hpp"
#include "norm_address.hpp"
#include "stream_listener_base.hpp"

#include <normApi.h>
#include <normSocket.h>

namespace zmq
{

class norm_listener_t ZMQ_FINAL : public stream_listener_base_t
{
  public:
    norm_listener_t (zmq::io_thread_t *io_thread_,
                     zmq::socket_base_t *socket_,
                     const options_t &options_);

    // Set Address to listen on
    int set_local_address (const char *addr_);

  protected:
    std::string get_socket_name (fd_t fd_, socket_end_t socket_end_) const;

  private:
    class ClientInfo
    {
        public:
            ClientInfo(uint8_t ipVersion = 0, const char* theAddr = NULL, uint16_t thePort = 0);
            bool operator < (const ClientInfo& a) const;
            
            int GetAddressFamily() const;
            const char* GetAddress() const
                {return client_addr;}
            UINT16 GetPort() const
                {return client_port;}
            
            const char* GetAddressString();
            void Print(FILE* filePtr);
            
        private:
            uint8_t               addr_version;    // 4 or 6
            char                client_addr[16]; // big enough for IPv6
            uint16_t              client_port;
            
    };  // end class ClientInfo

    class Client
    {
        public:
            Client(NormSocketHandle clientSocket);
            ~Client();
            
            NormSocketHandle GetSocket() const
                {return client_socket;}
            
            bool GetWriteReady() const
                {return write_ready;}
            void SetWriteReady(bool state)
                {write_ready = state;}
            
            unsigned int GetBytesWritten() const
                {return bytes_written;}
            void SetBytesWritten(unsigned long numBytes)
                {bytes_written = numBytes;}
                   
        private:
            NormSocketHandle   client_socket;
            // These are state variables for unicast server -> client communication
            bool                write_ready;
            unsigned int        bytes_written;
            
    };  // end class Client

    // Handlers for I/O events
    void in_event ();

    int create_socket (const char *addr_);

    // C++ map used to index client sessions by the client source addr/port
    typedef std::map<ClientInfo, Client*> ClientMap;
    ClientMap _clientMap;
    unsigned int _clientCount;

    ClientInfo NormGetClientInfo(NormNodeHandle client);
    ClientInfo NormGetSocketInfo(NormSocketHandle socket);
    NormSocketHandle FindClientSocket(ClientMap& clientMap, const ClientInfo& clientInfo);
    Client* FindClient(ClientMap& clientMap, const ClientInfo& clientInfo);

    // Address to listen on
    const char* _address;

    // Port to listen on
    uint16_t _serverPort;

    NormSocketHandle _norm_server_socket;
    NormSessionHandle _norm_session;
    NormInstanceHandle _norm_instance;
    NormNodeId _serverId;

    ZMQ_NON_COPYABLE_NOR_MOVABLE (norm_listener_t)
};
}

#endif // ZMQ_HAVE_NORM

#endif // !__ZMQ_NORM_LISTENER_HPP_INCLUDED__