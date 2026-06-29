/**
 * @file    menu_settings.h
 * @brief   焊台设置菜单接口（移植自 AxxSolder）
 *
 * 三级菜单结构：
 *   Level 0 — 功能分组选择（模式/预设/显示/声音/系统）
 *   Level 1 — 参数列表（参数名 + 当前值）
 *   Level 2 — 值编辑（白底黑字反色，编码器调值）
 *
 * 进入方式：长按编码器按键（主循环中检测 ENC_SW_LONG_PRESS）
 */

#ifndef SOURCE_MENU_SETTINGS_H
#define SOURCE_MENU_SETTINGS_H

#include "main.h"

/* 菜单是否处于激活状态（由 main.c 管理） */
extern uint8_t menu_active;

/* 菜单入口函数（阻塞式，在菜单期间暂停正常加热） */
void menu_enter(void);

#endif /* SOURCE_MENU_SETTINGS_H */
