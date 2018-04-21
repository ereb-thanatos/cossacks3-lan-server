/*
	Copyright (c) 2018 Ereb @ habrahabr.ru

	This source code is distributed under the MIT license.
	See LICENSE.MIT for details.
*/
#pragma once
#include "Precompiled.hpp"

/*
	The Packet class is a functional wrapper for the Session buffer.
	It provides (de)serialization, stores packet header values and
	packet source id. It is used in Lobby::process_buf() for easy
	sequential reading and writing to the Session buffer and in
	Lobby::send() to determine how much to copy to the send buffer.
*/
class Packet
{
public:
	Packet( Buffer& buf, unsigned int source_id );

	enum { packet_header_size = 14 };
	enum LengthType { Byte, Short, Int };
	
	//client ID of packet sender, set in constructor
	unsigned int source() const { return m_source_id; };

	//header variables, set in constructor and write_header()
	unsigned int   size() const { return m_size;      };
	unsigned short  cmd() const { return m_cmd;       };
	unsigned int    id1() const { return m_id1;       };
	unsigned int    id2() const { return m_id2;       };

	//move seek position forward, useful for skipping bytes
	void seek( unsigned int offset ) { m_seek_pos += offset; };

	//move seek position to the start or the end of data section
	//useful for overwriting data or for keeping and forwarding it
	void seek_to_start() { m_seek_pos = packet_header_size;          };
	void seek_to_end()   { m_seek_pos = packet_header_size + m_size; };

	//return read result and adjust seek position
	unsigned char  read_byte();
	unsigned short read_short();
	unsigned int   read_int();
	std::string    read_string( LengthType lt = Byte );
	
	//write and adjust seek position
	void write_byte  ( unsigned char  b );
	void write_short ( unsigned short s );
	void write_int   ( unsigned int   i );
	void write_string( const std::string& str, LengthType lt = Byte );

	//has to be called before the packet is passed to send()
	void write_header( unsigned short cmd, unsigned int id1 = 0, unsigned int id2 = 0 );
	
	//helper functions to forward the packet unchanged or with another command code
	void keep_whole_message() { keep_whole_message( m_cmd ); };
	void keep_whole_message( unsigned short cmd );
	
	//get reference to the Client / Session buffer for reading packet data
	const Buffer& buf() const { return m_buf; };

	//total packet size; available after write_header()
	size_t send_size() const { return m_send_size; };


#ifndef NDEBUG
	friend std::ostream& operator<< ( std::ostream &out, const Packet &p );
#endif


private:
	union byte_int
	{
		unsigned char b[4];
		unsigned int i;
	};
	union byte_short
	{
		unsigned char b[2];
		unsigned short s;
	};

	const unsigned int m_source_id;//from client session id

	size_t m_send_size;//set by write_header()

	unsigned int   m_size;
	unsigned short m_cmd;
	unsigned int   m_id1;
	unsigned int   m_id2;

	unsigned int m_seek_pos;
	Buffer&      m_buf;//Session buffer
};
