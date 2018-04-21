/*
	Copyright (c) 2018 Ereb @ habrahabr.ru

	This source code is distributed under the MIT license.
	See LICENSE.MIT for details.
*/
#pragma once
#include "Precompiled.hpp"


/*
	Room class stores room data which must be presented to newcomers, and a
	hidden status for started games which must not be shown in 0x19b.

	Stores separate host ID for easier lookup, and all player IDs (host
	included as 1st entry) in a vector for easy iteration.
*/
class Room
{
private:
	typedef std::vector<unsigned int> IdVector;

public:
	Room( int host_id, const std::string& description ) :
		m_host_id( host_id ), m_description( description ), m_info( "0" ), m_hidden( false )
	{ m_players.reserve( 8 ); };
	
	unsigned int           host_id() const { return m_host_id;     };
	const IdVector&        players() const { return m_players;     };
	const std::string& description() const { return m_description; };
	const std::string&        info() const { return m_info;        };

	void set_info( const std::string& s ) { m_info    =  s; };
	void set_new_host ( unsigned int id ) { m_host_id = id; };
	void    add_player( unsigned int id ) { m_players.push_back( id ); };
	void remove_player( unsigned int id ) { m_players.erase( std::remove( m_players.begin(), m_players.end(), id ), m_players.end() ); };

	//used in 0x19b response to hide started games
	//(players which already are in lobby get the start game notification)
	bool is_hidden() const { return m_hidden; };
	void hide_from_lobby() { m_hidden = true; };

private:
	/* Room state explanations:
	- host id can change if the host disconnects during the game
	- description syntax: "Roomname"\t"Password"\t[0|h]ClientBuild
	    Examples: "2v2  0pt"\t""\t008C7
	              "historical battle"\t"secret"\th08C7
	- info syntax: %d|%d|%d|%d|%d|%d
	    1) status (1: joinable; 3: full / in game)
	    2) number of human players in room
	    3) number of ai players in room
	    4) number of closed slots
	    5) unknown / 0
	    6) unknown / 0
	*/
	const std::string m_description;
	unsigned int m_host_id;
	std::string  m_info;
	IdVector     m_players;
	bool         m_hidden;
};
