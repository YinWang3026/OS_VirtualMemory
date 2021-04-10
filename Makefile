mmu: mmu.c
	gcc -g -Wall mmu.c -o mmu

clean:
	rm -rf mmu *~
