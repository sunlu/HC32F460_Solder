/**
 * @file    storage.c
 * @brief   Flash 存储 — 设置参数持久化
 *
 * 使用内部 Flash 最后一个 8KB 扇区 (EEPROM_BASE_ADDR = 0x0807F000)
 * 存储 Flash_values_t 结构体。
 *
 * 格式：[4字节魔数: 0x4A424332 "JBC2"] [Flash_values_t 数据] [4字节 CRC32]
 *
 * 擦除整个扇区后写入。使用 EFM_ProgramWord 进行 32 位写入。
 *
 * 注意：同 Bank 擦写会挂死 CPU —— 保存函数需从 RAM 执行，
 * 或通过菜单手动触发（中断禁用期间）。
 */

#include "storage.h"



//==============================================================================
// EEPROM (Flash模拟)
//==============================================================================
#define EEPROM_BASE_ADDR        0x0807F000UL
#define EEPROM_SECTOR_SIZE      8192
#define EEPROM_SLOT_SIZE        128
#define EEPROM_MAX_SLOTS        32

/* 魔数标记 */
#define FLASH_MAGIC     0x4A424332  /* "JBC2" */
#define FLASH_SECTOR    EEPROM_BASE_ADDR

/* 简单的 CRC32 用于数据完整性校验 */
static uint32_t crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else         crc >>= 1;
        }
    }
    return ~crc;
}

/* ========================================================================
 * storage_init — 检查 Flash 中是否有有效设置
 * ======================================================================== */
void storage_init(void)
{
    if (!storage_load()) {
        /* 无效数据 → 使用默认值（暂不保存,
         * 同 Bank Flash 擦写会挂死 CPU）。
         * 后续通过菜单或设置变更时再保存。 */
        storage_reset_defaults();
    }
}

/* ========================================================================
 * storage_save — 擦除扇区并写入设置到 Flash
 *
 * 警告：必须从 RAM 执行！同 Bank Flash 编程会挂死 CPU。
 * ======================================================================== */
void storage_save(void)
{
    uint32_t *pData = (uint32_t *)&config_val;
    uint32_t  dataSize = sizeof(t_config_value);
    uint32_t  wordCount = (dataSize + 3) / 4;
    uint32_t  crc;

    /* 计算 CRC */
    crc = crc32((uint8_t *)&config_val, dataSize);

    /* 解锁 Flash 寄存器 */
    EFM_REG_Unlock();

    /* 擦除扇区 */
    EFM_SectorErase(FLASH_SECTOR);

    /* 等待擦除完成 */
    while (EFM_GetStatus(EFM_FLAG_RDY) == RESET);

    /* 写入魔数 */
    EFM_ProgramWord(FLASH_SECTOR, FLASH_MAGIC);

    /* 写入数据（32位字） */
    for (uint32_t i = 0; i < wordCount; i++) {
        EFM_ProgramWord(FLASH_SECTOR + 4 + (i * 4), pData[i]);
    }

    /* 写入 CRC */
    EFM_ProgramWord(FLASH_SECTOR + 4 + (wordCount * 4), crc);

    /* 锁定 Flash 寄存器 */
    EFM_REG_Lock();
}

/* ========================================================================
 * storage_load — 从 Flash 加载设置
 * @return 1 = 有效数据已加载, 0 = 无效/空
 * ======================================================================== */
uint8_t storage_load(void)
{
    uint32_t magic = *(uint32_t *)FLASH_SECTOR;

    /* 检查魔数 */
    if (magic != FLASH_MAGIC) {
        return 0;
    }

    /* 读取数据 */
    uint32_t  dataSize = sizeof(t_config_value);
    uint32_t  wordCount = (dataSize + 3) / 4;
    uint32_t *pSrc = (uint32_t *)(FLASH_SECTOR + 4);
    uint32_t *pDst = (uint32_t *)&config_val;

    for (uint32_t i = 0; i < wordCount; i++) {
        pDst[i] = pSrc[i];
    }

    /* 验证 CRC */
    uint32_t stored_crc =
        *(uint32_t *)(FLASH_SECTOR + 4 + (wordCount * 4));
    uint32_t computed_crc = crc32((uint8_t *)&config_val, dataSize);

    if (stored_crc != computed_crc) {
        return 0;  /* CRC 不匹配 */
    }

    return 1;
}

/* ========================================================================
 * storage_reset_defaults — 将 flash_values 恢复为出厂默认值
 * ======================================================================== */
void storage_reset_defaults(void)
{
    config_val = config_val_default;
}
