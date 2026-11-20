// Positive control of kernel.el-silent (KernelSilentProbe.cmake): writes one known
// line to standard error, so the script shows it captures what a process writes
// there before it counts an empty capture as "the kernel wrote nothing".
#include <cstdio>

int main() {
    std::fputs("GYO silent probe: positive control\n", stderr);
    return 0;
}
