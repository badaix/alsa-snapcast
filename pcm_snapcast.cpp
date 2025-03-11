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



// local headers
#include "aixlog.hpp"
#include "sample_format.hpp"
#include "snapstream.hpp"
#include "string_utils.hpp"
#include "uri.hpp"

// 3rd party headers
#include <alsa/asoundlib.h>
#include <alsa/conf.h>
#include <alsa/pcm.h>
#include <alsa/pcm_external.h>
#include <alsa/pcm_ioplug.h>

// standard headers
#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>


static constexpr auto LOG_TAG = "SnapcastPCM";


/// An ALSA PCM I/O plugin that uses SnapStream for forwarding audio to Snapserver
class SnapcastPcm
{
private:
    std::mutex mutex;
    std::shared_ptr<SnapStream> stream;
    Uri uri;
    int64_t written;
    std::chrono::time_point<std::chrono::steady_clock> next{std::chrono::seconds(0)};

    static int Start(snd_pcm_ioplug_t* ext)
    {
        auto* self{static_cast<SnapcastPcm*>(ext->private_data)};
        std::scoped_lock lock{self->mutex};
        LOG(INFO, LOG_TAG) << "Start\n";
        if (!self->stream)
            return -EBADFD; // This should be checked by pcm_ioplug but we'll do it here too.

        self->stream->start();
        // oboe::Result result{self->stream->requestStart()};
        // if (result != oboe::Result::OK) {
        //     std::cerr << "[ALSA Oboe] Failed to start stream: " << oboe::convertToText(result) << std::endl;
        //     return -1;
        // }

        return 0;
    }

    static int Stop(snd_pcm_ioplug_t* ext)
    {
        auto* self{static_cast<SnapcastPcm*>(ext->private_data)};
        std::scoped_lock lock{self->mutex};
        LOG(INFO, LOG_TAG) << "Stop\n";

        if (!self->stream)
            return -EBADFD;

        // self->stream->stop();
        // oboe::StreamState state{self->stream->getState()};
        // if (state == oboe::StreamState::Stopped || state == oboe::StreamState::Flushed)
        //     return 0; // We don't need to do anything if the stream is already stopped.

        // oboe::Result result{self->stream->requestPause()};
        // if (result != oboe::Result::OK) {
        //     std::cerr << "[ALSA Oboe] Failed to pause stream: " << oboe::convertToText(result) << std::endl;
        //     return -1;
        // }

        // state = self->stream->getState();
        // while (state != oboe::StreamState::Paused) {
        //     // AAudio documentation states that requestFlush() is valid while the stream is Pausing.
        //     // However, in practice it returns InvalidState, so we'll just wait for the stream to pause.
        //     result = self->stream->waitForStateChange(state, &state, TimeoutNanoseconds);
        //     if (result != oboe::Result::OK) {
        //         std::cerr << "[ALSA Oboe] Failed to wait for pause: " << oboe::convertToText(result) << std::endl;
        //         return -1;
        //     }
        // }

        // result = self->stream->requestFlush();
        // if (result != oboe::Result::OK) {
        //     std::cerr << "[ALSA Oboe] Failed to flush stream: " << oboe::convertToText(result) << std::endl;
        //     return -1;
        // }

        // state = self->stream->getState();
        // while (state != oboe::StreamState::Flushed) {
        //     result = self->stream->waitForStateChange(oboe::StreamState::Flushing, &state, TimeoutNanoseconds);
        //     if (result != oboe::Result::OK) {
        //         std::cerr << "[ALSA Oboe] Failed to wait for flush: " << oboe::convertToText(result) << std::endl;
        //         return -1;
        //     }
        // }

        return 0;
    }

    static snd_pcm_sframes_t Pointer(snd_pcm_ioplug_t* ext)
    {
        auto* self{static_cast<SnapcastPcm*>(ext->private_data)};
        std::scoped_lock lock{self->mutex};
        // if (!self->stream)
        //     return -EBADFD;

        // // Note: This function would return an error for any Xruns but we don't bother as Oboe automatically recovers
        // from them.

        // int64_t framesWritten{self->stream->getFramesWritten()};
        // if (framesWritten < 0) {
        //     std::cerr << "[ALSA Oboe] Failed to get frames written: " << framesWritten << std::endl;
        //     return -1;
        // }

        // We don't care about the device ring buffer position as Oboe handles writing samples to it.
        // Instead, we just need to return the current position relative to the imaginary ALSA buffer size.
        auto res = self->written % ext->buffer_size;
        LOG(DEBUG, LOG_TAG) << "Pointer, return: " << res << "\n";
        return res;
    }

    static snd_pcm_sframes_t Transfer(snd_pcm_ioplug_t* ext, const snd_pcm_channel_area_t* areas,
                                      snd_pcm_uframes_t offset, snd_pcm_uframes_t size)
    {
        auto* self{static_cast<SnapcastPcm*>(ext->private_data)};
        std::unique_lock lock{self->mutex};
        LOG(DEBUG, LOG_TAG) << "Transfer, offset: " << offset << ", size: " << size << ", non-block: " << ext->nonblock
                            << "\n";
        if (!self->stream)
            return -EBADFD;

        if (size == 0)
            return 0;

        self->stream->start();

        // if (self->stream->getState() != oboe::StreamState::Started) {
        //     // ALSA expects us to automatically start the stream if it's not started.
        //     oboe::Result result{self->stream->requestStart()};
        //     if (result != oboe::Result::OK) {
        //         std::cerr << "[ALSA Oboe] Failed to start stream from transfer: " << oboe::convertToText(result) <<
        //         std::endl; return -1;
        //     }
        // }

        auto& firstArea{areas[0]};
        auto* address{reinterpret_cast<uint8_t*>(firstArea.addr)};

        self->stream->write(address, size * 4);

        auto now = std::chrono::steady_clock::now();
        if (self->next == std::chrono::time_point<std::chrono::steady_clock>(std::chrono::seconds(0)))
        {
            self->next = now;
        }
        double sec = static_cast<double>(size) / static_cast<double>(ext->rate);
        self->next += std::chrono::microseconds(static_cast<int64_t>(sec * 1000 * 1000));

        // When using MPD, non-block is true, but seems to be required
        // if (ext->nonblock == 0)
        {
            std::this_thread::sleep_for(self->next - now);
        }

#ifndef NDEBUG
        uint channelOffset{0};
        for (unsigned int c{0}; c < ext->channels; ++c)
        {
            auto& area{areas[c]};
            if (area.addr != firstArea.addr || area.step != firstArea.step || area.first >= firstArea.step)
            {
                LOG(ERROR, LOG_TAG) << "[ALSA Snapcast] Attempt to transfer non-interleaved samples\n";
                return -1;
            }
        }
#endif

        // oboe::ResultWithValue<int32_t> result{self->stream->write(address, size, ext->nonblock ? 0 :
        // TimeoutNanoseconds)}; if (result != oboe::Result::OK) {
        //     std::cerr << "[ALSA Oboe] Failed to write samples to stream: " << oboe::convertToText(result.error()) <<
        //     std::endl; return -1;
        // } else if (result.value() == 0) {
        //     if (!ext->nonblock)
        //         std::cerr << "[ALSA Oboe] Cannot write samples in blocking mode" << std::endl;
        //     return -EAGAIN; // Oboe will return 0 if the stream is non-blocking and there's no space in the buffer.
        // }

        self->written += size;
        // return result.value();
        return size;
    }

    static int Close(snd_pcm_ioplug_t* ext)
    {
        LOG(INFO, LOG_TAG) << "Close\n";
        if (ext->private_data)
        {
            auto* self{static_cast<SnapcastPcm*>(ext->private_data)};
            if (self->stream)
                self->stream->stop();
            delete self;
            ext->private_data = nullptr;
        }
        return 0;
    }

    static int Prepare(snd_pcm_ioplug_t* ext)
    {
        auto* self{static_cast<SnapcastPcm*>(ext->private_data)};
        std::scoped_lock lock{self->mutex};
        LOG(INFO, LOG_TAG) << "Prepare: " << ext->rate << ":" << ext->format << ":" << ext->channels
                           << ", buffer size: " << ext->buffer_size << ", period size: " << ext->period_size << "\n";

        //     ->setChannelCount(ext->channels)
        //     ->setChannelConversionAllowed(true)
        //     ->setSampleRate(ext->rate)
        //     ->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium)
        //     ->setBufferCapacityInFrames(ext->buffer_size)

        if (self->stream)
            return 0;

        self->stream = std::make_shared<SnapStream>(self->uri);
        return 0;

        // oboe::AudioStreamBuilder builder;
        // builder.setUsage(oboe::Usage::Game)
        //     ->setDirection(oboe::Direction::Output)
        //     // Note: There is some instability related to using LowLatency mode on certain devices.
        //     // Notably, while running mono 16-bit 48kHz audio on certain QCOM devices, the HAL simply raises a
        //     SIGABRT with no logs.
        //     ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        //     ->setSharingMode(oboe::SharingMode::Shared)
        //     ->setFormat([fmt = ext->format]() {
        //         switch (fmt) {
        //             case SND_PCM_FORMAT_S16_LE:
        //                 return oboe::AudioFormat::I16;
        //             case SND_PCM_FORMAT_FLOAT_LE:
        //                 return oboe::AudioFormat::Float;
        //             case SND_PCM_FORMAT_S24_3LE:
        //                 return oboe::AudioFormat::I24;
        //             case SND_PCM_FORMAT_S32_LE:
        //                 return oboe::AudioFormat::I32;
        //             default:
        //                 return oboe::AudioFormat::Invalid;
        //         }
        //     }())
        //     ->setFormatConversionAllowed(true)
        //     ->setChannelCount(ext->channels)
        //     ->setChannelConversionAllowed(true)
        //     ->setSampleRate(ext->rate)
        //     ->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium)
        //     ->setBufferCapacityInFrames(ext->buffer_size)
        //     ->setAudioApi(oboe::AudioApi::OpenSLES);

        // oboe::Result result{builder.openStream(self->stream)};
        // if (result != oboe::Result::OK) {
        //     std::cerr << "[ALSA Oboe] Failed to open stream: " << oboe::convertToText(result) << std::endl;
        //     return -1;
        // }

        // if (self->stream->getBufferCapacityInFrames() < ext->buffer_size) {
        //     // Note: This should never happen with AAudio, but it's possible with OpenSL ES.
        //     std::cerr << "[ALSA Oboe] Buffer size smaller than requested: " <<
        //     self->stream->getBufferCapacityInFrames() << " < " << ext->buffer_size << std::endl;
        //     self->stream.reset(); return -EIO;
        // }
    }

    static int Drain(snd_pcm_ioplug_t* ext)
    {
        auto self{static_cast<SnapcastPcm*>(ext->private_data)};
        std::scoped_lock lock{self->mutex};
        LOG(INFO, LOG_TAG) << "Drain\n";
        // if (!self->stream)
        //     return -EBADFD;

        // // We need to wait for the stream to read all samples during a drain.
        // // According to AAudio documentation, requestStop() guarantees that the stream's contents have been written
        // to the device.
        // // However, in practice it doesn't seem to be the case, so we'll just poll the frames read until it reaches
        // the frames written.

        // struct timespec start;
        // clock_gettime(CLOCK_MONOTONIC, &start);
        // start.tv_sec += 1;

        // while (true) {
        //     int64_t framesRead{self->stream->getFramesRead()};
        //     if (framesRead < 0) {
        //         std::cerr << "[ALSA Oboe] Failed to get frames read: " << framesRead << std::endl;
        //         return -1;
        //     }

        //     int64_t framesWritten{self->stream->getFramesWritten()};
        //     if (framesWritten < 0) {
        //         std::cerr << "[ALSA Oboe] Failed to get frames written: " << framesWritten << std::endl;
        //         return -1;
        //     }

        //     if (framesRead == framesWritten)
        //         break;

        //     usleep(1000);

        //     struct timespec now;
        //     clock_gettime(CLOCK_MONOTONIC, &now);
        //     if ((now.tv_sec > start.tv_sec || (now.tv_sec == start.tv_sec && now.tv_nsec > start.tv_nsec)) &&
        //     framesRead == 0) {
        //         // AAudio has a bug where it won't read any samples until an arbitrary minimum amount of samples have
        //         been written.
        //         // We just wait for a second and if no samples have been read, we'll assume that AAudio is broken.
        //         break;
        //     }
        // }

        // oboe::Result result{self->stream->requestStop()};
        // if (result != oboe::Result::OK) {
        //     std::cerr << "[ALSA Oboe] Failed to stop stream: " << oboe::convertToText(result) << std::endl;
        //     return -1;
        // }

        // oboe::StreamState state{self->stream->getState()};
        // while (state != oboe::StreamState::Stopped) {
        //     result = self->stream->waitForStateChange(state, &state, TimeoutNanoseconds);
        //     if (result != oboe::Result::OK) {
        //         std::cerr << "[ALSA Oboe] Failed to wait for stop: " << oboe::convertToText(result) << std::endl;
        //         return -1;
        //     }
        // }

        return 0;
    }

    static int Pause(snd_pcm_ioplug_t* ext, int enable)
    {
        auto self{static_cast<SnapcastPcm*>(ext->private_data)};
        std::scoped_lock lock{self->mutex};
        LOG(INFO, LOG_TAG) << "Pause, enable: " << enable << "\n";
        // if (!self->stream)
        //     return -EBADFD;

        // oboe::Result result{self->stream->requestPause()};
        // if (result != oboe::Result::OK) {
        //     std::cerr << "[ALSA Oboe] Failed to pause stream: " << oboe::convertToText(result) << std::endl;
        //     return -1;
        // }

        return 0;
    }

    constexpr static snd_pcm_ioplug_callback_t Callbacks{
        .start = &Start,
        .stop = &Stop,
        .pointer = &Pointer,
        .transfer = &Transfer,
        .close = &Close,
        .prepare = &Prepare,
        .drain = &Drain,
        .pause = &Pause,
        .resume = &Start,
    };

public:
    snd_pcm_ioplug_t plug{
        .version = SND_PCM_IOPLUG_VERSION,
        .name = "ALSA <-> Snapcast PCM I/O Plugin",
        .mmap_rw = false,
        .callback = &Callbacks,
        .private_data = this,
    };

    SnapcastPcm()
    {
        LOG(INFO, LOG_TAG) << "Create SnapcastPcm\n";
    }

    int Initialize(const char* name, snd_pcm_stream_t stream, int mode, const SampleFormat& sampleformat,
                   const Uri& uri)
    {
        LOG(INFO, LOG_TAG) << "Initialize name: " << name << ", mode: " << mode
                           << ", sample format: " << sampleformat.toString() << ", uri: " << uri.toString() << "\n";
        this->uri = uri;

        if (stream != SND_PCM_STREAM_PLAYBACK)
            return -EINVAL; // We only support playback for now.

        int err{snd_pcm_ioplug_create(&plug, name, stream, mode)};
        if (err < 0)
            return err;

        auto setParamList{[io = &plug](int type, std::initializer_list<unsigned int> list)
        { return snd_pcm_ioplug_set_param_list(io, type, list.size(), list.begin()); }};

        err = setParamList(SND_PCM_IOPLUG_HW_ACCESS, {SND_PCM_ACCESS_RW_INTERLEAVED});
        if (err < 0)
            return err;

        _snd_pcm_format format = SND_PCM_FORMAT_S16_LE;
        switch (sampleformat.bits())
        {
            case 8:
                format = SND_PCM_FORMAT_S8;
                break;
            case 16:
                format = SND_PCM_FORMAT_S16_LE;
                break;
            case 24:
                format = SND_PCM_FORMAT_S24_LE;
                break;
            case 32:
                format = SND_PCM_FORMAT_S32_LE;
                break;
            default: // TODO: error handling
                format = SND_PCM_FORMAT_S16_LE;
        }

        err = setParamList(SND_PCM_IOPLUG_HW_FORMAT, {static_cast<unsigned int>(format)});
        if (err < 0)
            return err;

        // We could support more than 2 channels, but it's fairly complicated due to channel mappings.
        // err = snd_pcm_ioplug_set_param_minmax(&plug, SND_PCM_IOPLUG_HW_CHANNELS, 1, 2);
        err = snd_pcm_ioplug_set_param_minmax(&plug, SND_PCM_IOPLUG_HW_CHANNELS, sampleformat.channels(),
                                              sampleformat.channels());
        if (err < 0)
            return err;

        // Oboe supports any sample rate with sample rate conversion, but we'll limit it to a reasonable range (8kHz -
        // 192kHz).
        // err = snd_pcm_ioplug_set_param_minmax(&plug, SND_PCM_IOPLUG_HW_RATE, 8000, 192000);
        err = snd_pcm_ioplug_set_param_minmax(&plug, SND_PCM_IOPLUG_HW_RATE, sampleformat.rate(), sampleformat.rate());
        if (err < 0)
            return err;

        // Oboe will decide the period/buffer size internally after starting the stream and it's not a detail that we
        // can expose properly. We set arbitrary values that should be reasonable for most use cases.
        err = snd_pcm_ioplug_set_param_minmax(&plug, SND_PCM_IOPLUG_HW_PERIODS, 2, 4); // 4);
        if (err < 0)
            return err;
        err = snd_pcm_ioplug_set_param_minmax(&plug, SND_PCM_IOPLUG_HW_BUFFER_BYTES, 32 * 1024, 64 * 1024);
        if (err < 0)
            return err;

        return 0;
    }

    ~SnapcastPcm()
    {
        std::scoped_lock lock{mutex};
        stream.reset();
        LOG(INFO, LOG_TAG) << "~SnapcastPcm\n";
    }
};

extern "C"
{
    SND_PCM_PLUGIN_DEFINE_FUNC(snapcast)
    {
        // Note: We don't need to do anything with the config, so we can just ignore it.
        snd_config_iterator_t i, next;
        int err;
        snd_pcm_t* spcm;
        snd_config_t *slave = nullptr, *sconf;
        const char *fname = nullptr, *ifname = nullptr;
        const char* format = nullptr;
        long fd = -1, ifd = -1, trunc = 1;
        long perm = 0600;
        Uri uri("tcp://127.0.0.1:4953");
        SampleFormat sampleformat("44100:16:2");
        AixLog::Filter logfilter(AixLog::Severity::info);
        std::string logfile;

        snd_config_for_each(i, next, conf)
        {
            snd_config_t* n = snd_config_iterator_entry(i);
            const char* id;
            if (snd_config_get_id(n, &id) < 0)
                continue;
            // LOG(INFO, LOG_TAG) << "config id: " << id << "\n";
            if (strcmp(id, "uri") == 0)
            {
                const char* uri_param = nullptr;
                err = snd_config_get_string(n, &uri_param);
                // TODO: error handling
                uri = Uri(uri_param);
                if (!uri.port.has_value())
                    uri.port = 4953;

                if (err < 0)
                {
                }
                continue;
            }

            if (strcmp(id, "sampleformat") == 0)
            {
                const char* sample_param = nullptr;
                err = snd_config_get_string(n, &sample_param);
                // TODO: error handling
                sampleformat = SampleFormat(sample_param);

                if (err < 0)
                {
                }
                continue;
            }

            if (strcmp(id, "logfilter") == 0)
            {
                logfilter = AixLog::Filter();
                const char* param = nullptr;
                err = snd_config_get_string(n, &param);
                auto filters = utils::string::split(param, ',');
                for (const auto& filter : filters)
                    logfilter.add_filter(filter);
                continue;
            }

            if (strcmp(id, "logfile") == 0)
            {
                const char* param = nullptr;
                err = snd_config_get_string(n, &param);
                logfile = param;
                continue;
            }
        }

        if (!logfile.empty())
            AixLog::Log::init<AixLog::SinkFile>(logfilter, logfile);
        else
            AixLog::Log::init<AixLog::SinkNative>("snapstream", logfilter);

        SnapcastPcm* plugin{new (std::nothrow) SnapcastPcm{}};
        if (!plugin)
            return -ENOMEM;

        err = plugin->Initialize(name ? name : "Snapcast PCM", stream, mode, sampleformat, uri);
        if (err < 0)
        {
            delete plugin;
            return err;
        }

        *pcmp = plugin->plug.pcm;
        return 0;
    }

    SND_PCM_PLUGIN_SYMBOL(snapcast);
}