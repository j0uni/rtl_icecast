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
    tree \
    file && \
    rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /build

# Debug: Show initial state
RUN echo "=== Initial directory state ===" && \
    pwd && \
    echo "Current directory:" && \
    ls -la

# Copy source files and build script
COPY . .

# Debug: Show initial state
RUN echo "=== Source directory state ===" && \
    pwd && \
    echo "Current directory:" && \
    ls -la

# Make build script executable
RUN chmod +x build_docker.sh

# Run the build script
RUN build_docker.sh
