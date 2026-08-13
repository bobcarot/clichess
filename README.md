# CLI Lichess client
Command Line Interface chess client for Lichess backend written on modern C++.

## Dependencies:
- **LLVM** 17.0.1
- **CMake** 3.28
- **Boost**: Asio, Beast, System
- **OpenSSL** 3.6.3

## Build
```bash
$ mkdir build
$ cmake -B build
$ cmake --build build -j$(nproc)
$ ./build/clichess maia1
```
