#ifndef BOOT_JUMP_H
#define BOOT_JUMP_H

typedef enum {
    BOOT_JUMP_OK = 0,
    BOOT_JUMP_ERROR_VECTOR = -1,
    BOOT_JUMP_RETURNED = -2
} BootJumpStatus_t;

int BootJump_IsApplicationValid(void);
BootJumpStatus_t BootJump_ToApplication(void);

#endif /* BOOT_JUMP_H */
