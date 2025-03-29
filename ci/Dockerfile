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

# Copy source files
COPY Makefile config.cpp config.h rtl_icecast.cpp ./

# Debug: Show files after copy
RUN echo "=== Files after copy ===" && \
    echo "Current directory:" && \
    ls -la && \
    echo "File details:" && \
    file * && \
    echo "File contents:" && \
    for f in Makefile config.cpp config.h rtl_icecast.cpp; do \
        echo "=== First line of $f ==="; \
        if [ -f "$f" ]; then \
            head -n 1 "$f" || echo "Failed to read $f"; \
        else \
            echo "File $f not found"; \
        fi \
    done

# Build the application
RUN echo "=== Starting build ===" && \
    if [ -f Makefile ]; then \
        echo "Makefile exists:" && \
        cat Makefile && \
        make; \
    else \
        echo "Makefile not found in:" && \
        pwd && \
        ls -la; \
        exit 1; \
    fi

# Verify the binary exists and is executable
RUN echo "=== Checking binary ===" && \
    ls -l rtl_icecast && \
    file rtl_icecast && \
    test -x rtl_icecast
