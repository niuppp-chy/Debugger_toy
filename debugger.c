#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <string.h>




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
#if 1
    while (WIFSTOPPED(wait_status)) {
        icounter++;
        struct user_regs_struct regs;
        ptrace(PTRACE_GETREGS, child_pid, 0, &regs);
        // unsigned instr = ptrace(PTRACE_PEEKTEXT, child_pid, regs.rip /* 64-bit */, 0);
        // procmsg("icounter = %u.  EIP = 0x%08x.  instr = 0x%08x\n", icounter, regs.rip, instr);

        procmsg("Child started. RIP: 0x%08x\n", regs.rip);

        /* Look at the word at the address we're interested in */
        // unsigned addr = 0x401797; // a = 5 的指令地址
        unsigned addr = 0x4017ac; // b = 6
        unsigned data = ptrace(PTRACE_PEEKTEXT, child_pid, (void*)addr, 0); // peek:窥探对应地址的值
        procmsg("Original data at 0x%08x: 0x%08x\n", addr, data);


        /* Write the trap instruction 'int 3' into the address */
        unsigned data_with_trap = (data & 0xFFFFFF00) | 0xCC;
        ptrace(PTRACE_POKETEXT, child_pid, (void*)addr, (void*)data_with_trap);

        /* See what's there again... */
        unsigned readback_data = ptrace(PTRACE_PEEKTEXT, child_pid, (void*)addr, 0);
        procmsg("After trap, data at 0x%08x: 0x%08x\n", addr, readback_data);

    #if 0
        // 只有运行了这条语句，child才会继续往下走一步
        if (ptrace(PTRACE_SINGLESTEP, child_pid, 0, 0) < 0) {
            perror("ptrace");
            return;
        }
    #endif
        /* Let the child run to the breakpoint and wait for it to
        ** reach it
        */
        ptrace(PTRACE_CONT, child_pid, 0, 0);

        wait(&wait_status);
        if (WIFSTOPPED(wait_status)) {
            procmsg("Child got a signal: %s\n", strsignal(WSTOPSIG(wait_status)));
        } else {
            perror("wait");
            return;
        }

        /* See where the child is now */
        ptrace(PTRACE_GETREGS, child_pid, 0, &regs);
        procmsg("Child stopped at RIP = 0x%08x\n", regs.rip); // 在0x401798停下
        // wait(&wait_status);

        /* Remove the breakpoint by restoring the previous data
        ** at the target address, and unwind the RIP back by 1 to
        ** let the CPU execute the original instruction that was
        ** there.
        */
        ptrace(PTRACE_POKETEXT, child_pid, (void*)addr, (void*)data); //poke：把新值插入到对应的地址上
        regs.rip -= 1; // 回退一条指令
        unsigned data_after = ptrace(PTRACE_PEEKTEXT, child_pid, (void*)addr, 0);
        procmsg("unwind rip at 0x%08x: 0x%08x\n", regs.rip, data_after);
        ptrace(PTRACE_SETREGS, child_pid, 0, &regs);

        /* The child can continue running now */
        ptrace(PTRACE_CONT, child_pid, 0, 0);
// retry:
        wait(&wait_status);
        if (WIFEXITED(wait_status)) {
            procmsg("Child exited\n");
        } else {
            procmsg("Unexpected signal\n");
            // goto retry;
        }
    }
#else
    // ...
#endif



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