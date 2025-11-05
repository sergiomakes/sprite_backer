# Release Process

This document describes how to create a new release for Sprite Backer.

## Automated Release Process

The project uses GitHub Actions to automatically build release binaries for all platforms.

### Creating a Release

1. **Tag the release:**
   ```bash
   git tag -a v1.0.0 -m "Release version 1.0.0"
   git push origin v1.0.0
   ```

2. **GitHub Actions will automatically:**
   - Build release binaries for Windows, Linux, and macOS
   - Create a GitHub release
   - Upload the binaries to the release
   - Generate SHA256 checksums

3. **Edit the release notes** on GitHub to describe the changes

### Manual Trigger

You can also manually trigger the build workflow from the GitHub Actions tab:

1. Go to the "Actions" tab
2. Select "Build Release" workflow
3. Click "Run workflow"
4. Select the branch and click "Run workflow"

This will build the binaries but won't create a release (useful for testing).

## Release Artifacts

The following files are generated for each release:

- `sprite_backer-windows-x64.exe` - Windows executable
- `sprite_backer-linux-x64.tar.gz` - Linux binary (tarball)
- `sprite_backer-macos-universal.tar.gz` - macOS binary (tarball)
- `checksums.txt` - SHA256 checksums for all binaries

## Version Numbering

This project follows [Semantic Versioning](https://semver.org/):

- **MAJOR** version for incompatible API changes
- **MINOR** version for new functionality in a backwards compatible manner
- **PATCH** version for backwards compatible bug fixes

Example: `v1.2.3`

