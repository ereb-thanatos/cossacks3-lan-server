/*
	Copyright (c) 2018 Ereb @ habrahabr.ru

	This source code is distributed under the MIT license.
	See LICENSE.MIT for details.
*/
#pragma once
#include <iostream>
#include <typeinfo>

/*
	A debugging class for tracing object creation and destruction.
	To trace a class, add a member variable	of the type Trace<ClassName>:

	#include "Trace.hpp"
	class Room
	{
		...
	private:
		Trace<Room> t;
	};
*/
template<typename T>
class Trace
{
private:
	inline void out( const std::string& action )
	{
		std::cout << "Object " << typeid( T ).name() << " #" << id << " " << action << std::endl;
	};
	static int c;//global incremental object counter (separate for every type T)
	int       id;//local copy of the obejct number
public:
	Trace()               : id( ++c ) { out( "constructed"      ); };
	Trace( const Trace& ) : id( ++c ) { out( "copy-constructed" ); };
	Trace( Trace&&      ) : id( ++c ) { out( "move-constructed" ); };
	Trace& operator =( const Trace& ) { out( "copied through =" ); return *this; };
	Trace& operator =( Trace&&      ) { out( "moved through ="  ); return *this; };
	~Trace() { out( "destroyed" ); };
};
template<typename T> int Trace<T>::c = 0;//necessary for the linker to resolve static symbol
