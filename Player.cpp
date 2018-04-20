/*
	Copyright (c) 2018 Ereb @ habrahabr.ru

	This source code is distributed under the MIT license.
	See LICENSE.MIT for details.
*/
#include "Precompiled.hpp"

#include "Player.hpp"

/*
	Updates player status, creates Player <> Room link,
	adds player id to Room::m_players.
*/
void Player::join_room( Room& room )
{
	if ( room.host_id() == m_id )
	{
		m_status = 0x05;
	}
	else
	{
		m_status = 0x03;
	}
	m_room = &room;
	m_room->add_player( m_id );
}


/*
	Updates player status, erases player id from Room::m_players,
	clears Player <> Room link.
*/
void Player::leave_room()
{
	assert( m_room );

	m_status = 0x01;
	if ( m_room )
	{
		m_room->remove_player( m_id );
		m_room = nullptr;
	}
}