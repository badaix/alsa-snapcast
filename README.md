# ALSA <-> Snapcast I/O PCM Plugin

## Description

ALSA userspace library ([`alsa-lib`](https://github.com/alsa-project/alsa-lib/)) adding a virtual Snapcast device.

## Configuration ([`.asoundrc`](https://www.alsa-project.org/wiki/Asoundrc))

* **Basic**: This will only support anything directly exposed by the plugin, that being mono/stereo `S16`/`S24`/`S32` LE @ 8kHz-48kHz audio.

```txt
pcm.!default {
    type snapcast
    hint {
        show {
            @func refer
            name defaults.namehint.basic
        }
        description "Snapcast PCM"
    }
}
```

* **Advanced**: Adding `plug` in front of the plugin allows for any unsupported format, channel or rate to be automatically converted into a supported equivalent.

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
