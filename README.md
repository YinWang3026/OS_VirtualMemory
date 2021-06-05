# Memory Management Unit
Virtual memory is used by the Operating System to efficiently and fairly share limited physical memory among multiple running processes. Here, different policies of selecting the physical memory to dealloc and alloc to a process is simulated using Object Oriented Programming. The policies simulated are FIFO (First In First Out), Random, Clock, Enhanced Second Chance, Aging, and Working Set. The program reads the processes, the instructions, and runs until all processes exits. The results of each policy is displayed at the end.

## Running the Program
1. Compile the cpp file with the Makefile
2. ./mmu –f< num_frames > -a< algo > [-o< OPFSxfya >] inputfile randomfile

- The -OPFS flags are optional flags that prints the states of page table entries and physical frames
- The -xfya flags are optional flags for more runtime information
- The -f flag specifies the number of physical frames availiable 
- The -a flag specifies the policy
  - F=FIFO
  - R=Random
  - C=Clock
  - E=Enhanced Second Chance
  - A=Aging
  - W=Working Set

## Input Files
- Mandatory inputs
- Processes and its virtual memory areas are defined first, and then instructions for access the memory follows
- The random file contains the random intgers for the Random policy