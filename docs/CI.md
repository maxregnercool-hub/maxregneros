# CI and Releases

Three workflows drive the project. All run on GitHub-hosted runners — nothing
needs to be installed locally, and no browser is involved at any point.

Drop the three `.yml` files into `.github/workflows/` in your repo. They are
already bundled inside the Magisk module at that path, so a flashed device also
carries its own build recipe.

## `ci.yml` — every push and pull request

| Job | What it does |
| --- | --- |
| `sources` | `sh -n` on **every** `.sh` in the tree. Validates `module.prop` has all six required keys. Checks the five Magisk control files exist. |
| `host-build` | Compiles the client with the **strict** warning set, runs the unit and socket suites, then confirms `--version` reports. |
| `wire-conformance` | Builds `gen_vectors`, emits wire bytes from the C packer, and feeds them to the Python server's parser. |

The strict job is the important one. `-Wshadow` and `-Wcast-align` are what
caught the byte-order mismatch between the C header packer and the Python
`HDR_STRUCT` reader — a defect that silently rejected every frame.

## `build.yml` — matrix build

Builds all three ABIs with the Android NDK:

| ABI | API | Triple |
| --- | --- | --- |
| `arm64-v8a` | 29 | `aarch64-linux-android` |
| `armeabi-v7a` | 21 | `armv7a-linux-androideabi` |
| `x86_64` | 29 | `x86_64-linux-android` |

Each job runs `readelf -h` on the result and asserts **Class** and **Machine**
before uploading, so a toolchain misconfiguration fails the build instead of
shipping a wrong-architecture binary. It also checks the dynamic section, which
should be absent on a static build.

## `release.yml` — tag-driven

Push a tag (`git tag v2.1.0 && git push --tags`) and it:

1. builds all three ABIs in parallel
2. downloads them into `bin/<abi>/mrca-client`
3. runs `tools/assemble_module.sh` to produce the flashable zip
4. attaches it to a GitHub Release with generated notes

## Local equivalents

```sh
# from the repo root
make client                             # host build
make client-test                        # unit + socket suites

# one ABI (needs ANDROID_NDK_HOME)
cd payload/client/src && ABI=arm64-v8a API=29 sh build.sh --ndk

# module zip from whatever happens to be in bin/
sh tools/assemble_module.sh v2.1.0
```

## Why the module works without the binaries

The native client is an optimisation, not a dependency. `bin/maxregner-arch`
probes `ro.product.cpu.abi` at boot and selects the matching binary; when none
is present the shell runtime drives the takeover instead. That is why the module
is flashable immediately and simply gains the native path once CI has run once.

`service.sh` records the resolved path in `state/client.path` so the choice is
made once per boot rather than on every lookup, and falls through to the shell
engine when native mode is requested but no binary exists.
