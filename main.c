#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>




void procmsg(const char *format, ...)
{
    va_list ap;
    fprintf(stdout, "[%d] ", getpid());
    va_start(ap, format);
    vfprintf(stdout, format, ap); // include va_arg(ap, xxx)
    va_end(ap);
}

void run_target(const char *program_name)
{
    procmsg("target started. will run '%s' \n", program_name);

    if (ptrace(PTRACE_TRACEME, 0, 0, 0) < 0) {
        perror(ptrace);
        return;
    }

    execl(program_name /* path name */, program_name /* arg[0]:should point to the filename */, 0);
}

void run_debugger(pid_t child_pid)
{
    int wait_status;
    unsigned icounter = 0;
    procmsg("debugger started\n");

    wait(&wait_status); // waitpid(-1, &wstatus, 0);

    while (WIFSTOPPED(wait_status)) {
        icounter++;
        struct user_regs_struct regs;
        ptrace(PTRACE_GETREGS, child_pid, 0, &regs);
        unsigned instr = ptrace(PTRACE_PEEKTEXT, child_pid, regs.rip /* 64-bit */, 0);

        procmsg("icounter = %u.  EIP = 0x%08x.  instr = 0x%08x\n",
                    icounter, regs.rip, instr);


        if (ptrace(PTRACE_SINGLESTEP, child_pid, 0, 0) < 0) {
            perror("ptrace");
            return;
        }

        wait(&wait_status);
    }

    procmsg("the child executed %u instructions\n", icounter);
}


int main(int argc, char *argv[])
{
    pid_t child_pid;

    if (argc < 2) {
        fprintf(stderr, "Expected a pragram name as argument\n");
        return -1;
    }

    child_pid = fork();
    if (child_pid == 0) {
        run_target(argv[1]);
    } else if (child_pid > 0) {
        run_debugger(child_pid);
    } else {
        perror("fork");
        return -1;
    }

    return 0;
}