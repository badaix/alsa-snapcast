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
#include <string>
#include <thread>


static constexpr auto LOG_TAG = "SnapStream";

using boost::asio::ip::tcp;
using namespace std::chrono_literals;


SnapStream::SnapStream(Uri uri)
    : socket_(io_context_), resolver_(io_context_), timer_(io_context_), uri_(std::move(uri)), connected_(false)
{
    LOG(INFO, LOG_TAG) << "Create SnapStream: " << uri_.toString() << "\n";
}


void SnapStream::resolve()
{
    resolver_.async_resolve(uri_.host, std::to_string(uri_.port.value()),
                            [this](const boost::system::error_code& ec, const tcp::resolver::results_type& results)
    {
        if (ec)
        {
            LOG(ERROR, LOG_TAG) << "Failed to resolve host '" << uri_.host << "', error: " << ec
                                << ", message: " << ec.message() << "\n";
            timer_.expires_after(1s);
            timer_.async_wait(
                [this](const boost::system::error_code& ec)
            {
                if (!ec)
                {
                    resolve();
                }
            });
        }
        else
        {
            for (const auto& iter : results)
                LOG(INFO, LOG_TAG) << "Resolved IP: " << iter.endpoint().address().to_string() << "\n";

            connect(results.begin()->endpoint());
        }
    });
}


void SnapStream::connect(const boost::asio::ip::basic_endpoint<tcp>& ep)
{
    LOG(DEBUG, LOG_TAG) << "Connecting to: " << ep << "\n";
    socket_.async_connect(ep,
                          [this, ep](boost::system::error_code ec)
    {
        if (!ec)
        {
            LOG(INFO, LOG_TAG) << "Connected to " << ec << "\n";
            connected_ = true;
            read();
        }
        else
        {
            LOG(ERROR, LOG_TAG) << "Failed to connect: " << ec << ", message: " << ec.message() << "\n";
            timer_.expires_after(1s);
            timer_.async_wait(
                [this, ep](const boost::system::error_code& ec)
            {
                if (!ec)
                {
                    connect(ep);
                }
            });
        }
    });
}


void SnapStream::start()
{
    if (t_.joinable())
        return;
    LOG(INFO, LOG_TAG) << "Start\n";

    resolve();
    t_ = std::thread([&]() { io_context_.run(); });
}


void SnapStream::stop()
{
    io_context_.stop();
    connected_ = false;
    t_.join();
}


void SnapStream::write(const void* data, uint32_t size)
{
    LOG(DEBUG, LOG_TAG) << "Write " << size << " bytes\n";
    if (!connected_)
    {
        LOG(DEBUG, LOG_TAG) << "Not connected\n";
        return;
    }

    boost::asio::async_write(socket_, boost::asio::buffer(data, size),
                             [this](boost::system::error_code ec, std::size_t length)
    {
        if (!ec)
        {
            LOG(DEBUG, LOG_TAG) << "Wrote " << length << " bytes\n";
        }
        else
        {
            LOG(ERROR, LOG_TAG) << "Failed to write: " << ec << ", message: " << ec.message() << "\n";
            connected_ = false;
            socket_.close();
            resolve();
        }
    });
}

void SnapStream::read()
{
    boost::asio::async_read(socket_, boost::asio::buffer(buffer_.data(), buffer_.size()),
                            [this](boost::system::error_code ec, std::size_t length)
    {
        if (!ec)
        {
            LOG(DEBUG, LOG_TAG) << "Read " << length << " bytes\n";
            read();
        }
        else
        {
            LOG(ERROR, LOG_TAG) << "Failed to read: " << ec << ", message: " << ec.message() << "\n";
            connected_ = false;
            socket_.close();
            resolve();
        }
    });
}
