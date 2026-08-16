# Nova Voice Engine

An experimental real-time voice communication prototype written in C++.

The project explores a simple end-to-end audio pipeline built around PortAudio, Opus, and UDP. It captures microphone input, processes and encodes audio frames, transmits them over UDP, and decodes received frames for playback.

This repository was originally developed as part of earlier peer-to-peer communication experiments and is kept as a standalone reference implementation.

## Overview

The current implementation provides a bidirectional voice pipeline:

```text
Microphone
    |
    v
PortAudio Capture
    |
    v
Audio Processing
    |
    v
Opus Encoder
    |
    v
Packetization
    |
    v
UDP Transport
    |
    v
UDP Receiver
    |
    v
Opus Decoder
    |
    v
Playback Buffer

Each instance can simultaneously send audio to a remote peer and listen for incoming UDP packets on a local port.

Current Features
Full-duplex audio capture and playback using PortAudio
Opus encoding and decoding
10 ms audio frames
Variable-bitrate Opus configuration
Opus DTX and in-band FEC enabled at the codec level
UDP-based packet transport
Sequence-numbered packet representation
Basic MTU-oriented payload slicing
Dedicated receiver thread
Playback buffering
Basic echo-cancellation experiment
Basic noise-suppression experiment
Cross-platform socket handling for Linux and Windows
CMake-based build system
Architecture

The project is divided into several small components:

src/
├── app/
│   ├── application.cpp
│   └── main.cpp
├── audio/
│   └── audio_manager.cpp
├── codec/
│   └── opus_codec.cpp
├── core/
│   └── packet.cpp
├── network/
│   ├── udp_sender.cpp
│   └── udp_receiver.cpp
├── processing/
│   ├── echo_canceller.cpp
│   └── noise_suppressor.cpp
├── streaming/
│   ├── collector.cpp
│   └── slicer.cpp
└── tools/
    └── network_test.cpp
Audio

AudioManager provides microphone capture and speaker playback through PortAudio.

The current configuration processes audio in approximately 10 ms frames.

Codec

OpusCodec wraps the Opus encoder and decoder.

The encoder is configured for voice traffic with:

64 kbps target bitrate
variable bitrate
voice signal optimization
discontinuous transmission (DTX)
Opus in-band FEC
Transport

Voice packets are transmitted directly over UDP.

UdpSender sends serialized packets to a configured peer, while UdpReceiver runs a dedicated receive loop and forwards decoded packet objects to the application layer.

The transport layer is intentionally simple and does not implement congestion control, retransmission, NAT traversal, or a reliable delivery protocol.

Packetization

Encoded audio is passed through a small packetization layer before transmission.

Packets carry monotonically increasing sequence numbers and payload data. The current implementation uses a configurable maximum slice size, with the application using 1200-byte payload slices.

Playback

Received Opus frames are decoded and appended to a protected playback buffer.

When insufficient audio is available, the output callback emits silence rather than blocking the real-time audio callback.

Building
Requirements
C++17-compatible compiler
CMake 3.15+
pkg-config
PortAudio
Opus

On Debian/Ubuntu:

sudo apt install build-essential cmake pkg-config \
    portaudio19-dev libopus-dev
Compile
git clone https://github.com/abkarada/nova_voice_engine.git
cd nova_voice_engine


cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

The main executable is:

build/voice_engine

The repository also builds a small UDP network test utility:

build/network_test
Running

The application expects a remote peer address together with local and remote UDP ports.

Run one instance on each peer with the corresponding addresses and ports.

For exact command-line options:

./build/voice_engine --help
Project Status

This repository is an experimental prototype and is not intended to be a production voice stack.

In particular, the current implementation does not provide:

a complete adaptive jitter buffer
congestion control
reliable packet recovery
cryptographic transport security
NAT traversal
production-grade acoustic echo cancellation
production-grade noise suppression
multipath transport

Some components were created as part of broader experiments and remain intentionally simple or incomplete.

Purpose

The project was primarily built to explore the interaction between:

real-time audio capture
audio codecs
packetization
UDP transport
receiver-side buffering
basic audio processing

It is kept public as a small systems/networking experiment and as a reference for later real-time communication work.
    |
    v
PortAudio Output
