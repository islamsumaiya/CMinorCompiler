#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include "../../include/backend.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run_win_cmd(const char *cmd_line, bool capture_out, char *out_buf, size_t out_sz) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    HANDLE hRead = NULL, hWrite = NULL;
    SECURITY_ATTRIBUTES sa;

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    if (capture_out) {
        ZeroMemory(&sa, sizeof(sa));
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return -1;
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdOutput = hWrite;
        si.hStdError = hWrite;
    }

    char *mutable_cmd = _strdup(cmd_line);
    if (!mutable_cmd) {
        if (hRead) CloseHandle(hRead);
        if (hWrite) CloseHandle(hWrite);
        return -1;
    }

    BOOL ok = CreateProcessA(NULL, mutable_cmd, NULL, NULL, capture_out ? TRUE : FALSE,
                             0, NULL, NULL, &si, &pi);
    free(mutable_cmd);
    if (hWrite) CloseHandle(hWrite);

    if (!ok) {
        if (hRead) CloseHandle(hRead);
        return 127;
    }

    if (capture_out && out_buf && out_sz > 0) {
        DWORD total = 0, rd = 0;
        out_buf[0] = '\0';
        while (total + 1 < out_sz && ReadFile(hRead, out_buf + total, (DWORD)(out_sz - 1 - total), &rd, NULL) && rd > 0) {
            total += rd;
        }
        out_buf[total] = '\0';
    }
    if (hRead) CloseHandle(hRead);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)exit_code;
}

bool run_native_backend(BackendKind kind, const char *il_path, const char *asm_path, DiagList *diags) {
    const char *env = kind == BACKEND_LLVM ? getenv("CMINOR_LLC") : getenv("CMINOR_QBE");
    const char *tool = (env && env[0]) ? env : (kind == BACKEND_LLVM ? "llc" : "qbe");
    char cmd[2048];
    if (kind == BACKEND_LLVM) {
        snprintf(cmd, sizeof(cmd), "\"%s\" -O0 -mtriple=x86_64-pc-linux-gnu -filetype=asm \"%s\" -o \"%s\"",
                 tool, il_path, asm_path);
    } else {
        snprintf(cmd, sizeof(cmd), "\"%s\" -t amd64_sysv -o \"%s\" \"%s\"",
                 tool, asm_path, il_path);
    }
    int rc = run_win_cmd(cmd, false, NULL, 0);
    if (rc != 0) {
        diag_emit_tool(diags, DIAG_BACKEND_ERROR, "backend '%s' exited with %d", tool, rc);
        return false;
    }
    return true;
}

bool detect_backend_version(BackendKind kind, char *buf, size_t sz) {
    const char *env = kind == BACKEND_LLVM ? getenv("CMINOR_LLC") : getenv("CMINOR_QBE");
    const char *tool = (env && env[0]) ? env : (kind == BACKEND_LLVM ? "llc" : "qbe");
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "\"%s\" %s", tool, kind == BACKEND_LLVM ? "--version" : "-h");
    char raw[256];
    int rc = run_win_cmd(cmd, true, raw, sizeof(raw));
    if (rc == 127 || raw[0] == '\0') {
        snprintf(buf, sz, "not found in PATH");
        return false;
    }
    for (size_t i = 0; raw[i]; i++) {
        if (raw[i] == '\n' || raw[i] == '\r') { raw[i] = '\0'; break; }
    }
    snprintf(buf, sz, "%s", raw);
    return true;
}
#else
#define _POSIX_C_SOURCE 200809L
#include "../../include/backend.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int spawn_proc(const char *const argv[], int out_fd, int err_fd) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        if (out_fd >= 0) { dup2(out_fd, STDOUT_FILENO); close(out_fd); }
        if (err_fd >= 0) { dup2(err_fd, STDERR_FILENO); close(err_fd); }
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    int st = 0;
    if (waitpid(pid, &st, 0) < 0) return -1;
    return WIFEXITED(st) ? WEXITSTATUS(st) : 128;
}

bool run_native_backend(BackendKind kind, const char *il_path, const char *asm_path, DiagList *diags) {
    const char *env = kind == BACKEND_LLVM ? getenv("CMINOR_LLC") : getenv("CMINOR_QBE");
    const char *tool = (env && env[0]) ? env : (kind == BACKEND_LLVM ? "llc" : "qbe");
    if (kind == BACKEND_LLVM) {
        const char *const argv[] = {tool, "-O0", "-mtriple=x86_64-pc-linux-gnu", "-filetype=asm", il_path, "-o", asm_path, NULL};
        int rc = spawn_proc(argv, -1, -1);
        if (rc != 0) { diag_emit_tool(diags, DIAG_BACKEND_ERROR, "backend '%s' exited with %d", tool, rc); return false; }
    } else {
        const char *const argv[] = {tool, "-t", "amd64_sysv", "-o", asm_path, il_path, NULL};
        int rc = spawn_proc(argv, -1, -1);
        if (rc != 0) { diag_emit_tool(diags, DIAG_BACKEND_ERROR, "backend '%s' exited with %d", tool, rc); return false; }
    }
    return true;
}

bool detect_backend_version(BackendKind kind, char *buf, size_t sz) {
    const char *env = kind == BACKEND_LLVM ? getenv("CMINOR_LLC") : getenv("CMINOR_QBE");
    const char *tool = (env && env[0]) ? env : (kind == BACKEND_LLVM ? "llc" : "qbe");
    int pfd[2];
    if (pipe(pfd) != 0) { snprintf(buf, sz, "unavailable"); return false; }
    const char *const argv[] = {tool, kind == BACKEND_LLVM ? "--version" : "-h", NULL};
    int rc = spawn_proc(argv, pfd[1], pfd[1]);
    close(pfd[1]);
    char raw[256]; ssize_t n = read(pfd[0], raw, sizeof(raw) - 1); close(pfd[0]);
    if (n <= 0 || rc == 127) { snprintf(buf, sz, "not found in PATH"); return false; }
    raw[n] = '\0';
    for (ssize_t i = 0; i < n; i++) if (raw[i] == '\n' || raw[i] == '\r') { raw[i] = '\0'; break; }
    snprintf(buf, sz, "%s", raw);
    return true;
}
#endif
