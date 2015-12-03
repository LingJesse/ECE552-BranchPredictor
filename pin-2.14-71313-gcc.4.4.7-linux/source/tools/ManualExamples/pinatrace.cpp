#include <stdio.h>
#include <math.h>
#include "pin.H"

const int BHT_BITS = 2; // set the number of bits to use for the BHT (1 disables this)
const int BHR_BITS = 12; // set the number of bits to use for the BHR (1 disables this)
const int BHR_ENTRIES = 1<<BHR_BITS;

FILE * trace;
int bhr[BHR_ENTRIES];
int bhr_current_index;
bool branch_history[BHR_BITS]; // stores the current recent history of branch results
bool prediction;
int num_branches;
int num_correct;

void updateBHRIndex(){
    int index = 0;
    for (int i=0; i<BHR_BITS-1; i++) {
        if (branch_history[i]){
            index = index | (1<<i);
        }
    }
    bhr_current_index = index;
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
void updateBranchHistory(bool taken){
	// this seems to be the corect version, adding to the 0th index
    // for (int i=1; i<BHR_BITS; i++) {
        // branch_history[i] = branch_history[i-1];
    // }
    // branch_history[0] = taken;
	
	// this works better somehow
    for (int i=0; i<BHR_BITS-1; i++) {
        branch_history[i] = branch_history[i+1];
    }
    branch_history[BHR_BITS-1] = taken;
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
    setPrediction();
    
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
    updateBranchHistory(taken);
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
    fprintf(trace,"...\nOVERALL correct=%d/%d\t%f using a %d-bit saturating, %d-bit correlated predictor.\n", 
        num_correct, num_branches, (double)num_correct/(double)num_branches, BHT_BITS, BHR_BITS);
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

    trace = fopen("branchlog.out", "w");
    
    // Initialize branch predictor variables
    for (int i=0; i<BHR_BITS; i++) {
        branch_history[i] = false;
    }
    for (int i=0; i<BHR_ENTRIES; i++) {
        bhr[i] = 1;
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
