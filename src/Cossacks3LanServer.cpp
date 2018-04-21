/*
	A cross-platform game server for the Cossacks 3 RTS game.

	Copyright (c) 2018 Ereb @ habrahabr.ru
	This source code is distributed under the MIT license.
	See LICENSE.MIT for details.

	Cossacks 3 RTS   (c) 1995-2018 GSC Game World
	Asio C++ Library (c) 2003-2018 Christopher M. Kohlhoff

	This software utilizes knowledge gained by the author through reverse
	engineering of the network protocol used by the game and the official
	server. The author is not associated with GSC Game World.
*/
#include "Precompiled.hpp"

#include "Server.hpp"

int main()
{
	std::cout << "Cossacks 3 LAN Server starting up...";
	try
	{
		//see asio examples for details about library usage
		//https://github.com/chriskohlhoff/asio/tree/master/asio/src/examples
		asio::io_service io_service;
		Server server( io_service );
		std::cout << " running on port " << port << std::endl;
		io_service.run();
	}
	catch ( std::exception& e )
	{
		std::cerr << "Exception in main(): " << e.what() << "\n";
	}

	return 0;
}
