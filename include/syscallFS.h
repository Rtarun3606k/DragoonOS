#ifndef SYSCALL_FS_H
#define SYSCALL_FS_H

#include <stddef.h>
#include <stdint.h>

#define SYSCALL_FS_FD_OFFSET 3U

/*
 * Canonicalises input against the calling task's cwd.  The result always
 * starts with '/', has no "." / ".." / "//" segments and never ends with a
 * slash (except for the root "/").  Paths longer than out_size are truncated
 * and NUL-terminated.
 */
void fs_resolve_path(const char *input, char *out, size_t out_size);

uint32_t syscall_fs_open(uint32_t path_addr, uint32_t flags);
uint32_t syscall_fs_close(uint32_t fd);
uint32_t syscall_fs_read(uint32_t fd, uint32_t buf_addr, uint32_t len);
uint32_t syscall_fs_write(uint32_t fd, uint32_t buf_addr, uint32_t len);
uint32_t syscall_fs_lseek(uint32_t fd, int32_t offset, uint32_t whence);
uint32_t syscall_fs_stat(uint32_t path_addr, uint32_t stat_addr);
uint32_t syscall_fs_fstat(uint32_t fd, uint32_t stat_addr);
uint32_t syscall_fs_mkdir(uint32_t path_addr);
uint32_t syscall_fs_unlink(uint32_t path_addr);
uint32_t syscall_fs_readdir(uint32_t path_addr, uint32_t target_idx, uint32_t dirent_addr);
uint32_t syscall_fs_chdir(uint32_t path_addr);
void syscall_fs_close_all(void);
void syscall_fs_close_for_pid(int pid);

#endif
