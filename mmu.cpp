#include <iostream> //cout
#include <fstream> //ifstream
#include <sstream> //String stream
#include <string>
#include <vector>
#include <stdio.h>
#include <getopt.h> //Arg parsing
#include <stdlib.h>
#include <queue>

using namespace std;

//Constants
const int MAX_VPAGES = 64; //Total virtual pages per process
const int MAX_FRAMES = 128; //Supports up to 128 total possible physical frames

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
// #define vtrace(fmt...)  do { if (vFlag) { printf(fmt); fflush(stdout); } } while(0)

//Structs
struct pte_t{ //Page table entries - must be 32 bits
    pte_t(): valid(0),referenced(0), modified(0), writeProtected(0), 
        pagedOut(0), frame(0), unused(0){}
    
    void printPTE(){
        printf("valid[%d] ref[%d] mod[%d] writeP[%d] pageO[%d] frame[%d] unused[%d]\n",
            valid, referenced, modified, writeProtected, pagedOut, frame, unused);
    }
    unsigned valid:1;
    unsigned referenced:1;
    unsigned modified:1;
    unsigned writeProtected:1;
    unsigned pagedOut:1;
    unsigned frame:7; //Max 128 = 7 bits
    unsigned unused:20;
}; 

struct frame_t { //Frames
    frame_t(): pid(-1), mappedTo(){}
    void printFrame(){
        printf("Mapped to PID[%d]\t",pid);
        mappedTo.printPTE();
    }
    int pid; //Which process do I map to?
    pte_t mappedTo; //Which virtual addres do I map to?
}; 

struct virtualMemoryArea { //VMA
    virtualMemoryArea(int s, int e, bool w, bool f):
        start_vpage(s), end_vpage(e), write_protected(w), file_mapped(f){}
    void printVMA(){
        printf("start[%d] end[%d] write_pro[%d] file_map[%d]\n", start_vpage,end_vpage, write_protected, file_mapped);
    }
    int start_vpage;
    int end_vpage;
    bool write_protected;
    bool file_mapped;
};

struct process {
    static int count;
    process(): pid(count++), VAMList(){
        for (int i = 0; i < MAX_VPAGES; i++){
            page_table[i] = pte_t();
        }
    }

    void printProcess(){
        printf("======PID: %d\n",pid);
        printf("VAM List\n");
        for (size_t i = 0; i < VAMList.size(); i++){
            VAMList[i].printVMA();
        }
        // printf("PTEs\n");
        // for (int i = 0; i < MAX_VPAGES; i++){
        //     if (sizeof(page_table[i]) != 4){
        //         page_table[i].printPTE();
        //     }
        // }
    }
    int pid;
    vector<virtualMemoryArea> VAMList;
    pte_t page_table[MAX_VPAGES];
};
int process::count = 0; //Init the static count for pid

struct frames {
    frames(int num_frames){}
    frame_t frame_table[MAX_FRAMES]; //All the frames
    queue<frame_t> free_pool; //Free pool
};

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

    nProcs = 0;
    while(getline(ifile, line)){
        if (line.empty() || line[0] == '#') {
            continue; //Ignoring empty and # lines
        }
        istringstream iss(line);
        iss >> nProcs; //number of procs
        break;
    }
    // printf("nProc[%d]\n",nProcs);
    if (nProcs == 0) {
        cerr << "No processes read in input file.\n";
        exit(1);
    }
    
    int i = 0;
    vector<process*> procList; //Holding all procs
    while (i < nProcs){
        getline(ifile, line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        process* temp = new process(); //Creating a process
        procList.push_back(temp); //Adding process to list

        istringstream iss(line);
        iss >> nVAM; //number of VAM
        // printf("nVAM[%d]\n",nVAM);
        int j = 0;
        while (j < nVAM) {
            getline(ifile, line);
            if (line.empty() || line[0] == '#') {
                continue;
            }
            istringstream iss(line);
            iss >> start_vpage >> end_vpage >> write_protected >> file_mapped; //VAMs
            virtualMemoryArea aVAM = virtualMemoryArea(start_vpage, end_vpage, write_protected, file_mapped);
            temp->VAMList.push_back(aVAM); //Adding VAM to proc
            // printf("start[%d] end[%d] write_p[%d] file_mapped[%d]\n",start_vpage,end_vpage,write_protected,file_mapped);
            j++;
        }
        i++;
    }  
    //Print Process List
    for(size_t i = 0; i < procList.size(); i++){
        procList[i]->printProcess();
    }

    while(getline(ifile, line)){ //Get Instructions
        if (line.empty() || line[0] == '#') {
            continue;
        }
        istringstream iss(line);
        iss >> inst >> instNum;
        // printf("inst[%c] instNum[%d]\n", inst, instNum);
    }

    //Clean up
    ifile.close();
    for(size_t i = 0; i < procList.size(); i++){
        delete procList[i];
    }
}

//The random function
int myrandom(int burst) {
    static int ofs = 0;
    if (ofs >= randvals.size()) {
        ofs = 0;
    }
    return randvals[ofs++] % burst;
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




