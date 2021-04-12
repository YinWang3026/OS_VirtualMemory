#include <iostream> //cout
#include <fstream> //ifstream
#include <sstream> //String stream
#include <string>
#include <vector>
#include <stdio.h>
#include <getopt.h> //Arg parsing
#include <stdlib.h>
#include <deque>

using namespace std;

//Constants
const int MAX_VPAGES = 64;
const int MAX_FRAMES = 128;

//Flags
char algo; //-a
int num_frames; //-f
int OFlag = 0; //-o
int PFlag = 0;
int FFlag = 0;
int SFlag = 0;
int xFlag = 0;
int yFlag = 0;
int fFlag = 0;
int aFlag = 0;

//Macro definitions
#define vtrace(fmt...)  do { if (vFlag) { printf(fmt); fflush(stdout); } } while(0)

//Enum definitions

//Struct definitions
// typedef struct {
    
// } pte_t; //Page table entries - must be 32 bits

// typedef struct {

// } frame_t; //Frames - must be 32 bits

//Class definitions
// class Pager {
//     virtual frame_t* select_victim_frame() = 0; // virtual base class
// };
// frame_t* get_frame() {
//     frame_t *frame = allocate_frame_from_free_list();
//     if (frame == NULL) frame = THE_PAGER->select_victim_frame();
//         return frame;
// }
// pte_t page_table[MAX_VPAGES]; // a per process array of fixed size=64 of pte_t not pte_t pointers !
// frame_t frame_table[MAX_FRAMES]; //All the frames

//Global var
vector<int> randvals; //Vector containg the random integers

//Function prototypes
int myrandom(int); //The random function
void simulation(); //Simulation

int main(int argc, char* argv[]) {
    int c;
    while ((c = getopt(argc,argv,"f:a:o:")) != -1 )
    {   
        // ./mmu –f<num_frames> -a<algo> [-o<options>] -x -y -f -a inputfile randomfile
        // options = OPFSxfya
        //Argument parsing
        switch(c) {
            case 'f':
                sscanf(optarg, "%d",&num_frames);
                if (num_frames >= MAX_FRAMES) {
                    cerr << "num_frames >= 128: " << num_frames << endl;;
                    exit(1);
                }
                break;
            case 'a': 
                sscanf(optarg, "%c",&algo);
                break;
            case 'o':
                for (int i = 0; optarg[i] != '\0'; i++){
                    // printf("O: %c\n", optarg[i]);
                    switch (optarg[i]) {
                        case 'O':
                            OFlag = 1;
                            break;
                        case 'P':
                            PFlag = 1;
                            break;
                        case 'F':
                            FFlag = 1;
                            break;
                        case 'S':
                            SFlag = 1;
                            break;
                        case 'x': //Prints the current page table after each instruction
                            xFlag = 1;
                            break;
                        case 'y': //Prints the page table of all processes after each instruction
                            yFlag = 1;
                            break;
                        case 'f': //Prints the frame table after each instruction
                            fFlag = 1;
                            break;
                        case 'a': //Prints "aging" information during victim_selection, and after each instruction for complex algo
                            aFlag = 1;
                            break;
                    }
                }
                break;
        }        
    }
    //Arguments statements
    printf("frames[%d] algo[%c] O[%d] P[%d] F[%d] S[%d] x[%d] y[%d] f[%d] a[%d]\n", num_frames, algo, OFlag, PFlag, FFlag, SFlag, xFlag, yFlag, fFlag, aFlag);

    if ((argc - optind) < 2) { //optind is the index of current argument
        cerr << "Missing input file and rfile\n";
        exit(1);
    }
    //Gettng file names
    char* inputFile = argv[optind];
    char* randomFile = argv[optind+1];
    printf("Input file: %s\trfile: %s\n",inputFile,randomFile);
    
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
    int nProcs, nVAM, start_vpage, end_vpage, write_protected, file_mapped, instNum;
    char inst;
    string line;
    while(getline(ifile, line)){
        if (line.empty() || line[0] == '#') {
            continue; //Ignoring empty and # lines
        }
        istringstream iss(line);
        iss >> nProcs; //number of procs
        break;
    }
    printf("nProc[%d]\n",nProcs);

    int i = 0;
    while (i < nProcs){
        getline(ifile, line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        istringstream iss(line);
        iss >> nVAM; //Get VAM number
        printf("nVAM[%d]\n",nVAM);
        int j = 0;
        while (j < nVAM) {
            getline(ifile, line);
            if (line.empty() || line[0] == '#') {
                continue;
            }
            istringstream iss(line);
            iss >> start_vpage >> end_vpage >> write_protected >> file_mapped;
            printf("start[%d] end[%d] write_p[%d] file_mapped[%d]\n",start_vpage,end_vpage,write_protected,file_mapped);
            j++;
        }
        i++;
    }  
    while(getline(ifile, line)){ //Instructions
        if (line.empty() || line[0] == '#') {
            continue;
        }
        istringstream iss(line);
        iss >> inst >> instNum; //Get instructions
        // printf("inst[%c] instNum[%d]\n", inst, instNum);
    }


    ifile.close();
}

//The random function
int myrandom(int burst) {
    static int ofs = 0;
    if (ofs >= randvals.size()) {
        ofs = 0;
    }
    return 1 + (randvals[ofs++] % burst);
}

void simulation(){

    // while (get_next_instruction(&operation, &vpage)) {
    //     // handle special case of “c” and “e” instruction
    //     // now the real instructions for read and write
    //     pte_t *pte = &current_process.page_table[vpage];// in reality this is done by hardware
    //     if ( ! pte->present) {
    //         // this in reality generates the page fault exception and now you execute // verify this is actually a valid page in a vma if not raise error and next inst
    //         frame_t *newframe = get_frame();
    //         //-> figure out if/what to do with old frame if it was mapped
    //         // see general outline in MM-slides under Lab3 header
    //         // see whether and how to bring in the content of the access page.
    //     }
    //     // check write protection
    //     // simulate instruction execution by hardware by updating the R/M PTE bits update_pte(read/modify) bits based on operations.
    // }
}




