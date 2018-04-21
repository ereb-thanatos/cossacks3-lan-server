/*
	Copyright (c) 2018 Ereb @ habrahabr.ru

	This source code is distributed under the MIT license.
	See LICENSE.MIT for details.
*/
#pragma once
#include "Precompiled.hpp"

#include "Session.hpp"

using namespace asio::ip;

//TCP port to listen on (default 31523)
const unsigned short port = 31523;

class Server
{
public:
	Server( asio::io_service& io_service );

private:
	void do_accept();

	tcp::acceptor m_acceptor;
	tcp::socket   m_socket;
	Lobby         m_lobby;
};
