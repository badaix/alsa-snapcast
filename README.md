# ALSA <-> Snapcast I/O PCM Plugin

## Description

ALSA userspace library ([`alsa-lib`](https://github.com/alsa-project/alsa-lib/)) adding a virtual Snapcast device.

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

- **Basic**: This will only support anything directly exposed by the plugin, that being mono/stereo `S16`/`S24`/`S32` LE @ 8kHz-48kHz audio.

```txt
pcm.!default {
    type snapcast
    uri "tcp://localhost:4953"
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
            }
        }
        format S16_LE
        rate unchanged
        channels unchanged
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
