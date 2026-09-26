# Extra tools

## LLVM

`clang-format` is bundled under `tools/clang-format/` and must match the
version used to format the sources (currently 23.1.2) - even a minor mismatch
may reformat differently.

Other tools like `clang-tidy` are not bundled. You may provide them yourself.

### Linux build instructions

The official LLVM release for Linux don't run on all our target systems. Therefore we build it ourself using Rocky 8.

Required software (may be incomplete):

```bash
sudo dnf groupinstall -y "Development Tools"
sudo dnf install -y wget cmake ninja-build
```

```bash
wget https://github.com/llvm/llvm-project/archive/refs/tags/llvmorg-23.1.2.tar.gz
tar xf llvmorg-23.1.2.tar.gz
cd llvm-project-llvmorg-23.1.2
cmake -S llvm -B build -G Ninja  \
    -DLLVM_ENABLE_PROJECTS="clang" \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_TARGETS_TO_BUILD="X86" \
    -DLLVM_DISTRIBUTION_COMPONENTS="clang-format" \
    -DCMAKE_INSTALL_PREFIX="$PWD/install" \
    -DLLVM_ENABLE_ZLIB=OFF \
    -DLLVM_ENABLE_ZSTD=OFF
ninja install-distribution
```

This takes a long time. Resulting binaries are in `llvm-project-llvmorg-23.1.2/install/bin` based on the original path when starting the build.
