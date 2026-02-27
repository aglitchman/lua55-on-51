# Lua 5.1 API Compat Layer for Lua 5.5.0 and Luau - Work-in-progress

Drop-in Lua 5.1 C API compatibility on top of **Lua 5.5** and **Luau**.

## Building (Lua 5.5 compat)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build    # produces build/libcompat55.a (or compat55.lib on Windows)
```

To also build the test executable:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCOMPAT55_BUILD_TESTS=ON
cmake --build build    # also produces build/test_lua55
```

## License

MIT — same as [Lua](https://www.lua.org/license.html).
