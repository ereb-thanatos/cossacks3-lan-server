/*
	Copyright (c) 2018 Ereb @ habrahabr.ru

	This source code is distributed under the MIT license.
	See LICENSE.MIT for details.
*/
#pragma once
#include "Precompiled.hpp"

#include "Room.hpp"

/*
	Player class stores player data which must be presented to newcomers
	or on 0x192 requests. Also stores a Player <> Room link through simple
	pointer to ensure fast room lookup. Stores player status.

	Provides helper functions for joining and leaving rooms, which also take
	care of Room state.
*/
class Player
{
public:
	/*
		The Player ID is set by Lobby::connect as Client ID. Name is derived
		from Game Key input (see Lobby::process_buf(), 0x19a branch for
		explanation). Ver1 and ver2 strings also are sent by the client on
		login. State 0x01 means "in lobby". Use default properties string.
	*/
	Player( int id, std::string name, std::string ver1, std::string ver2 ) :
		m_id( id ), m_name( name ), m_ver1( ver1 ), m_ver2( ver2 ),
		m_status( 0x01 ), m_room( nullptr ),
		//m_score( "ps=1000|pw=0|pg=0" ),
		m_props( "pur|0|dlc|0|ram|4|sic|0|si1|0|si2|0|si3|0|snc||sn1||sn2||sn3|" )
	{};

	unsigned int      id() const { return m_id;     };
	unsigned char status() const { return m_status; };

	std::string name()  const { return m_name;  };
	std::string ver1()  const { return m_ver1;  };
	std::string ver2()  const { return m_ver2;  };
	//std::string score() const { return m_score; };
	std::string props() const { return m_props; };

	void set_status ( unsigned char s ) { m_status  = s; };
	void set_props  ( const std::string& props ) { m_props = props; };
	
	Room* room() { return m_room; };
	void join_room( Room& room );
	void leave_room();

private:
	const unsigned int m_id;
	const std::string m_name;
	
	//four digit version string (1.0.0.7), unknown purpose
	const std::string m_ver1;
	//three digit version string (2.0.7), displayed in game menu
	const std::string m_ver2;
	//client score string (ps=%d|pw=%d|pg=%d)
	//std::string m_score;
	//client properties string (pur|%d|dlc|%d|ram|%d|...)
	std::string m_props;

	/* status values:
	   1  default / in lobby
	   3  member in a room
	   5  host in a room
	   b  member in a game
	   f  host in a game
	*/
	unsigned char m_status;

	Room* m_room;
};
