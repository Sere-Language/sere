# Performance Benchmark Results

Generated on **2026-09-20 06:03:28**

This report compares the runtime performance, peak memory usage, and file size of equivalent programs implemented in:

- C
- Sere
- Python

Each implementation was executed **250 times**.

---

## Summary

| Metric | Result |
| --- | --- |
| Fastest average runtime | **Sere** |
| Lowest average memory usage | **Sere** |
| Smallest file size | **Python** |
| Number of runs | **250** |

### Relative performance

Using C as a baseline:

- Sere averaged **0.47x** the runtime of C.
- Python averaged **2.05x** the runtime of C.
- Python averaged **4.37x** the runtime of Sere.

A value close to `1.00x` means the implementations performed similarly.

---

# Runtime Performance

| Language | Average | Fastest | Slowest | Median | Std Dev | Relative |
| --- | --- | --- | --- | --- | --- | --- |
| C | 33.861 ms | 9.262 ms | 410.613 ms | 28.940 ms | 30.032 ms | 2.13x |
| Sere | 15.898 ms | 10.418 ms | 186.216 ms | 14.199 ms | 15.652 ms | 1.00x |
| Python | 69.549 ms | 50.549 ms | 456.355 ms | 66.180 ms | 34.993 ms | 4.37x |

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
| C | 3.89 MB | 3.43 MB | 3.98 MB | 3.97 MB |
| Sere | 3.61 MB | 3.00 MB | 3.77 MB | 3.62 MB |
| Python | 10.87 MB | 10.77 MB | 10.95 MB | 10.86 MB |

Memory measurements represent the process Resident Set Size, or **RSS**, observed while the program was running.

RSS approximates the amount of physical memory currently mapped into the process.

The benchmark polls memory approximately once every millisecond, so extremely short-lived memory peaks may not always be observed.

---

# File Size

| Language | Human-readable size | Bytes |
| --- | --- | --- |
| C | 129.13 KB | 132,228 |
| Sere | 492.50 KB | 504,320 |
| Python | 24.00 B | 24 |

For C and Sere, this represents the compiled executable size.

For Python, this represents the source file size rather than the size of the Python interpreter or its runtime environment.

Because of this, Python's value is **not directly equivalent** to the compiled executable sizes.

---

# Individual Runtime Results

| Run | C | Sere | Python |
| --- | --- | --- | --- |
| 1 | 43.961 ms | 17.442 ms | 84.135 ms |
| 2 | 30.669 ms | 11.905 ms | 71.720 ms |
| 3 | 30.036 ms | 14.564 ms | 65.661 ms |
| 4 | 28.653 ms | 10.637 ms | 61.648 ms |
| 5 | 28.582 ms | 16.261 ms | 58.705 ms |
| 6 | 30.278 ms | 19.923 ms | 60.321 ms |
| 7 | 30.860 ms | 11.671 ms | 63.211 ms |
| 8 | 30.398 ms | 15.250 ms | 62.790 ms |
| 9 | 28.108 ms | 12.258 ms | 59.647 ms |
| 10 | 27.093 ms | 14.718 ms | 65.205 ms |
| 11 | 29.023 ms | 13.806 ms | 70.440 ms |
| 12 | 33.919 ms | 12.708 ms | 60.444 ms |
| 13 | 29.344 ms | 13.735 ms | 58.429 ms |
| 14 | 26.337 ms | 14.011 ms | 93.099 ms |
| 15 | 27.756 ms | 17.792 ms | 52.115 ms |
| 16 | 26.191 ms | 14.994 ms | 56.271 ms |
| 17 | 28.176 ms | 12.528 ms | 56.158 ms |
| 18 | 27.413 ms | 16.392 ms | 57.420 ms |
| 19 | 32.388 ms | 13.082 ms | 53.947 ms |
| 20 | 31.537 ms | 16.568 ms | 52.937 ms |
| 21 | 50.968 ms | 15.888 ms | 54.970 ms |
| 22 | 28.092 ms | 14.664 ms | 58.441 ms |
| 23 | 26.883 ms | 14.893 ms | 53.822 ms |
| 24 | 26.065 ms | 13.420 ms | 54.574 ms |
| 25 | 28.863 ms | 15.045 ms | 53.153 ms |
| 26 | 30.909 ms | 15.253 ms | 54.153 ms |
| 27 | 28.016 ms | 13.954 ms | 52.626 ms |
| 28 | 31.401 ms | 16.739 ms | 53.732 ms |
| 29 | 29.557 ms | 14.320 ms | 55.799 ms |
| 30 | 28.514 ms | 14.339 ms | 54.923 ms |
| 31 | 29.663 ms | 16.121 ms | 56.758 ms |
| 32 | 31.385 ms | 16.766 ms | 54.823 ms |
| 33 | 28.690 ms | 15.453 ms | 51.189 ms |
| 34 | 26.053 ms | 15.801 ms | 52.444 ms |
| 35 | 29.221 ms | 18.424 ms | 59.350 ms |
| 36 | 29.018 ms | 15.529 ms | 57.668 ms |
| 37 | 35.826 ms | 19.606 ms | 55.133 ms |
| 38 | 28.325 ms | 17.508 ms | 51.305 ms |
| 39 | 36.533 ms | 15.692 ms | 57.745 ms |
| 40 | 31.190 ms | 16.930 ms | 56.648 ms |
| 41 | 31.547 ms | 24.690 ms | 55.190 ms |
| 42 | 28.475 ms | 21.097 ms | 51.947 ms |
| 43 | 33.180 ms | 16.166 ms | 53.042 ms |
| 44 | 31.541 ms | 14.029 ms | 56.828 ms |
| 45 | 31.543 ms | 16.405 ms | 62.734 ms |
| 46 | 30.465 ms | 14.933 ms | 53.683 ms |
| 47 | 28.611 ms | 12.913 ms | 56.559 ms |
| 48 | 31.296 ms | 16.256 ms | 51.385 ms |
| 49 | 35.872 ms | 12.426 ms | 53.494 ms |
| 50 | 45.760 ms | 17.834 ms | 56.131 ms |
| 51 | 47.663 ms | 17.391 ms | 57.134 ms |
| 52 | 58.739 ms | 12.927 ms | 55.698 ms |
| 53 | 46.889 ms | 19.416 ms | 54.529 ms |
| 54 | 47.859 ms | 16.373 ms | 52.123 ms |
| 55 | 47.989 ms | 14.332 ms | 57.294 ms |
| 56 | 47.721 ms | 16.793 ms | 53.525 ms |
| 57 | 51.097 ms | 16.383 ms | 55.399 ms |
| 58 | 54.509 ms | 14.801 ms | 52.478 ms |
| 59 | 48.205 ms | 16.462 ms | 54.847 ms |
| 60 | 57.487 ms | 14.204 ms | 59.538 ms |
| 61 | 50.459 ms | 14.398 ms | 52.192 ms |
| 62 | 48.606 ms | 16.866 ms | 52.068 ms |
| 63 | 53.315 ms | 14.423 ms | 57.965 ms |
| 64 | 58.706 ms | 18.681 ms | 52.078 ms |
| 65 | 50.770 ms | 14.276 ms | 55.240 ms |
| 66 | 63.463 ms | 12.937 ms | 56.831 ms |
| 67 | 50.710 ms | 14.850 ms | 54.184 ms |
| 68 | 44.270 ms | 15.004 ms | 55.031 ms |
| 69 | 42.694 ms | 12.759 ms | 59.675 ms |
| 70 | 55.297 ms | 13.120 ms | 53.691 ms |
| 71 | 46.774 ms | 12.467 ms | 55.597 ms |
| 72 | 45.191 ms | 15.043 ms | 50.549 ms |
| 73 | 44.335 ms | 12.103 ms | 53.959 ms |
| 74 | 50.916 ms | 14.832 ms | 60.494 ms |
| 75 | 54.238 ms | 11.769 ms | 61.222 ms |
| 76 | 51.211 ms | 15.505 ms | 53.931 ms |
| 77 | 50.876 ms | 15.718 ms | 59.431 ms |
| 78 | 65.316 ms | 13.023 ms | 59.250 ms |
| 79 | 50.716 ms | 15.721 ms | 55.731 ms |
| 80 | 51.153 ms | 12.981 ms | 63.729 ms |
| 81 | 42.266 ms | 14.407 ms | 61.151 ms |
| 82 | 48.822 ms | 15.492 ms | 66.132 ms |
| 83 | 47.714 ms | 14.239 ms | 62.388 ms |
| 84 | 92.497 ms | 16.527 ms | 68.906 ms |
| 85 | 44.977 ms | 15.424 ms | 67.711 ms |
| 86 | 42.885 ms | 14.238 ms | 64.287 ms |
| 87 | 60.340 ms | 17.475 ms | 60.775 ms |
| 88 | 47.956 ms | 16.451 ms | 62.700 ms |
| 89 | 45.617 ms | 15.717 ms | 56.028 ms |
| 90 | 51.132 ms | 17.525 ms | 65.081 ms |
| 91 | 49.256 ms | 16.073 ms | 61.747 ms |
| 92 | 69.432 ms | 14.624 ms | 70.625 ms |
| 93 | 40.695 ms | 15.985 ms | 62.489 ms |
| 94 | 49.531 ms | 13.437 ms | 62.961 ms |
| 95 | 42.015 ms | 15.061 ms | 67.377 ms |
| 96 | 49.639 ms | 14.090 ms | 59.209 ms |
| 97 | 41.663 ms | 12.375 ms | 70.206 ms |
| 98 | 52.140 ms | 13.106 ms | 63.376 ms |
| 99 | 47.037 ms | 14.351 ms | 59.809 ms |
| 100 | 44.795 ms | 15.428 ms | 73.953 ms |
| 101 | 55.396 ms | 11.464 ms | 108.557 ms |
| 102 | 61.845 ms | 14.591 ms | 100.008 ms |
| 103 | 48.298 ms | 11.842 ms | 84.227 ms |
| 104 | 43.782 ms | 14.184 ms | 78.098 ms |
| 105 | 51.484 ms | 13.976 ms | 84.524 ms |
| 106 | 40.722 ms | 13.054 ms | 76.576 ms |
| 107 | 55.074 ms | 16.402 ms | 79.125 ms |
| 108 | 46.717 ms | 12.349 ms | 79.236 ms |
| 109 | 51.143 ms | 13.527 ms | 79.117 ms |
| 110 | 44.181 ms | 12.711 ms | 65.823 ms |
| 111 | 61.743 ms | 16.768 ms | 71.523 ms |
| 112 | 44.783 ms | 14.421 ms | 66.891 ms |
| 113 | 43.672 ms | 12.996 ms | 74.519 ms |
| 114 | 44.871 ms | 15.336 ms | 68.468 ms |
| 115 | 47.010 ms | 11.369 ms | 63.531 ms |
| 116 | 42.740 ms | 15.160 ms | 68.224 ms |
| 117 | 53.241 ms | 11.857 ms | 64.537 ms |
| 118 | 44.505 ms | 14.085 ms | 68.356 ms |
| 119 | 45.756 ms | 15.042 ms | 64.948 ms |
| 120 | 106.241 ms | 12.270 ms | 76.606 ms |
| 121 | 49.651 ms | 15.256 ms | 73.138 ms |
| 122 | 49.793 ms | 10.418 ms | 70.688 ms |
| 123 | 55.014 ms | 13.738 ms | 64.490 ms |
| 124 | 54.508 ms | 11.296 ms | 71.332 ms |
| 125 | 46.241 ms | 12.452 ms | 63.155 ms |
| 126 | 51.315 ms | 13.148 ms | 64.655 ms |
| 127 | 43.282 ms | 14.230 ms | 66.904 ms |
| 128 | 41.981 ms | 15.604 ms | 63.757 ms |
| 129 | 46.789 ms | 12.719 ms | 65.668 ms |
| 130 | 90.362 ms | 11.810 ms | 70.074 ms |
| 131 | 44.909 ms | 12.226 ms | 71.697 ms |
| 132 | 46.708 ms | 12.268 ms | 69.197 ms |
| 133 | 43.976 ms | 11.670 ms | 67.194 ms |
| 134 | 53.312 ms | 12.490 ms | 72.648 ms |
| 135 | 41.532 ms | 12.329 ms | 71.580 ms |
| 136 | 54.250 ms | 14.164 ms | 67.759 ms |
| 137 | 47.190 ms | 12.005 ms | 63.551 ms |
| 138 | 45.665 ms | 13.783 ms | 72.576 ms |
| 139 | 74.237 ms | 13.913 ms | 66.090 ms |
| 140 | 74.023 ms | 14.019 ms | 64.698 ms |
| 141 | 54.847 ms | 14.821 ms | 68.763 ms |
| 142 | 72.380 ms | 13.204 ms | 70.167 ms |
| 143 | 55.667 ms | 14.327 ms | 65.313 ms |
| 144 | 25.070 ms | 25.025 ms | 63.035 ms |
| 145 | 20.403 ms | 16.220 ms | 72.557 ms |
| 146 | 21.519 ms | 16.610 ms | 66.228 ms |
| 147 | 18.861 ms | 18.685 ms | 70.971 ms |
| 148 | 18.316 ms | 13.360 ms | 62.296 ms |
| 149 | 21.790 ms | 19.577 ms | 67.007 ms |
| 150 | 19.708 ms | 16.552 ms | 75.067 ms |
| 151 | 19.119 ms | 13.311 ms | 67.700 ms |
| 152 | 19.346 ms | 14.035 ms | 65.902 ms |
| 153 | 410.613 ms | 12.991 ms | 423.194 ms |
| 154 | 21.215 ms | 11.716 ms | 138.165 ms |
| 155 | 14.795 ms | 10.946 ms | 74.980 ms |
| 156 | 85.902 ms | 13.637 ms | 73.299 ms |
| 157 | 13.977 ms | 12.552 ms | 68.563 ms |
| 158 | 15.080 ms | 13.143 ms | 70.876 ms |
| 159 | 14.476 ms | 14.139 ms | 69.478 ms |
| 160 | 13.280 ms | 12.092 ms | 68.664 ms |
| 161 | 14.229 ms | 13.781 ms | 66.914 ms |
| 162 | 13.508 ms | 12.380 ms | 71.891 ms |
| 163 | 16.792 ms | 15.969 ms | 63.587 ms |
| 164 | 16.552 ms | 12.813 ms | 72.992 ms |
| 165 | 15.233 ms | 14.259 ms | 66.030 ms |
| 166 | 17.711 ms | 13.511 ms | 65.686 ms |
| 167 | 18.210 ms | 12.271 ms | 70.440 ms |
| 168 | 13.770 ms | 13.741 ms | 60.835 ms |
| 169 | 18.884 ms | 186.216 ms | 60.585 ms |
| 170 | 18.800 ms | 183.408 ms | 68.500 ms |
| 171 | 14.395 ms | 14.143 ms | 62.884 ms |
| 172 | 15.035 ms | 62.409 ms | 83.536 ms |
| 173 | 20.903 ms | 18.159 ms | 72.012 ms |
| 174 | 16.026 ms | 14.013 ms | 67.107 ms |
| 175 | 20.812 ms | 12.367 ms | 69.060 ms |
| 176 | 15.365 ms | 10.668 ms | 68.184 ms |
| 177 | 15.204 ms | 11.027 ms | 66.537 ms |
| 178 | 14.701 ms | 15.317 ms | 66.847 ms |
| 179 | 13.395 ms | 12.485 ms | 69.805 ms |
| 180 | 16.849 ms | 13.014 ms | 62.385 ms |
| 181 | 15.151 ms | 13.405 ms | 63.450 ms |
| 182 | 15.153 ms | 11.638 ms | 72.395 ms |
| 183 | 14.204 ms | 14.044 ms | 68.366 ms |
| 184 | 13.182 ms | 11.123 ms | 71.148 ms |
| 185 | 16.134 ms | 16.012 ms | 83.773 ms |
| 186 | 13.816 ms | 13.208 ms | 106.332 ms |
| 187 | 18.360 ms | 15.430 ms | 82.683 ms |
| 188 | 18.655 ms | 14.827 ms | 86.612 ms |
| 189 | 14.841 ms | 11.669 ms | 76.500 ms |
| 190 | 15.793 ms | 12.707 ms | 73.513 ms |
| 191 | 17.789 ms | 11.898 ms | 75.443 ms |
| 192 | 14.548 ms | 13.530 ms | 74.822 ms |
| 193 | 17.610 ms | 11.786 ms | 78.958 ms |
| 194 | 19.829 ms | 13.869 ms | 68.735 ms |
| 195 | 16.847 ms | 11.928 ms | 75.420 ms |
| 196 | 16.176 ms | 15.175 ms | 89.881 ms |
| 197 | 18.369 ms | 11.137 ms | 75.220 ms |
| 198 | 17.775 ms | 15.771 ms | 79.394 ms |
| 199 | 15.969 ms | 11.791 ms | 77.543 ms |
| 200 | 15.722 ms | 14.544 ms | 89.607 ms |
| 201 | 18.330 ms | 15.189 ms | 77.078 ms |
| 202 | 14.909 ms | 12.348 ms | 78.480 ms |
| 203 | 16.060 ms | 14.864 ms | 70.793 ms |
| 204 | 21.023 ms | 12.324 ms | 80.727 ms |
| 205 | 16.044 ms | 14.858 ms | 73.111 ms |
| 206 | 12.879 ms | 12.250 ms | 74.868 ms |
| 207 | 22.387 ms | 16.642 ms | 72.959 ms |
| 208 | 19.173 ms | 15.016 ms | 73.951 ms |
| 209 | 17.039 ms | 11.815 ms | 71.863 ms |
| 210 | 14.207 ms | 13.625 ms | 76.311 ms |
| 211 | 16.957 ms | 13.394 ms | 75.924 ms |
| 212 | 14.739 ms | 14.865 ms | 73.731 ms |
| 213 | 14.584 ms | 13.029 ms | 456.355 ms |
| 214 | 13.914 ms | 13.702 ms | 89.913 ms |
| 215 | 14.431 ms | 12.012 ms | 78.089 ms |
| 216 | 13.163 ms | 13.178 ms | 77.200 ms |
| 217 | 12.238 ms | 13.528 ms | 73.569 ms |
| 218 | 12.153 ms | 10.665 ms | 69.053 ms |
| 219 | 10.174 ms | 14.784 ms | 66.062 ms |
| 220 | 10.170 ms | 13.447 ms | 68.659 ms |
| 221 | 11.737 ms | 13.822 ms | 72.745 ms |
| 222 | 10.823 ms | 11.975 ms | 65.552 ms |
| 223 | 9.757 ms | 14.194 ms | 67.258 ms |
| 224 | 9.782 ms | 12.884 ms | 69.698 ms |
| 225 | 9.584 ms | 13.484 ms | 76.378 ms |
| 226 | 10.196 ms | 11.220 ms | 65.060 ms |
| 227 | 11.020 ms | 14.476 ms | 63.506 ms |
| 228 | 11.766 ms | 14.295 ms | 63.988 ms |
| 229 | 12.462 ms | 11.881 ms | 66.606 ms |
| 230 | 9.762 ms | 15.071 ms | 64.997 ms |
| 231 | 11.041 ms | 12.378 ms | 71.209 ms |
| 232 | 11.148 ms | 17.092 ms | 68.256 ms |
| 233 | 11.445 ms | 11.532 ms | 62.653 ms |
| 234 | 14.263 ms | 14.976 ms | 67.254 ms |
| 235 | 12.617 ms | 14.380 ms | 64.193 ms |
| 236 | 11.566 ms | 12.366 ms | 67.182 ms |
| 237 | 12.198 ms | 13.267 ms | 68.206 ms |
| 238 | 13.002 ms | 12.406 ms | 61.602 ms |
| 239 | 12.684 ms | 15.590 ms | 71.917 ms |
| 240 | 10.145 ms | 11.883 ms | 67.105 ms |
| 241 | 10.203 ms | 16.406 ms | 66.334 ms |
| 242 | 9.262 ms | 19.761 ms | 68.108 ms |
| 243 | 15.923 ms | 13.480 ms | 65.185 ms |
| 244 | 26.772 ms | 17.417 ms | 67.584 ms |
| 245 | 13.297 ms | 15.743 ms | 66.545 ms |
| 246 | 15.887 ms | 14.276 ms | 73.265 ms |
| 247 | 13.889 ms | 15.450 ms | 65.470 ms |
| 248 | 13.697 ms | 15.761 ms | 67.196 ms |
| 249 | 13.930 ms | 12.474 ms | 68.257 ms |
| 250 | 12.530 ms | 14.733 ms | 66.829 ms |

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
