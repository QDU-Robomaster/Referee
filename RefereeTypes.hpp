#pragma once

#include <cstdint>

/**
 * @brief 裁判模块发布的数据类型。
 *        Data types published by the referee Module.
 */
namespace RefereeTypes
{
/**
 * @brief 0x0001 比赛状态数据。
 *        0x0001 game status data.
 */
struct [[gnu::packed]] GameStatus
{
  uint8_t game_type : 4;      /* 比赛类型 */
  uint8_t game_progress : 4;  /* 当前比赛阶段 */
  uint16_t stage_remain_time; /* 当前阶段剩余时间 */
  uint64_t sync_time_stamp;   /* 时间戳 */
};

/**
 * @brief 0x0201 机器人性能体系数据。
 *        0x0201 robot performance data.
 */
struct [[gnu::packed]] RobotStatus
{
  uint8_t robot_id;                  /* 本机器人 ID */
  uint8_t robot_level;               /* 机器人等级 */
  uint16_t remain_hp;                /* 机器人当前血量 */
  uint16_t max_hp;                   /* 机器人血量上限 */
  uint16_t shooter_cooling_value;    /* 机器人射击热量每秒冷却值 */
  uint16_t shooter_heat_limit;       /* 机器人射击热量上限 */
  uint16_t chassis_power_limit;      /* 机器人底盘功率上限 */
  float bullet_speed_limit;          /* 射击初速度上限，m/s */
  uint8_t power_gimbal_output : 1;   /* gimbal输出，0为无输出，1为24V输出 */
  uint8_t power_chassis_output : 1;  /* chassis输出，0为无输出，1为24V输出*/
  uint8_t power_launcher_output : 1; /* shooter输出，0为无输出，1为24V 输出 */
};

/**
 * @brief 0x0203 机器人位置数据。
 *        0x0203 robot position data.
 */
struct [[gnu::packed]] RobotPOS
{
  float x;     /* 本机器人位置x坐标，单位m */
  float y;     /* 本机器人位置y坐标，单位m */
  float angle; /* 本机器人测速模块的朝向 单位：度 正北为0度 */
};

/**
 * @brief 0x0209 机器人 RFID 模块状态。
 *        0x0209 robot RFID module status.
 */
struct [[gnu::packed]] RFID
{
  // Protocol 2027 table 1-19: two independent bit masks, not the 2025 bit layout.
  uint32_t rfid_status;
  uint16_t rfid_status_2;
};

/**
 * @brief 0x020B 地面机器人位置数据。
 *        0x020B ground robot position data.
 */
struct [[gnu::packed]] RobotPosForSentry
{
  float heavy_x; /*己方重装机器人位置 x 轴坐标，单位：m*/
  float heavy_y; /*己方重装机器人位置 y 轴坐标，单位：m*/
  float reserved_x;
  float reserved_y;
  float standard_3_x; /*己方 3 号步兵机器人位置 x 轴坐标，单位：m*/
  float standard_3_y; /*己方 3 号步兵机器人位置 y 轴坐标，单位：m*/
  float standard_4_x; /*己方 4 号步兵机器人位置 x 轴坐标，单位：m*/
  float standard_4_y; /*己方 4 号步兵机器人位置 y 轴坐标，单位：m*/
  float res_1;        /*保留位*/
  float res_2;        /*保留位*/
};

/**
 * @brief 0x020D 哨兵自主决策相关信息同步。
 *        0x020D sentry autonomous decision information.
 */
struct [[gnu::packed]] SentryInfo
{
  uint32_t exchanged_bullet_num : 11;  /*允许发弹量*/
  uint32_t exchanged_bullet_times : 4; /*成功远程兑换允许发弹量的次数*/
  uint32_t exchanged_blood_times : 4;  /*哨兵机器人成功远程兑换血量的次数*/
  uint32_t could_risen_free : 1;       /*当前是否可以确认免费复活*/
  uint32_t could_risen_exchanged : 1;  /*哨兵机器人当前是否可以兑换立即复活*/
  uint32_t risen_cost : 10;            /*哨兵机器人当前若兑换立即复活需要花费的金币数*/
  uint32_t res1 : 1;                   /*保留位*/

  uint16_t out_of_combat : 1;
  uint16_t team_bullet_allowance : 11;
  uint16_t current_state : 2;
  uint16_t own_mech_state : 1;
  uint16_t enhanced_state : 1;
  uint8_t attack_time;
  uint8_t defend_time;
  uint8_t move_time;
  uint8_t reserved_time;
  uint8_t enhanced_attack_time;
  uint8_t enhanced_defend_time;
  uint8_t enhanced_move_time;
  uint8_t reserved_enhanced_time;
};

/**
 * @brief 机器人、比赛与发射相关的裁判系统摘要，紧凑布局共 116 字节。
 *        Referee summary of robot, game and launcher data, 116 bytes in packed layout.
 */
struct [[gnu::packed]] RobotGameRefereePack
{
  RobotStatus robot_status;  /* 机器人状态 */
  GameStatus game_status;    /* 比赛信息 */
  SentryInfo sentry_info;    /*哨兵数据*/
  RFID rfid;                 /*机器人RFID模块状态*/
  uint16_t bullet_17_remain; /*  17mm 弹丸允许发弹量 */
  uint16_t outpost_r_hp;     /* R/B refer to the initial field side, not allegiance. */
  uint16_t outpost_b_hp;
  uint16_t ally_base_hp;
  uint16_t enemy_base_hp;
  int16_t damage_difference;
  uint16_t ally_control_degree;
  uint16_t enemy_control_degree;
  RobotPosForSentry sentry_pos; /* 0x020B */
  RobotPOS robot_pos;
};
}  // namespace RefereeTypes
