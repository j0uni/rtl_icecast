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
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /build

# Debug: Show current directory
RUN pwd && echo "Current directory contents before copy:"
RUN ls -la .

# Debug: Show context directory
RUN echo "Context directory contents:"
RUN ls -la /build

# Copy source files
COPY . .

# Debug: Show files after copy
RUN echo "Files after copy:"
RUN ls -la
RUN echo "File contents and permissions:"
RUN file * || true
RUN stat * || true

# Debug: Show environment
RUN echo "Environment:"
RUN env

# Build the application
RUN echo "Starting build..." && make

# Verify the binary exists and is executable
RUN echo "Checking binary..." && \
    ls -l rtl_icecast && \
    file rtl_icecast && \
    test -x rtl_icecast