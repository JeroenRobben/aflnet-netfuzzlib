# AFLNet × netfuzzlib

A fork of [AFLNet](https://github.com/aflnet/aflnet) that drives the target's
network I/O through the [netfuzzlib](https://github.com/JeroenRobben/netfuzzlib)
framework instead of over real sockets.
Everything AFLNet does works unchanged, only the transport is replaced.

## LLVM toolchain

AFLNet's LLVM pass builds against **LLVM ≤ 12**. Skip this step if you already
have `clang`/`llvm-config` ≤ 12 on your `PATH`, otherwise you can get a prebuilt LLVM 12:

```sh
curl -fL -o clang-llvm-12.tar.xz \
  https://github.com/llvm/llvm-project/releases/download/llvmorg-12.0.1/clang+llvm-12.0.1-x86_64-linux-gnu-ubuntu-16.04.tar.xz
mkdir -p /opt/llvm12 && tar xf clang-llvm-12.tar.xz -C /opt/llvm12 --strip-components=1

export PATH="/opt/llvm12/bin:$PATH"       # clang-12 + llvm-config
export AFL_CC="/opt/llvm12/bin/clang"     # afl-clang-fast compiles targets with this
```

## Build

```sh
git clone --recurse-submodules https://github.com/JeroenRobben/aflnet-netfuzzlib
cd aflnet-netfuzzlib

make CC=clang                              # afl-fuzz
make CC=clang -C llvm_mode                 # afl-clang-fast

# netfuzzlib harness
cmake -S nfl-module -B nfl-module/build -DNFL_BUILD_TESTS=OFF
cmake --build nfl-module/build -j          # -> nfl-module/build/libnfl-aflnet.so
```

## Example: fuzzing bftpd 6.7

[bftpd](https://bftpd.sourceforge.net/) is a small FTP daemon.

**1. Build it with instrumentation (+ ASan):**

```sh
tar xzf bftpd-6.7.tar.gz && cd bftpd
AFL_PATH=/path/to/repo AFL_USE_ASAN=1 \
    make CC=/path/to/repo/afl-clang-fast
```

**2. Make a fuzz-friendly config.** bftpd refuses connections
("*Server disabled for security reasons*") unless it can open its log and utmp
files, so point them somewhere writable.

```sh
mkdir -p /tmp/bftpd_run
sed -e 's|^  PORT="21"|  PORT="2200"|' \
    -e 's|^  LOGFILE=.*|  LOGFILE="/tmp/bftpd_run/bftpd.log"|' \
    -e 's|^  PATH_BFTPDUTMP=.*|  PATH_BFTPDUTMP="/tmp/bftpd_run/bftpdutmp"|' \
    bftpd.conf > /tmp/bftpd_fuzz.conf
```

**3. Seed**:

```sh
mkdir -p in
printf 'USER anonymous\r\nPASS a@b.c\r\nSYST\r\nPWD\r\nLIST\r\nQUIT\r\n' > in/s1
```

**4. Fuzz**:

```sh
AFL_PRELOAD=/path/to/repo/nfl-module/build/libnfl-aflnet.so \
/path/to/repo/afl-fuzz -i in -o out -N tcp://127.0.0.1/2200 -P FTP -E -m none -t 1000 -d \
    -- ./bftpd/bftpd -D -c /tmp/bftpd_fuzz.conf
```

As non-root with the default config this only reaches the **pre-auth** surface:
bftpd `chroot`/`setuid`s on login, so `USER`/`PASS` fail. To fuzz the
authenticated commands (much more coverage), either run as **root**, or let
login succeed without privileges by adding to the config:

```sh
printf 'ftpuser * ftp /tmp/bftpd_run\n' > /tmp/bftpd_auth   # user, "*" = any password, group, home
sed -i -e 's|^  DO_CHROOT="yes"|  DO_CHROOT="no"|' \
       -e 's|^  #FILE_AUTH=.*|  FILE_AUTH="/tmp/bftpd_auth"|' /tmp/bftpd_fuzz.conf
```

and seeding a logged-in session (`printf 'USER ftpuser\r\nPASS x\r\nSYST\r\nPWD\r\nQUIT\r\n' > in/s1`).

## Delayed forkserver

Optionally, set `__AFL_DEFER_FORKSRV=1` to put AFL's forkserver at the 
[latest-safe-point](https://github.com/JeroenRobben/netfuzzlib/tree/main/examples/afl#delayed-forkserver) to increase fuzzing throughput.

```sh
AFL_PRELOAD=/path/to/repo/nfl-module/build/libnfl-aflnet.so __AFL_DEFER_FORKSRV=1 \
/path/to/repo/afl-fuzz -i in -o out -N tcp://127.0.0.1/2200 -P FTP -E -m none -t 1000 -d \
    -- ./bftpd/bftpd -D -c /tmp/bftpd_fuzz.conf
```
