# ECE552-BranchPredictor

To compile, go to the `source/tools/ManualExamples` directory and run:
`make`

From the same directory, to run the tool on some binary, type:
`sudo ../../../pin.sh -t obj-intel64/pinatrace.so -- /bin/ls`
This is running the pintool on the `ls` command.

To see the first 40 lines of output of the branch logger, type:
`head -40 branchlog.log`

## pinatrace.cpp Changes
You may specify the number of bits to use for the BHT at the top using the `BHT_BITS` constant.

To run all at once, run:

`make; sudo ../../../pin.sh -t obj-intel64/pinatrace.so -- /bin/ls; head -20 branchlog.log; tail -3 branchlog.log`
