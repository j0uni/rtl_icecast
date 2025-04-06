# RTL-SDR Icecast Streamer

<img width="970" alt="rtl_icecast" src="https://github.com/user-attachments/assets/305a31e2-0fc4-49e5-905b-35808e5bc6da" />

A command-line application that uses RTL-SDR to receive FM and AM radio signals and stream them to an Icecast server in MP3 format.

The main target usage for this app is to stream your local HAM FM repeater audio to your public shoutcast/icecast server with just a RTL-SDR USB-receiver and for example Raspberry Pi. It can be used to stream broadcast FM too, of course.

This program also contains a scanner functionality which one can use for example to scan your local airbands or monitor all local HAM radio FM channels, and much more.

The app is native app and uses the RTL-SDR on hardware level directly. No need to pipe or install anything extra. This works out of the box (well, I hope!).

## Features

- Wide and Narrow FM demodulation
- AM (Amplitude Modulation) demodulation
- Scanning functionality (user defined frequency list)
- Adjustable squelch with threshold and hold time
- Low-cut filter with configurable frequency (to cut off FM repeater tone-squelch)
- Real-time status display with signal strength meter
- Automatic reconnection to Icecast server
- MP3 encoding with configurable quality and bitrate
- Configuration via config file or command-line arguments
- Manual gain control
- PPM correction for RTL-SDR oscillator inaccuracy

## Requirements

- RTL-SDR compatible USB dongle
- Icecast server (local or remote)
- Debian/Ubuntu Linux system (or other Linux distributions with equivalent packages)
- macOS with Homebrew (for macOS users)

## Installation

### 1. Install Required Dependencies

#### On Debian/Ubuntu systems:

Install the required libraries:

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

#### On macOS:

Install the required libraries using Homebrew:

```bash
# Install Homebrew if you don't have it already
# /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install librtlsdr liquid-dsp lame libshout
```

### 2. Clone and Build RTL-Icecast

```bash
git clone https://github.com/j0uni/rtl_icecast
cd rtl_icecast
make
```

The build will automatically detect your operating system and use the appropriate compiler flags. All build artifacts will be placed in the `build` directory.

The executable will be located at `build/rtl_icecast`.

## Configuration

Create a `config.ini` file in the same directory as the executable:

```ini
[rtl_sdr]
sample_rate = 1024000

center_freq_mhz = 145.75
gain_mode = 0        ; 0 = automatic gain, 1 = manual gain
tuner_gain = 90      ; in tenths of dB (e.g., 90 = 9.0 dB), only used when gain_mode = 1
ppm_correction = 0   ; frequency correction in PPM (0 = disabled)
fm_mode = narrow     ; wide or narrow

[audio]
audio_rate = 48000
mp3_bitrate = 128
mp3_quality = 2
audio_buffer_seconds = 2

[audio_filters]
lowcut_enabled = true    ; true or false
lowcut_freq = 500.0      ; in Hz, frequencies below this will be attenuated
lowcut_order = 4         ; filter order (higher = steeper cutoff, but more CPU)

[squelch]
enabled = true    ; true or false
threshold = -20.0  ; in dB, signals below this will be muted
hold_time = 500    ; in milliseconds, how long to keep squelch open after signal drops

[icecast]
host = server.com
port = 8000
mount = /rtl-fm-stream.mp3
station_title=station-example
password = your_streamer_password
user = source
protocol = http    ; only http is supported for now
format = mp3
reconnect_attempts = 5
reconnect_delay_ms = 2000 

[scanner]
scan = true ; false
step_delay = 100 ; ms

[scanlist]
ch0 = 127.100,AM,Sector G
ch1 = 118.700,AM,EFTP TWR
ch2 = 126.200,AM,EFTP Radar
ch5 = 119.100,AM,EFHK a
ch6 = 119.900,AM,EFHK b
ch7 = 126.900,AM,Pietari
ch8 = 132.325,AM,Sector M
```

Adjust the values according to your needs:

- `center_freq`: The frequency to tune to in MHz
- `gain_mode`: 0 for auto gain, 1 for manual gain
- `tuner_gain`: Gain value in tenths of dB (e.g., 90 = 9.0 dB), only used when gain_mode = 1
- `ppm_correction`: Frequency correction in Parts Per Million (PPM) to compensate for RTL-SDR oscillator inaccuracy
- `fm_mode`: 
  - `wide` for commercial FM radio (75 kHz deviation)
  - `narrow` for narrow FM (12.5 kHz deviation, used for amateur radio, etc.)
  - `am` for Amplitude Modulation (AM broadcast, aircraft communications, etc.)
- `host`, `port`, `mount`, `user`, `password`: Your Icecast server details

## Usage

```bash
./rtl_icecast [options]
```

### Command-line Options

- `-c, --config <file>`: Use specified config file (default: config.ini)
- `-q, --quiet`: Run (mostly) quietly without regular status output (default: false)
- `-h, --help`: Show help message

## Status Display

Unless started with `--quiet`, the application displays real-time status information:

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

## Linux Systemd Service

Use the file _rtl-icecast.service_ to create a _systemd_ "user" service. Modify this file, specifying the location of your _config.ini_ (default is _$HOME/.config/rtl_icecast/config.ini_). Save _rtl-icecast.service_ in $HOME/.local/share/systemd/user, then:

    systemctl --user daemon-reload

and:

    systemctl --user enable rtl-icecast.service

finally, to start the service:

    systemctl --user start rtl-icecast.service

For extra credit, enable the systemd service to continue running after you log out with:

    sudo loginctl enable-linger <USERNAME>

where \<USERNAME\> is your Linux user name.

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

(C) Jouni OH3CUF 2025,  (C) Jarmo OH3BSG 2025

This project is licensed under the MIT License - see the LICENSE file for details. 

The MIT License is a permissive free software license that allows for the software to be used, copied, modified, merged, published, distributed, sublicensed, and sold. It is one of the most straightforward and widely used open-source licenses. The key conditions are that the original license and copyright notice must be included in all copies or substantial portions of the software. This means that while you can use the software for commercial purposes, you must acknowledge the original creators. The license also provides a disclaimer of warranty, meaning that the software is provided "as is" without any guarantees of performance or reliability.
