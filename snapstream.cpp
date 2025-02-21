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

// prototype/interface header file
#include "snapstream.hpp"

// local headers
#include "aixlog.hpp"

// 3rd party headers
#include <boost/asio.hpp>

// standard headers
#include <optional>
#include <thread>


static constexpr auto LOG_TAG = "SnapStream";

using boost::asio::ip::tcp;

void SnapStream::start()
{
    if (t_.joinable())
        return;
    LOG(DEBUG, LOG_TAG) << "Start\n";

    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), 4955);
    socket_ = tcp::socket(io_context_);
    LOG(DEBUG, LOG_TAG) << "Connecting to: " << endpoint << "\n";
    socket_->async_connect(endpoint,
                           [this](boost::system::error_code ec)
    {
        if (!ec)
        {
            LOG(DEBUG, LOG_TAG) << "Connected\n";
            read();
        }
        else
        {
            LOG(ERROR, LOG_TAG) << "Failed to connect: " << ec << "\n";
        }
    });
    t_ = std::thread([&]() { io_context_.run(); });
}

void SnapStream::write(const void* data, uint32_t size)
{
    LOG(DEBUG, LOG_TAG) << "Write " << size << " bytes\n";
    boost::asio::async_write(*socket_, boost::asio::buffer(data, size),
                             [this](boost::system::error_code ec, std::size_t length)
    {
        if (!ec)
            LOG(DEBUG, LOG_TAG) << "Wrote " << length << " bytes\n";
        else
            LOG(ERROR, LOG_TAG) << "Failed to write: " << ec << "\n";
    });
}

void SnapStream::read()
{
    boost::asio::async_read(*socket_, boost::asio::buffer(buffer_.data(), buffer_.size()),
                            [this](boost::system::error_code ec, std::size_t length)
    {
        if (!ec)
        {
            LOG(DEBUG, LOG_TAG) << "Read " << length << " bytes\n";
            read();
        }
        else
        {
            LOG(ERROR, LOG_TAG) << "Failed to read: " << ec << "\n";
        }
    });
}
