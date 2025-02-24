# ALSA <-> Snapcast I/O PCM Plugin

## Description

ALSA userspace library ([`alsa-lib`](https://github.com/alsa-project/alsa-lib/)) adding a virtual Snapcast device, that can be used as input for the [TCP stream source](https://github.com/badaix/snapcast/blob/develop/doc/configuration.md#tcp-server).

Example `snapserver.conf`:

```ini
# Stream settings #############################################################
#
[stream]

source = tcp://127.0.0.1?name=TCP&sampleformat=44100:16:2

...

```

## Milestones

- [ ] Alsa plugin that sends PCM data over TCP
- [ ] New Snapstream protocol
  - [ ] Basic events (play, pause, ...), format information, raw audio stream
  - [ ] Meta information (Track title, album art, ...)
  - [ ] Authentication and authorization

## Installation

```shell
cd <project dir>
mkdir build
cd build
cmake ..
make
sudo cp libasound_module_pcm_snapcast.so /usr/lib/x86_64-linux-gnu/alsa-lib/
```

## Configuration ([`.asoundrc`](https://www.alsa-project.org/wiki/Asoundrc))

- **Basic**: This will only support anything directly exposed by the plugin, defaulting to `44100:16:2`.

Supported parameters:

- `uri` [string, optional]: the url of the TCP server where the audio is sent to (default: `tcp://localhost:4953`)
- `sampleformat` [string, optional]: the supported sample format of this virtual device (default: `44100:16:2`)

```txt
pcm.!default {
    type snapcast
    uri "tcp://localhost:4953"
    sampleformat "44100:16:2"
    hint {
        show {
            @func refer
            name defaults.namehint.basic
        }
        description "Snapcast PCM"
    }
}
```

- **Advanced**: Adding `plug` in front of the plugin allows for any unsupported format, channel or rate to be automatically converted into a supported equivalent.

```txt
pcm.!default {
    type plug
    slave {
        pcm {
            type mmap_emul
            slave.pcm {
                type snapcast
                uri "tcp://localhost:4953"
                sampleformat "44100:16:2"
            }
        }
        format S16_LE
        rate 44100
        channels 2
    }
    hint {
        show {
            @func refer
            name defaults.namehint.basic
        }
        description "Snapcast PCM"
    }
}
```
