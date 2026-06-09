# mspc

Windows CLI client for MovieStarPlanet authentication and AMF gateway calls.

**made by what | discord : aq2o | github : https://github.com/whattgit**

## About

| | |
|---|---|
| **Language** | C++20 |
| **Platform** | Windows x64 only |
| **Build** | CMake 3.20+, Visual Studio 2022 |
| **Output** | Native CLI (`mspc.exe`) |

The client talks to two stacks:

1. **Nebula OAuth** (HTTPS) — `access_token`, `profile_id`
2. **MSP AMF gateway** (HTTPS + AMF3) — login, `LoadActorDetailsExtended`, etc.

Networking uses **WinInet** (TLS). Crypto uses **Windows Crypt32** (MD5, SHA1, Base64). AMF payloads are encoded/decoded in-house.

## MSP1 backend

The MovieStarPlanet 1 AMF gateway backend is unreliable. HTTP **500** responses, random timeouts, and malformed payloads are common — that is a server-side issue, not a client bug.

The MSP1 team does not maintain this legacy stack properly. `mspc` handles compression, checksums, and ticket formatting correctly; if a call still fails with 500, retry later or assume the gateway is down.

## Why miniz?

[`miniz`](vendor/miniz/miniz.c) is a small single-file zlib/gzip library, vendored under `vendor/miniz/`.

It is required because MSP gateway responses are not always raw AMF bytes:

- Some HTTP bodies arrive as **gzip** or **zlib** even with WinInet decoding enabled
- Some AMF **ByteArray** fields are zlib-compressed inside the payload

Without decompression, the AMF decoder receives binary garbage and login / actor-details calls fail.

Used in:

| File | Purpose |
|------|---------|
| `x7k9_c5d6.cpp` | inflate gzip/zlib HTTP bodies before AMF decode |
| `x7k9_m9p1.cpp` | inflate zlib AMF byte arrays during decode |

No external install needed — `miniz.c` is compiled directly into `mspc.exe`.

## Limitations

Not implemented yet:

- **Proxy** — no HTTP/SOCKS proxy support (direct connection only via WinInet)
- **Config file** — credentials and server must be passed on the command line
- **Other AMF methods** — only login and `LoadActorDetailsExtended` are exposed

## Build

```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

Binary: `build/Release/mspc.exe`

The CLI uses **ANSI colors** on Windows 10+ (Windows Terminal, VS Code, modern PowerShell).

## Usage

```
mspc made by what | discord : aq2o | github : https://github.com/whattgit

usage:
  mspc --login --username <user> --password <pass> --server <FR|UK|...>
  mspc --logandactordetails --username <user> --password <pass> --server <FR|UK|...>

modes:
  --login              ticket, access_token, profile_id
  --logandactordetails login + LoadActorDetailsExtended (compact json)
```

## Source layout

| File | Role |
|------|------|
| `src/app/x7k9_entry.cpp` | CLI entry |
| `src/internal/x7k9_core.hpp` | Internal API |
| `src/internal/x7k9_a3f1.cpp` | AMF encode |
| `src/internal/x7k9_b2e4.cpp` | AMF checksum |
| `src/internal/x7k9_c5d6.cpp` | gzip inflate |
| `src/internal/x7k9_d7f8.cpp` | crypto hash |
| `src/internal/x7k9_e1g2.cpp` | HTTP OAuth |
| `src/internal/x7k9_f3h4.cpp` | JSON parse |
| `src/internal/x7k9_g5i6.cpp` | Nebula auth |
| `src/internal/x7k9_h7j8.cpp` | AMF gateway |
| `src/internal/x7k9_i9k0.cpp` | login parse |
| `src/internal/x7k9_j1l2.cpp` | TLS WinInet |
| `src/internal/x7k9_k3m4.cpp` | ticket header |
| `src/internal/x7k9_m9p1.cpp` | AMF decode |
| `src/internal/x7k9_sxc.hpp` | string crypto |
| `vendor/miniz/miniz.c` | gzip/zlib inflate |

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 (Desktop development with C++)
- CMake 3.20+
