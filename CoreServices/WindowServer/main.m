/*
 * Copyright (C) 2022-2025 Zoe Knox <zoe@pixin.net>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#import <unistd.h>
#import <sys/param.h>
#import "common.h"
#import "WindowServer.h"
#import <servers/bootstrap.h>
#import "message.h"

extern int optopt;

void *machSvcLoop(void *arg) {
    WindowServer *ws = (__bridge WindowServer *)arg;
    while(1)
        [ws receiveMachMessage];
}

void *kqSvcLoop(void *arg) {
    WindowServer *ws = (__bridge WindowServer *)arg;
    while(1)
        [ws processKernelQueue];
}

int main(int argc, const char *argv[]) {
    int logLevel = WS_ERROR;
    srandomdev();
    int curShell = LOADING;
    WindowServer *ws = nil;

    /* Become immortal. Mwahahahaha!
     * Note: don't trap SIGCHLD - we need it to wait on process exits
     */
    signal(SIGHUP, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGALRM, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGUSR1, SIG_IGN);
    signal(SIGUSR2, SIG_IGN);

    /* Start a new session without a controlling terminal. The kernel
     * console gives up the screen when we open the framebuffer.
     */
    setsid();

    ws = [WindowServer new];
    if(ws == nil)
        exit(1);

    while(getopt(argc, (char * const *)argv, "LxvD:") != -1) {
        switch(optopt) {
            case 'L': // bypass loginwindow, run desktop for current user
                curShell = DESKTOP;
                break;
            case 'x': // just run the compositor
                curShell = NONE;
                break;
            case 'v':
                logLevel++;
                break;
            case 'D':
                if(optarg != NULL) {
                    if(optarg[1] != '=') break;
                    int level = MAX(0, optarg[2] - '0');
                    level = MIN(level, WS_INFO);
                    [ws setDebugLevel:level subsystem:optarg[0]];
                }
                break;
        }
    }
    [ws setLogLevel:logLevel];

    pthread_t machSvcThread;
    pthread_create(&machSvcThread, NULL, machSvcLoop, (__bridge void *)ws);

    pthread_t kqThread;
    pthread_create(&kqThread, NULL, kqSvcLoop, (__bridge void *)ws);

    [ws setShell:curShell];
    [ws run];
    ws = nil;

    pthread_cancel(machSvcThread);
    pthread_cancel(kqThread);
    exit(0);
}
