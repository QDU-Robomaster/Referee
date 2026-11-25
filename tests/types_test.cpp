#include <cstddef>
#include <type_traits>

#include "RefereeTypes.hpp"

using Packet = RefereeTypes::RobotGameRefereePack;
static_assert(std::is_trivially_copyable_v<Packet>);
static_assert(std::is_standard_layout_v<Packet>);
static_assert(alignof(Packet) == 1);
static_assert(sizeof(RefereeTypes::RobotStatus) == 17);
static_assert(sizeof(RefereeTypes::GameStatus) == 11);
static_assert(sizeof(RefereeTypes::SentryInfo) == 14);
static_assert(sizeof(RefereeTypes::RFID) == 6);
static_assert(sizeof(RefereeTypes::RobotPosForSentry) == 40);
static_assert(sizeof(RefereeTypes::RobotPOS) == 12);
static_assert(sizeof(Packet) == 116);
static_assert(offsetof(Packet, robot_status) == 0);
static_assert(offsetof(Packet, game_status) == 17);
static_assert(offsetof(Packet, sentry_info) == 28);
static_assert(offsetof(Packet, rfid) == 42);
static_assert(offsetof(Packet, bullet_17_remain) == 48);
static_assert(offsetof(Packet, outpost_r_hp) == 50);
static_assert(offsetof(Packet, outpost_b_hp) == 52);
static_assert(offsetof(Packet, ally_base_hp) == 54);
static_assert(offsetof(Packet, enemy_base_hp) == 56);
static_assert(offsetof(Packet, damage_difference) == 58);
static_assert(offsetof(Packet, ally_control_degree) == 60);
static_assert(offsetof(Packet, enemy_control_degree) == 62);
static_assert(offsetof(Packet, sentry_pos) == 64);
static_assert(offsetof(Packet, robot_pos) == 104);
static_assert(offsetof(RefereeTypes::RobotStatus, bullet_speed_limit) == 12);
static_assert(offsetof(RefereeTypes::SentryInfo, attack_time) == 6);
static_assert(offsetof(RefereeTypes::RFID, rfid_status_2) == 4);

int main() { return 0; }
