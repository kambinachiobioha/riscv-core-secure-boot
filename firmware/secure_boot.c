#include <stdint.h>

#define OTP_ROT_BASE         0x10002000
#define MONOTONIC_CNTR_BASE  0x10003000
#define ED25519_ENGINE_BASE  0x10004000
#define BOOT_STATUS_BASE     0x10005000

#define REG_OTP_PUBKEY_PTR   0x00
#define REG_MONOTONIC_VAL    0x00
#define REG_ED_SIG_PTR       0x00
#define REG_ED_MSG_PTR       0x04
#define REG_ED_KEY_PTR       0x08
#define REG_ED_CTRL_STAT     0x0C
#define REG_BOOT_STATE       0x00

#define ED25519_START_MASK   0x01
#define ED25519_READY_MASK   0x02
#define ED25519_VALID_MASK   0x04

#define MMIO_REG(addr)       (*(volatile uint32_t *)(addr))

typedef enum {
    BOOT_STATE_INIT      = 0x01,
    BOOT_STATE_VERIFYING = 0x02,
    BOOT_STATE_SUCCESS    = 0x03,
    BOOT_STATE_RECOVERY   = 0xFF
} boot_state_t;

void trap_to_fail_closed_recovery(uint32_t error_code) {
    MMIO_REG(BOOT_STATUS_BASE + REG_BOOT_STATE) = BOOT_STATE_RECOVERY;
    while (1) {
        __asm__ volatile("wfi");
    }
}

void configure_pmp_boundaries(void) {
    uint64_t pmpaddr0 = 0x20040000 >> 2;
    uint64_t pmpcfg0  = 0x19;
    __asm__ volatile("csrw pmpaddr0, %0" :: "r"(pmpaddr0));
    __asm__ volatile("csrw pmpcfg0, %0" :: "r"(pmpcfg0));
}

int main(void) {
    MMIO_REG(BOOT_STATUS_BASE + REG_BOOT_STATE) = BOOT_STATE_INIT;
    uint32_t *otp_public_key = (uint32_t *)MMIO_REG(OTP_ROT_BASE + REG_OTP_PUBKEY_PTR);
    
    uint32_t *signature_ptr = (uint32_t *)0x40000000;
    uint32_t *kernel_msg_ptr = (uint32_t *)0x40001000;
    uint32_t current_fw_version = MMIO_REG(0x40000FF0);

    uint32_t hardware_rollback_floor = MMIO_REG(MONOTONIC_CNTR_BASE + REG_MONOTONIC_VAL);
    if (current_fw_version < hardware_rollback_floor) {
        trap_to_fail_closed_recovery(0xDEAD0001);
    }

    MMIO_REG(BOOT_STATUS_BASE + REG_BOOT_STATE) = BOOT_STATE_VERIFYING;
    MMIO_REG(ED25519_ENGINE_BASE + REG_ED_SIG_PTR) = (uint32_t)signature_ptr;
    MMIO_REG(ED25519_ENGINE_BASE + REG_ED_MSG_PTR) = (uint32_t)kernel_msg_ptr;
    MMIO_REG(ED25519_ENGINE_BASE + REG_ED_KEY_PTR) = (uint32_t)otp_public_key;
    
    MMIO_REG(ED25519_ENGINE_BASE + REG_ED_CTRL_STAT) |= ED25519_START_MASK;

    while (!(MMIO_REG(ED25519_ENGINE_BASE + REG_ED_CTRL_STAT) & ED25519_READY_MASK)) {}

    if (!(MMIO_REG(ED25519_ENGINE_BASE + REG_ED_CTRL_STAT) & ED25519_VALID_MASK)) {
        trap_to_fail_closed_recovery(0xDEAD0002);
    }

    configure_pmp_boundaries();
    MMIO_REG(BOOT_STATUS_BASE + REG_BOOT_STATE) = BOOT_STATE_SUCCESS;

    void (*kernel_entry)(void) = (void (*)(void))0x20000000;
    kernel_entry();

    return 0;
}
