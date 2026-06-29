#ifndef __MAIN_H__
#define __MAIN_H__

#include "hc32_ll.h"
#include "config.h"

/* ============================================================================
 * 驱动及系统底层函数原型声明
 * ============================================================================ */
void bsp_clk_init(void);
void sys_tick_init(void);
void gpio_all_init(void);
 

#endif /* __MAIN_H__ */

