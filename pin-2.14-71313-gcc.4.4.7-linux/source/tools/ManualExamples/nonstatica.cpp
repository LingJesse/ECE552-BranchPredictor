#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pin.H"


const int HISTORY_LENGTH = 62; // set the number of bits to use for branch history
const float theta = (1.93 * HISTORY_LENGTH) + 14;

FILE * trace;
bool branch_history[HISTORY_LENGTH]; // stores the current recent history of branch results
float weight[HISTORY_LENGTH];  //storage for weights
float current_perceptron_result;

bool prediction;
int num_branches;
int num_correct;

// generate the current perceptron result
void updatePerceptronResult(){
    current_perceptron_result = 0.0;
    for (int i=0; i<HISTORY_LENGTH; i++){
        float temp_history = -1.0;
        if (branch_history[i]) { temp_history = 1.0; }
        current_perceptron_result += weight[i] * temp_history;
    }
}

// set the prediction to true or false depending on the perceptron result
void setPrediction(){
    if (current_perceptron_result > 0){
        prediction  = true;
    }
    else
    {
        prediction  = false;
    }
}

// train the perceptron with the current branch taken result
void trainPerceptron(bool taken){
    float temp_taken = -1.0;
    if (taken) { temp_taken = 1.0; }
    float temp_result = current_perceptron_result;
    if (temp_result<0) {temp_result *= -1.0;}
    if (((current_perceptron_result<0) != (temp_taken<0)) || 
        (temp_result < theta)){
        for (int i=0; i<HISTORY_LENGTH; i++){
            float temp_history = -1.0;
            if (branch_history[i]) { temp_history = 1.0; }
            weight[i] = weight[i] + temp_taken*temp_history;
        }
    }
}

// update the recent history of branch results
void updateBranchHistory(bool taken){    
    for (int i=0; i<HISTORY_LENGTH-1; i++) {
        branch_history[i] = branch_history[i+1];
    }
    branch_history[HISTORY_LENGTH-1] = taken;
}

// Print a branch record
VOID RecordBranch(VOID * ip, BOOL taken, VOID * addr)
{
    fprintf(trace,"%p: %p", ip, addr);
    num_branches++;
    
    updatePerceptronResult();
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
    fprintf(trace," %.2f\tcorrect=%d/%d\n", current_perceptron_result, num_correct, num_branches);
    
    trainPerceptron(taken);
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
    fprintf(trace,"...\nOVERALL correct=%d/%d\t%f using a %d-deep history perceptron predictor.\n", 
        num_correct, num_branches, (double)num_correct/(double)num_branches, HISTORY_LENGTH);
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
    
    // Initialize branch predictor variables
    for (int i=0; i<HISTORY_LENGTH; i++){
        branch_history[i] = false;
        weight[i] = 0.0;
    }
    current_perceptron_result = 0.0;
    num_branches = 0;
    num_correct = 0;

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}
