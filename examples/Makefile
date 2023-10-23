default: all

LIBCOMPILERRT ?= ~/Workspace/ckb-lua-vm/deps/compiler-rt-builtins-riscv/build/libcompiler-rt.a
BUILDDIR ?= ./build

all: float_conversion_bug

float_conversion_bug: float_conversion_bug.c
	clang -v -fuse-ld=lld --target=riscv64 -march=rv64imc_zba_zbb_zbc_zbs -nostdinc -nostdlib -I ../libc -I ../libc/src -o $(BUILDDIR)/$@ $< $(LIBCOMPILERRT)

clean:
	rm -f $(BUILDDIR)/float_conversion_bug

.PHONY: all float_conversion_bug clean
