obj-m += unbound_wpr_probe.o

NVIDIA_SRC ?= $(CURDIR)/../open-gpu-kernel-modules
NVIDIA_VERSION ?= $(shell modinfo -F version nvidia)
ifneq ($(wildcard $(NVIDIA_SRC)/kernel-open/Module.symvers),)
obj-m += unbound_gsp_access_probe.o
ccflags-y += -I$(NVIDIA_SRC)/kernel-open/common/inc
ccflags-y += -I$(NVIDIA_SRC)/src/common/sdk/nvidia/inc
ccflags-y += -I$(NVIDIA_SRC)/src/nvidia/arch/nvalloc/unix/include
ccflags-y += -DNV_VERSION_STRING=\"$(NVIDIA_VERSION)\"
KBUILD_EXTRA_SYMBOLS := $(NVIDIA_SRC)/kernel-open/Module.symvers
endif

.PHONY: all clean
all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(CURDIR) CC=clang LD=ld.lld KBUILD_EXTRA_SYMBOLS=$(KBUILD_EXTRA_SYMBOLS) modules

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(CURDIR) CC=clang LD=ld.lld KBUILD_EXTRA_SYMBOLS=$(KBUILD_EXTRA_SYMBOLS) clean
