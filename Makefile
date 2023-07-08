# 模块名
MODULE_NAME := my_module

# 源文件
MODULE_SRC := my_module.c

# 编译标志
# EXTRA_CFLAGS += -Werror
ccflags-y+=-std=gnu99

# 内核源码路径
KERNEL_SRC := /lib/modules/$(shell uname -r)/build

# 构建目标
obj-m += $(MODULE_NAME).o

# 构建规则
all:
	make -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	make -C $(KERNEL_SRC) M=$(PWD) clean
