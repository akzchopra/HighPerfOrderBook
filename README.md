# High-Performance Order Book

A modern C++20 implementation of a high-performance order book system capable of processing up to 1 million orders per second with sub-microsecond latency.

## Requirements

- C++20 or higher
- CMake 3.20 or higher
- MacOS (M1 or higher)

## Building the Project

```bash
mkdir build
cd build
cmake ..
make
./order_book_main
```

## Running Tests

```bash
cd build
ctest
```


## Performance

- Order processing: Up to 1,000,000 orders per second
- Latency: Sub-microsecond

## Implementation Details

### Lock-free Algorithms
### Memory-Mapped File Arrays
### SIMD Operations
### Template Metaprogramming

### SIMD
```bash
Memory Layout:

quantities[4] = {500, 600, 700, 800}
counts[4]     = {10,  20,  30,  40 }
deltas[4]     = {50,  -30, 20,  10 }

After SIMD Loading:

q_vec    = [500|600|700|800]  // One 128-bit register holding 4 quantities
c_vec    = [10 |20 |30 |40 ]  // One 128-bit register holding 4 counts
d_vec    = [50 |-30|20 |10 ]  // One 128-bit register holding 4 deltas
one_vec  = [1  |1  |1  |1  ]  // One 128-bit register holding 4 ones

SIMD operations:

// Add deltas to quantities
q_vec = vaddq_u32(q_vec, vreinterpretq_u32_s32(d_vec));
// Result: [550|570|720|810]

// Increment counts
c_vec = vaddq_u32(c_vec, one_vec);
// Result: [11 |21 |31 |41 ]

All operations happen simultaneously on all 4 values!
```
