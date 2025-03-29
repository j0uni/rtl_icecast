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

# Debug: Show initial state
RUN echo "=== Initial directory state ===" && \
    pwd && \
    echo "Current directory:" && \
    ls -la

# Copy the source files to /app
COPY . /app

# Debug: Show files after copy
RUN echo "=== Files after copy ===" && \
    echo "Current directory:" && \
    ls -la /app && \
    echo "File details:" && \
    file /app/* && \
    echo "File contents:" && \
    for f in /app/Makefile /app/config.cpp /app/config.h /app/rtl_icecast.cpp; do \
        echo "=== First line of $f ==="; \
        if [ -f "$f" ]; then \
            head -n 1 "$f" || echo "Failed to read $f"; \
        else \
            echo "File $f not found"; \
        fi \
    done