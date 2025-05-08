void *memcpy(void *restrict dest, const void *restrict src, long n) {
	__asm__ volatile("cld; rep movsb"
	            : "=c"((int){0})
	            : "D"(dest), "S"(src), "c"(n)
	            : "flags", "memory");
	return dest;
}