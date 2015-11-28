/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2015 Intel Corporation. All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>

#define RUN_MODE_UNUSED 1
#define RUN_MODE_USED 2

int is_invalid_errno()
{
    switch (errno)
    {
        case EACCES:
        case EADDRINUSE:
            return 0;
        default:
            return 1;
    }
}

void usage(char* cmd)
{
    fprintf(stderr, "Usage: %s <unused|used>\n"
            "\tunused - Return unused random port and exit\n"
            "\tused - Print a port that is being used, and hang on\n", cmd);
    exit(3);
}

void alarm_signal_handler(int sig)
{
}

int main(int argc, char* argv[])
{
    if (2 != argc)
    {
        usage(argv[0]);
    }
    int run_mode = 0;
    if (!strcmp("unused", argv[1]))
    {
        run_mode = RUN_MODE_UNUSED;
    }
    else if (!strcmp("used", argv[1]))
    {
        run_mode = RUN_MODE_USED;
    }
    else
    {
        usage(argv[0]);
    }
    srand ((unsigned int)time(NULL));
    do
    {
        int random_port = 0x400 + (rand() % 0xffff - 0x400); //Picks a random number from 1024 to 65535
        struct sockaddr_in serv_addr;
        int listenfd = socket(AF_INET, SOCK_STREAM, 0);
        memset(&serv_addr, 0, sizeof(serv_addr));

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        serv_addr.sin_port = htons(random_port);
        if (0 != bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)))
        {
            close(listenfd);
            if (is_invalid_errno())
            {
                perror("bind");
                return 1;
            }
            continue;
        }
        if (0 != listen(listenfd, 1))
        {
            close(listenfd);
            if (is_invalid_errno())
            {
                perror("listen");
                return 2;
            }
            continue;
        }
        printf("%d\n", random_port);
        fflush(stdout);
        if (run_mode == RUN_MODE_USED)
        {
            /*
             * User specified that he wants to get a TCP port that is being used.
             * So, fork another process that will listen to this port and
             * exit the original process so that the parent process (make) can
             * continue.
             */
            pid_t ppid = getppid(); //ppid is the process ID of "make"
            pid_t pid = fork();
            if (pid < 0)
            {
                perror("fork");
                return 4;
            }
            if (pid > 0)
            {
                //After we printed the random port to STDOUT, the parent process
                //(make) can continue
                exit(0);
            }
            int count = 60;
            // Hang on while ppid is still running
            while (kill(ppid, 0) == 0)
            {
                socklen_t length;
                struct sockaddr dummy;
                struct sigaction saction;
                if (count-- <= 0)
                {
                    break;
                }
                memset(&saction, 0, sizeof(saction));
                //We don't want the default SIGALRM behavior (i.e. kill the process)
                saction.sa_handler = alarm_signal_handler;
                //Enables SIGALRM to interrupt accept()
                sigaction(SIGALRM, &saction, NULL);
                alarm(1);
                int newfd = accept(listenfd, &dummy, &length);
                if (newfd < 0)
                {
                    if (EINTR != errno)
                    {
                        perror("accept");
                        break;
                    }
                }
                else
                {
                    close(newfd);
                }
            }
        }
        close(listenfd);
    }
    while (0);

    return 0;
}
