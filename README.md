# kave

Zero-Dependency In-Memory Key-Value Store (RESP Compatible).

kave adalah storage engine in-memory yang ditulis dalam C11 dengan hanya bergantung pada POSIX API standar. Server menggunakan event-driven non-blocking I/O dan kompatibel dengan subset protokol RESP2/RESP3 sehingga dapat diuji dengan klien Redis standar.

## Fitur

- Event-driven non-blocking I/O menggunakan epoll (Linux) dan kqueue (BSD/macOS).
- Parser state-machine penuh untuk RESP2/RESP3 (simple string, error, integer, bulk string, array, null, boolean, double, map).
- Struktur data inti: dynamic string (SDS), hash table open-addressing dengan progressive rehashing, skiplist untuk sorted set, doubly-linked list, integer set dengan encoding variabel.
- Custom memory allocator dengan tracking aktif per-komponen dan statistik global.
- Expiration engine berbasis timer wheel dengan active dan passive expiry.
- Eviction policy LRU dan LFU dengan tracking akses per-key.
- Persistence engine Append-Only File (AOF) dengan mode off, everysec, always, dan rewrite.
- Client connection state machine dengan buffer read/write terkelola.
- Signal handler graceful untuk SIGINT, SIGTERM, dan SIGPIPE.
- Zero memory leak divalidasi melalui AddressSanitizer, UndefinedBehaviorSanitizer, dan Valgrind.
- Kompilasi bebas warning dengan -Wall -Wextra -Wpedantic -Werror -std=c11.
- Seluruh source code bebas komentar dan self-documenting melalui penamaan.

## Arsitektur

    +--------------------+
    |     kave-server    |
    |    (src/main.c)    |
    +---------+----------+
              |
              v
    +---------+----------+     +---------------------+
    |     server.c       |<--->|    event_loop.c     |
    | (lifecycle, init)  |     | (epoll / kqueue)    |
    +---------+----------+     +----------+----------+
              |                           |
              v                           v
    +---------+----------+     +----------+----------+
    |      client.c      |<--->|     socket.c        |
    | (state, buffers)   |     | (POSIX wrappers)    |
    +---------+----------+     +---------------------+
              |
              v
    +---------+----------+     +---------------------+
    |  resp_parser.c     |     |    commands.c       |
    |  (RESP2 / RESP3)   |---->| (GET, SET, ZADD...) |
    +--------------------+     +----+-----------+----+
                                    |           |
                                    v           v
                          +---------+--+   +----+-------+
                          | hash_table |   |  skiplist  |
                          | + rehash   |   | (ZSET)     |
                          +-----+------+   +-----+------+
                                |                |
                                v                v
                          +-----+------+   +-----+------+
                          |    sds     |   |  intset    |
                          +------------+   +------------+
                                |
                                v
                          +-----+------+
                          | allocator  |
                          | (tracked)  |
                          +------------+
                                |
        +-----------------------+---------------------+
        |                       |                     |
        v                       v                     v
    +---+----+          +-------+------+       +------+-------+
    | expire |          |  eviction    |       | persistence  |
    | (TTL)  |          | (LRU / LFU)  |       | (AOF)        |
    +--------+          +--------------+       +--------------+

## Protokol RESP

kave mengimplementasikan subset RESP2 dengan dukungan RESP3 opsional. Setiap perintah dikirim sebagai array bulk string.

Contoh permintaan RESP2:

    *3\r\n
    $3\r\n
    SET\r\n
    $3\r\n
    foo\r\n
    $3\r\n
    bar\r\n

Contoh balasan:

    +OK\r\n

Tipe RESP yang didukung:

- Simple string: +OK\r\n
- Error: -ERR message\r\n
- Integer: :123\r\n
- Bulk string: $5\r\nhello\r\n
- Null bulk: $-1\r\n
- Array: *N\r\n...\r\n
- Null array: *-1\r\n
- Boolean (RESP3): #t\r\n atau #f\r\n
- Double (RESP3): ,3.14\r\n
- Map (RESP3): %N\r\n...

## Perintah yang Diimplementasikan

- GET key
- SET key value
- DEL key [key ...]
- MGET key [key ...]
- INCR key
- EXPIRE key seconds
- ZADD key score member [score member ...]
- ZRANGE key start stop

Perintah tambahan akan ditambahkan pada iterasi berikutnya tanpa mengubah antarmuka protokol.

## Struktur Direktori

    .
    |-- .github/
    |   `-- workflows/
    |       `-- ci.yml
    |-- .gitignore
    |-- CMakeLists.txt
    |-- cmake/
    |   |-- CompilerWarnings.cmake
    |   `-- Sanitizers.cmake
    |-- include/
    |   `-- kave/
    |       |-- allocator.h
    |       |-- client.h
    |       |-- commands.h
    |       |-- config.h
    |       |-- event_loop.h
    |       |-- eviction.h
    |       |-- expire.h
    |       |-- hash_table.h
    |       |-- intset.h
    |       |-- list.h
    |       |-- persistence.h
    |       |-- rehash.h
    |       |-- resp_parser.h
    |       |-- sds.h
    |       |-- server.h
    |       |-- signal.h
    |       |-- skiplist.h
    |       `-- socket.h
    |-- src/
    |   |-- aof.c
    |   |-- allocator.c
    |   |-- client.c
    |   |-- commands.c
    |   |-- event_loop.c
    |   |-- eviction.c
    |   |-- expire.c
    |   |-- hash_table.c
    |   |-- intset.c
    |   |-- list.c
    |   |-- main.c
    |   |-- rehash.c
    |   |-- resp_parser.c
    |   |-- sds.c
    |   |-- server.c
    |   |-- signal.c
    |   |-- skiplist.c
    |   `-- socket.c
    |-- tests/
    |   |-- CMakeLists.txt
    |   |-- benchmark/
    |   |   `-- benchmark.c
    |   |-- integration/
    |   |   `-- test_server.c
    |   `-- unit/
    |       |-- test_hash.c
    |       |-- test_list.c
    |       |-- test_resp.c
    |       |-- test_sds.c
    |       `-- test_skiplist.c
    `-- README.md

## Persyaratan Build

- CMake 3.10 atau lebih baru
- Kompiler C11 (GCC 9+ atau Clang 10+)
- Linux (epoll) atau BSD/macOS (kqueue)
- Valgrind (opsional, untuk memeriksa kebocoran memori)

## Build

    cmake -B build -DCMAKE_BUILD_TYPE=Debug
    cmake --build build -j

Untuk mengaktifkan AddressSanitizer dan UndefinedBehaviorSanitizer:

    cmake -B build -DCMAKE_BUILD_TYPE=Debug -DSANITIZE=ON
    cmake --build build -j

## Menjalankan Server

    ./build/kave-server --port 6380

Opsi baris perintah:

- --port, -p : port listen (default 6380)
- --max-clients, -c : batas koneksi (default 1024)
- --timeout, -t : timeout client dalam detik (default 300)
- --max-memory, -m : batas memori untuk eviction dalam byte
- --aof, -a : mengaktifkan AOF persistence
- --eviction, -e : policy eviction (lru, lfu, none)
- --test : menjalankan self-test dan keluar
- --help, -h : menampilkan bantuan

Contoh mengaktifkan eviction LRU dengan memori terbatas:

    ./build/kave-server --port 6380 --max-memory 134217728 --eviction lru

## Pengujian dengan redis-cli

    redis-cli -p 6380 SET foo bar
    redis-cli -p 6380 GET foo
    redis-cli -p 6380 INCR counter
    redis-cli -p 6380 ZADD myzset 1.5 alice
    redis-cli -p 6380 ZADD myzset 2.5 bob
    redis-cli -p 6380 ZRANGE myzset 0 -1

## Menjalankan Test Suite

Seluruh test diregistrasi dengan CTest:

    cd build
    ctest --output-on-failure

Test individual:

    ./build/test_sds
    ./build/test_hash
    ./build/test_skiplist
    ./build/test_resp
    ./build/test_list
    ./build/test_server

## Memeriksa Kebocoran Memori

    valgrind --leak-check=full --error-exitcode=1 ./build/kave-server --test

Valgrind juga dijalankan otomatis oleh pipeline CI pada setiap push ke branch main.

## Benchmark

Benchmark mengukur throughput operasi hash table dan skiplist:

    ./build/benchmark 1000000

Contoh keluaran:

    kave-server benchmark (iterations=1000000)
    ========================================
    HT SET   :  1000000 ops in 0.412 s = 2427184 ops/sec
    HT GET   :  1000000 ops in 0.318 s = 3144654 ops/sec
    HT DEL   :  1000000 ops in 0.305 s = 3278688 ops/sec
    SL INSERT:  1000000 ops in 0.712 s = 1404494 ops/sec
    SL FIND  :  1000000 ops in 0.658 s = 1519756 ops/sec
    SL TRAV  :  1000000 ops in 0.024 s = 41666666 ops/sec

Nilai aktual bergantung pada perangkat keras. Untuk membandingkan dengan Redis, jalankan redis-benchmark pada instance Redis yang setara:

    redis-benchmark -p 6379 -n 1000000 -t set,get -q

## Continuous Integration

Workflow GitHub Actions pada .github/workflows/ci.yml menjalankan:

- Build dengan GCC pada matrix compiler.
- Build dengan Clang pada matrix compiler.
- Sanitizer AddressSanitizer dan UndefinedBehaviorSanitizer.
- Valgrind leak check dengan error-exitcode aktif.
- Seluruh unit test, integration test, dan self-test server.

## Lisensi

Belum ditentukan. Akan diumumkan sebelum rilis publik pertama.
