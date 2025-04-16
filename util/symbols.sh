#!/bin/bash

readelf -S bin/ksym.elf | grep -w .text | head -n 1 | awk '{print "text_ld = 0x" $5 ";"}' > bin/ksym.ld
readelf -S bin/ksym.elf | grep -w .rodata | head -n 1 | awk '{print "rodata_ld = 0x" $5 ";"}' >> bin/ksym.ld
readelf -S bin/ksym.elf | grep -w .data | head -n 1 | awk '{print "data_ld = 0x" $5 ";"}' >> bin/ksym.ld
readelf -S bin/ksym.elf | grep -w .bss | head -n 1 | awk '{print "bss_ld = 0x" $5 ";"}' >> bin/ksym.ld
cat bin/ksym.ld modules/linker.ld > bin/mod.ld