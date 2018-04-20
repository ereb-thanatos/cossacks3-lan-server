/*
	Copyright (c) 2018 Ereb @ habrahabr.ru

	This source code is distributed under the MIT license.
	See LICENSE.MIT for details.
*/
#include "Precompiled.hpp"

#ifndef NDEBUG //include for timer
#include <chrono>
#endif

#include "Lobby.hpp"
#include "Packet.hpp"
#include "Session.hpp"

/*
	Assigns incremented ID to Client and stores pointer in map
*/
void Lobby::connect( std::shared_ptr<Client> client )
{
	const auto id = ++m_last_issued_id;
	client->set_id( id );
	m_clients.emplace( std::make_pair( id, client ) );
}


/*
	Gracefully removes the Client from the Lobby and Rooms.
	Sends notifications to others if necessary.
*/
void Lobby::disconnect( std::shared_ptr<Client> client )
{
	std::cout << "Client disconnected: " << std::setfill(' ') << std::setw(15)
	          << std::right << client->address() << std::endl;

	//first, delete session to prevent asio send errors
	const auto id = client->id();
	m_clients.erase( id );

	auto it = m_players.find( id );

	//stop here if the disconnect happened before login (no Player object)
	if ( m_players.end() == it ) return;

	Packet p( client->buf(), id );

	//check if we need to simulate a "leave room" message
	if ( it->second->room() )
	{
		//trick process_buf() into sending "leaves room" notifications
		//this will also take care about room host transition
		p.write_header( 0x1a0, id, 0 );
		process_buf( client );
	}

	//we needed the player object while processing the 0x1a0 "message" above
	m_players.erase( id );

	/* 0x1a7 notification format
	id1 = id of leaving player
	id2 = 0
	data: none
	*/
	p.seek_to_start();
	p.write_header( 0x1a7, id, 0 );
	send( p, Everyone );
}


/*
	Queues the Packet buffer for all targeted Clients.
	The buffer is allocated dynamically with shared ownership to make sure it
	lives long enough for the asynchronous writes to complete.

	Packet::write_header() MUST be called before passing the Packet to send()!
	Since we are still using the Session's big buffer, there would be no way
	to know how many bytes to copy into the shared buffer, and we don't want
	to copy around the whole Session buffer every time.
*/
void Lobby::send( const Packet& p, SendTo target ) const
{
	auto const send_size = p.send_size();
	assert( 0 < send_size );
	if ( 0 == send_size ) return;//should never happen

	const auto src_id = p.source();
	//the shared pointer will be passed by value and pushed into Session queues
	//queue will be pop'ed after async_write() completes, ensuring buffer lifetime
	const auto buf_ptr = std::make_shared<Buffer>( p.buf().begin(), p.buf().begin() + send_size );

	//find Client instances and pass buffer pointer according to desired target
	try
	{
		if      ( target == Source )
		{
			m_clients.at( src_id )->queue_buf( buf_ptr );
		}
		else if ( target == Id2 )
		{
			m_clients.at( p.id2() )->queue_buf( buf_ptr );
		}
		else if ( target == Everyone )
		{
			for ( auto client : m_clients )
			{
				client.second->queue_buf( buf_ptr );
			}
		}
		else if ( target == EveryoneButSource )
		{
			for ( auto client : m_clients )
			{
				if ( src_id == client.first ) continue;
				client.second->queue_buf( buf_ptr );
			}
		}
		else //target depends on Player <> Room link
		{
			const auto& room = m_players.at( src_id )->room();
			assert( room );
			if ( !room ) return;//should never happen

			if      ( target == RoomHost )
			{
				const auto room_host_id = room->host_id();
				m_clients.at( room_host_id )->queue_buf( buf_ptr );
			}
			else if ( target == EveryoneInRoom )
			{
				const auto players = room->players();
				for ( const auto p_id : players )
				{
					m_clients.at( p_id )->queue_buf( buf_ptr );
				}
			}
			else if ( target == EveryoneInRoomButSource )
			{
				const auto players = room->players();
				for ( auto p_id : players )
				{
					if ( src_id == p_id ) continue;
					m_clients.at( p_id )->queue_buf( buf_ptr );
				}
			}
			else if ( target == PropagateInRoom )
			{
				//game data forwarding depends on packet source
				const auto room_host_id = room->host_id();
				if ( src_id == room_host_id )
				{
					//host -> everyone in the room
					for ( auto p_id : room->players() )
					{
						if ( p_id == src_id ) continue;
						m_clients.at( p_id )->queue_buf( buf_ptr );
					}
				}
				else
				{
					//player -> room host
					m_clients.at( room_host_id )->queue_buf( buf_ptr );
				}
			}
		}
	}
	catch ( std::out_of_range e )
	{
		std::cerr << "[WARNING] Lobby::send() -- ID map lookup failed\n";
	}
}


/*
	Contains server logic regarding parsing and reaction to packets.
	Initializes a Packet instance to wrap the raw Client buffer.
	Proceeds to sequentially read the packet buffer and then compose a
	response packet in the same Client buffer, before calling
	Packet::write_header() and Lobby::send().
*/
void Lobby::process_buf( std::shared_ptr<Client> client )
{
	const auto c_id = client->id();
	//Packet constructor parses the header and sets seek position to data
	Packet p( client->buf(), c_id );
	const auto size = p.size();
	const auto  cmd = p.cmd();
	const auto  id1 = p.id1();
	const auto  id2 = p.id2();


#ifndef NDEBUG //display recieved message codes
	static auto lt = std::chrono::steady_clock::now();
	auto        nt = std::chrono::steady_clock::now();
	//display delimiter lines for intervals over 500 ms for better readability
	auto dt = std::chrono::duration_cast<std::chrono::milliseconds>( nt - lt ).count();
	if ( 500 < dt )
	{
		std::cout << std::setfill( '-' ) << std::setw( 40 ) << "" << std::endl;
		lt = nt;
	}
	std::cout << client->id() << ": " << std::hex << std::setw( 2 )
	          << std::setfill( ' ' ) << cmd << std::endl;
#endif


	//game data
	if      ( 0x4b0 == cmd /* game data           */ )
	{
		/* 0x4b0 message format
		id1 = source id
		id2 = 0
		data: binary map data stream
		*/
		p.keep_whole_message();
		send( p, PropagateInRoom );
	}
	else if ( 0x032 == cmd /* array of variables  */ )
	{
		/* 0x032 message format
		id1 = player id
		id2 = 0
		data:
			4 int = 4 ?
			4 len = 5 ?
			^ string = empty
			4 int = 1 (number of arrays?)
			4 int = number of strings in following array
				4 int = len
				^ str = index of string in ascii
				4 int = len
				^ string = True | False | ...
				4 int = 0
		*/
		p.keep_whole_message();
		send( p, EveryoneInRoomButSource );
	}
	else if ( 0x456 == cmd /* data recieved       */ )
	{
		/* 0x456 message format
		id1 = player id
		id2 =
		data: none
		*/
		p.keep_whole_message();
		send( p, PropagateInRoom );
	}
	else if ( 0x457 == cmd /* end of transmission */ )
	{
		/* 0x457 message format
		id1 = room host id
		id2 =
		data: none
		*/
		p.keep_whole_message();
		send( p, EveryoneInRoomButSource );
	}
	else if ( 0x460 == cmd /* end of transmission */ )
	{
		/* 0x460 message format
		id1 = source id
		id2 = 0
		data: none
		*/
		p.keep_whole_message();
		send( p, RoomHost );
	}
	else if ( 0x461 == cmd /* all players loaded  */ )
	{
		/* 0x461 message format
		id1 = room host id
		id2 = 0
		data: none
		*/
		p.keep_whole_message();
		send( p, EveryoneInRoomButSource );
	}

	//information exchange
	else if ( 0x064 == cmd /* player status (room)  */ )
	{
		/* 0x64 message format
		id1 = player id
		id2 = 0
		data:
			2 short = 0
			2 short = len
			^ nickname
			1 0h
			4 int = player id
			4 int = player status? (seen: 0x01, 0x0a, 0x0b, 0x0c, 0x0d, 0x0f) (linked with 0x1ab message?)
			1 0
			1 byte = 1
			1 byte = 0 or 2
		*/
		p.keep_whole_message();
		send( p, RoomHost );
	}
	else if ( 0x065 == cmd /* player status (room)  */ )
	{
		//0x65 message format same as 0x64
		p.keep_whole_message();
		send( p, RoomHost );
	}
	else if ( 0x066 == cmd /* player status (room)  */ )
	{
		//0x66 message format same as 0x64
		p.keep_whole_message();
		send( p, Source );
	}
	else if ( 0x192 == cmd /* request player info   */ )
	{
		/* 0x192 message format
		id1 = client id
		id2 = 0
		data:
			4 int = id of requested player
		*/
		const auto info_id = p.read_int();

		auto& player = m_players.at( info_id );

		/* 0x193 response format
		id1 = if of requested player
		id2 = client id
		data:
			4 int = id of requested player
			1 status = { 3, 7 } (3: in room; 7: room host)
			1 len
			^ player nickname
			1 len (optional, for ranked only)
			^ player score = ps=%d|pw=%d|pg=%d (optional)
			4 int = ? (value close to player score; often 0x3e8 = 1000)
			4 int = ? (can be 0)
			4 int = ? (can be 0)
			4 int = ? (can be 0)
			4 int = ? (can be 0)
			1 len
			^ client properties = pur|%d|dlc|%d|ram|%d
		*/
		p.seek_to_start();
		p.write_int( info_id );
		p.write_byte( player->status() );
		p.write_string( player->name() );
		p.write_byte( 0 );//skip player ranked info string
		p.write_int( 0 );
		p.write_int( 0 );
		p.write_int( 0 );
		p.write_int( 0 );
		p.write_int( 0 );
		p.write_string( player->props() );
		p.write_header( 0x193, info_id, id1 );
		send( p, Source );
	}
	else if ( 0x1ab == cmd /* player status         */ )
	{
		/* 0x1ab message format
		id1 = client id
		id2 = 0
		data:
			1 status byte
		*/

		/* 0x1ac response format
		id1 = client id
		id2 = 0
		data:
			1 status byte
		*/
		p.keep_whole_message( 0x1ac );
		send( p, Everyone );
	}
	else if ( 0x1ad == cmd /* version check         */ )
	{
		/* 0x1ad message format
		id1 = 0
		id2 = 0
		data:
			1 len
			^ client version = %d.%d.%d
		*/
		auto& player = m_players.at( client->id() );

		/* 0x1ae response format
		id1 = 0
		id2 = client id
		data:
			1 len
			^ some version string = %d.%d.%d.%d
			1 len
			^ client version = %d.%d.%d
			4 int = 0
		*/
		p.write_string( player->ver1() );
		p.write_string( player->ver2() );
		p.write_int( 0 );
		p.write_header( 0x1ae, 0, player->id() );
		send( p, Source );
	}
	else if ( 0x1b3 == cmd /* set player properties */ )
	{
		/* 0x1b3 message format
		id1 = player id
		id2 = 0
		data:
			1 len
			^ password
			1 len
			^ nickname
			1 0h (score string)
			1 len
			^ properties = pur|%d|dlc|%d|ram|%d
		*/
		p.read_string();
		p.read_string();
		p.read_string();
		const auto props = p.read_string();

		m_players.at( c_id )->set_props( props );

		/* 0x1b4 response format
		id1 = player id
		id2 = 0
		data:
			1 len
			^ nickname
			1 len (or 0)
			^ score string (optional)
			1 len
			^ properties = pur|%d|dlc|%d|ram|%d
			1 status byte
		*/
		//response seems unnecessary and can cause incorrect status display
	}
	else if ( 0x1b7 == cmd /* purpose unknown       */ )
	{
		/* 0x1b7 message format
		id1 = player id
		id2 = 0
		data:
			4 int = player id
			4 int = player id
		*/
		//purpose unknown
	}

	//rooms
	else if ( 0x0c8 == cmd /* forward room properties */ )
	{
		/* 0xc8 message and notification format
		id1 = room host id
		id2 = 0
		data:
			2 short = len
			^ room description = "roomname"\t"pass"\tBUILD
			2 short = len
			^ room info = %d|%d|%d|%d|%d|%d
			4 int = 8
			4 int = unknown constant = 0x30d42 = 200002
			2 short = len
			^ pc hostname
			7 0h
		*/
		p.keep_whole_message();
		send( p, EveryoneButSource );
	}
	else if ( 0x0c9 == cmd /* forward room properties */ )
	{
		/* 0xc9 message & response format
		id1 = source of info id (room host id)
		id2 = client id
		data:
			4 int = source of info id (room host id)
			2 short = len
			^ description = "roomname"\t"pass"\tBUILD
			2 short = len
			^ info = %d|%d|%d|%d|%d|%d (without joining player)
			4 int = 8
			4 int = 0x00030d42 (magic constant?)
			2 short = len
			^ hostname of room host pc
			7 0h (padding?)
			4 int = number of players in room
				4 int = player id
				2 short = 0
				2 short = len
				^ player nickname
				1 0h
				4 int = player id
				1 player status byte -> transferred in 064 & 1ab to server
				6 unknown bytes
		*/
		p.keep_whole_message();
		send( p, Id2 );
	}
	else if ( 0x19c == cmd /* create room             */ )
	{
		/* 0x19c message format
		id1 = client id
		id2 = 0
		data:
			4 int = 8
			1 0h
			1 len
			^ description = "roomname"\t"pass"\t[0|h]BUILD
			1 len
			^ info = 0
			4 int = ? (same in 1x9d response)
			2 short = 0
		*/
		p.seek( 5 );
		const auto desc  = p.read_string();
		const auto info  = p.read_string();
		const auto magic = p.read_int();

		//create player object and get reference at one go
		auto& room   = m_rooms.emplace( c_id, std::make_unique<Room>( c_id, desc ) ).first->second;
		auto& player = m_players.at( c_id );
		//establish Player <> Room link for future lookups, add player id to Room::m_players
		player->join_room( *room );

		/* 0x19d notification format
		id1 = client id
		id2 = 0
		data:
			1 7h
			4 int = 8
			1 len
			^ description = "roomname"\t"pass"\tBUILD
			1 len
			^ info = 0
			6 0h
		*/
		p.seek_to_start();
		p.write_byte( 7 );
		p.write_int( 8 );
		p.write_string( desc );
		p.write_string( info );
		p.write_int( magic );
		p.write_short( 0 );
		p.write_header( 0x19d, id1, 0 );
		send( p, Everyone );
	}
	else if ( 0x19e == cmd /* join room               */ )
	{
		/* 0x19e message format
		id1 = client id
		id2 = 0
		data:
			4 int = id of room host
		*/
		const int room_host_id = p.read_int();

		auto& room   = m_rooms.at( room_host_id );
		auto& player = m_players.at( id1 );
		//establish Player <> Room link for future lookups, add player id to Room::m_players
		player->join_room( *room );

		/* 0x19f notification format
		id1 = client id
		id2 = 0
		data:
			4 int = id of room host
			1 3h (status?)
		*/
		p.seek_to_start();
		p.write_int( room_host_id );
		p.write_byte( player->status() );
		p.write_header( 0x19f, id1, 0 );
		send( p, Everyone );
	}
	else if ( 0x1a0 == cmd /* leave room or game      */ )
	{
		/* 0x1a0 message format
		id1 = player id
		id2 = 0
		data: none
		*/
		auto& player = m_players.at( c_id );
		auto  room   = player->room();

		//leaving host can trigger multiple 0x1a0 messages from players
		//they MUST NOT be forwarded or responded
		if ( nullptr == room ) return;

		const auto room_id = room->host_id();
		const auto players = room->players();
		const auto status  = player->status();

		//0x05 if still in room, 0x0f if during a game
		const bool room_host_leaving    = ( 0x05 == status || 0x0f == status )     ? true : false;
		//can be necessary even with 2 human players because of AI enemies
		const bool host_transfer_needed = ( 0x0f == status && 1 < players.size() ) ? true : false;

		//grab the last player id in room in case we'll need a new host
		const auto new_host_id = players.at( players.size() - 1 );

		/* 0x1a1 notification format
		id1 = player id
		id2 = 0
		data:
			1 (unknown byte: 0 for player or 1 for leaving host?)
			4 int = number of player id / status byte pairs
				4 int = player id
				1 status byte
		*/
		if ( room_host_leaving )
		{
			//kick-notify everyone in room at one go
			p.write_byte( 1 );
			p.write_int( static_cast<unsigned int>( players.size() ) );
			for ( auto p_id : players )
			{
				auto& pl = m_players.at( p_id );
				//remove Player <> Room link, erase player id from Room::m_players
				pl->leave_room();
				p.write_int( p_id );
				p.write_byte( pl->status() );
			}
		}
		else
		{
			//notify about this one player only
			player->leave_room();
			p.write_byte( 0 );
			p.write_int( 1 );
			p.write_int( player->id() );
			p.write_byte( player->status() );
		}
		p.write_header( 0x1a1, id1, 0 );
		send( p, Everyone );

		if ( host_transfer_needed )
		{
			/* 0x1bd message format
			id1 = new host id
			id2 = new host id
			data:
				4 int = len till data end
				4 int = 0
				4 int = 1 (number of arrays?)
				1 0h
				4 int = number of key <> value string pairs
					4 int = len
					^ key
					4 int or number of arrays, followed by 0
					^ value string or array:
						4 int = number of elements
						4 int = len
						^ some string char (* for host)
						4 int = len
						^ value (ID as ascii number)
					4 int = 0 (separator)

			Examples for key-values:
				gamename    "name"\t"pass"\t0BUILD
				mapname     1|2|2|0|0|0
				master      (ID of new host, in decimal ascii)
				session     (unique decimal number, nowhere to be found in dec or hex)
				clients     1 (number of remaining clients, in ascii)
				clientslist array:
					*    ID of host in decimal ascii (ascii key * seems irrelevant)
			*/
			p.seek_to_start();
			p.seek( 4 );      //will write length int afterwards, skip for now
			p.write_int ( 0 );//separator
			p.write_int ( 1 );//number of arrays
			p.write_byte( 0 );//separator
			p.write_int ( 6 );//number of key <> value pairs

			p.write_string( "gamename", Packet::Int );
			p.write_string( room->description(), Packet::Int );
			p.write_int( 0 );

			p.write_string( "mapname", Packet::Int );
			p.write_string( room->info(), Packet::Int );
			p.write_int( 0 );

			p.write_string( "master", Packet::Int );
			p.write_string( std::to_string( new_host_id ), Packet::Int );
			p.write_int( 0 );
			
			p.write_string( "session", Packet::Int );
			p.write_string( "1337", Packet::Int );//in reality unique 7 digit decimal of unknown origin
			p.write_int( 0 );

			p.write_string( "clients", Packet::Int );
			p.write_string( std::to_string( players.size() - 1 ), Packet::Int );
			p.write_int( 0 );

			p.write_string( "clientslist", Packet::Int );
			p.write_int ( 1 );
			p.write_byte( 0 );
			//list all player ids starting with the 2nd (1st was old host)
			p.write_int( static_cast<unsigned int>( players.size() ) - 1 );
			for ( auto it = players.begin() + 1; it != players.end(); ++it )
			{
				p.write_string( "*", Packet::Int );//character * seems irrelevant?
				p.write_string( std::to_string( *it ), Packet::Int );
			}
			p.write_int( 0 );
			
			//call write_header() to get total size (seek position)
			p.write_header( 0x1bd, new_host_id, new_host_id );
			//write the size int that we skipped at data start
			p.write_int( p.size() - 4 );
			//this message is for the new host only
			send( p, Id2 );

			/* 0x1bd message format
			id1 = new host id
			id2 = target player id
			data: none
			*/
			p.seek_to_start();
			//start with the 2nd player to exclude old host
			for ( auto it = players.begin() + 1; it != players.end(); ++it )
			{
				//send to all in room except the new host
				if ( new_host_id == *it ) continue;
				p.write_header( 0x1be, new_host_id, *it );
				send( p, Id2 );
			}
		}
		
		if( room_host_leaving )
		{
			//on host transfer the new host will recreate the room after recieving 0x1bd
			//old room must be deleted in either case
			m_rooms.erase( room_id );
		}
	}
	else if ( 0x1a2 == cmd /* start game              */ )
	{
		/* 0x1a2 message format
		id1 = room host id
		id2 = 0
		data:
			4 int = number of players in room (host is first)
				4 int = player id
				1 status byte
		*/
		const auto& room = m_players.at( c_id )->room();
		if ( !room ) return;//should never happen

		//remember to not show this room to newcomers through 0x19b
		room->hide_from_lobby();

		auto players = room->players();

		/* 0x1a3 notification format
		id1 = room host id
		id2 = 0
		data:
			4 int = number of players in room (host is last)
				4 int = player id
				1 status byte = 0b for players, 0f for host
		*/
		p.write_int( static_cast<unsigned int>( players.size() ) );
		//iterate backwards through players in room
		for ( size_t i = players.size(); 0 < i; )
		{
			const auto p_id = players[--i];
			auto& player    = m_players.at( p_id );

			//0x1a2 comes from the host, set status accordingly
			if ( p_id == c_id ) player->set_status( 0x0f );//host
			else player->set_status( 0x0b );//normal player

			p.write_int( p_id );
			p.write_byte( player->status() );
		}
		p.write_header( 0x1a3, id1, 0 );
		send( p, Everyone );
	}
	else if ( 0x1aa == cmd /* room update from host   */ )
	{
		/* 0x1aa message format
		id1 = client id = room host id
		id2 = 0
		data:
			1 len
			^ room description = "roomname"\t"pass"\tBUILD
			1 len
			^ room info = %d|%d|%d|%d|%d|%d
			6 unknown bytes = 0
		*/
		const std::string desc = p.read_string();
		const std::string info = p.read_string();

		auto room    = m_players.at( c_id )->room();
		if ( !room ) return;//should never happen
		auto players = room->players();
		
		room->set_info( info );

		/* 0x1a5 notification format
		id1 = room host id
		id2 = 0
		data:
			4 int = 8
			1 len
			^ description = "roomname"\t"pass"\tBUILD
			1 len
			^ info = %d|%d|%d|%d|%d|%d (without joining player)
			6 0h (unknown / padding)
			4 int = number of players in room (host included)
				[per player]
				4 int = player id
				1 role = { 3, 7 } (3: normal, 7: room host)
		*/
		p.seek_to_start();
		p.write_int( 8 );
		p.write_string( desc );
		p.write_string( info );
		p.write_int( 0 );
		p.write_short( 0 );
		p.write_int( static_cast<unsigned int>( players.size() ) );
		//iterate backwards through players in room
		for ( size_t i = players.size(); 0 < i; )
		{
			const int p_id = players[--i];
			p.write_int( p_id );
			p.write_byte( m_players.at( p_id )->status() );
		}
		p.write_header( 0x1a5, id1, 0 );
		send( p, Everyone );
	}
	else if ( 0x1af == cmd /* player leaves game      */ )
	{
		/* 0x1af format
		id1 = player id
		id2 = 0
		data: none
		*/
		p.keep_whole_message();
		send( p, Everyone );
	}
	else if ( 0x1b5 == cmd /* player kicked from game */ )
	{
		/* 0x1b5 message format
		id1 = room host id
		id2 = 0
		data:
			4 int = id of kicked player
		*/
		const unsigned int kick_id = p.read_int();

		//0x1b6 notification format same as 0x1b5 message
		p.keep_whole_message( 0x1b6 );
		send( p, Everyone );
		
		/* 0x1a1 notification format
		id1 = player id
		id2 = 0
		data:
			1 (unknown byte: 0 or 1?)
			4 int = number of player id / status byte pairs
				4 int = player id
				1 status byte
		*/
		p.seek_to_start();
		p.write_byte( 0 );
		p.write_int( 1 );
		p.write_int( kick_id );
		p.write_byte( 1 );
		p.write_header( 0x1a1, kick_id, 0 );
		send( p, Everyone );
	}
	else if ( 0x1bb == cmd /* room settings changed   */ )
	{
		/* 0x1bb short format (other than room host)
		id1 = room host id (complete settings) or player id (player settings)
		id2 = room host id (complete settings) or 0 (player settings)
		data:
			4 int = { 14, 100, 102, ... } (type of info?) 
			4 int = len (often 3)
			3 string (often tmp)
			4 int = 1
			1 0h
			4 int = number of { [len] key [len] value [0] } entries
				4 int = len
				^ string key
				4 int = len
				^ string value
					example: room settings string
					8x  %d,%d,%d,%d,%d|    player section (or x| for closed slots)
					... %d|                game settings section
					or just %d|%d|%d (nation,team,flag) for short player settings
				4 int = 0 (separator)
		*/
		p.keep_whole_message( 0x1bc );
		send( p, EveryoneInRoom );
	}

	//messaging
	else if ( 0x194 == cmd /* room message  */ )
	{
		/* 0x196 message format
		id1 = client id
		id2 = 0 or recipient id if private message
		data:
			1 len
			^ string = number|text message (0: to all; 2: to allies)
		*/
		p.keep_whole_message( 0x195 );
		send( p, EveryoneInRoom );
	}
	else if ( 0x196 == cmd /* lobby message */ )
	{
		/* 0x196 message format
		id1 = client id
		id2 = 0 or recipient id if private message
		data:
			1 len
			^ text message
		*/

		/* 0x197 notify format
		id1 = message source id
		id2 = 0 or recipient id if private message
		data:
			1 len
			^ text message
		*/
		p.keep_whole_message( 0x197 );

		//target depends on id constellation in header
		if ( 0 == id2 )
		{
			//public message
			send( p, Everyone );
		}
		else if ( id1 == id2 )
		{
			//system message
			send( p, Source );
		}
		else
		{
			//private message
			send( p, Source );
			send( p, Id2 );
		}
	}

	//login
	else if ( 0x1a8 == cmd /* email form        */ )
	{
		/* 0x1a8 message format
		id1 = 0
		id2 = 0
		data:
			1 len
			^ email
		*/

		/* 0x1a9 response format
		id1 = 0
		id2 = 0
		data:
			1 len
			^ email
			1 response code (0: unknown email; 1: registered email)
		*/
		p.seek_to_end();  //keep message data
		p.write_byte( 1 );//append response code
		p.write_header( 0x1a9 );
		send( p, Source );
	}
	else if ( 0x198 == cmd /* registration form */ )
	{
		/* 0x198 message format
		id1 = 0
		id2 = 0
		data:
			1 len
			^ version string = %d.%d.%d.%d
			1 len
			^ version string = %d.%d.%d
			1 len
			^ email
			1 len
			^ password
			1 len
			^ game key
			1 len
			^ nickname
			1 0h
			1 len
			^ properties = pur|%d|dlc|%d|ram|%d
		*/
		/* 0x199 response format
		id1 = 0
		id2 = 0
		data:
			1 error code
		*/
	}
	else if ( 0x19a == cmd /* login form        */ )
	{
		/* 0x19a message format
		id1 = client id
		id2 = 0
		data:
			1 len
			^ version string = %d.%d.%d.%d
			1 len
			^ version string = %d.%d.%d
			1 len
			^ email
			1 len
			^ password
			1 len
			^ game key
		*/
		const auto ver1 = p.read_string();
		const auto ver2 = p.read_string();
		p.seek( p.read_byte() );//skip email
		p.seek( p.read_byte() );//skip password
		auto name = p.read_string();//key = nickname

		/*
			We use the Game Key input field because it is the less restrictive.
			We need to get to login or registration form to get client version.

			Theoretically we could hardcode "1.0.0.7" as ver1 and get ver2 from
			0x1ad, then we could use email as nickname and jump to lobby after
			initial email message 0x1a8.
			There are 2 problems:
				1) We do not know what ver1 is and if it'll change (ver2 is the
				   one displayed in menu corner, e.g. "2.0.7").
				2) Email input field is more restrictive on special chars than
				   the original name field.

			Therefore we grab the text from Game Key form field and tailor it
			to original client restrictions (you can test them in registration
			form).
		*/

		//shrink or expand name length; allowed are 4 to 16 characters
		auto len = name.length();
		if ( 4 > len )
		{
			//expand with underscore
			for ( auto i = len; i < 4; ++i ) name.push_back( '_' );
		}
		else if ( 16 < len )
		{
			name = name.substr( 0, 16 );
		}

		//substitute illegal characters; allowed are a-zA-Z0-9()+-_.[]
		const std::string& ac = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789()+-_.[]";
		//executes lambda for every char in string and substitutes it with returned char
		std::transform( name.begin(), name.end(), name.begin(),
		[&ac]( const char c )
		{
			//if we can't find char in allowed chars string, substitute by underscore
			if ( std::string::npos == ac.find( c ) ) return '_';
			else return c;
		} );

		//create player object and get reference at one go
		auto& player = m_players.emplace( c_id, std::make_unique<Player>( c_id, name, ver1, ver2 ) ).first->second;

		/* 0x19b response format
		id1 = client id
		id2 = client id
		data:
			1 0h
			1 len
			^ nickname
			1 0h
			4 int = client score
			16 0h (unknown / padding)
			1 len
			^ client properties = pur|%d|dlc|%d|ram|%d
			^ [players in lobby]
				4 player id
				1 status (1: none, 2: in room, 4: room host, 8: playing)
				1 len
				^ nickname
				1 len (optional = 0)
				^ player score = ps=%d|pw=%d|pg=%d (optional)
				1 len
				^ player properties = pur|%d|dlc|%d|ram|%d
			4 0h (separator)
			^ [open rooms in reversed order]
				4 player id of room host
				4 int = 8
				1 len
				^ description = "roomname"\t"pass"\tBUILD
				1 len
				^ info = %d|%d|%d|%d|%d|%d
				6 0h (unknown / padding)
				4 int = number of players in room (host included)
				^ int values = player ids in room, reversed order (host is last)
			4 0h (eof)
		*/
		p.seek_to_start();
		p.write_byte( 0 );
		p.write_string( name );
		p.write_byte( 0 );
		p.write_int( 0 );//player score
		p.write_int( 0 );
		p.write_int( 0 );
		p.write_int( 0 );
		p.write_int( 0 );
		p.write_string( player->props() );
		//players in lobby
		for ( auto& it : m_players )
		{
			auto id = it.first;
			auto& pl = it.second;
			p.write_int( id );
			p.write_byte( pl->status() );
			p.write_string( pl->name() );
			p.write_byte( 0 );
			p.write_string( pl->props() );
		}
		p.write_int( 0 );
		//open rooms, reversed order
		for ( auto it = m_rooms.rbegin(); it != m_rooms.rend(); ++it )
		{
			auto id = it->first;
			auto& rm = it->second;
			if ( rm->is_hidden() ) continue;//skip rooms with started games
			auto pl = rm->players();
			p.write_int( id );
			p.write_int( 8 );
			p.write_string( rm->description() );
			p.write_string( rm->info() );
			p.write_int( 0 );
			p.write_short( 0 );
			p.write_int( static_cast<unsigned int>( pl.size() ) );
			//players in room, reversed order
			for ( size_t i = pl.size(); 0 < i; )
			{
				p.write_int( pl[--i] );
			}
		}
		p.write_int( 0 );
		p.write_header( 0x19b, c_id, c_id );
		send( p, Source );

		//"player joined lobby" notification (necessary for the client himself, too)
		/* 0x1a6 notification format
		id1 = new player's id
		id2 = 0
		data:
			1 len
			^ nickname
			1 0h
			1 len
			^ player score = ps=%d|pw=%d|pg=%d (optional)
			1 len
			^ player properties = pur|%d|dlc|%d|ram|%d
			1 1h (status)
		*/
		p.seek_to_start();
		p.write_string( player->name() );
		p.write_byte( 0 );
		p.write_string( player->props() );
		p.write_byte( player->status() );
		p.write_header( 0x1a6, c_id, 0 );
		send( p, Everyone );
	}
	
#ifndef NDEBUG //print packets with unknown command codes
	else std::cout << "Unknown packet:\n" << p << std::endl;
#endif
}
