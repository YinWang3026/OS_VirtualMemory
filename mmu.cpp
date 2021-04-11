#include <iostream> //cout
#include <fstream> //ifstream
#include <string>
#include <vector>
#include <stdio.h>
#include <getopt.h> //Arg parsing
#include <stdlib.h>
#include <deque>

using namespace std;

//Flags


//Macro definitions

//Enum definitions



//Struct definitions

//Class definitions


//Global var
vector<int> randvals; //Vector containg the random integers

//Function prototypes


int main(int argc, char* argv[]) {
    int c;
    int num_frames;
    char algo;
    while ((c = getopt(argc,argv,"f:a:o:")) != -1 )
    {   
        // ./mmu –f<num_frames> -a<algo> [-o<options>] inputfile randomfile
        // options = OPFS
        // S has more options -x-y-f-a
        //Argument parsing
        switch(c) {
        case 'f':
            sscanf(optarg, "%d",&num_frames);
            cout << "num_frames: " << num_frames << endl; 
            break;
        case 'a': 
            sscanf(optarg, "%c",&algo);
            cout << "algo: " << algo << endl; 
            break;
        case 'o':
            char options;
            sscanf(optarg, "%c",&options);
            switch (options) {
                case 'O':
                    cout << "option: " << options << endl; 
                    break;
                case 'P':
                    cout << "option: " << options << endl; 
                    break;
                case 'F':
                    cout << "option: " << options << endl; 
                    break;
                case 'S':
                    cout << "option: " << options << endl;
                    break;
            }
            break;
        }
    }
    //Debug statements, invoked using -v flag
    // vtrace("vflag = %d  tflag = %d eflag = %d\n",vFlag,tFlag,eFlag);
    // vtrace("Scheduler: %s, quantum: %d, maxprio: %d\n",myScheduler->getSchedName().c_str(),
    //     myScheduler->getQuantum(),myScheduler->getMaxprio());

    if ((argc - optind) < 2) { //optind is the index of current argument
        cerr << "Missing input file and rfile\n";
        exit(1);
    }
    //Gettng file names
    char* inputFile = argv[optind];
    char* randomFile = argv[optind+1];
    // vtrace("Input file: %s\trfile: %s\n",inputFile,randomFile);
    
    //Opening random value file
    ifstream rfile(randomFile);
    if (!rfile) {
        cerr << "Could not open the rfile.\n";
        exit(1);
    }
    int r, rsize; //Random int, and total random integer count
    rfile >> rsize; //Reading the size
    while (rfile >> r) {
        randvals.push_back(r); //Populating the random vector
    }
    rfile.close();

    //Opening input file
    ifstream ifile(inputFile);
    if (!ifile) {
        cerr << "Could not open the input file.\n";
        exit(1);
    }

    cout << "ZHello world." << endl;
    ifile.clear();
}

//The random function
int myrandom(int burst) {
    static int ofs = 0;
    if (ofs >= randvals.size()) {
        ofs = 0;
    }
    return 1 + (randvals[ofs++] % burst);
}



// typedef struct { ... } pte_t; // can only be total of 32-bit size and will check on this typedef struct { ... } frame_t;
// frame_t frame_table[MAX_FRAMES];
// pte_t page_table[MAX_VPAGES]; // a per process array of fixed size=64 of pte_t not pte_t pointers !
// class Pager {
// virtual frame_t* select_victim_frame() = 0; // virtual base class
// };
// frame_t *get_frame() {
// frame_t *frame = allocate_frame_from_free_list();
// if (frame == NULL) frame = THE_PAGER->select_victim_frame();
//        return frame;
// }
// while (get_next_instruction(&operation, &vpage)) {
// // handle special case of “c” and “e” instruction
// // now the real instructions for read and write
// pte_t *pte = &current_process.page_table[vpage];// in reality this is done by hardware if ( ! pte->present) {
// // this in reality generates the page fault exception and now you execute // verify this is actually a valid page in a vma if not raise error and next inst
// frame_t *newframe = get_frame();
// //-> figure out if/what to do with old frame if it was mapped
// // see general outline in MM-slides under Lab3 header
// // see whether and how to bring in the content of the access page.
// }
// // check write protection
// // simulate instruction execution by hardware by updating the R/M PTE bits update_pte(read/modify) bits based on operations.
//     }