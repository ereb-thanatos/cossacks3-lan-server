/*
	Copyright (c) 2018 Ereb @ habrahabr.ru

	This source code is distributed under the MIT license.
	See LICENSE.MIT for details.
*/
#pragma once
#pragma warning( push, 0 )

#include <algorithm>
#include <deque>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <map>
#include <memory>
#include <vector>

#define ASIO_STANDALONE
#include <asio.hpp>

#pragma warning( pop )

typedef std::vector<unsigned char> Buffer;
typedef std::shared_ptr<Buffer>    BufPtr;