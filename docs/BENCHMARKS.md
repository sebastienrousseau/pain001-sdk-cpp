<!-- SPDX-License-Identifier: Apache-2.0 OR MIT -->

# Benchmarks

None are published. Each SDK call is one HTTP request to a pain001
server, which does the validation and generation. The SDK's own work is
building a JSON body and decoding the response, so a timing taken here
would measure the server and the network rather than this library.

If a benchmark is added later, it will state the server version, the
machine, and whether the server ran on the same host, and it will assert
nothing about absolute times.
