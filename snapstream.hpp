/***
    This file is part of alsa-snapcast
    Copyright (C) 2025  Johannes Pohl

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

#pragma once

// local headers

// 3rd party headers
#include <boost/asio.hpp>

// standard headers
#include <optional>
#include <thread>



using boost::asio::ip::tcp;

class SnapStream
{
public:
    void start();
    void write(const void* data, uint32_t size);
    void read();

private:
    std::thread t_;
    boost::asio::io_context io_context_;
    std::optional<tcp::socket> socket_;
    std::array<char, 100> buffer_;
};
