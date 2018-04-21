/*
	Copyright (c) 2018 Ereb @ habrahabr.ru

	This source code is distributed under the MIT license.
	See LICENSE.MIT for details.
*/
#pragma once
#include "Precompiled.hpp"

/*
	Provides a Session interface for the Lobby Object.
	See Session class for details.
*/
class Client
{
public:
	virtual ~Client() {};
	virtual       void       queue_buf( const BufPtr& buf ) = 0;
	virtual       void          set_id( unsigned int id   ) = 0;
	virtual       unsigned int      id() const              = 0;
	virtual const std::string& address() const              = 0;
	virtual       Buffer&          buf()                    = 0;
};