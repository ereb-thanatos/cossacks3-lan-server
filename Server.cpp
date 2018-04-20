/*
	Copyright (c) 2018 Ereb @ habrahabr.ru

	This source code is distributed under the MIT license.
	See LICENSE.MIT for details.
*/
#include "Precompiled.hpp"

#include "Server.hpp"
#include "Session.hpp"

using namespace asio::ip;

/*
	Creates Asio TCP acceptor (default on port 31523), starts recursive
	asynchronous connection acceptor.
*/
Server::Server( asio::io_service& io_service ) :
	m_acceptor( io_service, tcp::endpoint( tcp::v4(), port ) ),
	m_socket  ( io_service )
{
	do_accept();
}


/*
	Recursive asynchronous connection acceptor. For details see
	https://github.com/chriskohlhoff/asio/tree/master/asio/src/examples
*/
void Server::do_accept()
{
	m_acceptor.async_accept( m_socket,
	[this]( std::error_code ec )
	{
		if ( !ec )
		{
			std::cout << "Client connected:    " << std::setfill(' ') << std::setw(15) << std::right
			          << m_socket.remote_endpoint().address().to_string() << std::endl;
			std::make_shared<Session>( std::move( m_socket ), m_lobby )->start();
		}
		else
		{
			std::cerr << "[ERROR] Could not accept connection: " << ec << "\n";
		}
		do_accept();
	} );
}
