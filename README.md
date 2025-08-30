


# Market Feed

A lightweight C++ project that simulates a simple exchange feed.  
It includes:

- **Wire protocol** for encoding/decoding order and trade messages
- **Order book** implementation (bid/ask matching, cancels, best quote queries)
- **Engine** (WIP) to handle new orders, cancels, acknowledgements, and trades
- **Tests** using CTest to validate functionality

## Build

This project uses CMake (>= 3.16):

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

## Run Tests

After building, run all tests:

```bash
cd build
ctest --output-on-failure
```

## Next Steps

- Extend the engine for more order types (IOC, GTC, etc.)
- Add market data sequencing and snapshot/replay
- Wire to sockets (TCP/UDP) for real feeds