<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# Benchmarks

Every SDK call is one HTTP request to a pain001 server, which does the
validation and generation. The benchmarks separate the two costs:

- **Client-side** (`pain001_bench_client`): building the request and
  decoding the response for each typed method, with no network. A
  `ReplayTransport` returns a response recorded from pain001 0.0.87
  (`benchmarks/responses/`), and the request bodies are the kit's
  fixtures (`credit-transfer.pain.001.001.09.csv` and `.xml`). This is
  the SDK's own work: JSON encoding of the fixture, URL and headers,
  JSON parsing and decoding into the typed struct.
- **Loopback round trip** (`pain001_bench_loopback`): the same methods
  against a `pain001 serve` on 127.0.0.1 through libcurl, with
  `PAIN001_API_KEY` set. Most of this time is the server's. Each
  benchmark skips with a message when `PAIN001_BASE_URL` is unset.

Nothing is asserted: CI smoke-runs both sets on every pull request
(`make bench BENCH_ARGS=--benchmark_min_time=0.01s` in the conformance
job) so they keep compiling and running.

## Method

```sh
make bench BENCH_ARGS="--benchmark_repetitions=5 --benchmark_report_aggregates_only=true"
```

`make bench` builds a Release (`-O3`) tree in `build-bench/` with Google
Benchmark 1.10.0 and runs both binaries. Google Benchmark picks the
iteration count; each benchmark ran five repetitions, and the tables
give the median and the coefficient of variation (CV) across them. Wall
time is what a caller waits; CPU time is the time the benchmark thread
was on a CPU, which for the loopback set excludes the server's work.

## Environment

| | |
| :--- | :--- |
| Date | 2026-11-14 |
| Machine | Apple A18 Pro, 6 cores, 8 GB |
| OS | macOS 27.2 (26B123) |
| Compiler | Apple clang 21.0.0 (clang-2100.1.1.101), Release |
| libcurl | 8.7.1 (system) |
| Server | pain001 0.0.87 (`pain001[api]`), `pain001 serve` on 127.0.0.1, `PAIN001_API_KEY` set |
| Load average during the run | 353.8, 351.5, 309.6 (client), 334.1, 348.8, 311.6 (loopback) |

The machine was heavily loaded by other builds while these ran (load
average above 300 on 6 cores). The CPU-time columns are the ones to
compare: their CV is low. The wall times carry the scheduling noise,
which the CV columns show; read them as an upper bound on an idle
machine, not as a figure to quote.

## Client-side cost, no network

| Method | Median CPU time | CV (CPU) | Median wall time | CV (wall) |
| :--- | ---: | ---: | ---: | ---: |
| `health()` | 1.53 µs | 2.9% | 8.44 µs | 47.2% |
| `validate()` (5-row CSV) | 12.48 µs | 2.1% | 14.65 µs | 38.5% |
| `generate()` (5-row CSV, 4.4 KB XML back) | 48.98 µs | 0.8% | 72.23 µs | 14.9% |
| `generate()` refused (422, `GenerationRefused` thrown and caught) | 65.49 µs | 0.5% | 91.97 µs | 16.6% |
| `check()` (credit-transfer XML) | 22.08 µs | 4.5% | 23.11 µs | 8.4% |
| `faces()` | 6.73 µs | 1.6% | 8.66 µs | 17.7% |
| `project_face()` (Berlin Group, 4.5 KB back) | 79.29 µs | 2.0% | 178.22 µs | 53.7% |
| `request("GET", ...)` | 1.46 µs | 0.9% | 1.84 µs | 70.5% |

## Loopback round trip

| Method | Median wall time | CV (wall) | Median CPU time (client thread) | CV (CPU) |
| :--- | ---: | ---: | ---: | ---: |
| `health()` | 740 µs | 21.9% | 116 µs | 0.9% |
| `validate()` | 4,733 µs | 31.3% | 147 µs | 17.8% |
| `generate()` | 10,254 µs | 12.3% | 215 µs | 4.7% |
| `check()` | 11,931 µs | 42.1% | 203 µs | 12.0% |
| `faces()` | 2,573 µs | 29.7% | 144 µs | 6.4% |
| `project_face()` | 16,529 µs | 37.8% | 314 µs | 2.5% |
| `request("GET", ...)` | 1,548 µs | 33.6% | 117 µs | 2.5% |

## Reading the numbers

- The client's own work is microseconds; a round trip is milliseconds.
  For `generate()`, the SDK's 49 µs of CPU is under 1% of the 10 ms
  round trip on this machine. Optimising the client will not make a
  call noticeably faster; the server's validation and the network
  decide that.
- The client-side cost grows with the size of what is encoded and
  decoded: `generate()` and `project_face()` carry 4 to 5 KB of XML or
  JSON back, `health()` and `request()` under 100 bytes.
- Throwing and catching `GenerationRefused` costs about 16 µs more than
  decoding a success, in this run.

These numbers are from one machine under heavy load, one day. They are
not a promise and should not be compared with another SDK's numbers
measured elsewhere.
