#include "mcu.h"

#define OTS_XTAL_K (737272.73F)
#define OTS_XTAL_M (27.55F)
#define OTS_TIMEOUT_VAL (10000U)

void mcu_init(void) {
  stc_ots_init_t stcOtsInit;

  /* 1. 使能 OTS 外设时钟 */
  FCG_Fcg3PeriphClockCmd(FCG3_PERIPH_OTS, ENABLE);

  /* 2. 使能 LRC（OTS 内部需要） */
  CLK_LrcCmd(ENABLE);

  /* 3. 如果使用 XTAL，需确保 XTAL 已使能（通常在系统时钟初始化时已做） */
  /*    如果使用 HRC，需使能 HRC 和 XTAL32，见例程 */

  /* 4. 配置 OTS 初始化结构体 */
  OTS_StructInit(&stcOtsInit);
  stcOtsInit.u16ClockSrc = OTS_CLK_XTAL; // 使用 XTAL
  stcOtsInit.f32SlopeK = OTS_XTAL_K;     // XTAL 对应的 K
  stcOtsInit.f32OffsetM = OTS_XTAL_M;    // XTAL 对应的 M

  /* 5. 初始化 OTS */
  OTS_Init(&stcOtsInit);
}
float mcu_get_temp(void) {

  float temperature = 0; 
  /* 等待转换完成（轮询状态标志） */
  OTS_Polling(&temperature, OTS_TIMEOUT_VAL);

  return temperature;
}
