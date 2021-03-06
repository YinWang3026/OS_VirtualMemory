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

//Global Constants
const int MAX_VPAGES = 64; //Total virtual pages per process
const int MAX_FRAMES = 128; //Supports up to 128 total possible physical frames

//Flags
int OFlag = 0;
int PFlag = 0;
int FFlag = 0;
int SFlag = 0;
int xFlag = 0;
int yFlag = 0;
int fFlag = 0;
int aFlag = 0;

//Macro definitions
#define Otrace(fmt...)  do { if (OFlag) { printf(fmt); fflush(stdout); } } while(0)
#define atrace(fmt...)  do { if (aFlag) { printf(fmt); fflush(stdout); } } while(0)

//Structs
struct pte{ //Page table entries - must be 32 bits
    pte(): valid(0),referenced(0), modified(0), writeProtected(0), 
        pagedOut(0), frame(0), fileMapped(0), initalized(0), unused(0){}
    void printPTE(){
        string s = "";
        if (valid == 1){
            s += to_string(pteid) + ":";
            if (referenced) { s+= "R"; } else { s+= "-"; }
            if (modified) { s+= "M"; } else { s+= "-"; }
            if (pagedOut) { s+= "S"; } else { s+= "-"; }
        } else {
            //Invalid
            if (pagedOut) { s+= "#"; } //Paged out, has swap area
            else if (fileMapped || !pagedOut) { s+= "*"; } //File mapped and not paged out, no swap area
        }
        printf("%s ", s.c_str());
    }
    void clearAllBits(){
        valid = 0;
        referenced = 0;
        modified = 0;
        writeProtected = 0;
        pagedOut = 0;
        frame = 0;
        fileMapped = 0;
        initalized = 0;
        pteid = 0;
    }
    unsigned valid:1; //Am i mapped to a frame, translation exists
    unsigned referenced:1; //On read or write
    unsigned modified:1; //On write
    unsigned writeProtected:1; //Cannot write, can read
    unsigned pagedOut:1; //aka swapped out
    unsigned frame:7; //Max 128 = 7 bits
    unsigned fileMapped:1; //Mapped to a file
    unsigned initalized:1; //First time to page fault?
    unsigned pteid:6; //Max 64 = 6 bits
    unsigned unused:12;
}; 

struct frame { //Frame
    static int count;
    frame(): frameid(count++), pid(-1), pteptr(NULL), age(0), long_age(0) {}
    void printFrame(){
        //<pid:virtual page>
        //* if not currently mapped by a virtual page
        if (pid == -1){
            printf("* ");
        } else{
            printf("%d:%d ", pid, pteptr->pteid);
        }
    }
    int frameid; //id of this frame
    int pid; //Which process do I map to?
    pte* pteptr; //Which virtual addres in the PID do i map to?
    unsigned int age; //For aging algo
    unsigned long long_age; //For workset algo
}; 
int frame::count = 0;

struct virtualMemoryArea { //VMA
    virtualMemoryArea(int s, int e, bool w, bool f):
        start_vpage(s), end_vpage(e), write_protected(w), file_mapped(f){}
    void printVMA(){
        printf("start[%d] end[%d] write_pro[%d] file_map[%d]\n", start_vpage,end_vpage, write_protected, file_mapped);
    }
    //Checks if given pte is in VMA
    bool checkRange(int x){
        return (start_vpage <= x && x <= end_vpage) ? true : false;
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
            page_table[i].printPTE();
        }
        printf("\n");
    }
    //Inits a PTE
    bool initPTE(int pte_id){
        for (size_t i = 0; i < VAMList.size(); i++){
            if (VAMList[i].checkRange(pte_id)) {
                page_table[pte_id].writeProtected = VAMList[i].getWriteProtected();
                page_table[pte_id].fileMapped = VAMList[i].getFileMapped();
                page_table[pte_id].initalized = 1;
                page_table[pte_id].pteid = pte_id;
                return true; //Successfully init
            }
        }
        return false; //Failed to init, becaues this pte_id falls in a hole
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
    frame* getFrame(int x){
        return frame_table[x];
    }
    void addFrameToPool(frame* f){
        free_pool.push(f);
    }
    int getSize(){
        return n;
    }
private:
    vector<frame*> frame_table; //All the frames
    queue<frame*> free_pool; //Free pool
    int n; //Number of frames
};

// Class definitions
class Pager {
public:
    virtual ~Pager() {}
    virtual frame* select_victim_frame() = 0; // virtual base class
    virtual void setAgeZero(frame* f){ //Aging algo
        return;
    }
private:
};

//Globals
aLotOfFrames* myFrames; //Pointer to my frames
vector<int> randvals; //Vector containg the random integers
Pager* myPager; //Pointer to the pager algorithm
unsigned long instCount = 0; //Artificial timer

//Functions
frame* get_frame() {
    frame *frame = myFrames->getFrameFromPool();
    if (frame == NULL) { //No more empty frame
        frame = myPager->select_victim_frame();
    }
    return frame;
}
//The random function
int myrandom(int size) {
    static unsigned int ofs = 0;
    if (ofs >= randvals.size()) {
        ofs = 0;
    }
    return randvals[ofs++] % size;
}

//Pagers
class FIFO : public Pager {
public:
    FIFO() : hand(0) {}
    frame* select_victim_frame(){
        if (hand >= myFrames->getSize()){ hand = 0; }
        atrace("ASELECT %d\n", hand);
        return myFrames->getFrame(hand++);
    }
private:
    int hand;
};

class RANDOM : public Pager {
public:
    RANDOM(){}
    frame* select_victim_frame(){
        int random = myrandom(myFrames->getSize());
        atrace("ASELECT %d\n", random);
        return myFrames->getFrame(random);
    }
};

class CLOCK : public Pager {
public:
    CLOCK() : hand(0) {}
    frame* select_victim_frame(){
        int counter = 0;
        int startingPosition = (hand < myFrames->getSize()) ? hand : 0;
        while (counter < myFrames->getSize()+1){
            if (hand >= myFrames->getSize()){ hand = 0; }
            pte* temp = myFrames->getFrame(hand)->pteptr;
            if (temp->referenced == 0) {
                //Found
                counter+=1;
                break;
            } else {
                //Set zero and move on
                temp->referenced = 0;
                hand+=1;
                counter+=1;
            }
        }
        atrace("ASELECT %d %d\n", startingPosition, counter);
        return myFrames->getFrame(hand++);
    }
private:
    int hand;
};

class NRU : public Pager {
public:
    NRU(): classes(4,NULL), timer(0), hand(0) {}
    frame* select_victim_frame(){
        bool reset = (instCount - timer >= 50) ? 1 : 0; //Reset every 50 inst
        if (reset) { timer = instCount; } //Mark reset time
        for (size_t i = 0; i < classes.size(); i++){
            classes[i] = NULL; //Reset all class
        }
        atrace("ASELECT: hand=%d %d ", hand, reset);
        int count = 0;
        int smallestC = 5; // 2 bits = 3 is max
        while(count < myFrames->getSize()){
            if (hand >= myFrames->getSize()){ hand = 0; }
            frame* temp = myFrames->getFrame(hand);
            int c = 2*temp->pteptr->referenced + temp->pteptr->modified;
            if (classes[c] == NULL){ //First encounter
                classes[c] = temp;
                smallestC = min(c, smallestC);
            }
            if(reset) { //Reset reference bit
                temp->pteptr->referenced = 0;
            }
            if (c == 0 && reset == 0) { //Found smallest
                hand += 1;
                count += 1;
                break;
            }
            hand += 1;
            count += 1;
        }
        hand = classes[smallestC]->frameid;
        atrace("| %d %d %d\n", smallestC, hand, count);
        hand += 1;
        return classes[smallestC];
    }
private:
    vector<frame*> classes;
    unsigned long timer;
    int hand;
};

class AGE : public Pager {
public:
    AGE(): hand(0) {}
    frame* select_victim_frame(){
        unsigned int smallest = 0xFFFFFFFF; //Set to max
        int start = hand % myFrames->getSize();
        int end = (hand-1+myFrames->getSize()) % myFrames->getSize();
        frame* result;
        atrace("ASELECT %d-%d | ", start, end);
        for (int counter = 0; counter < myFrames->getSize(); counter++){
            if (hand >= myFrames->getSize()){ hand = 0; }
            frame* temp = myFrames->getFrame(hand);
            temp->age = temp->age >> 1; //Shift age
            if (temp->pteptr->referenced == 1){ //If ref'ed, set first bit to 1
                temp->age = (temp->age | 0x80000000);
                temp->pteptr->referenced = 0; //Reset reference to zero
            }
            atrace("%d:%04X ", temp->frameid, temp->age); //4 Byte hex
            if (temp->age < smallest){ //Finding smallest in the round
                smallest = temp->age;
                result = temp;
            }
            hand += 1;
        }
        atrace("| %d\n", result->frameid);
        hand = result->frameid + 1;
        return result;
    }
    void setAgeZero(frame* f){
        f->age = 0;
    }
private:
    int hand;
};

class WKSET : public Pager {
public:
    WKSET(): hand(0) {}
    frame* select_victim_frame(){
        int start = hand % myFrames->getSize();
        int end = (hand-1+myFrames->getSize()) % myFrames->getSize();
        unsigned long currentTime = instCount - 1;
        unsigned long smallestTime = currentTime; //Largest time so far
        frame* result = NULL;
        frame* oldest = myFrames->getFrame(hand); //Whatever at hand is first choice
        atrace("ASELECT %d-%d | ", start, end);
        // atrace("ASELECT %d %d-%d | ", currentTime, start, end);
        for (int counter = 0; counter < myFrames->getSize(); counter++){
            if (hand >= myFrames->getSize()){ hand = 0; }
            frame* temp = myFrames->getFrame(hand);
            int refBit = temp->pteptr->referenced;
            atrace("%d(%d %d:%d %lu) ", temp->frameid, refBit, temp->pid, temp->pteptr->pteid, temp->long_age);
            if (refBit == 1){
                temp->pteptr->referenced = 0; //Reset r
                temp->long_age = currentTime; //Record current time
            } else if (refBit == 0){
                //Threshold
                if ((currentTime - temp->long_age) >= 50){
                    result = temp;
                    atrace("STOP(%d) ", counter+1);
                    break;
                }
                //Always finding the oldest non ref'ed 
                if (temp->long_age < smallestTime){
                    smallestTime = temp->long_age;
                    oldest = temp;
                }
            }
            hand += 1;
        }
        if (result == NULL) { result = oldest; }
        atrace("| %d\n", result->frameid);
        hand = result->frameid + 1;
        return result;
    }
private:
    int hand;
};

int main(int argc, char* argv[]) {
    int c;
    while ((c = getopt(argc,argv,"f:a:o:")) != -1 )
    {   
        // ./mmu ???f<num_frames> -a<algo> [-o<OPFSxfya>] inputfile randomfile
        switch(c) {
            case 'f':
                int num_frames;
                sscanf(optarg, "%d",&num_frames);
                if (num_frames >= MAX_FRAMES) {
                    cerr << "Error: num_frames[" << num_frames << "] >= 128\n";
                    exit(1);
                }
                myFrames = new aLotOfFrames(num_frames);
                break;
            case 'a': 
                char algo;
                sscanf(optarg, "%c",&algo);
                switch (algo) {
                    case 'f':
                        myPager = new FIFO();
                        break;
                    case 'r':
                        myPager = new RANDOM();
                        break;
                    case 'c':
                        myPager = new CLOCK();
                        break;
                    case 'e':
                        myPager = new NRU();
                        break;
                    case 'a':
                        myPager = new AGE();
                        break;
                    case 'w':
                        myPager = new WKSET();
                        break;
                    default:
                        cerr << "Error: Unknown algo: " << algo << endl;
                        exit(1);
                }
                break;
            case 'o':
                for (int i = 0; optarg[i] != '\0'; i++){
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
                            xFlag = 0;
                            break;
                        case 'f': //Prints the frame table after each instruction
                            fFlag = 1;
                            break;
                        case 'a': //Prints information during victim_selection
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
    if ((argc - optind) < 2) { //optind is the index of current argument
        cerr << "Error: Missing input file and rfile\n";
        exit(1);
    }

    //Gettng file names
    char* inputFile = argv[optind];
    char* randomFile = argv[optind+1];
    
    //Opening random value file and creating random number generator
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
        
    //Opening input file
    ifstream ifile(inputFile);
    if (!ifile) {
        cerr << "Error: Could not open the input file.\n";
        exit(1);
    }

    //Reading processes
    int nProcs = 0;
    string line;
    while(getline(ifile, line)){
        if (line.empty() || line[0] == '#') continue; //Ignoring empty and # lines
        istringstream iss(line);
        iss >> nProcs; //number of procs
        break;
    }
    if (nProcs == 0) {
        cerr << "Error: No processes read in input file.\n";
        exit(1);
    }
    
    //Reading VMAs
    int i = 0;
    int nVAM, start_vpage, end_vpage, write_protected, file_mapped;
    vector<process*> procList; //Holding all procs
    while (i < nProcs){
        getline(ifile, line);
        if (line.empty() || line[0] == '#') continue;
        process* temp = new process(); //Creating a process
        procList.push_back(temp); //Adding process to list
        istringstream iss(line);
        iss >> nVAM; //number of VAM
        int j = 0;
        while (j < nVAM) {
            getline(ifile, line);
            if (line.empty() || line[0] == '#') continue;
            istringstream iss(line);
            iss >> start_vpage >> end_vpage >> write_protected >> file_mapped; //VAMs
            virtualMemoryArea aVAM = virtualMemoryArea(start_vpage, end_vpage, write_protected, file_mapped);
            temp->VAMList.push_back(aVAM); //Adding VAM to proc
            j++;
        }
        i++;
    }  
    
    //Reading instructions / Simulation
    unsigned long ctxSwitches = 0;
    unsigned long processExits = 0;
    process* currentProcess = NULL;
    int instNum;
    char inst;
    while(getline(ifile, line)){ //Get Instructions
        if (line.empty() || line[0] == '#') continue;
        istringstream iss(line);
        iss >> inst >> instNum;
        if (OFlag) {
            //Output flag
            printf("%lu: ==> %c %d\n", instCount, inst, instNum);
        }
        instCount += 1;
        if (inst == 'c'){
            //instNum = pid
            ctxSwitches += 1;
            currentProcess = procList[instNum];
            continue;
        } else if (inst == 'e'){
            //Release all the frame on current process
            Otrace("EXIT current process %d\n", currentProcess->pid);
            processExits += 1;
            //Go through all the vpages
            for(int i = 0; i < MAX_VPAGES; i++){
                //Found a valid page
                pte* currentPTE = &currentProcess->page_table[i];
                if (currentPTE->valid){
                    //UNMAP
                    //Reset the frame
                    frame* currentFrame = myFrames->getFrame(currentPTE->frame);
                    Otrace(" UNMAP %d:%d\n", currentFrame->pid, currentFrame->pteptr->pteid);
                    currentProcess->unmaps += 1;
                    currentFrame->pid = -1;
                    currentFrame->pteptr = NULL;
                    myFrames->addFrameToPool(currentFrame);
                    //FOUT - modified and filemapped
                    if (currentPTE->modified && currentPTE->fileMapped){
                        Otrace(" FOUT\n");
                        currentProcess->fouts += 1;
                    }
                }
                //Reset the PTE
                currentPTE->clearAllBits();
            }
            continue;
        } else if (inst == 'r' || inst == 'w'){
            //instNum = pte index
            pte* currentPte = &currentProcess->page_table[instNum]; //get the page entry
            if (currentPte->valid == 0){ 
                //Page fault                
                if (currentPte->initalized == 0){
                    //Initalize the PTE
                    if (currentProcess->initPTE(instNum) == false) {
                        //Cannot init pte, a hole
                        Otrace(" SEGV\n");
                        currentProcess->segv += 1;
                        continue;
                    }
                }
                frame* newFrame = get_frame();
                //UNMAP
                if (newFrame->pid != -1) {
                    process* unmapProc = procList[newFrame->pid];
                    int pte = newFrame->pteptr->pteid;
                    int pid = newFrame->pid;
                    Otrace(" UNMAP %d:%d\n", pid, pte);
                    unmapProc->unmaps += 1;
                    //Unmap
                    unmapProc->page_table[pte].valid = 0;
                    unmapProc->page_table[pte].frame = 0;
                    //OUT/FOUT
                    if (unmapProc->page_table[pte].modified == 1) {
                        if (unmapProc->page_table[pte].fileMapped){
                            Otrace(" FOUT\n");
                            unmapProc->fouts += 1;
                        } else {
                            Otrace(" OUT\n");
                            unmapProc->outs += 1;
                            unmapProc->page_table[pte].pagedOut = 1; //only page out for non-file mapped
                        }
                    }
                }
                //IN/FIN
                if (currentPte->fileMapped){
                    //Bring file in
                    Otrace(" FIN\n");
                    currentProcess->fins += 1;
                } else if (currentPte->pagedOut == 1){
                    //Bring in from swap device
                    Otrace(" IN\n");
                    currentProcess->ins += 1;
                } else if (currentPte->pagedOut == 0 && currentPte->fileMapped == 0){
                    //ZERO
                    Otrace(" ZERO\n");
                    currentProcess->zeros += 1;
                }
                //MAP
                Otrace(" MAP %d\n", newFrame->frameid);
                currentProcess->maps += 1;
                //Update PTE
                currentPte->valid = 1;
                currentPte->modified = 0;
                currentPte->referenced = 0;
                currentPte->frame = newFrame->frameid;
                //Update PTE in frame
                newFrame->pid = currentProcess->pid;
                newFrame->pteptr = currentPte;
                newFrame->long_age = instCount-1; //Workset algo
                myPager->setAgeZero(newFrame); //Aging algo
            }
            //At this point, PTE should be set up.
            currentPte->referenced = 1; //Read or write triggers referenced
            if (inst == 'w') {
                //Write instruction
                if (currentPte->writeProtected == 1) {
                    //No write
                    Otrace(" SEGPROT\n");
                    currentProcess->segprot += 1;
                } else {
                    //Read and write
                    currentPte->modified = 1;
                }
            }
        } else {
            cerr << "Error: Invalid instruction: " << inst << endl;
            exit(1);
        }
        //Debug information
        if (xFlag) {
            //Print the current process page table
            currentProcess->printProcessPageTable();
        }
        if (yFlag) {
            //Print the page table for all processes
            for (size_t i = 0; i < procList.size(); i++){
                procList[i]->printProcessPageTable();
            }
        }
        if (fFlag) {
            //Print the frame table
            myFrames->printFT();
        }
    }
    if (PFlag) {
        for (size_t i = 0; i < procList.size(); i++){
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
        for (size_t i = 0; i < procList.size(); i++){
            process* p = procList[i];
            cost += p->unmaps*400 + p->maps*300 + p->ins*3100 + p->outs*2700 + p->fins*2800
                + p->fouts*2400 + p->zeros*140 + p->segv*340 + p->segprot*420;
            printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
                p->pid, p->unmaps, p->maps, p->ins, p->outs, p->fins, p->fouts, p->zeros,
                p->segv, p->segprot);
        }
        cost += (instCount-ctxSwitches-processExits) + ctxSwitches*130 + processExits*1250;
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