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
RUN ls -la

# Debug: Show root directory
RUN echo "Root directory contents:"
RUN ls -la /

# Debug: Show build context
RUN echo "Attempting to list possible source files:"
RUN find / -name "Makefile" -o -name "config.cpp" -o -name "config.h" -o -name "rtl_icecast.cpp" 2>/dev/null || true

# Copy source files one by one with debug output
RUN echo "Starting file copy process..."
COPY Makefile ./Makefile
RUN echo "Copied Makefile:" && ls -l Makefile
COPY config.cpp ./config.cpp
RUN echo "Copied config.cpp:" && ls -l config.cpp
COPY config.h ./config.h
RUN echo "Copied config.h:" && ls -l config.h
COPY rtl_icecast.cpp ./rtl_icecast.cpp
RUN echo "Copied rtl_icecast.cpp:" && ls -l rtl_icecast.cpp

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