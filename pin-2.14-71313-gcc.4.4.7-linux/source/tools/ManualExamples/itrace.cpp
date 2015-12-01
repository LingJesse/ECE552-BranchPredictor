#include <stdio.h>
#include <math.h>
#include "pin.H"

const int BHT_BITS = 2; // set the number of bits to use for the BHT (1 disables this)
const int PATH_DEPTH = 4; // set the number of bits to use for the BHR 
const int TABLE_ENTRIES = 1<<16; //2^16

FILE * trace;
int bhr[TABLE_ENTRIES];
string bhr_entries[TABLE_ENTRIES];
int bhr_current_index;
string branch_history[PATH_DEPTH]; // stores the current recent history of branch results
bool prediction;
int num_branches;
int num_correct;
int pathsUsed = 0;

void updateBHRIndex(){
	string currentPath;
	for (int i=0; i<PATH_DEPTH; i++){
		currentPath = currentPath + branch_history[i];
	}
	
	bool found = false;
	for (int i=0; i<TABLE_ENTRIES; i++){
		string checkPath = bhr_entries[i];
		if(currentPath.compare(checkPath) == 0){
			bhr_current_index = i;
			//printf("Path Match Found");
			found = true;
			break;
		}
	}
	if(!found){
		if(pathsUsed == TABLE_ENTRIES){
			pathsUsed = 0;
		}
		bhr_entries[pathsUsed] = currentPath;
		bhr_current_index = pathsUsed;
		pathsUsed += 1;
	}
	
/*     int index = 0;
    for (int i=0; i<PATH_DEPTH-1; i++) {
        if (branch_history[i]){
            index = index | (1<<i);
        }
    }
    bhr_current_index = index; */
}

void setPrediction(){
    if (bhr[bhr_current_index] > 0){
        prediction  = true;
    }
    else
    {
        prediction  = false;
    }
}

// update the recent history of branch results
void updateBranchHistory(string address){    
	//printf("%s", address.c_str());
     for (int i=0; i<PATH_DEPTH-1; i++) {
        branch_history[i] = branch_history[i+1];
    }
    branch_history[PATH_DEPTH-1] = address; 
}

void updateBHT(bool taken){
    if (taken)
    {
        if (bhr[bhr_current_index] == -1) bhr[bhr_current_index]++;
        bhr[bhr_current_index]++;
    }
    else
    {
        if (bhr[bhr_current_index] == 1) bhr[bhr_current_index]--;
        bhr[bhr_current_index]--;
    }
    bhr[bhr_current_index] = std::max(bhr[bhr_current_index], -BHT_BITS);
    bhr[bhr_current_index] = std::min(bhr[bhr_current_index], BHT_BITS);
}

// Print a branch record
VOID RecordBranch(VOID * ip, BOOL taken, VOID * addr)
{
    fprintf(trace,"%p: %p", ip, addr);
    num_branches++;
    updateBHRIndex();
	//Warming Up Branch Predictor
	if(num_branches<PATH_DEPTH)
	{
		prediction = false;
	}
	else
	{
		setPrediction();
	}
    
    if (taken)
    {
        fprintf(trace,"\tT");
        if (prediction) {
            num_correct++;
            fprintf(trace," ");
        }
        else
        {
            fprintf(trace,"*");
        }
    }
    else
    {
        fprintf(trace,"\tN");
        if (!prediction) {
            num_correct++;
            fprintf(trace," ");
        }
        else
        {
            fprintf(trace,"*");
        }
    }
    fprintf(trace," %+d\tcorrect=%d/%d\n", bhr[bhr_current_index], num_correct, num_branches);
    updateBHT(taken);
	
	char buffer [100];
	sprintf(buffer,"%p", addr);
	std::string str1 (buffer);
	//printf("%s",str1.c_str());
    updateBranchHistory(str1);
}

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{
    if (INS_IsBranch(ins))
    {
        INS_InsertCall(
            ins, IPOINT_BEFORE, (AFUNPTR)RecordBranch,
            IARG_INST_PTR, IARG_BRANCH_TAKEN, IARG_BRANCH_TARGET_ADDR, 
            IARG_END);
    }
}

VOID Fini(INT32 code, VOID *v)
{
    fprintf(trace,"...\nOVERALL correct=%d/%d\t%f using a %d-bit saturating, %d-deep path based predictor.\n", 
        num_correct, num_branches, (double)num_correct/(double)num_branches, BHT_BITS, PATH_DEPTH);
    fprintf(trace, "#eof\n");
    fclose(trace);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
   
INT32 Usage()
{
    PIN_ERROR( "This Pintool prints a trace of branch instructions\n" 
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    if (PIN_Init(argc, argv)) return Usage();

    trace = fopen("branchlog.log", "w");
    
    //Initialize branch predictor variables
    for (int i=0; i<PATH_DEPTH; i++) {
       branch_history[i] = "";
    }
    for (int i=0; i<TABLE_ENTRIES; i++) {
        bhr[i] = -1;
    }
    bhr_current_index = 0;
    num_branches = 0;
    num_correct = 0;

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}