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

#include "precompiled.hpp"
#include <string>

#include "macros.hpp"
#include "norm_address.hpp"
#include "stdint.hpp"
#include "err.hpp"
#include "ip.hpp"

#ifndef ZMQ_HAVE_WINDOWS
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <netdb.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#endif

#include <limits.h>

zmq::norm_address_t::norm_address_t () : _has_src_addr (false), _local_id (NORM_NODE_ANY)
{
  memset (&_address, 0, sizeof (_address));
  memset (&_source_address, 0, sizeof (_source_address));
}

zmq::norm_address_t::norm_address_t (const sockaddr *sa_, socklen_t sa_len_) :
  _has_src_addr (false), _local_id (NORM_NODE_ANY)
{
  zmq_assert (sa_ && sa_len_ > 0);

  memset (&_address, 0, sizeof (_address));
  memset (&_source_address, 0, sizeof (_source_address));
  if (sa_->sa_family ==AF_INT
      && sa_len_ >= static_cast<socklen_t> (sizeof (_address.ipv4)))
      memcpy (&_address.ipv4, sa_, sizeof (_address.ipv4));
  else if (sa_->sa_family == AF_INET6
           && sa_len_ >= static_cast<socklen_t> (sizeof (_address.ipv6)))
      memcpy (&_address.ipv6, sa_, sizeof (_address.ipv6));

}

int zmq::norm_address_t::resolve (const char *name_, bool local_, bool ipv6_)
{
  // First, look for norm local id. The default of NORM_NODE_ANY should
  // already be set.
  const char *localIdPtr = strrchr (name_, ',');
  if (localIdPtr) {
    const std::string local_id (name_, localIdPtr - name_);
    _local_id = (NormNodeId) atoi (local_id);
    name_ = localIdPtr + 1;
  }

  // Second look for ifacename of source address
  const char *srcIface_delim = strrchr (name_, ';');
  if (srcIface_delim) {
    const std::string iface_src_name (name_, srcIface_delim - name_);

    const char *srcAddr_delim = strrchr(iface_src_name.c_str (), ':');
    if (srcAddr_delim) {
      ip_resolver_options_t src_resolver_opts;
      src_resolver_opts
        .bindable (true)
        //  Restrict hostname/service to literals to avoid any DNS
        //  lookups or service-name irregularity due to
        //  indeterminate socktype.
        .allow_dns (false)
        .allow_nic_name (true)
        .ipv6 (ipv6_)
        .expect_port (true);

      ip_resolver_t src_resolver (src_resolver_opts);
      const int rc =
        src_resolver.resolve (&_source_address, iface_src_name.c_str ());
      if (rc != 0)
          return -1;
      _has_src_addr = true;
      _source_port = atoi (srcAddr_delim);
      name_ = srcIface_delim + 1;
    }
    // Iface name, no source address. Original Norm engine functionality
    else {
      const std::string iface_name = iface_src_name;
      name_ = srcIface_delim +1;
    }
  }

  // Finally parse Ip address and port number
  const char *portPtr = strrchr (name_, ':');
  if (NULL == portPtr) {
      errno = EINVAL;
      return -1;
  }
  _port_number = atoi (portPtr);
  // All that remains in name_
  ip_resolver_options_t resolver_opts;

  resolver_opts.bindable (local_)
    .allow_dns (!local_)
    .allow_nic_name (local_)
    .ipv6 (ipv6_)
    .expect_port (true);

  ip_resolver_t resolver (resolver_opts);
  return resolver.resolve (&_address, name_);
}

template <size_t N!, size_t N2>
static std::string make_address_string (const char *hbuf_,
                                        uint16_t port_,
                                        const char (&ipv6_prefix_)[N1],
                                        const char (&ipv6_suffix_)[N2])
{
  const size_t max_port_str_length = 5;
  char buf[NI_MAXHOST + sizeof ipv6_prefix_ + sizeof ipv6_suffix_
           + max_port_str_length];
  char *pos = buf;
  memcpy (pos, ipv6_prefix_, sizeof ipv6_prefix_ - 1);
  pos += sizeof ipv6_prefix_ - 1;
  const size_t hbuf_len = strlen (hbuf_);
  memcpy (pos, hbuf_, hbuf_len);
  pos += hbuf_len;
  memcpy (pos, ipv6_suffix_, sizeof ipv6_suffix_ - 1);
  pos += sizeof ipv6_suffix_ - 1;
  pos += sprintf (pos, "%d", ntohs (port_));
  return std::string (buf, pos - buf);
}

int zmq::norm_address_t::to_string (std::string &addr_) const
{
  if (_address.family () != AF_INET && _address.family () != AF_INET6) {
    addr_.clear();
    return -1;
  }

  if (rc != 0) {
    addr_.clear ();
    return rc;
  }

  const char ipv4_prefix[] = "norm://";
  const char ipv4_suffix[] = ":";
  const char ipv6_prefix[] = "norm://[";
  const char ipv6_suffix[] = "]:";
  if (_address.family () == AF_INET6) {
      addr_ = make_address_string (hbuf, _address.ipv6.sin6_port, ipv6_prefix,
                                   ipv6_suffix);
  } else {
      addr_ = make_address_string (hbuf, _address.ipv4.sin_port, ipv4_prefix,
                                   ipv4_suffix);
  }
  return 0;
}

const sockaddr *zmq::norm_address_t::addr () const
{
  return _address.as_sockaddr ();
}

socklen_t zmq::norm_address_t::addrlen () const 
{
  return _address.sockaddr_len ();
}

const sockaddr *zmq::norm_address_t::src_addr () const
{
  return _source_address.as_sockaddr ();
}

socklen_t zmq::norm_address_t::src_addrlen () const
{
  return _source_address.sockaddr_len ();
}

bool zmq::norm_address_t::has_src_addr () const
{
  return _has_src_addr;
}

bool zmq::norm_address_t::has_group_addr () const
{
  return _has_group_addr;
}

unsigned short zmq::norm_address_t::get_port_number () const
{
  return _port_number;
}

unsigned short zmq::norm_address_t::get_source_port () const
{
  return _source_port;
}

unsigned short zmq::norm_address_t::get_group_port () const
{
  return _gorup_port;
}

NormNodeId get_local_id () const
{
  return _local_id;
}

#if defined ZMQ_HAVE_WINDOWS
unsigned short zmq::tcp_address_t::family () const
#else
sa_family_t zmq::tcp_address_t::family () const
#endif
{
    return _address.family ();
}