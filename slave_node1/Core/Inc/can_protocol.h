#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include <stdint.h>

/* 标准协议 ID */

//从机 2 的紧急/高温告警帧
#define CAN_ID_S2_ALARM      0x081U

//主机发给从机 1 的控制/按键查询命令帧
#define CAN_ID_S1_COMMAND    0x100U

//主机发给从机 2 的控制/按键查询命令帧
#define CAN_ID_S2_COMMAND    0x101U

//从机 1 的常规遥测数据帧
#define CAN_ID_S1_TELEMETRY  0x180U

//从机 2 的常规遥测数据帧
#define CAN_ID_S2_TELEMETRY  0x181U

//触发从机“立即上传数据”
#define CAN_COMMAND_QUERY_NOW 0x01U

/* 把16位的数据拆分成八位数组 */
void CanProtocol_PutU16BE(uint8_t *dst, uint16_t value);
uint16_t CanProtocol_GetU16BE(const uint8_t *src);

/* 有符号处理*/
void CanProtocol_PutS16BE(uint8_t *dst, int16_t value);
int16_t CanProtocol_GetS16BE(const uint8_t *src);

#endif
