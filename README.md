# ECE552-BranchPredictor

To compile, go to the `source/tools/ManualExamples` directory and run:
`make`

From the same directory, to run the tool on some binary, type:
`sudo ../../../pin.sh -t obj-intel64/pinatrace.so -- /bin/ls`
This is running the pintool on the `ls` command.

To see the first 40 lines of output of the branch logger, type:
`head -40 branchlog.log`

## pinatrace.cpp Changes: Basic Saturated/Correlated Predictor
You may specify the number of bits to use for the BHT at the top using the `BHT_BITS` constant.

To run all at once, run:

`make; sudo ../../../pin.sh -t obj-intel64/pinatrace.so -- /bin/ls; head -20 branchlog.out; tail -3 branchlog.out`

## itrace.cpp Changes: Path-based Predictor
You may specify the depth of the target path using the `PATH_DEPTH` constant.

To run all at once, run:

`make; sudo ../../../pin.sh -t obj-intel64/itrace.so -- /bin/ls; head -20 branchlog.out; tail -3 branchlog.out`

## nonstatica.cpp Changes: Perceptron Predictor

To run all at once, run:

`make; sudo ../../../pin.sh -t obj-intel64/nonstatica.so -- /bin/ls; head -20 branchlog.out; tail -3 branchlog.out`

## strace.cpp Changes: GEHL Predictor

To run all at once, run:

`make; sudo ../../../pin.sh -t obj-intel64/strace.so -- /bin/ls; head -20 branchlog.out; tail -3 branchlog.out`

## proccount.cpp Changes: Tournament Predictor

To run all at once, run:

`make; sudo ../../../pin.sh -t obj-intel64/proccount.so -- /bin/ls; head -20 branchlog.out; tail -3 branchlog.out`

## staticcount.cpp Changes: Data-correlated Predictor

To run all at once, run:

`make; sudo ../../../pin.sh -t obj-intel64/staticcount.so -- /bin/ls; head -20 branchlog.out; tail -3 branchlog.out`
