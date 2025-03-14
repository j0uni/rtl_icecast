# RTL-SDR Icecast Streamer

<img width="970" alt="rtl_icecast" src="https://github.com/user-attachments/assets/305a31e2-0fc4-49e5-905b-35808e5bc6da" />

A command-line application that uses RTL-SDR to receive FM radio signals and stream them to an Icecast server in MP3 format.

The main target usage for this app is to stream your local HAM FM repeater audio to your public shoutcast/icecast server with just a RTL-SDR USB-receiver and for example Raspberry Pi. It can be used to stream broadcast FM too, of course.

The app is native app and uses the RTL-SDR on hardware level directly. No need to pipe or install anything extra. This works out of the box (well, I hope!).

## Features

- Wide and Narrow FM demodulation
- Adjustable squelch with threshold and hold time
- Low-cut filter with configurable frequency (to cut off FM repeater tone-squelch)
- Real-time status display with signal strength meter
- Automatic reconnection to Icecast server
- MP3 encoding with configurable quality and bitrate
- Configuration via config file or command-line arguments

## Requirements

- RTL-SDR compatible USB dongle
- Icecast server (local or remote)
- Debian/Ubuntu Linux system (or other Linux distributions with equivalent packages)

## Installation

### 1. Install Required Dependencies

On Debian/Ubuntu systems, install the required libraries:

```bash
sudo apt update
sudo apt install build-essential cmake git \
    librtlsdr-dev libshout3-dev libmp3lame-dev \
    libliquid-dev libfftw3-dev
```

If libliquid is not available in your distribution's repositories, you can build it from source:

```bash
git clone https://github.com/jgaeddert/liquid-dsp.git
cd liquid-dsp
./bootstrap.sh
./configure
make
sudo make install
sudo ldconfig
```

### 2. Clone and Build RTL-Icecast

```bash
git clone [THIS_REPO](https://github.com/j0uni/rtl_iceccast
cd rtl_icecast
make
```


## Configuration

Create a `config.ini` file in the same directory as the executable:

```ini
[rtl_sdr]
sample_rate = 1024000
center_freq = 99.9
gain_mode = 0

[audio]
audio_rate = 48000
wide_fm = true

[mp3]
bitrate = 128
quality = 2

[icecast]
host = localhost
port = 8000
mount = /fm.mp3
user = source
password = hackme
reconnect_attempts = 10
reconnect_delay_ms = 5000

[squelch]
enabled = false
threshold = -30
hold_time = 500

[lowcut]
enabled = false
freq = 300
order = 5
```

Adjust the values according to your needs:

- `center_freq`: The FM frequency to tune to in MHz
- `gain_mode`: 0 for auto gain, 1 for manual gain
- `wide_fm`: true for commercial FM radio, false for narrow FM (amateur radio, etc.)
- `host`, `port`, `mount`, `user`, `password`: Your Icecast server details

## Usage

```bash
./rtl_icecast [options]
```

### Command-line Options

- `-c, --config <file>`: Use specified config file (default: config.ini)
- `-h, --help`: Show help message

## Status Display

The application displays real-time status information:

```
[rtl_icecast] 99.900 MHz | WFM | Squelch: OFF | Buffer: 2.000s | Signal: [########        ] -15.234 dB | mp3-Queue: 2/10 | Last: 4096 bytes | Connected
```

This shows:
- Current frequency
- FM mode (WFM or NFM)
- Squelch status
- Audio buffer size
- Signal strength with visual meter
- MP3 queue status
- Last packet size
- Icecast connection status

## Troubleshooting

### RTL-SDR Device Not Found

Ensure your RTL-SDR device is properly connected and recognized:

```bash
rtl_test
```

If you get a "device or resource busy" error, check if any other application is using the device.


## Setting Up Icecast Server

If you don't have an Icecast server, you can set one up on your local machine:

```bash
sudo apt install icecast2
```

During installation, you'll be prompted to configure the server. You can also edit the configuration file later:

```bash
sudo nano /etc/icecast2/icecast.xml
```

Key settings to check:
- `<source-password>`: Password for source clients (like rtl_icecast)
- `<admin-password>`: Password for admin access
- `<hostname>`: Your server's hostname
- `<port>`: The port Icecast will listen on (default: 8000)

Start the Icecast server:

```bash
sudo systemctl start icecast2
```

To make it start automatically at boot:

```bash
sudo systemctl enable icecast2
```

You can access the Icecast web interface at http://localhost:8000

## License

(C) Jouni OH3CUF 2025

This project is licensed under the MIT License - see the LICENSE file for details. 

The MIT License is a permissive free software license that allows for the software to be used, copied, modified, merged, published, distributed, sublicensed, and sold. It is one of the most straightforward and widely used open-source licenses. The key conditions are that the original license and copyright notice must be included in all copies or substantial portions of the software. This means that while you can use the software for commercial purposes, you must acknowledge the original creators. The license also provides a disclaimer of warranty, meaning that the software is provided "as is" without any guarantees of performance or reliability.
