/*
	Copyright (c) 2018 Ereb @ habrahabr.ru

	This source code is distributed under the MIT license.
	See LICENSE.MIT for details.
*/
#pragma once
#include "Precompiled.hpp"

#include "Client.hpp"
#include "Packet.hpp"
#include "Player.hpp"
#include "Room.hpp"

/*
	The Lobby class keeps references to all Rooms and Players and controls
	all network communication between Clients. It also issues Client IDs and
	state changes in all Room and Player instances.
*/
class Lobby
{
public:
	Lobby() : m_last_issued_id( 0 ) {};

	void connect    ( std::shared_ptr<Client> client );
	void disconnect ( std::shared_ptr<Client> client );
	void process_buf( std::shared_ptr<Client> client );

private:
	enum SendTo
	{
		Source, Id2, Everyone, EveryoneButSource,
		RoomHost, EveryoneInRoom, EveryoneInRoomButSource,
		PropagateInRoom //used for game data, see Lobby::send() for details
	};
	void send( const Packet& p, SendTo target ) const;

	std::map<unsigned int, std::shared_ptr<Client>> m_clients;//key: Client ID
	std::map<unsigned int, std::unique_ptr<Player>> m_players;//key: Client ID
	std::map<unsigned int, std::unique_ptr<Room>>   m_rooms;  //key: room host Client ID

	//increment IDs independent of current map size
	unsigned int m_last_issued_id;
};
