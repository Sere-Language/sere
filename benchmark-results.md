# Performance Benchmark Results

Generated on **2026-09-20 02:44:38**

This report compares the runtime performance, peak memory usage, and file size of equivalent programs implemented in:

- C
- Sere
- Python

Each implementation was executed **150 times**.

---

## Summary

| Metric | Result |
| --- | --- |
| Fastest average runtime | **C** |
| Lowest average memory usage | **Sere** |
| Smallest file size | **Python** |
| Number of runs | **150** |

### Relative performance

Using C as a baseline:

- Sere averaged **1.03x** the runtime of C.
- Python averaged **4.63x** the runtime of C.
- Python averaged **4.52x** the runtime of Sere.

A value close to `1.00x` means the implementations performed similarly.

---

# Runtime Performance

| Language | Average | Fastest | Slowest | Median | Std Dev | Relative |
| --- | --- | --- | --- | --- | --- | --- |
| C | 17.151 ms | 14.069 ms | 22.148 ms | 16.945 ms | 1.596 ms | 1.00x |
| Sere | 17.580 ms | 14.139 ms | 25.887 ms | 17.216 ms | 1.963 ms | 1.03x |
| Python | 79.431 ms | 48.850 ms | 292.791 ms | 77.061 ms | 21.700 ms | 4.63x |

## Runtime interpretation

**Average** represents the mean execution time across all benchmark runs.

**Fastest** is the shortest observed execution time.

**Slowest** is the longest observed execution time.

**Median** is the middle execution time and can be more resistant to occasional operating-system scheduling spikes.

**Std Dev** shows how much timing variation occurred between runs. A lower standard deviation generally indicates more consistent performance.

---

# Memory Usage

| Language | Average | Minimum | Peak | Median |
| --- | --- | --- | --- | --- |
| C | 3.85 MB | 3.59 MB | 3.98 MB | 3.87 MB |
| Sere | 3.66 MB | 3.39 MB | 3.77 MB | 3.72 MB |
| Python | 10.85 MB | 10.76 MB | 10.94 MB | 10.85 MB |

Memory measurements represent the process Resident Set Size, or **RSS**, observed while the program was running.

RSS approximates the amount of physical memory currently mapped into the process.

The benchmark polls memory approximately once every millisecond, so extremely short-lived memory peaks may not always be observed.

---

# File Size

| Language | Human-readable size | Bytes |
| --- | --- | --- |
| C | 130.68 KB | 133,818 |
| Sere | 492.00 KB | 503,808 |
| Python | 24.00 B | 24 |

For C and Sere, this represents the compiled executable size.

For Python, this represents the source file size rather than the size of the Python interpreter or its runtime environment.

Because of this, Python's value is **not directly equivalent** to the compiled executable sizes.

---

# Individual Runtime Results

| Run | C | Sere | Python |
| --- | --- | --- | --- |
| 1 | 21.859 ms | 20.687 ms | 94.126 ms |
| 2 | 18.232 ms | 20.217 ms | 78.840 ms |
| 3 | 15.560 ms | 18.486 ms | 72.313 ms |
| 4 | 18.683 ms | 17.495 ms | 71.303 ms |
| 5 | 16.629 ms | 14.139 ms | 72.550 ms |
| 6 | 16.191 ms | 19.127 ms | 69.790 ms |
| 7 | 14.489 ms | 18.301 ms | 70.915 ms |
| 8 | 16.945 ms | 15.977 ms | 74.015 ms |
| 9 | 15.091 ms | 16.426 ms | 72.216 ms |
| 10 | 16.502 ms | 19.334 ms | 72.859 ms |
| 11 | 16.971 ms | 15.437 ms | 74.729 ms |
| 12 | 18.219 ms | 16.382 ms | 72.373 ms |
| 13 | 15.432 ms | 21.468 ms | 72.333 ms |
| 14 | 17.658 ms | 16.991 ms | 74.251 ms |
| 15 | 16.418 ms | 15.448 ms | 73.031 ms |
| 16 | 15.306 ms | 19.258 ms | 73.748 ms |
| 17 | 17.364 ms | 17.282 ms | 72.036 ms |
| 18 | 17.106 ms | 16.288 ms | 85.721 ms |
| 19 | 16.617 ms | 15.331 ms | 72.120 ms |
| 20 | 16.557 ms | 16.025 ms | 67.359 ms |
| 21 | 18.103 ms | 15.388 ms | 72.768 ms |
| 22 | 17.408 ms | 16.651 ms | 70.427 ms |
| 23 | 16.787 ms | 17.880 ms | 48.850 ms |
| 24 | 17.567 ms | 17.478 ms | 51.118 ms |
| 25 | 16.554 ms | 15.205 ms | 51.005 ms |
| 26 | 14.069 ms | 18.099 ms | 52.628 ms |
| 27 | 20.353 ms | 20.526 ms | 54.723 ms |
| 28 | 18.326 ms | 16.052 ms | 50.079 ms |
| 29 | 17.142 ms | 15.257 ms | 54.789 ms |
| 30 | 15.926 ms | 17.196 ms | 50.919 ms |
| 31 | 19.908 ms | 16.857 ms | 49.796 ms |
| 32 | 16.526 ms | 16.829 ms | 53.885 ms |
| 33 | 16.960 ms | 15.944 ms | 50.515 ms |
| 34 | 16.779 ms | 14.648 ms | 53.858 ms |
| 35 | 17.467 ms | 19.609 ms | 55.605 ms |
| 36 | 15.488 ms | 18.145 ms | 64.522 ms |
| 37 | 15.962 ms | 16.999 ms | 292.791 ms |
| 38 | 18.353 ms | 16.951 ms | 159.903 ms |
| 39 | 14.470 ms | 15.396 ms | 99.151 ms |
| 40 | 16.025 ms | 15.921 ms | 73.902 ms |
| 41 | 17.555 ms | 16.972 ms | 72.937 ms |
| 42 | 16.852 ms | 15.775 ms | 71.167 ms |
| 43 | 16.160 ms | 17.783 ms | 74.656 ms |
| 44 | 17.726 ms | 16.224 ms | 69.096 ms |
| 45 | 15.038 ms | 15.109 ms | 76.395 ms |
| 46 | 16.905 ms | 18.075 ms | 69.610 ms |
| 47 | 16.176 ms | 20.337 ms | 71.804 ms |
| 48 | 16.463 ms | 15.299 ms | 87.351 ms |
| 49 | 15.086 ms | 15.614 ms | 78.627 ms |
| 50 | 16.866 ms | 19.471 ms | 89.883 ms |
| 51 | 16.356 ms | 18.434 ms | 82.991 ms |
| 52 | 14.900 ms | 16.904 ms | 94.480 ms |
| 53 | 17.486 ms | 18.456 ms | 80.260 ms |
| 54 | 16.518 ms | 18.929 ms | 101.907 ms |
| 55 | 17.304 ms | 15.279 ms | 86.225 ms |
| 56 | 15.804 ms | 18.952 ms | 102.003 ms |
| 57 | 16.724 ms | 17.159 ms | 79.872 ms |
| 58 | 16.081 ms | 17.308 ms | 75.883 ms |
| 59 | 18.280 ms | 25.887 ms | 75.790 ms |
| 60 | 17.272 ms | 22.264 ms | 72.768 ms |
| 61 | 17.001 ms | 24.776 ms | 75.635 ms |
| 62 | 16.605 ms | 15.871 ms | 77.356 ms |
| 63 | 16.694 ms | 18.695 ms | 91.202 ms |
| 64 | 16.394 ms | 18.501 ms | 81.688 ms |
| 65 | 15.373 ms | 17.236 ms | 87.210 ms |
| 66 | 16.810 ms | 16.732 ms | 81.649 ms |
| 67 | 15.865 ms | 18.983 ms | 82.330 ms |
| 68 | 15.451 ms | 21.577 ms | 67.420 ms |
| 69 | 18.100 ms | 17.443 ms | 75.059 ms |
| 70 | 22.121 ms | 21.531 ms | 72.074 ms |
| 71 | 16.066 ms | 16.664 ms | 77.613 ms |
| 72 | 17.712 ms | 17.637 ms | 68.967 ms |
| 73 | 22.148 ms | 19.369 ms | 74.309 ms |
| 74 | 18.236 ms | 16.584 ms | 73.343 ms |
| 75 | 16.302 ms | 16.662 ms | 86.604 ms |
| 76 | 17.066 ms | 20.191 ms | 82.244 ms |
| 77 | 17.182 ms | 16.558 ms | 87.051 ms |
| 78 | 15.627 ms | 16.706 ms | 85.362 ms |
| 79 | 16.831 ms | 17.969 ms | 83.320 ms |
| 80 | 17.106 ms | 16.517 ms | 103.146 ms |
| 81 | 16.168 ms | 17.058 ms | 78.218 ms |
| 82 | 16.917 ms | 16.757 ms | 80.685 ms |
| 83 | 15.304 ms | 15.420 ms | 73.094 ms |
| 84 | 18.491 ms | 15.249 ms | 75.728 ms |
| 85 | 14.443 ms | 16.189 ms | 72.337 ms |
| 86 | 17.485 ms | 18.106 ms | 76.607 ms |
| 87 | 16.946 ms | 15.566 ms | 81.773 ms |
| 88 | 15.100 ms | 16.487 ms | 84.844 ms |
| 89 | 17.564 ms | 19.172 ms | 88.881 ms |
| 90 | 16.648 ms | 14.812 ms | 82.455 ms |
| 91 | 16.184 ms | 15.755 ms | 87.665 ms |
| 92 | 17.776 ms | 18.964 ms | 79.468 ms |
| 93 | 16.746 ms | 15.700 ms | 74.652 ms |
| 94 | 17.520 ms | 15.368 ms | 72.729 ms |
| 95 | 16.896 ms | 18.841 ms | 74.312 ms |
| 96 | 19.341 ms | 18.516 ms | 74.714 ms |
| 97 | 16.976 ms | 15.585 ms | 77.168 ms |
| 98 | 15.015 ms | 16.767 ms | 92.167 ms |
| 99 | 15.913 ms | 17.606 ms | 85.177 ms |
| 100 | 17.888 ms | 14.986 ms | 86.289 ms |
| 101 | 15.308 ms | 15.158 ms | 93.390 ms |
| 102 | 16.983 ms | 19.167 ms | 76.847 ms |
| 103 | 21.507 ms | 16.965 ms | 73.326 ms |
| 104 | 16.442 ms | 16.869 ms | 76.034 ms |
| 105 | 20.337 ms | 18.337 ms | 70.523 ms |
| 106 | 16.073 ms | 18.992 ms | 74.070 ms |
| 107 | 16.575 ms | 15.341 ms | 79.563 ms |
| 108 | 15.936 ms | 18.815 ms | 85.257 ms |
| 109 | 16.357 ms | 20.640 ms | 88.089 ms |
| 110 | 19.020 ms | 15.297 ms | 84.388 ms |
| 111 | 19.423 ms | 15.095 ms | 93.937 ms |
| 112 | 15.912 ms | 18.058 ms | 88.867 ms |
| 113 | 17.966 ms | 19.529 ms | 86.314 ms |
| 114 | 16.613 ms | 18.086 ms | 91.566 ms |
| 115 | 18.924 ms | 16.197 ms | 83.352 ms |
| 116 | 21.187 ms | 21.799 ms | 84.900 ms |
| 117 | 21.952 ms | 20.850 ms | 85.419 ms |
| 118 | 17.895 ms | 18.020 ms | 77.670 ms |
| 119 | 17.335 ms | 18.266 ms | 83.188 ms |
| 120 | 21.658 ms | 18.754 ms | 88.574 ms |
| 121 | 20.201 ms | 19.363 ms | 88.257 ms |
| 122 | 17.229 ms | 17.160 ms | 86.657 ms |
| 123 | 17.506 ms | 21.008 ms | 91.395 ms |
| 124 | 18.375 ms | 17.585 ms | 83.318 ms |
| 125 | 17.658 ms | 19.297 ms | 87.911 ms |
| 126 | 16.572 ms | 16.432 ms | 73.155 ms |
| 127 | 15.822 ms | 17.147 ms | 64.962 ms |
| 128 | 18.360 ms | 20.885 ms | 75.667 ms |
| 129 | 18.207 ms | 18.672 ms | 75.636 ms |
| 130 | 17.128 ms | 18.218 ms | 70.143 ms |
| 131 | 18.467 ms | 16.137 ms | 74.605 ms |
| 132 | 17.059 ms | 16.567 ms | 82.048 ms |
| 133 | 17.452 ms | 17.235 ms | 82.299 ms |
| 134 | 17.632 ms | 17.578 ms | 92.890 ms |
| 135 | 15.753 ms | 16.832 ms | 83.935 ms |
| 136 | 17.899 ms | 16.350 ms | 85.774 ms |
| 137 | 15.975 ms | 20.037 ms | 75.570 ms |
| 138 | 17.099 ms | 17.275 ms | 88.792 ms |
| 139 | 16.212 ms | 15.416 ms | 86.752 ms |
| 140 | 15.574 ms | 16.840 ms | 79.582 ms |
| 141 | 17.749 ms | 15.285 ms | 76.953 ms |
| 142 | 17.438 ms | 17.333 ms | 78.430 ms |
| 143 | 15.833 ms | 20.140 ms | 75.259 ms |
| 144 | 17.517 ms | 17.678 ms | 84.528 ms |
| 145 | 19.164 ms | 16.597 ms | 84.210 ms |
| 146 | 14.299 ms | 18.069 ms | 85.208 ms |
| 147 | 19.757 ms | 16.717 ms | 82.620 ms |
| 148 | 18.889 ms | 19.474 ms | 77.780 ms |
| 149 | 17.390 ms | 15.551 ms | 74.228 ms |
| 150 | 15.470 ms | 17.457 ms | 76.644 ms |

These are the raw runtime measurements for every benchmark iteration.

They are useful for identifying outliers, scheduler interference, startup noise, or unusually inconsistent runs.

---

# Benchmark Methodology

Each program is launched as a new operating-system process using Python's `subprocess.Popen`.

Execution time is measured using `time.perf_counter()`.

The timer begins immediately before process creation and ends after the process has terminated and its output has been collected.

Memory consumption is monitored with `psutil` using `process.memory_info().rss`.

The process is sampled approximately every **1 millisecond**.

---

# Important Benchmarking Context

These results measure more than just the execution speed of the program itself.

Because a new process is started for every benchmark run, the measured time also includes some operating-system overhead such as:

- process creation
- executable loading
- dynamic library loading
- runtime initialization
- Python interpreter startup
- process teardown
- stdout and stderr pipe handling

For programs that execute extremely quickly, these costs can represent a significant percentage of total measured runtime.

This is especially important when comparing compiled languages such as C and Sere.

If the actual benchmark workload only takes a fraction of a millisecond, Windows process startup may dominate the result.

---

# Python Comparison Caveat

Python has a larger startup cost because each benchmark run launches a complete Python interpreter.

This benchmark therefore measures:

> Python interpreter startup + script execution

rather than only the execution time of the Python benchmark workload itself.

This is still useful when evaluating short command-line programs, but it should not be interpreted as a pure comparison of language execution speed.

For CPU-bound language benchmarks, a better test is usually to perform the benchmark workload many thousands or millions of times inside each process.

---

# Memory Measurement Caveat

Peak memory is obtained through polling.

Because memory is sampled every millisecond, a memory allocation that exists for less than one polling interval may not be recorded.

The reported peak should therefore be considered an observed peak rather than a guaranteed absolute maximum.

---

# Interpreting Sere vs C

C provides a useful baseline because it compiles directly to native machine code with minimal runtime overhead.

Sere is also compiled, so comparing it to C can help reveal overhead introduced by areas such as:

- generated LLVM IR
- runtime initialization
- garbage collection
- bounds checks
- abstraction layers
- standard library initialization
- allocator behavior
- code generation quality

A Sere result close to C suggests that the generated native code and runtime overhead are relatively small for this workload.

Larger differences should be investigated using larger workloads before drawing conclusions, because startup overhead can heavily distort very short benchmarks.

---

# Recommendations for More Reliable Results

For more serious compiler benchmarking, consider:

1. Increasing the number of runs.
2. Running benchmark workloads many times inside each executable.
3. Performing several warm-up runs before collecting measurements.
4. Testing multiple workloads instead of a single program.
5. Measuring CPU-heavy, allocation-heavy, and I/O-heavy programs separately.
6. Building C and Sere with equivalent optimization levels.
7. Closing unnecessary background applications.
8. Running benchmarks while the system is idle.
9. Comparing generated assembly or LLVM IR for suspicious performance differences.
10. Reporting median and variance alongside average runtime.

For example:

    python perf.py 100

will usually provide a more stable average than a small number of runs.

---

# Benchmark Environment

This report was generated automatically by `perf.py`.

Python executable:

    C:\Python314\python.exe

Platform:

    win32

Benchmark files:

    C:      C:\Users\jackw\OneDrive\Desktop\git-projects\sere\test-c.exe
    Sere:   C:\Users\jackw\OneDrive\Desktop\git-projects\sere\test.exe
    Python: C:\Users\jackw\OneDrive\Desktop\git-projects\sere\test.py

---

*Generated automatically by the Sere benchmark script.*
