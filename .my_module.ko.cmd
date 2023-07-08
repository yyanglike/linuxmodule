cmd_/root/company/my_module.ko := ld -r -m elf_x86_64 -T ./scripts/module-common.lds --build-id  -o /root/company/my_module.ko /root/company/my_module.o /root/company/my_module.mod.o
