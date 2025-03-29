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
    libliquid-dev \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /build

# Copy source files
COPY . /build/

# List files to debug
RUN ls -la

# Build the application
RUN make

# Verify the binary exists and is executable
RUN test -x rtl_icecast
