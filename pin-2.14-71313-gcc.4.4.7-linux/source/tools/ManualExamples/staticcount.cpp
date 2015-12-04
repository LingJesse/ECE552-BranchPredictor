#include <stdio.h>
#include <math.h>
#include "pin.H"

long num_instr = 0;
const long NUM_INSTR_SKIPPED = 1000000;
REG reg;

const int BHT_BITS = 2; // set the number of bits to use for the BHT (1 disables this)
const int BHR_BITS = 12; // set the number of bits to use for the BHR (1 disables this)
const int SATURATING_NUM = (1<<BHT_BITS)/2;
const int BHR_ENTRIES = 1<<BHR_BITS;
const int REG_ENTRIES = 8;
const int REG_TABLE_SIZE = 1<<REG_ENTRIES;

FILE * trace;
int bhr[BHR_ENTRIES];
int bhr_current_index;
int reg_current_index;
bool branch_history[BHR_BITS]; // stores the current recent history of branch results
int reg_history[REG_ENTRIES]; // stores the history of register accesses
int reg_table[REG_TABLE_SIZE];
bool prediction;
int num_branches;
int num_correct;
int reg_predictions;

int reghash(int arr[]){
    long temp = 0;
    for (int i=0; i<REG_ENTRIES; i++){
        temp += reg_history[i]<<i;
    }
    return (int) temp%REG_TABLE_SIZE;
}

void updateIndices(){
    int index = 0;
    for (int i=0; i<BHR_BITS-1; i++) {
        if (branch_history[i]){
            index = index | (1<<i);
        }
    }
    bhr_current_index = index;
    reg_current_index = reghash(reg_history);
}

void setPredictionNoReg(){
    if (bhr[bhr_current_index] > 0){
        prediction  = true;
    }
    else
    {
        prediction  = false;
    }
}

void setPrediction(){
    if ((bhr[bhr_current_index]==1) || (bhr[bhr_current_index]==-1)){
        reg_predictions++;
        if (reg_history[reg_current_index] > 0){
            prediction  = true;
        }
        else
        {
            prediction  = false;
        }
    }
    else{
        setPredictionNoReg();
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

// update the recent history of register accesses
void updateRegisterHistory(int num){
    for (int i=0; i<REG_ENTRIES-1; i++) {
        reg_history[i] = reg_history[i+1];
    }
    reg_history[REG_ENTRIES-1] = num;
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
    bhr[bhr_current_index] = std::max(bhr[bhr_current_index], -SATURATING_NUM);
    bhr[bhr_current_index] = std::min(bhr[bhr_current_index], SATURATING_NUM);
}

void updateRegTable(bool taken){
    if (taken)
    {
        if (reg_table[reg_current_index] == -1) reg_table[reg_current_index]++;
        reg_table[reg_current_index]++;
    }
    else
    {
        if (reg_table[reg_current_index] == 1) reg_table[reg_current_index]--;
        reg_table[reg_current_index]--;
    }
    reg_table[reg_current_index] = std::max(reg_table[reg_current_index], -SATURATING_NUM);
    reg_table[reg_current_index] = std::min(reg_table[reg_current_index], SATURATING_NUM);
}

// Print a branch record
VOID RecordBranch(VOID * ip, BOOL taken, VOID * addr, int reg)
{
    fprintf(trace,"%p: %p", ip, addr);
    num_branches++;
    updateIndices();
    setPrediction();
    updateRegisterHistory((int) reg);
    
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
    fprintf(trace," %+d\tcorrect=%d/%d\treg\n", reg_table[reg_current_index], num_correct, num_branches);
    
    updateBHT(taken);
    updateRegTable(taken);
    updateBranchHistory(taken);
}

VOID RecordBranchNoReg(VOID * ip, BOOL taken, VOID * addr)
{
    fprintf(trace,"%p: %p", ip, addr);
    num_branches++;
    updateIndices();
    setPredictionNoReg();
    
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
    updateRegTable(taken);
    updateBranchHistory(taken);
}

// This function is called before every instruction is executed
VOID docount_andreg() { 
    num_instr++; 
    
}

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{
    // Insert a call to docount before every instruction, no arguments are passed
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount_andreg, IARG_END);
    if (num_instr <= NUM_INSTR_SKIPPED){
        num_correct = 0;
        num_branches = 0;
    }
    unsigned int temp = 1;
    reg = INS_RegR(ins, temp);
    // if (reg != 0){
        // INS_InsertCall(
            // ins, IPOINT_BEFORE, (AFUNPTR)updateRegisterHistory,
            // IARG_REG_VALUE, reg,
            // IARG_END);
    // }

    if (INS_IsBranch(ins))
    {
        if (reg != 0){
            INS_InsertCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordBranch,
                IARG_INST_PTR, IARG_BRANCH_TAKEN, IARG_BRANCH_TARGET_ADDR, IARG_REG_VALUE, reg,
                IARG_END);
        }
        else
        {
            INS_InsertCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordBranchNoReg,
                IARG_INST_PTR, IARG_BRANCH_TAKEN, IARG_BRANCH_TARGET_ADDR, 
                IARG_END);
        }
    }
}

VOID Fini(INT32 code, VOID *v)
{
    fprintf(trace,"...\nOVERALL correct=%d/%d\t%f using a data-correlated predictor.\n", 
        num_correct, num_branches, (double)num_correct/(double)num_branches);
    fprintf(trace,"%%register=%d/%d\t%f\n", 
        reg_predictions, num_branches, (double)reg_predictions/(double)num_branches);
    fprintf(trace, "num_instr=%lu\n", num_instr);
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
    for (int i=0; i<REG_ENTRIES; i++) {
        reg_history[i] = 0;
    }
    for (int i=0; i<REG_TABLE_SIZE; i++) {
        reg_table[i] = 1;
    }
    bhr_current_index = 0;
    num_branches = 0;
    num_correct = 0;
    reg_predictions = 0;

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}
