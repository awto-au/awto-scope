#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/sys/reboot.h>
#include "a2k_board/drv.h"
#include "a2k_board/supervisor.h"
#include "a2k_board/backlight.h"
#include "a2k_board/fpga.h"
#include "a2k_board/dmm.h"
#include "a2k_board/battery.h"
#include "a2k_board/calibration.h"
#include "a2k_board/probe_comp.h"
#include "a2k_board/usb_scope.h"
#include "a2k_board/usb_diag.h"
#include "a2k_board/awg.h"
#include "a2k_board/keypad.h"
#if defined(CONFIG_AW2_LCD)
#include "a2k/zephyr/lvgl_task.h"
#include "a2k/ui/screen_mgr.h"
#endif

LOG_MODULE_REGISTER(a2k_supervisor, LOG_LEVEL_INF);

/* Override weak halt: reboot on fault so Renode doesn't freeze the CPU.
 * The spurious ISR fault that fires at boot in emulation triggers this;
 * on real hardware the fault doesn't occur. */
FUNC_NORETURN void arch_system_halt(unsigned int reason)
{
    ARG_UNUSED(reason);
    sys_reboot(SYS_REBOOT_COLD);
    CODE_UNREACHABLE;
}

K_MSGQ_DEFINE(a2k_msgq, sizeof(struct a2k_msg), 32, 4);

bool g_is_renode;

/* STM32F103 RCC/FLASH registers read directly — reports what HW actually configured */
#define AW2_RCC_CR    (*(volatile uint32_t *)(0x40021000U))
#define AW2_RCC_CFGR  (*(volatile uint32_t *)(0x40021004U))
#define AW2_FLASH_ACR (*(volatile uint32_t *)(0x40022000U))

static void log_environment(bool is_renode)
{
    LOG_INF("env: Cortex-M3  sysclk_hz=%u  renode=%d",
            CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC, is_renode);

    /* Decode RCC_CFGR — what the clock tree actually settled to */
    uint32_t cfgr  = AW2_RCC_CFGR;
    uint32_t sws   = (cfgr >> 2) & 0x3;
    uint32_t hpre  = (cfgr >> 4) & 0xF;
    uint32_t ppre1 = (cfgr >> 8) & 0x7;
    uint32_t ppre2 = (cfgr >> 11) & 0x7;
    uint32_t pllmul = (cfgr >> 18) & 0xF;
    uint32_t pllsrc = (cfgr >> 16) & 0x1;

    static const char * const sws_str[] = {"HSI", "HSE", "PLL", "?"};
    static const uint32_t ahb_div[]  = {1,1,1,1,1,1,1,1,2,4,8,16,64,128,256,512};
    static const uint32_t apb_div[]  = {1,1,1,1,2,4,8,16};

    uint32_t pll_in  = pllsrc ? 8000000U : 4000000U;   /* HSE=8MHz or HSI/2=4MHz */
    uint32_t sysclk  = pll_in * (pllmul + 2);
    uint32_t hclk    = sysclk / ahb_div[hpre];
    uint32_t apb1    = hclk  / apb_div[ppre1];
    uint32_t apb2    = hclk  / apb_div[ppre2];

    LOG_INF("clk: src=%s  PLL=%ux%s  SYSCLK=%uMHz  HCLK=%uMHz",
            sws_str[sws], pllmul + 2, pllsrc ? "HSE" : "HSI/2",
            sysclk / 1000000U, hclk / 1000000U);
    LOG_INF("clk: APB1=%uMHz(max36)  APB2=%uMHz(max72)  CFGR=0x%08x",
            apb1 / 1000000U, apb2 / 1000000U, cfgr);

    uint32_t latency = AW2_FLASH_ACR & 0x7;
    LOG_INF("flash: latency=%u ws  ACR=0x%08x", latency, AW2_FLASH_ACR);

    uint32_t cr = AW2_RCC_CR;
    LOG_INF("osc: HSI=%s  HSE=%s  PLL=%s",
            (cr & BIT(1))  ? "rdy" : "off",
            (cr & BIT(17)) ? "rdy" : "off",
            (cr & BIT(25)) ? "rdy" : "off");

    LOG_INF("ker: tick=%uHz  timeslice=%uus",
            CONFIG_SYS_CLOCK_TICKS_PER_SEC, CONFIG_TIMESLICE_SIZE);
}

#if defined(CONFIG_AW2_RENODE_DETECT)
bool a2k_is_renode(void)
{
    volatile uint32_t *sentinel = (volatile uint32_t *)0x50000000;
    uint32_t cfsr_before = SCB->CFSR;
    SCB->CCR |= SCB_CCR_BFHFNMIGN_Msk;
    __DMB();
    uint32_t val = *sentinel;
    __DMB();
    SCB->CCR &= ~SCB_CCR_BFHFNMIGN_Msk;
    bool fault = (SCB->CFSR != cfsr_before);
    SCB->CFSR = SCB->CFSR;
    return !fault && (val == 0x524E4F44U);
}
#else
bool a2k_is_renode(void) { return false; }
#endif


void supervisor_init(void)
{
    g_is_renode = a2k_is_renode();
    LOG_INF("supervisor: init (renode=%d)", g_is_renode);
    log_environment(g_is_renode);

#if defined(CONFIG_AW2_BACKLIGHT)
    backlight_init();
#endif
#if defined(CONFIG_AW2_FPGA)
    fpga_init();
#endif
#if defined(CONFIG_AW2_DMM)
    dmm_init();
#endif
#if defined(CONFIG_AW2_BATTERY)
    battery_init();
#endif
#if defined(CONFIG_AW2_CALIBRATION)
    calibration_init();
#endif
#if defined(CONFIG_AW2_PROBE_COMP)
    probe_comp_init();
#endif
#if defined(CONFIG_AW2_USB)
    usb_scope_init();
#endif
#if defined(CONFIG_AW2_USB_DIAG)
    usb_diag_init();
    usb_diag_printf("supervisor", "INF", "init renode=%d", g_is_renode ? 1 : 0);
#endif
#if defined(CONFIG_AW2_AWG)
    awg_init();
#endif
#if defined(CONFIG_AW2_KEYPAD)
    keypad_init();
#endif
}

int supervisor_post(enum a2k_msg_type type, uint32_t arg)
{
    struct a2k_msg msg = { .type = type, .arg = arg };
    return k_msgq_put(&a2k_msgq, &msg, K_NO_WAIT);
}

void supervisor_run(void)
{
    LOG_INF("supervisor: running");
    struct a2k_msg msg;

    while (1) {
        while (k_msgq_get(&a2k_msgq, &msg, K_MSEC(10)) == 0) {
            switch (msg.type) {
            case MSG_ERROR:
                LOG_ERR("supervisor: error 0x%08x", msg.arg);
#if defined(CONFIG_AW2_USB_DIAG)
                usb_diag_printf("supervisor", "ERR", "error 0x%08x", msg.arg);
#endif
                break;
            case MSG_KEYPAD:
#if defined(CONFIG_AW2_LCD)
                a2k_screen_key((uint8_t)msg.arg);
#endif
                break;
            case MSG_CAPTURE_DONE:
                LOG_DBG("supervisor: capture done");
                break;
            case MSG_BATTERY_LOW:
                LOG_WRN("supervisor: battery low");
#if defined(CONFIG_AW2_USB_DIAG)
                usb_diag_printf("supervisor", "WRN", "battery low");
#endif
                break;
            default:
                break;
            }
        }
        /* Renode keypad injection: btn_panel writes key code (1-20) to 0x50000004.
         * Firmware clears it after posting to avoid double-fire. */
        if (g_is_renode) {
            volatile uint32_t *inject = (volatile uint32_t *)0x50000004;
            uint32_t key = *inject;
            if (key >= 1 && key <= 20) {
                *inject = 0;
                supervisor_post(MSG_KEYPAD, (uint8_t)key);
            }
        }
#if defined(CONFIG_AW2_LCD)
        a2k_lvgl_tick(10);
        a2k_lvgl_run();
        a2k_screen_render();
#endif
    }
}
