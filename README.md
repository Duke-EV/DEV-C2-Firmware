# Duke Electric Vehicle Club Software Development Platform

This repository contains all software for all programmable modules on 
the DEV platform.

## Project Organization

This repository is structured into 2 directories.

### boards

The programmable modules of the DEV. Each directory inside is a PlatformIO project

### libcannetwork

The common library implementing the CANbus messaging specification between all
boards on the network

## Development shortcuts

A top-level `Makefile` is included for common firmware tasks.

### Common commands

- `make lint` — run C++ lint checks across the shared library and board source files
- `make format` — auto-format C++ code using `clang-format`
- `make check-format` — verify formatting without modifying files
- `make build BOARD=throttle` — build a specific PlatformIO board project
- `make upload BOARD=throttle` — build and upload a specific board project

Example:

```bash
make build BOARD=communications
make upload BOARD=throttle
```

If you are using a different board, replace `BOARD` with the project directory name under `boards/`.

## Contributing

Please see CONTRIBUTING.md to learn more about contributing to this repository.