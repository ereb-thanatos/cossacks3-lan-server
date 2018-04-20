/*
	Copyright (c) 2018 Ereb @ habrahabr.ru

	This source code is distributed under the MIT license.
	See LICENSE.MIT for details.
*/
#include "Precompiled.hpp"

#include "Packet.hpp"

/*
	Reads header values and sets seek position to data start
*/
Packet::Packet( Buffer& buf, unsigned int source_id ):
	m_source_id( source_id ),
	m_buf      ( buf ),
	m_seek_pos ( 0 ),
	m_send_size( 0 )
{
	m_size = read_int();
	m_cmd  = read_short();
	m_id1  = read_int();
	m_id2  = read_int();
}


/*
	Overloaded output opearator for debug purposes:
	Packet p(...);
	std::cout << "Packet: " << p << std::endl;
*/
#ifndef NDEBUG
std::ostream& operator<< ( std::ostream &out, const Packet &p )
{
	out << "Command: " << std::hex << p.cmd() << "\nId1 = " << p.id1() << "\nId2 = " << p.id2() << "\n";
	const auto& b = p.buf();
	out << std::hex << std::setfill( '0' ) << std::setw( 2 );
	for ( int i = 0; i < 14; ++i ) out << std::hex << std::setfill( '0' ) << std::setw( 2 ) << static_cast<unsigned int>(b[i]) << ' ';
	out << '\n';
	for ( size_t i = 0; i < p.size(); ++i )
	{
		out << std::hex << std::setfill( '0' ) << std::setw( 2 ) << static_cast<unsigned int>( b[Packet::packet_header_size + i] ) << ' ';
		if ( 15 == i % 16 ) out << '\n';
	}
	out << '\n' << std::flush;
	return out;
}
#endif


/*
	Read value according to name, advance seek position.
*/
unsigned char Packet::read_byte()
{
	return m_buf[m_seek_pos++];
}
unsigned short Packet::read_short()
{
	byte_short si;
	si.b[0] = m_buf[m_seek_pos++];
	si.b[1] = m_buf[m_seek_pos++];
	return si.s;
}
unsigned int Packet::read_int()
{
	byte_int bi;
	bi.b[0] = m_buf[m_seek_pos++];
	bi.b[1] = m_buf[m_seek_pos++];
	bi.b[2] = m_buf[m_seek_pos++];
	bi.b[3] = m_buf[m_seek_pos++];
	return bi.i;
}
//LengthType describes how many bytes in front of the string contain it's length
std::string Packet::read_string( LengthType lt )//default: Byte
{
	size_t l = 0;
	if      ( Byte  == lt ) l = read_byte();
	else if ( Short == lt ) l = read_short();
	else if ( Int   == lt ) l = read_int();
	const std::string str( reinterpret_cast<const char*>( &m_buf[m_seek_pos] ), l );
	m_seek_pos += static_cast<unsigned int>( l );
	return str;
}


/*
	Write value according to name, advance seek position.
*/
void Packet::write_byte( unsigned char b )
{
	m_buf[m_seek_pos++] = b;
}
void Packet::write_short( unsigned short s )
{
	byte_short si;
	si.s = s;
	write_byte( si.b[0] );
	write_byte( si.b[1] );
}
void Packet::write_int( unsigned int i )
{
	byte_int bi;
	bi.i = i;
	write_byte( bi.b[0] );
	write_byte( bi.b[1] );
	write_byte( bi.b[2] );
	write_byte( bi.b[3] );
}
//LengthType describes how many bytes in front of the string should contain it's length
void Packet::write_string( const std::string& str, LengthType lt )//default: Byte
{
	const size_t l = str.length();
	if      ( Byte  == lt ) write_byte ( static_cast<unsigned char> ( l ) );
	else if ( Short == lt )	write_short( static_cast<unsigned short>( l ) );
	else if ( Int   == lt )	write_int  ( static_cast<unsigned int>  ( l ) );
	std::memcpy( &m_buf[m_seek_pos], str.c_str(), l );
	m_seek_pos += static_cast<unsigned int> ( l );
}


/*
	Calculates data size from seek position. Seeks to packet start and
	overwrites the header with passed values. Updates header variables.
	Sets m_send_size, which is necessary for Lobby::send().
*/
void Packet::write_header( unsigned short cmd, unsigned int id1, unsigned int id2 )
{
	m_send_size = m_seek_pos;
	m_seek_pos = 0;
	m_size = static_cast<unsigned int>( m_send_size ) - packet_header_size;
	m_cmd  = cmd;
	m_id1  = id1;
	m_id2  = id2;
	write_int  ( m_size );
	write_short( m_cmd );
	write_int  ( m_id1 );
	write_int  ( m_id2 );
}


/*
	Helper function to keep message body and overwrite the command only.
	Useful for packet forwarding. Calls write_header() and therefore sets
	m_send_size. See Lobby::process_buf() for usage examples.
*/
void Packet::keep_whole_message( unsigned short cmd )
{
	seek_to_end();
	write_header( cmd, m_id1, m_id2 );
}
