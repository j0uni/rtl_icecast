# CI Build Setup for RTL-SDR Icecast Streamer

This directory contains the CI configuration for verifying the build of the RTL-SDR Icecast Streamer.

## Components

- `Dockerfile`: Builds and verifies the application compilation in a Debian-based container
- `build_docker.sh`: Build script that provides verbose output and validation
- `.github/workflows/ci-compile.yml`: GitHub Actions workflow for CI builds

## GitHub Actions

The CI pipeline will:
1. Build the application in a Docker container
2. Verify that compilation succeeds with verbose output
3. Run the container to ensure it starts properly

The workflow runs on:
- Push to main branch
- Pull requests to main branch

## Local Testing

To test the build locally using Docker, run from the project root directory:

```bash
# Build the Docker image
docker build -t rtl-icecast:latest -f ci/Dockerfile .

# Clean up any existing container
docker rm -f rtl-icecast || true

# Run the container
docker run -d --name rtl-icecast rtl-icecast:latest

# View build logs
docker logs rtl-icecast
```

## Notes

- The build script provides detailed information about:
  - Build environment and system information
  - Source file verification
  - Compilation process with VERBOSE=1
  - Binary validation and dependencies
- The build environment uses Debian Bullseye as the base image
- Required dependencies:
  - build-essential, cmake, git
  - librtlsdr-dev
  - libshout3-dev
  - libmp3lame-dev
  - libliquid-dev
  - libfftw3-dev
