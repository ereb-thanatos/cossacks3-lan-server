/*
	Copyright (c) 2018 Ereb @ habrahabr.ru

	This source code is distributed under the MIT license.
	See LICENSE.MIT for details.
*/
#pragma once
#include "Precompiled.hpp"

#include "Client.hpp"
#include "Lobby.hpp"

using namespace asio::ip;

/*
	Session class provides asynchronous reading and writing to/from a
	local buffer large enough for the biggest packets (map data on game
	start). It stores Client ID (assigned by Lobby on connection), and
	a local queue of shared pointers to send buffers. The pointers are
	shared with other targetet Clients and ensure that the buffer stays
	alive until every Session finishes the async_write() and pop's the
	pointer from it's local queue.
*/
class Session : public Client, public std::enable_shared_from_this<Session>
{
public:
	Session( tcp::socket socket, Lobby& lobby );

	void start();

	void set_id( unsigned int id ) { m_client_id = id; };

	unsigned int       id()      const { return m_client_id;      };
	const std::string& address() const { return m_client_address; };
	Buffer&            buf()           { return m_buf;            };

	void queue_buf( const BufPtr& buf );

	enum { max_packet_size    = 0x100000 };//1 MiB
	enum { packet_header_size = 14       };
	
private:
	void do_read_header();
	void do_read_body( size_t data_size );
	void do_send_buf();

	unsigned int m_client_id;
	std::string  m_client_address;
	tcp::socket  m_socket;
	Lobby&       m_lobby;
	Buffer       m_buf;

	std::deque<BufPtr> m_buf_queue;

	//necessary to read 1st int in header (data size)
	union byte_int
	{
		unsigned char b[4];
		unsigned int i;
	};
};
