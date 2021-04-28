mmu: mmu.cpp
	g++ -std=c++11 -g -O -Wall mmu.cpp -o mmu
clean:
	rm -rf mmu *~
