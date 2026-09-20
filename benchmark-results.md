# Performance Benchmark Results

Generated on **2026-09-20 05:44:44**

This report compares the runtime performance, peak memory usage, and file size of equivalent programs implemented in:

- C
- Sere
- Python

Each implementation was executed **200 times**.

---

## Summary

| Metric | Result |
| --- | --- |
| Fastest average runtime | **Sere** |
| Lowest average memory usage | **Sere** |
| Smallest file size | **Python** |
| Number of runs | **200** |

### Relative performance

Using C as a baseline:

- Sere averaged **0.78x** the runtime of C.
- Python averaged **3.61x** the runtime of C.
- Python averaged **4.64x** the runtime of Sere.

A value close to `1.00x` means the implementations performed similarly.

---

# Runtime Performance

| Language | Average | Fastest | Slowest | Median | Std Dev | Relative |
| --- | --- | --- | --- | --- | --- | --- |
| C | 31.777 ms | 22.564 ms | 283.628 ms | 28.189 ms | 26.131 ms | 1.29x |
| Sere | 24.711 ms | 16.778 ms | 252.865 ms | 21.743 ms | 20.847 ms | 1.00x |
| Python | 114.772 ms | 85.101 ms | 653.523 ms | 97.663 ms | 84.228 ms | 4.64x |

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
| C | 4.03 MB | 3.76 MB | 4.08 MB | 4.07 MB |
| Sere | 3.75 MB | 2.97 MB | 3.87 MB | 3.79 MB |
| Python | 11.03 MB | 10.84 MB | 11.20 MB | 11.01 MB |

Memory measurements represent the process Resident Set Size, or **RSS**, observed while the program was running.

RSS approximates the amount of physical memory currently mapped into the process.

The benchmark polls memory approximately once every millisecond, so extremely short-lived memory peaks may not always be observed.

---

# File Size

| Language | Human-readable size | Bytes |
| --- | --- | --- |
| C | 130.68 KB | 133,818 |
| Sere | 492.50 KB | 504,320 |
| Python | 24.00 B | 24 |

For C and Sere, this represents the compiled executable size.

For Python, this represents the source file size rather than the size of the Python interpreter or its runtime environment.

Because of this, Python's value is **not directly equivalent** to the compiled executable sizes.

---

# Individual Runtime Results

| Run | C | Sere | Python |
| --- | --- | --- | --- |
| 1 | 34.973 ms | 40.819 ms | 112.946 ms |
| 2 | 28.671 ms | 30.406 ms | 101.935 ms |
| 3 | 27.856 ms | 30.240 ms | 98.183 ms |
| 4 | 25.464 ms | 30.170 ms | 97.078 ms |
| 5 | 26.902 ms | 34.381 ms | 92.762 ms |
| 6 | 24.162 ms | 28.072 ms | 98.651 ms |
| 7 | 24.797 ms | 25.219 ms | 92.388 ms |
| 8 | 27.046 ms | 30.105 ms | 106.538 ms |
| 9 | 26.030 ms | 29.760 ms | 95.832 ms |
| 10 | 26.598 ms | 38.010 ms | 93.902 ms |
| 11 | 25.384 ms | 47.616 ms | 111.600 ms |
| 12 | 31.203 ms | 26.853 ms | 93.972 ms |
| 13 | 24.024 ms | 23.454 ms | 103.926 ms |
| 14 | 26.283 ms | 20.631 ms | 94.666 ms |
| 15 | 27.978 ms | 19.814 ms | 95.232 ms |
| 16 | 29.165 ms | 17.443 ms | 93.786 ms |
| 17 | 24.834 ms | 23.147 ms | 94.029 ms |
| 18 | 24.161 ms | 21.993 ms | 104.633 ms |
| 19 | 32.026 ms | 23.232 ms | 101.073 ms |
| 20 | 24.935 ms | 24.725 ms | 624.441 ms |
| 21 | 28.892 ms | 22.827 ms | 136.526 ms |
| 22 | 29.822 ms | 21.582 ms | 106.304 ms |
| 23 | 29.905 ms | 23.795 ms | 94.250 ms |
| 24 | 28.921 ms | 20.622 ms | 95.973 ms |
| 25 | 27.492 ms | 18.228 ms | 92.008 ms |
| 26 | 28.454 ms | 18.299 ms | 99.729 ms |
| 27 | 23.889 ms | 22.274 ms | 97.353 ms |
| 28 | 25.944 ms | 21.536 ms | 99.770 ms |
| 29 | 24.594 ms | 23.234 ms | 99.681 ms |
| 30 | 24.721 ms | 21.697 ms | 95.400 ms |
| 31 | 28.180 ms | 25.563 ms | 135.432 ms |
| 32 | 26.997 ms | 25.148 ms | 92.890 ms |
| 33 | 25.143 ms | 20.052 ms | 92.537 ms |
| 34 | 28.951 ms | 22.078 ms | 88.978 ms |
| 35 | 28.157 ms | 22.062 ms | 85.101 ms |
| 36 | 23.863 ms | 21.174 ms | 102.284 ms |
| 37 | 25.724 ms | 21.058 ms | 94.339 ms |
| 38 | 28.080 ms | 189.646 ms | 94.323 ms |
| 39 | 27.688 ms | 252.865 ms | 97.437 ms |
| 40 | 25.156 ms | 22.434 ms | 88.085 ms |
| 41 | 23.496 ms | 83.256 ms | 134.862 ms |
| 42 | 23.580 ms | 20.331 ms | 95.741 ms |
| 43 | 26.259 ms | 20.241 ms | 94.440 ms |
| 44 | 25.574 ms | 22.293 ms | 93.377 ms |
| 45 | 24.436 ms | 20.661 ms | 95.354 ms |
| 46 | 24.319 ms | 24.197 ms | 99.679 ms |
| 47 | 28.068 ms | 22.410 ms | 91.616 ms |
| 48 | 26.162 ms | 19.963 ms | 98.328 ms |
| 49 | 24.827 ms | 22.015 ms | 94.099 ms |
| 50 | 25.003 ms | 17.216 ms | 653.523 ms |
| 51 | 29.698 ms | 20.779 ms | 93.111 ms |
| 52 | 25.311 ms | 20.346 ms | 103.520 ms |
| 53 | 27.133 ms | 25.489 ms | 96.504 ms |
| 54 | 24.886 ms | 20.665 ms | 97.876 ms |
| 55 | 27.935 ms | 20.121 ms | 93.187 ms |
| 56 | 28.330 ms | 23.429 ms | 93.301 ms |
| 57 | 28.213 ms | 21.828 ms | 91.129 ms |
| 58 | 27.113 ms | 20.372 ms | 94.827 ms |
| 59 | 27.657 ms | 19.819 ms | 107.302 ms |
| 60 | 22.564 ms | 23.206 ms | 102.462 ms |
| 61 | 30.252 ms | 21.521 ms | 94.403 ms |
| 62 | 25.927 ms | 23.734 ms | 100.791 ms |
| 63 | 27.374 ms | 21.433 ms | 122.400 ms |
| 64 | 29.345 ms | 21.818 ms | 90.948 ms |
| 65 | 27.445 ms | 19.628 ms | 110.169 ms |
| 66 | 23.551 ms | 20.785 ms | 97.497 ms |
| 67 | 24.960 ms | 18.197 ms | 94.330 ms |
| 68 | 29.664 ms | 21.322 ms | 95.052 ms |
| 69 | 24.431 ms | 20.264 ms | 104.845 ms |
| 70 | 25.057 ms | 20.266 ms | 87.459 ms |
| 71 | 32.261 ms | 20.306 ms | 99.425 ms |
| 72 | 25.208 ms | 20.798 ms | 117.195 ms |
| 73 | 32.448 ms | 19.166 ms | 98.327 ms |
| 74 | 32.212 ms | 23.179 ms | 101.197 ms |
| 75 | 30.430 ms | 22.050 ms | 97.963 ms |
| 76 | 26.355 ms | 20.291 ms | 92.975 ms |
| 77 | 27.564 ms | 24.760 ms | 93.416 ms |
| 78 | 31.900 ms | 18.458 ms | 101.885 ms |
| 79 | 32.290 ms | 21.800 ms | 93.202 ms |
| 80 | 27.487 ms | 22.836 ms | 96.368 ms |
| 81 | 28.397 ms | 18.813 ms | 117.617 ms |
| 82 | 27.774 ms | 21.021 ms | 87.268 ms |
| 83 | 29.345 ms | 19.890 ms | 93.979 ms |
| 84 | 27.554 ms | 19.880 ms | 101.092 ms |
| 85 | 29.918 ms | 19.794 ms | 98.873 ms |
| 86 | 38.469 ms | 20.843 ms | 100.804 ms |
| 87 | 41.317 ms | 20.584 ms | 93.176 ms |
| 88 | 27.637 ms | 19.053 ms | 98.978 ms |
| 89 | 27.831 ms | 19.310 ms | 631.790 ms |
| 90 | 27.814 ms | 21.782 ms | 122.529 ms |
| 91 | 34.899 ms | 22.585 ms | 109.402 ms |
| 92 | 27.258 ms | 19.201 ms | 97.462 ms |
| 93 | 28.753 ms | 19.785 ms | 93.990 ms |
| 94 | 30.182 ms | 19.972 ms | 92.560 ms |
| 95 | 33.863 ms | 19.866 ms | 95.752 ms |
| 96 | 28.471 ms | 20.779 ms | 95.249 ms |
| 97 | 30.118 ms | 26.643 ms | 94.501 ms |
| 98 | 31.963 ms | 23.521 ms | 98.344 ms |
| 99 | 27.958 ms | 22.965 ms | 102.662 ms |
| 100 | 29.228 ms | 24.155 ms | 95.410 ms |
| 101 | 33.127 ms | 23.428 ms | 104.145 ms |
| 102 | 34.972 ms | 25.586 ms | 87.127 ms |
| 103 | 26.964 ms | 21.583 ms | 91.210 ms |
| 104 | 27.464 ms | 24.574 ms | 129.393 ms |
| 105 | 28.307 ms | 19.353 ms | 98.577 ms |
| 106 | 26.748 ms | 24.089 ms | 98.219 ms |
| 107 | 33.751 ms | 22.118 ms | 99.227 ms |
| 108 | 30.679 ms | 25.028 ms | 98.967 ms |
| 109 | 27.280 ms | 21.051 ms | 96.135 ms |
| 110 | 26.171 ms | 21.056 ms | 101.097 ms |
| 111 | 27.536 ms | 17.784 ms | 90.313 ms |
| 112 | 27.702 ms | 21.319 ms | 96.560 ms |
| 113 | 30.362 ms | 20.453 ms | 99.657 ms |
| 114 | 29.980 ms | 20.458 ms | 126.187 ms |
| 115 | 26.866 ms | 21.396 ms | 97.199 ms |
| 116 | 26.708 ms | 16.778 ms | 89.614 ms |
| 117 | 29.877 ms | 23.023 ms | 96.479 ms |
| 118 | 32.602 ms | 23.617 ms | 105.578 ms |
| 119 | 274.853 ms | 25.922 ms | 96.025 ms |
| 120 | 283.628 ms | 20.326 ms | 99.816 ms |
| 121 | 29.126 ms | 22.633 ms | 560.060 ms |
| 122 | 126.555 ms | 21.352 ms | 138.345 ms |
| 123 | 54.646 ms | 20.204 ms | 97.983 ms |
| 124 | 32.352 ms | 21.704 ms | 95.620 ms |
| 125 | 36.778 ms | 23.030 ms | 101.904 ms |
| 126 | 27.371 ms | 19.534 ms | 95.864 ms |
| 127 | 27.313 ms | 19.871 ms | 100.436 ms |
| 128 | 30.980 ms | 23.683 ms | 90.461 ms |
| 129 | 27.335 ms | 20.561 ms | 94.173 ms |
| 130 | 26.669 ms | 22.429 ms | 93.917 ms |
| 131 | 29.103 ms | 22.375 ms | 129.592 ms |
| 132 | 28.171 ms | 24.125 ms | 97.865 ms |
| 133 | 29.915 ms | 21.313 ms | 88.001 ms |
| 134 | 32.109 ms | 25.358 ms | 100.673 ms |
| 135 | 31.637 ms | 24.043 ms | 97.588 ms |
| 136 | 25.770 ms | 20.951 ms | 105.986 ms |
| 137 | 24.882 ms | 21.126 ms | 95.516 ms |
| 138 | 29.622 ms | 19.602 ms | 96.930 ms |
| 139 | 29.449 ms | 18.116 ms | 95.424 ms |
| 140 | 29.234 ms | 21.533 ms | 116.406 ms |
| 141 | 29.792 ms | 22.490 ms | 90.415 ms |
| 142 | 27.936 ms | 20.554 ms | 97.737 ms |
| 143 | 29.634 ms | 24.140 ms | 103.438 ms |
| 144 | 30.750 ms | 19.531 ms | 106.124 ms |
| 145 | 32.760 ms | 19.073 ms | 103.595 ms |
| 146 | 26.316 ms | 21.335 ms | 98.445 ms |
| 147 | 26.444 ms | 22.108 ms | 91.094 ms |
| 148 | 27.616 ms | 21.655 ms | 96.108 ms |
| 149 | 33.882 ms | 24.048 ms | 96.006 ms |
| 150 | 34.716 ms | 19.901 ms | 112.509 ms |
| 151 | 30.484 ms | 20.318 ms | 94.405 ms |
| 152 | 28.719 ms | 27.905 ms | 95.255 ms |
| 153 | 32.285 ms | 20.409 ms | 88.556 ms |
| 154 | 33.180 ms | 24.734 ms | 103.098 ms |
| 155 | 28.606 ms | 21.228 ms | 101.635 ms |
| 156 | 30.995 ms | 20.785 ms | 93.320 ms |
| 157 | 28.570 ms | 18.416 ms | 95.617 ms |
| 158 | 30.484 ms | 21.005 ms | 500.891 ms |
| 159 | 28.119 ms | 24.935 ms | 175.383 ms |
| 160 | 28.188 ms | 33.785 ms | 100.995 ms |
| 161 | 30.203 ms | 26.186 ms | 91.734 ms |
| 162 | 25.297 ms | 21.431 ms | 98.472 ms |
| 163 | 25.392 ms | 18.712 ms | 91.684 ms |
| 164 | 28.359 ms | 22.303 ms | 103.426 ms |
| 165 | 27.768 ms | 19.695 ms | 102.032 ms |
| 166 | 27.807 ms | 25.255 ms | 89.972 ms |
| 167 | 33.327 ms | 23.543 ms | 128.351 ms |
| 168 | 32.038 ms | 23.867 ms | 89.082 ms |
| 169 | 29.612 ms | 24.830 ms | 111.075 ms |
| 170 | 29.026 ms | 20.770 ms | 101.286 ms |
| 171 | 28.894 ms | 20.443 ms | 103.812 ms |
| 172 | 25.613 ms | 24.601 ms | 98.720 ms |
| 173 | 32.469 ms | 18.439 ms | 93.540 ms |
| 174 | 26.587 ms | 18.883 ms | 98.321 ms |
| 175 | 27.311 ms | 21.930 ms | 99.433 ms |
| 176 | 28.981 ms | 22.005 ms | 117.249 ms |
| 177 | 29.260 ms | 20.547 ms | 98.738 ms |
| 178 | 28.469 ms | 24.619 ms | 88.714 ms |
| 179 | 32.811 ms | 22.386 ms | 95.465 ms |
| 180 | 26.433 ms | 19.280 ms | 101.854 ms |
| 181 | 25.934 ms | 17.322 ms | 110.010 ms |
| 182 | 35.646 ms | 22.342 ms | 89.610 ms |
| 183 | 33.730 ms | 22.901 ms | 92.462 ms |
| 184 | 28.730 ms | 22.677 ms | 96.338 ms |
| 185 | 29.006 ms | 20.875 ms | 94.277 ms |
| 186 | 29.545 ms | 21.805 ms | 101.074 ms |
| 187 | 25.852 ms | 27.808 ms | 94.595 ms |
| 188 | 28.586 ms | 22.665 ms | 87.930 ms |
| 189 | 27.050 ms | 22.758 ms | 99.022 ms |
| 190 | 28.843 ms | 21.026 ms | 96.577 ms |
| 191 | 32.144 ms | 21.542 ms | 102.769 ms |
| 192 | 36.699 ms | 25.324 ms | 97.292 ms |
| 193 | 30.936 ms | 22.344 ms | 102.406 ms |
| 194 | 28.190 ms | 23.191 ms | 103.420 ms |
| 195 | 27.917 ms | 18.179 ms | 123.776 ms |
| 196 | 33.505 ms | 22.504 ms | 92.148 ms |
| 197 | 27.298 ms | 21.113 ms | 98.537 ms |
| 198 | 29.586 ms | 22.896 ms | 86.925 ms |
| 199 | 27.213 ms | 25.758 ms | 535.373 ms |
| 200 | 40.733 ms | 22.697 ms | 176.556 ms |

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
