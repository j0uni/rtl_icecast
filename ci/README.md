# CI Build Setup for RTL-SDR Icecast Streamer

This directory contains the CI configuration for verifying the build of the RTL-SDR Icecast Streamer.

## Components

- `Dockerfile`: Builds and verifies the application compilation in a Debian-based container
- `workflows/build.yml`: GitHub Actions workflow for CI builds

## GitHub Actions

The CI pipeline will:
1. Build the application in a Docker container
2. Verify that compilation succeeds
3. Cache build dependencies between runs

The workflow runs on:
- Push to main branch
- Pull requests to main branch

## Local Testing

To test the build locally using Docker:

```bash
# From the project root directory:
docker build -t rtl-icecast:test -f ci/Dockerfile .
```

## Notes

- The CI setup only verifies that the application compiles successfully
- Build artifacts are cached using GitHub's cache to speed up subsequent builds
- The build environment uses Debian Bullseye as the base image
