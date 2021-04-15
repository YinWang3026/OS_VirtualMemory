#include <iostream> //cout
#include <fstream> //ifstream
#include <sstream> //String stream
#include <string>
#include <vector>
#include <stdio.h>
#include <getopt.h> //Arg parsing
#include <stdlib.h>
#include <queue>
#include <limits.h> //INT_MAX

using namespace std;

//Global Constants
const int MAX_VPAGES = 64; //Total virtual pages per process
const int MAX_FRAMES = 128; //Supports up to 128 total possible physical frames

//Macro definitions
// #define vtrace(fmt...)  do { if (vFlag) { printf(fmt); fflush(stdout); } } while(0)

//Structs
struct pte{ //Page table entries - must be 32 bits
    pte(): valid(0),referenced(0), modified(0), writeProtected(0), 
        pagedOut(0), frame(0), file_mapped(0), firstTime(0), unused(0){}
    
    void printPTE(){
        string s = "";
        if (valid == 1){
            if (referenced) { s+= "R"; } else { s+= "-"; }
            if (modified) { s+= "M"; } else { s+= "-"; }
            if (pagedOut) { s+= "S"; } else { s+= "-"; }
        } else{
            if (pagedOut) { s+= "#"; } //Invalid, swapped out
            else { s+="*"; } //Invalid, not swap out
        }
        s += " ";
        printf("%s", s.c_str());
    }
    unsigned valid:1; //Am i mapped to a frame
    unsigned referenced:1; //On read or write
    unsigned modified:1; //On write
    unsigned writeProtected:1; //Cannot write, can read
    unsigned pagedOut:1; //aka swapped out
    unsigned frame:7; //Max 128 = 7 bits
    unsigned file_mapped:1; //Mapped to a file
    unsigned firstTime:1; //First time to page fault?
    unsigned unused:18;
}; 

//Need fix
struct frame { //Frame
    static int count;
    frame(): frameid(count++), pid(-1), pte_id(0) {}
    void printFrame(){
        //<pid:virtual page>
        //* if not currently mapped by a virtual page
        if (pid == -1){
            printf("* ");
        } else{
            printf("%d:%d ", pid, pte_id);
        }
    }
    int frameid; //id of this frame
    int pid; //Which process do I map to?
    unsigned pte_id:6; //Which virtual addres in the PID do i map to?
}; 
int frame::count = 0;

struct virtualMemoryArea { //VMA
    virtualMemoryArea(int s, int e, bool w, bool f):
        start_vpage(s), end_vpage(e), write_protected(w), file_mapped(f){}
    void printVMA(){
        printf("start[%d] end[%d] write_pro[%d] file_map[%d]\n", start_vpage,end_vpage, write_protected, file_mapped);
    }
    bool checkRange(int x){
        return (start_vpage <= x && x <= end_vpage) ? true :  false;
        // if (start_vpage <= x && x <= end_vpage){
        //     return true;
        // } else {
        //     return false;
        // }
    }
    bool getWriteProtected(){
        return write_protected;
    }
    bool getFileMapped(){
        return file_mapped;
    }
    private:
        int start_vpage;
        int end_vpage;
        bool write_protected;
        bool file_mapped;
};

struct process {
    static int count;
    process(): pid(count++), unmaps(0), maps(0), ins(0), outs(0), 
        fins(0), fouts(0), zeros(0), segv(0), segprot(0){}
    void printProcessPageTable(){
        printf("PT[%d]: ",pid);
        for (int i = 0; i < MAX_VPAGES; i++){
            printf("%d:",i);
            page_table[i].printPTE();
        }
        printf("\n");
    }
    bool inMyVMA(int pteNum) {
        for (int i = 0; i < VAMList.size(); i++){
            if (VAMList[i].checkRange(pteNum)) {
                return true;
            }
        }
        return false;
    }
    bool getWriteProtection(int pteNum) {
        for (int i = 0; i < VAMList.size(); i++){
            if (VAMList[i].checkRange(pteNum)) {
                return VAMList[i].getWriteProtected();
            }
        }
    }
    bool getFileMapped(int pteNum){
        for (int i = 0; i < VAMList.size(); i++){
            if (VAMList[i].checkRange(pteNum)) {
                return VAMList[i].getFileMapped();
            }
        }
    }
    int pid;
    unsigned long unmaps, maps, ins, outs, fins, fouts, zeros, segv, segprot;
    vector<virtualMemoryArea> VAMList; //virtual memory segments
    pte page_table[MAX_VPAGES]; // a per process array of fixed size=64 of pte_t not pte_t pointers !
};
int process::count = 0; //Init the static count for pid

struct aLotOfFrames {
    aLotOfFrames(int num_frames) : n(num_frames) {
        for(int i = 0; i < n; i++){
            frame* f = new frame();
            frame_table.push_back(f); //Add to total frame
            free_pool.push(f); //All initalized frame is free
        }
    }
    ~aLotOfFrames(){ //Clean up
        for(int i = 0; i < n; i++){
            delete frame_table[i];
        }
    }
    void printFT(){
        printf("FT: ");
        for (int i = 0; i < n; i++){
            frame_table[i]->printFrame();
        }
        printf("\n");
    }
    frame* getFrameFromPool(){
        if (free_pool.empty()){
            return NULL;
        }
        frame* res = free_pool.front(); //Get front pt
        free_pool.pop(); //Delete that first pt from queue
        return res;
    }
    void addFrameToPool(frame* f){
        free_pool.push(f);
    }
    private:
        vector<frame*> frame_table; //All the frames
        queue<frame*> free_pool; //Free pool
        int n; //Numbre of frames
};

struct randomNumberGenerator {
    randomNumberGenerator(char* randomFile){
        //Opening random value file
        ifstream rfile(randomFile);
        if (!rfile) {
            cerr << "Error: Could not open the rfile.\n";
            exit(1);
        }
        int r; //Random int
        rfile >> r; //Reading the size
        while (rfile >> r) {
            randvals.push_back(r); //Populating the random vector
        }
        rfile.close();
    }
    //The random function
    int myrandom(int size) {
        static int ofs = 0;
        if (ofs >= randvals.size()) {
            ofs = 0;
        }
        return randvals[ofs++] % size;
    }
    private:
        vector<int> randvals; //Vector containg the random integers
};

// Class definitions
class Pager {
    public:
        virtual frame* select_victim_frame() = 0; // virtual base class
    private:
};

//Global var
aLotOfFrames* myFrames; //Pointer to my frames
Pager* myPager; //Pointer to the pager algorithm

//Functions
frame* get_frame() {
    frame *frame = myFrames->getFrameFromPool();
    if (frame == NULL) { //No more empty frame
        frame = myPager->select_victim_frame();
    }
    return frame;
}

int main(int argc, char* argv[]) {
    int c;
    //Flags
    int OFlag = 0;
    int PFlag = 0;
    int FFlag = 0;
    int SFlag = 0;
    int xFlag = 0;
    int yFlag = 0;
    int fFlag = 0;
    int aFlag = 0;
    while ((c = getopt(argc,argv,"f:a:o:")) != -1 )
    {   
        // ./mmu –f<num_frames> -a<algo> [-o<options>] -x -y -f -a inputfile randomfile
        // options = OPFSxfya
        //Argument parsing
        switch(c) {
            case 'f':
                int num_frames;
                sscanf(optarg, "%d",&num_frames);
                if (num_frames >= MAX_FRAMES) {
                    fprintf(__stderrp, "Error: num_frames[%d] >= 128\n", num_frames);
                    exit(1);
                }
                myFrames = new aLotOfFrames(num_frames);
                break;
            case 'a': 
                char algo;
                sscanf(optarg, "%c",&algo);
                switch (algo) {
                    case 'F':
                        break;
                    case 'R':
                        break;
                    case 'C':
                        break;
                    case 'E':
                        break;
                    case 'A':
                        break;
                    case 'W':
                        break;
                    default:
                        cerr << "Error: Unknown algo: " << algo << endl;
                        exit(1);
                }
                break;
            case 'o':
                for (int i = 0; optarg[i] != '\0'; i++){
                    // printf("O: %c\n", optarg[i]);
                    switch (optarg[i]) {
                        case 'O': //Output
                            OFlag = 1;
                            break;
                        case 'P': //Print proc
                            PFlag = 1;
                            break;
                        case 'F': //Print Frame table
                            FFlag = 1;
                            break;
                        case 'S': //Print summary
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
                        default:
                            cerr << "Error: Unknown option: " << optarg[i] << endl;
                            exit(1);
                    }
                }
                break;
        }        
    }
    //Arguments statements
    // printf("frames[%d] algo[%c] O[%d] P[%d] F[%d] S[%d] x[%d] y[%d] f[%d] a[%d]\n", num_frames, algo, OFlag, PFlag, FFlag, SFlag, xFlag, yFlag, fFlag, aFlag);

    if ((argc - optind) < 2) { //optind is the index of current argument
        cerr << "Error: Missing input file and rfile\n";
        exit(1);
    }
    //Gettng file names
    char* inputFile = argv[optind];
    char* randomFile = argv[optind+1];
    // printf("Input file: %s\trfile: %s\n",inputFile,randomFile);
    
    //Opening random value file and creating random number generator
    randomNumberGenerator rng(randomFile);
    // for (int i = 0; i < 10010; i++){
    //     cout << rng.myrandom(1) << endl;
    // }

    //Opening input file
    ifstream ifile(inputFile);
    if (!ifile) {
        cerr << "Error: Could not open the input file.\n";
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
        cerr << "Error: No processes read in input file.\n";
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
            j++;
        }
        i++;
    }  
    
    //Simulation
    unsigned long instCount = 0;
    unsigned long ctxSwitches = 0;
    unsigned long processExits = 0;
    process* currentProcess = NULL;
    while(getline(ifile, line)){ //Get Instructions
        if (line.empty() || line[0] == '#') {
            continue;
        }
        istringstream iss(line);
        iss >> inst >> instNum;
        if (inst == 'c'){
            ctxSwitches += 1;
            currentProcess = procList[instNum];
        } else if (inst == 'e'){
            processExits += 1;
            //Release all the frame on current process

        } else if (inst == 'r' || inst == 'w'){
            //instNum = pte index
            pte* currentPte = &currentProcess->page_table[instNum];
            if (currentPte->valid == 0){ //Page fault
                //Verify this pte is valid in VMA
                if (currentProcess->inMyVMA(instNum)) {
                    frame* newFrame = get_frame();
                    //UNMAP the old process on the frame
                    //MAP the new process to the frame
                } else {
                    cerr << "Error: Pte not in VMA: " << instNum << endl;
                    continue;
                }
            }
            if (currentProcess->getWriteProtection(instNum)){
                //Raise error
            } else{

            }
        } else {
            cerr << "Error: Invalid instruction: " << inst << endl;
            exit(1);
        }
        instCount += 1;

        //Debug information
        if (xFlag) {
            //Print the current process page table
            currentProcess->printProcessPageTable();
        }
        if (yFlag) {
            //Print the page table for all processes
            for (int i = 0; i < procList.size(); i++){
                procList[i]->printProcessPageTable();
            }
        }
        if (fFlag) {
            //Print the frame table
            myFrames->printFT();
        }
        if (aFlag) {
            //Aging information
        }
    }
    if (PFlag) {
        for (int i = 0; i < procList.size(); i++){
            //Print the page table for every process
            procList[i]->printProcessPageTable();
        }
    }
    if (FFlag) {
        //Print the state of the frame table
        myFrames->printFT();
    }
    if (SFlag) {
        //Summary
        unsigned long long cost = 0;
        for (int i = 0; i < procList.size(); i++){
            process* p = procList[i];
            cost += p->unmaps*400 + p->maps*300 + p->ins*3100 + p->outs*2700 + p->fins*2800
                + p->fouts*2400 + p->zeros*140 + p->segv*340 + p->segprot*420;
            printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
                p->pid, p->unmaps, p->maps, p->ins, p->outs, p->fins, p->fouts, p->zeros,
                p->segv, p->segprot);
        }
        cost += instCount + ctxSwitches*130 + processExits*1250;
        printf("TOTALCOST %lu %lu %lu %llu %lu\n", instCount, ctxSwitches, processExits, cost, sizeof(pte));
    }

    //Clean up
    ifile.close();
    for(size_t i = 0; i < procList.size(); i++){
        delete procList[i];
    }
    delete myFrames;
    delete myPager;
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




