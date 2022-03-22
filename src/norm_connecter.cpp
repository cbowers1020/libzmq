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

#include "norm_connecter.hpp"
#include "io_thread.hpp"


zmq::norm_connecter_t::norm_connecter_t (zmq::io_thread_t *io_thread_,
                                        zmq::session_base_t *session_,
                                        const options_t &options_,
                                        address_t *addr_,
                                        bool delayed_start_) :
    stream_connecter_base_t (
      io_thread_, session_, options_, addr_, delayed_start_),
    _connect_timer_started (false)
{
  zmq_assert (_addr->protocol == protocol_name::norm); 
}

zmq::norm_connecter_t::~norm_connecter_t ()
{
  zmq_assert (!_connect_timer_started);
}

void zmq::norm_connecter_t::process_term (int linger_)
{
  if (_connect_timer_started) {
    cancel_timer (connect_timer_id);
    _connect_timer_started = false;
  }
}

void zmq::tcp_connecter_t::out_event ()
{
  if (_connect_timer_started) {
    cancel_timer (connect_timer_id);
    _connect_timer_started = false;
  }

  rm_handle ();

  const fd_t fd = connect ();

  if (fd == retired_fd
      && ((options.reconnect_stop & ZMQ_RECONNECT_STOP_CONN_REFUSED)
          && errno == ECONNREFUSED)) {
      send_conn_failed (_session);
      close ();
      terminate ();
      return;
  }

  //  Handle the error condition by attempt to reconnect.
  if (fd == retired_fd || !tune_socket (fd)) {
      close ();
      add_reconnect_timer ();
      return;
  }

  create_engine (fd, get_socket_name<norm_address_t> (fd, socket_end_local));
}

void zmq::norm_connecter_t::timer_event (int id_)
{
    if (id_ == connect_timer_id) {
        _connect_timer_started = false;
        rm_handle ();
        close ();
        add_reconnect_timer ();
    } else
        stream_connecter_base_t::timer_event (id_);
}

void zmq::norm_connecter_t::start_connecting ()
{
  const int rc = open ();

  //  Connect may succeed in synchronous manner.
  if (rc == 0) {
      _handle = add_fd (_s);
      out_event ();
  }

  //  Connection establishment may be delayed. Poll for its completion.
  else if (rc == -1 && errno == EINPROGRESS) {
      _handle = add_fd (_s);
      set_pollout (_handle);
      _socket->event_connect_delayed (
        make_unconnected_connect_endpoint_pair (_endpoint), zmq_errno ());

      //  add userspace connect timeout
      add_connect_timer ();
  }

  //  Handle any other error condition by eventual reconnect.
  else {
      if (_s != retired_fd)
          close ();
      add_reconnect_timer ();
  }
}


void zmq::norm_connecter_t::add_connect_timer ()
{
  if (options.connect_timeout > 0) {
    add_timer (options.connect_timeout, connect_timer_id);
    _connect_timer_started = true;
  }
}

// Create Norm Instance, etc.
int zmq::norm_connecter_t::open ()
{
  zmq_assert (_s == retired_df);

  //  Resolve the address
  if (_addr->resolved.norm_addr != NULL) {
      LIBZMQ_DELETE (_addr->resolved.norm_addr);
  }
  _addr->resolved.norm_addr = new (std::nothrow) norm_address_t ();
  alloc_assert (_addr->resolved.norm_addr);

  _norm_instance = NormCreateInstance ();
  if (NORM_INSTANCE_INVALID == _norm_instance) {
    LIBZMQ_DELETE (_addr->resolved.norm_addr);
    return -1
  }
  _s = NormGetDescriptor (norm_instance);

  if (_s == retired_fd) {
    LIBZMQ_DELETE (_addr->resolved.norm_addr);
    return -1;
  }

  _norm_client_socket = NormOpen (instance);

  zmq_assert (_addr->resolved.norm_addr != NULL);

  // Set the socket to non-blocking mode so that we get async connect().
  unblock_socket (_s);

  const norm_address_t *const norm_addr = _addr->resolved.norm_addr;

  // Set a source address for conversations
  // Will come back to this
  // if (norm_addr->has_src_addr ()) {
  //   //  Allow reusing of the address, to connect to different servers
  //   //  using the same source port on the client.
  //   NormListen (_norm_client_socket, norm_addr)
  // }
  if(NormConnect (_norm_client_socket, norm_addr->addr (), norm_addr->get_port_number, 0, /*GroupPtrAddress??*/, norm_addr->get_local_id ()))
    return 0;

  if (errno == EINTR)
    errno = EINPROGRESS;

  return -1;
}

bool zmq::norm_connecter_t::connect ()
{
  int err - 0;

  socklen_t len = sizeof err;

  const int rc = getsockopt (_s, SOL_SOCKET, SO_ERROR,
                               reinterpret_cast<char *> (&err), &len);

  if (rc == -1)
    err = errno;

  if (err != 0) {
    errno = err;
    errno_assert (errno != ENOPROTOOPT && errno != ENOTSOCK
                      && errno != ENOBUFS);
    return retired_fd;
  }

  const fd_t result = _s;
  _s = retired_fd;
  return result;
}

bool zmq::norm_connecter_t::tune_socket (const fd_t fd_)
{
  // TBD
  return true;
}