
# DotEnvy-CPP

A small, dependency-free `.env` file parser for modern C++ (`C++17` and later).
Header-only -- copy in one file, or consume it via `CMake`.

## Features
Supports parsing `*.env` files with all of the following:
- Comments (`# ...`) and blank lines
- `export KEY=value` (bash-compatible)
- Single-quoted values (literal, no escape processing) and double-quoted
  values (with `\n`, `\t`, `\r`, `\"`, `\\`, `\$` escapes)
- Inline comments after unquoted values (`TIMEOUT=30 # seconds`)
- Variable expansion: `${VAR}` and `$VAR`, resolved against keys defined
  earlier in the same file, falling back to the process environment
- Windows CRLF line endings and UTF-8 BOM (byte-order marks) handled transparently
- Type-safe access through a single templated `get<T>()`
- Clear separation between a missing key and an invalid value:
  - A missing key returns `std::nullopt`
  - A key that exists but cannot convert to the requested type throws `dotenvy::DotEnvError`
- Detailed parse errors with line numbers, or an opt-in mode that skips malformed lines instead of throwing
- Optional one-call loading directly into the process environment

## Example

```cpp
#include <iostream>
#include <dotenvy/dotenvy.hpp>

int main() {
    dotenvy::DotEnvy env{};
    env.load(".env");   // Only loads the variables into memory, doesn't set them

    /* Suppose .env file has the following:
     *      DB_HOST="localhost"
     *      DB_PORT="NaN"
     *      DEBUG=true
     */

    std::string host  = env.get("DB_HOST", "localhost");   // std::string
    bool debug        = env.get("DEBUG", false);           // bool

    /* A key that exists but doesn't convert throws dotenvy::DotEnvyError.
     * Note the two failure modes are different: a *missing* key gives you
     * std::nullopt (safe to check with if/has_value), while a key that exists
     * but holds the wrong kind of value throws. Don't dereference the optional
     * without checking it first -- that's undefined behavior if the key is
     * simply absent, not an exception you can catch
    */
    try {
        if (auto port = env.get<int>("DB_PORT"))
            std::cout << "DB_PORT = " << port.value() << '\n';
        else
            std::cout << "DB_PORT is not set\n";

    } catch (dotenvy::DotEnvyError& e) {
        std::cerr << "DB_PORT is set but not a valid integer: " << e.what() << '\n';
    }

    return 0;
}
```
> [!NOTE]
> See [`examples/dotenvy_example.cpp`](examples/dotenvy_example.cpp) for a complete walkthrough against [`example.env`](examples/example.env).

## API
| Method | What it does |
|---|---|
| `env.load(path)` | Reads and parses a `.env` file from disk |
| `env.load(stream)` | Same, but from any `std::istream` (useful for tests) |
| `env.load_dotenv(path)` | Loads the file **and** sets each variable in the current process's environment, so `std::getenv` sees them too |
| `env.contains(key)` | `true` if `key` was found in the file |
| `env.get<T>(key)` | Returns the value as `std::optional<T>` -- empty if the key wasn't found |
| `env.get<T>(key)` | Same idea, converted to a type `T` of your choice (`int`, `double`, `bool`, ...) |
| `env.get(key, default_value)` | Returns the value if present, otherwise `default_value` |
| `env.dotenv_values()` | Returns a plain `std::unordered_map<std::string, std::string>` with everything that was parsed |
| `env.apply_to_environment()` | Pushes every loaded value into the process environment manually (called automatically by `load_dotenv`) |


> [!NOTE]
> `get<T>(key)` and `get<T>(key, default_value)` works with `std::string`, `bool`, and any numeric type (`int`, `long`, `double`, ... etc.)

### Error handling

| Situation | Behavior |
|---|---|
| Key not found | `get<T>(key)` returns `std::nullopt`; `get<T>(key, default)` returns `default` |
| Key found, value converts to `T` | Returns the converted value |
| Key found, value does **not** convert to `T` | Throws `dotenvy::DotEnvError` |
| Malformed line during `load()` (bad syntax, invalid key name) | Throws `dotenvy::DotEnvError` with the offending line number, or is skipped if `OnError::Skip` is passed |

## Using DotEnvy in your project

### Option A: Copy the Header

`dotenvy.hpp` has no dependencies beyond the standard library. Copy
[`include/dotenvy/dotenvy.hpp`](include/dotenvy/dotenvy.hpp) into your
project and include it.

### Option B: `CMake FetchContent`

```cmake
include(FetchContent)
FetchContent_Declare(
    dotenvy
    GIT_REPOSITORY https://github.com/0xd3ba/dotenvy-cpp.git
    GIT_TAG v1.0.0
)
FetchContent_MakeAvailable(dotenvy)

target_link_libraries(your_app PRIVATE DotEnvy::dotenvy)
```

### Option C: Install system-wide, then `find_package`

```bash
cmake -B build -DDOTENVY_BUILD_TESTS=OFF -DDOTENVY_BUILD_EXAMPLES=OFF
cmake --build build
sudo cmake --install build
```

```cmake
find_package(DotEnvy REQUIRED)
target_link_libraries(your_app PRIVATE DotEnvy::dotenvy)
```

## Building this repository

```bash
cmake -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

> [!NOTE]
> Tests are built with [Catch2](https://github.com/catchorg/Catch2), fetched automatically through CMake -- no manual setup required.
> Pass `-DDOTENVY_BUILD_TESTS=OFF` or `-DDOTENVY_BUILD_EXAMPLES=OFF` to skip either.

## `*.env` Syntax Reference

```bash
# Comments and blank lines are ignored
NAME=value
export NAME=value          # "export" prefix is accepted and stripped

# Quoting
NAME='literal value'       # single quotes: no escapes, no expansion
NAME="value with \n"       # double quotes: escapes processed
NAME=bare value # comment  # unquoted: trimmed, trailing # starts a comment

# Variable expansion (references earlier keys, then the process environment)
BASE_URL=https://example.com
FULL_URL=${BASE_URL}/v1
FULL_URL=$BASE_URL/v1      # unbraced form also works
```

## License

Released under MIT License - see [LICENSE](LICENSE).
