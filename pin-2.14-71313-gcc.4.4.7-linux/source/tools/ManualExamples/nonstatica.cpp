#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pin.H"


const int HISTORY_LENGTH = 42; // set the number of bits to use for branch history
const float theta = (1.93 * HISTORY_LENGTH) + 14.0;

FILE * trace;
bool branch_history[HISTORY_LENGTH]; // stores the current recent history of branch results
float weight[HISTORY_LENGTH];  //storage for weights
float current_perceptron_result;

bool prediction;
int num_branches;
int num_correct;

float boolToFloat(bool input){
    float temp = -1.0;
    if (input) { temp = 1.0; }
    return temp;
}

// generate the current perceptron result
void updatePerceptronResult(){
    current_perceptron_result = 0.0;
    for (int i=0; i<HISTORY_LENGTH; i++){
        current_perceptron_result += weight[i] * boolToFloat(branch_history[i]);
    }
}

// set the prediction to true or false depending on the perceptron result
void setPrediction(){
    if (current_perceptron_result > 0.0){
        prediction  = true;
    }
    else
    {
        prediction  = false;
    }
}

// train the perceptron with the current branch taken result
void trainPerceptron(bool taken){
    float temp_result = current_perceptron_result;
    if (temp_result < 0.0) {temp_result *= -1.0;}
    
    if ((prediction != taken) || 
        (temp_result <= theta)){
        for (int i=0; i<HISTORY_LENGTH; i++){
            weight[i] = weight[i] + boolToFloat(taken)*boolToFloat(branch_history[i]);
        }
    }
}

// update the recent history of branch results
void updateBranchHistory(){
    for (int i=1; i<HISTORY_LENGTH; i++) {
        branch_history[i] = branch_history[i-1];
    }
    branch_history[0] = true;
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
    fprintf(trace," %g\tcorrect=%d/%d\n", current_perceptron_result, num_correct, num_branches);
    
    trainPerceptron(taken);
    updateBranchHistory();
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

    trace = fopen("branchlog.out", "w");
    
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
