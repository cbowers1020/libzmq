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

/*
    //  Underlying socket.
    fd_t _s;

    //  Handle corresponding to the listening socket.
    handle_t _handle;

    //  Socket the listener belongs to.
    zmq::socket_base_t *_socket;

    // String representation of endpoint to bind to
    std::string _endpoint;
*/

#include "precompiled.hpp"
#include <new>

#if defined ZMQ_HAVE_NORM

#include <string>
#include <stdio.h>

#include "norm_listener.hpp"
#include "io_thread.hpp"

zmq::norm_listener_t::norm_listener_t (io_thread_t *io_thread_,
                                       socket_base_t *socket_,
                                       const options_t &options_) :
    stream_listener_base_t (io_thread_, socket_, options_)
{
}


NormSocketHandle zmq::norm_listener_t::FindClientSocket(ClientMap& clientMap, const ClientInfo& clientInfo)
{
    Client* client = FindClient(clientMap, clientInfo);
    if (NULL == client)
        return NORM_SOCKET_INVALID;
    else
        return client->GetSocket();
}  // end FindClientSocket()


void zmq::norm_listener_t::in_event ()
{
  NormSocketEvent event;
  if (!NormGetSocketEvent(_norm_instance, &event)) {
    // NORM has died before we unplugged?!
    zmq_assert (false);
    return;
  }

  ClientInfo clientInfo;
  if (NORM_NODE_INVALID != event.sender) {
    clientInfo = NormGetClientInfo (event.sender);
  }
  else {
    clientInfo = NormGetSocketInfo (event.socket);
  }
  switch (event.type) {
    case NORM_SOCKET_ACCEPT:
    {
      if (event.socket == _norm_server_socket) {
        if (NORM_SOCKET_INVALID != FindClientSocket(_clientMap, clientInfo))
        {
          // We think we're already connected to this client
          // fprintf(stderr, "normServer: duplicative %s from client %s/%hu...\n",
          //         (NORM_REMOTE_SENDER_NEW == event.event.type) ? "new" : "reset",
          //         clientInfo.GetAddressString(), clientInfo.GetPort());
          continue;
        }
        NormSocketHandle clientSocket = NormAccept (_norm_server_socket, event.sender);

        if (clientSocket == retired_fd) {
          _socket->event_accept_failed (
            make_unconnected_bind_endpoint_pair (_endpoint), zmq_errno ());
          return;
        }

        Client* client = new Client (clientSocket);
        _clientMap[clientInfo] = client;

        client->SetWriteReady (true);
        _clientCount++;

        //  Create the engine object for this connection.
        create_engine (clientSocket);
      }
      else {
        // Should never get here, I think
      }
      break;
    }
    // case NORM_SOCKET_CONNECT:
    // {
    //   Client* client = FindClient (_clientMap, clientInfo);
    //   zmq_assert (NULL != client);
    // }
  }
}


/*
* This returns the address as a string, I believe. ref: src/address.hpp line 147ish
* Inherited from stream_listener_base
*/
std::string
zmq::norm_listener_t::get_socket_name (zmq::fd_t fd_,
                                       socket_end_t socket_end_) const
{
  // _norm_server_socket
  return zmq::get_socket_name<norm_address_t> (fd_, socket_end_); // ?
}

int zmq::norm_listener_t::create_socket (const char *addr_) const
{
  // Parse the "addr_" address int "iface", "addr", and "port"
  // norm endpoint format: [id,][<iface>;]<addr>:<port>
  // First, look for optional local NormNodeId
  // (default NORM_NODE_ANY causes NORM to use host IP addr for NormNodeId)
  _serverId = NORM_NODE_ANY;
  const char *ifacePtr = strchr (addr_, ',');
  if (NULL != ifacePtr) {
      size_t idLen = ifacePtr - addr_;
      if (idLen > 31)
          idLen = 31;
      char idText[32];
      strncpy (idText, addr_, idLen);
      idText[idLen] = '\0';
      _serverId = (NormNodeId) atoi (idText);
      ifacePtr++;
  } else {
      ifacePtr = addr_;
  }

  // Second, look for optional multicast ifaceName
  char ifaceName[256];
  const char *addrPtr = strchr (ifacePtr, ';');
  if (NULL != addrPtr) {
      size_t ifaceLen = addrPtr - ifacePtr;
      if (ifaceLen > 255)
          ifaceLen = 255; // return error instead?
      strncpy (ifaceName, ifacePtr, ifaceLen);
      ifaceName[ifaceLen] = '\0';
      ifacePtr = ifaceName;
      addrPtr++;
  } else {
      addrPtr = ifacePtr;
      ifacePtr = NULL;
  }

  // Finally, parse IP address and port number
  const char *portPtr = strrchr (addrPtr, ':');
  if (NULL == portPtr) {
      errno = EINVAL;
      return -1;
  }

  char serverAddr[256];
  size_t addrLen = portPtr - addrPtr;
  if (addrLen > 255)
      addrLen = 255;
  strncpy (serverAddr, addrPtr, addrLen);
  serverAddr[addrLen] = '\0';
  portPtr++;
  _serverPort = atoi (portPtr);

  _address = serverAddr;

  if (NORM_INSTANCE_INVALID == _norm_instance) {
        if (NORM_INSTANCE_INVALID == (_norm_instance = NormCreateInstance ())) {
            // errno set by whatever caused NormCreateInstance() to fail
            return -1;
        }
    }

  if (NORM_SOCKET_INVALID == _norm_server_socket) {
        if (NORM_SOCKET_INVALID == (_norm_server_socket = NormOpen(_norm_instance))) {
            // errno set by whatever cased NormOpen () to fail
            return -1;
        }
    }
  _norm_session = _norm_server_socket.GetSession();
  _clientCount = 0;

  // Emulating tcp_listener.cpp
  // //  Underlying socket _s;
  _s = NormGetDescriptor(_norm_instance);
  if (_s == retired_fd) {
      return -1;
  }

  //  TODO why is this only done for the listener?
  make_socket_noninheritable (_s);

  // The address should be unicast?
  if (NormIsUnicastAddress (serverAddr) || options.norm_unicast_nack) {
    NormSetDefaultUnicastNack (norm_session, true);
  }

  // NormListen(NormSocketHandle normSocket, UINT16 serverPort, const char* groupAddr, const char* serverAddr)
  if (!NormListen(_norm_server_socket, _serverPort, NULL, serverAddr)) {
    int savedErrno = errno;
    NormDestroyInstance (norm_instance);
    _norm_server_socket = NORM_SOCKET_INVALID;
    _norm_session = NORM_SESSION_INVALID;
    _norm_instance = NORM_INSTANCE_INVALID;
    errno = savedErrno;
    return -1;
  }

  return 0;
}

int zmq::norm_listener_t::set_local_address (const char *addr_)
{
  if (options.use_fd != -1) {
        //  in this case, the addr_ passed is not used and ignored, since the
        //  socket was already created by the application
        _s = options.use_fd;
    } else {
        if (create_socket (addr_) == -1)
            return -1;
    }

  _endpoint = get_socket_name(_s, socket_end_local)

  // Zmq socket
  _socket->event_listening (make_unconnected_bind_endpoint_pair (_endpoint), _s);

  return 0;
}

// Our "server" indexes clients by their source addr/port

zmq::norm_listener_t::ClientInfo::ClientInfo(uint8_t addrVersion, const char* clientAddr, uint16_t clientPort)
 : addr_version(addrVersion), client_port(clientPort)
{
    if (NULL == clientAddr) addrVersion = 0; // forces zero initialization
    switch (addrVersion)
    {
        case 4:
            memcpy(client_addr, clientAddr, 4);
            memset(client_addr+4, 0, 12);
            break;
        case 6:
            memcpy(client_addr, clientAddr, 16);
            break;
        default:
            memset(client_addr, 0, 16);
            break;
    }
}

// returns "true" if "this" less than "a" (used by C++ map)
bool zmq::norm_listener_t::ClientInfo::operator <(const ClientInfo& a) const
{
    if (addr_version != a.addr_version)
        return (addr_version < a.addr_version);
    else if (client_port != a.client_port)
        return (client_port < a.client_port);
    else if (4 == addr_version)
        return (0 > memcmp(client_addr, a.client_addr, 4));
    else
        return (0 > memcmp(client_addr, a.client_addr, 16));
}  // end ClientInfo::operator <()

int zmq::norm_listener_t::ClientInfo::GetAddressFamily() const
{
    if (4 == addr_version)
        return AF_INET;
    else
        return AF_INET6;
}  // end ClientInfo::GetAddressFamily()


const char* zmq::norm_listener_t::ClientInfo::GetAddressString() 
{
    static char text[64];
    text[63] = '\0';    
    int addrFamily;
    if (4 == addr_version)
        addrFamily = AF_INET;
    else
        addrFamily = AF_INET6;
    inet_ntop(addrFamily, client_addr, text, 63);
    return text;
}  // end ClientInfo::GetAddressString() 

void zmq::norm_listener_t::ClientInfo::Print(FILE* filePtr)
{
    char text[64];
    text[63] = '\0';    
    int addrFamily;
    if (4 == addr_version)
        addrFamily = AF_INET;
    else
        addrFamily = AF_INET6;
    inet_ntop(addrFamily, client_addr, text, 63);
    fprintf(filePtr, "%s/%hu", text, client_port);
}  // end ClientInfo::Print()

zmq::norm_listener_t::Client::Client(NormSocketHandle clientSocket)
 : client_socket(clientSocket), 
   write_ready(true), bytes_written(0)
{
}

zmq::norm_listener_t::Client::~Client()
{
}

zmq::norm_listener_t::ClientInfo zmq::norm_listener_t::NormGetClientInfo(NormNodeHandle client)
{
    char addr[16]; // big enough for IPv6
    unsigned int addrLen = 16;
    UINT16 port;
    NormNodeGetAddress(client, addr, &addrLen, &port);
    UINT8 version;
    if (4 == addrLen)
        version = 4;
    else
        version = 6;
    return ClientInfo(version, addr, port);
}  // end NormGetClientInfo(NormNodeHandle)

zmq::norm_listener_t::ClientInfo zmq::norm_listener_t::NormGetSocketInfo(NormSocketHandle socket)
{
    char addr[16]; // big enough for IPv6
    unsigned int addrLen = 16;
    UINT16 port;
    NormGetPeerName(socket, addr, &addrLen, &port);
    UINT8 version;
    if (4 == addrLen)
        version = 4;
    else
        version = 6;
    return ClientInfo(version, addr, port);
}  // end NormGetSocketInfo(NormSocketHandle)


zmq::norm_listener_t::Client* zmq::norm_listener_t::Client::FindClient(ClientMap& clientMap, const ClientInfo& clientInfo)
{
    ClientMap::iterator it = clientMap.find(clientInfo);
    if (clientMap.end() != it) 
        return it->second;
    else
        return NULL;
}  // end FindClient()

#endif // ZMQ_HAVE_NORM