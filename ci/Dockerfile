FROM debian:bullseye-slim

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    librtlsdr-dev \
    libshout3-dev \
    libmp3lame-dev \
    libfftw3-dev \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Build and install liquid-dsp from source
RUN cd /tmp && \
    git clone https://github.com/jgaeddert/liquid-dsp.git && \
    cd liquid-dsp && \
    ./bootstrap.sh && \
    ./configure && \
    make && \
    make install && \
    ldconfig && \
    cd / && \
    rm -rf /tmp/liquid-dsp

# Set working directory
WORKDIR /app

# Copy source files
COPY . .

# Build the application
RUN make

# Verify the binary exists and is executable
RUN test -x rtl_icecast
