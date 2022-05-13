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

#ifndef __ZMQ_NORM_CONNECTER_HPP_INCLUDED__
#define __ZMQ_NORM_CONNECTER_HPP_INCLUDED__

#if defined ZMQ_HAVE_NORM

#include "fd.hpp"
#include "stdint.hpp"
#include "stream_connecter_base.hpp"

#include <normApi.h>
#include <normSocket.h>

namespace zmq
{
class norm_connecter_t ZMQ_FINAL : public stream_connecter_base_t
{
  public:
    norm_connecter_t (zmq::io_thread_t *io_thread_,
                      zmq::session_base_t *session_,
                      const options_t &options_,
                      address_t *addr_,
                      bool delayed_start_);
    ~norm_connecter_t ();

  private:
    //  ID of the timer used to check the connect timeout, must be different from stream_connecter_base_t::reconnect_timer_id.
    enum
    {
        connect_timer_id = 2
    };

    // Handlers for incoming commands.
    void process_term (int linger_);

    // Handlers for i/o events.
    // What events does a norm client have to handle
    void out_event ();
    void timer_event (int id_);

    // Internal function to start the actual connection establishment.
    // Performs NormOpen
    void start_connecting ();

    // Internal function to add a connect timer
    void add_connect_timer ();

    //  Open NORM connecting socket. Returns -1 in case of error,
    //  0 if connect was successful immediately. Returns -1 with
    //  EAGAIN errno if async connect was launched.
    int open ();

    // Get file descriptor of newly created socket
    // Wrapper of NormConnect ()
    fd_t connect ();

    //  Tunes a connected socket.
    // Used to set options, rate, etc.
    bool tune_socket (fd_t fd_);

    bool norm_connect ();

    bool _connect_timer_started;

    // Address to connect tp
    // const char* _address;

    // Port to connect on
    uint16_t _serverPort;

    NormSocketHandle _norm_client_socket;
    NormSessionHandle _norm_session;
    NormInstanceHandle _norm_instance;
    NormNodeId _clientId;

    ZMQ_NON_COPYABLE_NOR_MOVABLE (norm_connecter_t)
};
}


#endif // ZMQ_HAVE_NORM

#endif // !__ZMQ_NORM_CONNECTER_HPP_INCLUDED__