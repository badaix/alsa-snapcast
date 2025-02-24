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
#include "uri.hpp"

// 3rd party headers
#include <atomic>
#include <boost/asio.hpp>

// standard headers
#include <boost/asio/ip/basic_endpoint.hpp>
#include <thread>



using boost::asio::ip::tcp;

class SnapStream
{
public:
    explicit SnapStream(Uri uri);

    void start();
    void stop();
    void write(const void* data, uint32_t size);

private:
    void resolve();
    void connect(const boost::asio::ip::basic_endpoint<tcp>& ep);
    void read();

    std::thread t_;
    boost::asio::io_context io_context_;
    tcp::socket socket_;
    std::array<char, 100> buffer_;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::steady_timer timer_;
    Uri uri_;
    std::atomic_bool connected_;
};
