/*
	Copyright (c) 2018 Ereb @ habrahabr.ru

	This source code is distributed under the MIT license.
	See LICENSE.MIT for details.
*/
#include "Precompiled.hpp"

#include "Session.hpp"

using namespace asio::ip;

/*
	Creates local Session buffer with max_packet_size bytes,
	obtains Asio socket, stores Lobby reference.
*/
Session::Session( tcp::socket socket, Lobby& lobby ) :
	m_buf   ( max_packet_size ),
	m_socket( std::move( socket ) ),
	m_lobby ( lobby )
{
	//store IP address as string for easier output
	m_client_address = m_socket.remote_endpoint().address().to_string();
}


/*
	Passes Client interface of own instance to Lobby, starts
	recursive packet reading.
*/
void Session::start()
{
	m_lobby.connect( shared_from_this() );
	do_read_header();
}


/*
	Pushes recieved shared pointer to local queue, starts recursive
	packet sending if necessary. The queue will ensure that the buffer
	will live until the async_write() completes.
*/
void Session::queue_buf( const BufPtr& buf )
{
#ifndef NDEBUG //display sent packets
	const auto b = buf.get()->at( 4 );
	std::cout << "     " << std::hex << std::setw( 2 ) << std::setfill( ' ' )
		<< (int) (unsigned char) buf.get()->at( 5 )
		<< (int) (unsigned char) buf.get()->at( 4 )
		<< " --> " << id() << std::endl;
#endif

	bool queue_is_empty = m_buf_queue.empty();
	m_buf_queue.push_back( buf );
	if ( queue_is_empty )
	{
		do_send_buf();
	}
}


/*
	Recursively iterates through queue and calls async_write() on every
	buffer. The queue element gets pop'ed after the write completes and
	allows the dynamically allocated buffer to be destroyed (if no other
	Session queue hold shared ownership to it).
*/
void Session::do_send_buf()
{
	auto self( shared_from_this() );
	const auto size = m_buf_queue.front()->size();
	asio::async_write( m_socket, asio::buffer( m_buf_queue.front().get()->data(), size ),
	[this, self, size]( asio::error_code ec, std::size_t bytes_sent )
	{
		if ( !ec )
		{
			if ( size != bytes_sent )
			{
				std::cerr << "[WARNING] Malformed packet sent to " << m_client_address << "\n";
			}
			m_buf_queue.pop_front();
			if ( !m_buf_queue.empty() )
			{
				do_send_buf();
			}
		}
		else
		{
			std::cerr << "[ERROR] Could not send packet to " << m_client_address << ": " << ec << "\n";
			m_lobby.disconnect( self );
		}
	} );
}


/*
	Recursively reads packet_header_size bytes, parses first int for data size,
	proceeds with do_read_body() if necessary. Enforces some sanity checks.
	Detects disconnection and reports to Lobby for notification purposes.
*/
void Session::do_read_header()
{
	auto self( shared_from_this() );
	asio::async_read( m_socket, asio::buffer( m_buf, packet_header_size ),
	[this, self]( asio::error_code ec, std::size_t bytes_read )
	{
		if ( !ec )
		{
			if ( packet_header_size > bytes_read )
			{
				std::cerr << "[WARNING] Malformed header recieved from " << m_client_address << "\n";
				do_read_header();
			}
			else
			{
				//get data size
				byte_int bi = {};
				for ( int i = 0; i < 4; ++i ) bi.b[i] = m_buf[i];
				const size_t data_size = bi.i;

				if ( 0 == data_size )
				{
					try
					{
						m_lobby.process_buf( self );
					}
					catch ( const std::out_of_range )
					{
						std::cerr << "[WARNING] Lobby::process_buf() -- ID map lookup failed\n";
					}
					do_read_header();
				}
				else if ( Session::max_packet_size - Session::packet_header_size < data_size )
				{
					std::cerr << "[ERROR] Announced packet body is too big (" << data_size << " bytes)\n";
					m_lobby.disconnect( self );
				}
				else
				{
					do_read_body( data_size );
				}
			}
		}
		else if ( asio::error::misc_errors::eof == ec )
		{
			m_lobby.disconnect( self );
		}
		else
		{
			std::cerr << "[ERROR] Could not read packet header from " << m_client_address  << ": " << ec << "\n";
			m_lobby.disconnect( self );
		}
	} );
}


/*
	Reads passed amount of bytes. Enforces some sanity checks.
	Detects disconnection and reports to Lobby for notification purposes.
*/
void Session::do_read_body( size_t data_size )
{
	auto self( shared_from_this() );
	asio::async_read( m_socket, asio::buffer( &m_buf[packet_header_size], data_size ),
	[this, self, data_size]( asio::error_code ec, std::size_t bytes_read )
	{
		if ( !ec )
		{
			if ( data_size != bytes_read )
			{
				std::cerr << "[WARNING] Malformed packet recieved from " << m_client_address << "\n";
			}
			else
			{
				try
				{
					m_lobby.process_buf( self );
				}
				catch ( const std::out_of_range )
				{
					std::cerr << "[WARNING] Lobby::process_buf() -- ID map lookup failed\n";
				}
			}
			do_read_header();
		}
		else if ( asio::error::misc_errors::eof == ec )
		{
			m_lobby.disconnect( self );
		}
		else
		{
			std::cerr << "[ERROR] Could not read packet body from " << m_client_address << ": " << ec << "\n";
			m_lobby.disconnect( self );
		}
	} );
}
