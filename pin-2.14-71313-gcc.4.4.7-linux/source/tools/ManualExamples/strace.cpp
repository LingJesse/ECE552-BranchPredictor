#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pin.H"

long num_instr = 0;
const long NUM_INSTR_SKIPPED = 1000000;

const int COUNTER_BITS = 4; // set the number of bits to use for the counter
const int SATURATING_NUM = (1<<COUNTER_BITS)/2;
const int NUM_TABLES = 6; //set the number of predictor tables to utilize
const int TABLE_SIZE = 1<<20 ; // size of each table

const int HISTORY_LENGTH = 1<<(NUM_TABLES-1); // set the number of bits to use for branch history (determined using a=2)
const float theta = NUM_TABLES;

FILE * trace;

int predictionTable[NUM_TABLES][TABLE_SIZE]; //Prediction Table
int historyVariations[NUM_TABLES]; //Variations on depth per table

string branch_history[HISTORY_LENGTH]; // stores the current recent history of branch results
float currentSum;

bool prediction;
bool wasCorrect; //Was the prediction correct?
int num_branches;
int num_correct;

//hash function
unsigned long hashing(const char *str)
    {
        unsigned long hash = 5381;
        int c;

        while ((c = *str++))
            hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

        return hash/(LLONG_MAX/(1<<19));
    }
	
//Creating and Initilizing Predictor Tables
void createPredictorTables(){
	for(int i=0; i<NUM_TABLES;i++){
		for(int j=0; j<TABLE_SIZE; j++){
			predictionTable[i][j] = 0;
		}
		historyVariations[i] = 1<<i;
	}
}


// generate the current result
void updateResult(){
    currentSum = NUM_TABLES/2.0;
    for (int i=0; i<NUM_TABLES; i++){
		int historyDepth = historyVariations[i];
		string currentPath;
		for (int j=0; j<historyDepth; j++){
				currentPath = currentPath + branch_history[HISTORY_LENGTH-(j+1)];
				
		}
		//int hash = atoi(currentPath.c_str());
		unsigned long hash = hashing(currentPath.c_str());
		//printf("%lu\n", hash);
		currentSum += predictionTable[i][hash];
    }
	//if (currentSum < 0){
	//	printf("%f\n", currentSum);
	//}
}

// set the prediction to true or false depending on the perceptron result
void setPrediction(){
    if (currentSum > 0.0){
        prediction  = true;
    }
    else
    {
        prediction  = false;
    }
}

// Update the predictor
void updatePredictor(bool wasCorrect, bool taken){
 	if(currentSum<=theta || !wasCorrect){
		for (int i=0; i<NUM_TABLES; i++){
			int historyDepth = historyVariations[i];
			string currentPath;
			for (int j=0; j<historyDepth; j++){
				currentPath = currentPath + branch_history[HISTORY_LENGTH-(j+1)];
				
			}
		unsigned long hash = hashing(currentPath.c_str());
		
		if(taken){
			predictionTable[i][hash]+=1;
			predictionTable[i][hash] = std::min(predictionTable[i][hash], SATURATING_NUM);
		}
		else{
			predictionTable[i][hash]+=(-1);
			predictionTable[i][hash] = std::max(predictionTable[i][hash], -SATURATING_NUM);
			//printf("False");
		}

		}
	} 
}

// update the recent history of branch results
// target address in [HISTORY_LENGTH-1]
void updateBranchHistory(string address){    
	//printf("%s", address.c_str());
     for (int i=0; i<HISTORY_LENGTH-1; i++) {
        branch_history[i] = branch_history[i+1];
    }
    branch_history[HISTORY_LENGTH-1] = address; 
}

// Print a branch record
VOID RecordBranch(VOID * ip, BOOL taken, VOID * addr)
{
    fprintf(trace,"%p: %p", ip, addr);
    num_branches++;
    
    updateResult();
    setPrediction();
    
    if (taken)
    {
        fprintf(trace,"\tT");
        if (prediction) {
			wasCorrect = true;
            num_correct++;
            fprintf(trace," ");
        }
        else
        {
			wasCorrect = false;
            fprintf(trace,"*");
        }
    }
    else
    {
        fprintf(trace,"\tN");
        if (!prediction) {
			wasCorrect = true;
            num_correct++;
            fprintf(trace," ");
        }
        else
        {
			wasCorrect = false;
            fprintf(trace,"*");
        }
    }
	
    fprintf(trace," %g\tcorrect=%d/%d\n", currentSum, num_correct, num_branches);
    
    updatePredictor(wasCorrect, taken);
  	char buffer [100];
	sprintf(buffer,"%p", addr);
	std::string str1 (buffer);
	//printf("%s",str1.c_str());
    updateBranchHistory(str1);
}

// This function is called before every instruction is executed
VOID docount() { num_instr++; }

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{
    // Insert a call to docount before every instruction, no arguments are passed
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount, IARG_END);
    if (num_instr <= NUM_INSTR_SKIPPED){
        num_correct = 0;
        num_branches = 0;
    }
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
    fprintf(trace,"...\nOVERALL correct=%d/%d\t%f using a %d-table GEHL predictor.\n", 
        num_correct, num_branches, (double)num_correct/(double)num_branches, NUM_TABLES);
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
    
	//Create Predictor Tables depending on parameters 
	createPredictorTables();
	
    // Initialize branch history
    for (int i=0; i<HISTORY_LENGTH; i++){
        branch_history[i] = "";
    }
    currentSum = 0.0;
    num_branches = 0;
    num_correct = 0;
	wasCorrect = false;

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}
