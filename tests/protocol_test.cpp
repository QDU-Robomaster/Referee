#include "test_helpers.hpp"

int main()
{
  LibXR::UART uart;
  Referee ref(uart);
  assert(ref.GetRobotID() == 0);
  assert(ref.data_.sentry_dec_data.current_state == 3);
  static_assert(static_cast<uint8_t>(Referee::RobotID::REF_BOT_RED_BASE) == 10);
  static_assert(static_cast<uint8_t>(Referee::RobotID::REF_BOT_RED_OUTPOST) == 11);
  static_assert(static_cast<uint8_t>(Referee::RobotID::REF_BOT_BLU_BASE) == 110);
  static_assert(static_cast<uint8_t>(Referee::RobotID::REF_BOT_BLU_OUTPOST) == 111);
  Referee::TerminalCommandMessage control{};
  assert(!ref.GetCustomControl(control));

  // A 30 m/s speed limit followed by 0b111 power flags. Old layout read flags as zero.
  Bytes status{7, 2, 0x34, 0x12, 0x78, 0x56, 10, 0, 100, 0, 80, 0, 0, 0, 0xF0, 0x41, 7};
  assert(receive(ref, uart, 0x0201, status));
  assert(ref.data_.robot_status.bullet_speed_limit == 30.0f);
  assert(ref.data_.robot_status.power_gimbal_output == 1);
  assert(ref.data_.robot_status.power_chassis_output == 1);
  assert(ref.data_.robot_status.power_launcher_output == 1);
  assert(ref.data_.robot_status.remain_hp == 0x1234);
  assert(LibXR::Topic::messages.at("robot_game_ref").size() == 116);
  assert(LibXR::Topic::messages.at("radar_ref").size() == sizeof(Referee::RadarPack));

  // Fixed payloads must have exactly the 2027 size: reject old and overlong layouts.
  for (const auto& entry : std::vector<std::pair<uint16_t, size_t>>{{0x0003, 24},
                                                                    {0x0101, 8},
                                                                    {0x0105, 3},
                                                                    {0x0201, 17},
                                                                    {0x0204, 7},
                                                                    {0x0206, 7},
                                                                    {0x0209, 6},
                                                                    {0x020B, 40},
                                                                    {0x020C, 2},
                                                                    {0x020D, 14},
                                                                    {0x020E, 8},
                                                                    {0x0303, 12},
                                                                    {0x0305, 48},
                                                                    {0x0A03, 10},
                                                                    {0x0A04, 12},
                                                                    {0x0A05, 47}})
  {
    assert(receive(ref, uart, entry.first, Bytes(entry.second)));
    assert(!receive(ref, uart, entry.first, Bytes(entry.second - 1)));
    assert(!receive(ref, uart, entry.first, Bytes(entry.second + 1)));
  }
  assert(!receive(ref, uart, 0x0201, Bytes(13)));
  assert(!receive(ref, uart, 0x0204, Bytes(8)));
  assert(!receive(ref, uart, 0x0A03, Bytes(12)));
  assert(!receive(ref, uart, 0xFFFF, Bytes(3)));

  assert(receive(ref, uart, 0x020D, {0, 0, 0, 0, 1, 0xF0, 1, 2, 3, 0, 4, 5, 6, 0}));
  assert(ref.data_.sentry_decision.current_state == 3);
  assert(ref.data_.sentry_decision.out_of_combat == 1);
  assert(ref.data_.sentry_decision.own_mech_state == 1);
  assert(ref.data_.sentry_decision.enhanced_state == 1);
  assert(ref.data_.sentry_decision.enhanced_move_time == 6);
  assert(receive(ref, uart, 0x0209, {1, 0, 0, 0, 0, 1}));
  assert(ref.data_.rfid.rfid_status_2 == 0x100);
  assert(receive(ref, uart, 0x020C, {0x20, 0x08}));
  assert(ref.data_.radar_mark_progress.mark_enemy_sentry_state == 1);
  assert(ref.data_.radar_mark_progress.mark_own_sentry_state == 1);
  assert(receive(ref, uart, 0x0303, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x67, 1}));
  assert(ref.data_.client_map.cmd_source == 0x167);
  assert(receive(ref, uart, 0x0003, {0, 0, 0, 0, 0, 0, 0, 0, 0xFE, 0xFF, 0, 0,
                                     1, 0, 2, 0, 3, 0, 4, 0, 5,    0,    6, 0}));
  assert(ref.robot_game_referee_pack_.outpost_r_hp == 1);
  assert(ref.robot_game_referee_pack_.ally_base_hp == 2);
  assert(ref.robot_game_referee_pack_.outpost_b_hp == 3);
  assert(ref.robot_game_referee_pack_.enemy_base_hp == 4);
  assert(ref.robot_game_referee_pack_.damage_difference == -2);
  assert(ref.robot_game_referee_pack_.ally_control_degree == 5);
  assert(ref.robot_game_referee_pack_.enemy_control_degree == 6);

  // Independent frame oracle verifies payload, address, sequence and both CRCs.
  assert(receive(ref, uart, 0x0201, status));
  ref.SetSwitchMode(Referee::State::ENHANCED_MOVE);
  ref.SetConfirmMechanism(true);
  assert(ref.SendSentryPack() == LibXR::ErrorCode::OK);
  assert(uart.tx == frame(0x0301, {0x20, 1, 7, 0, 0x80, 0x80, 0, 0, 0xC0, 1}));
  status[0] = 9;
  assert(receive(ref, uart, 0x0201, status));
  assert(receive(ref, uart, 0x020E, {2, 4, 0, 0, 0, 0, 0, 0}));
  Referee::RadarDecisionData radar{};
  radar.armor_fortify_cmd = 1;
  radar.fortify_center_x = 0x1234;
  radar.fortify_center_y = 0x5678;
  assert(ref.SendRadarDecision(radar) == LibXR::ErrorCode::OK);
  assert(uart.tx == frame(0x0301,
                          {0x21, 1, 9, 0, 0x80, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0x34,
                           0x12, 0x78, 0x56},
                          1));

  // Large and variable terminal payloads, zero-length messages, stale tail clearing.
  Bytes large(300);
  for (size_t i = 0; i < large.size(); ++i)
  {
    large[i] = static_cast<uint8_t>(i);
  }
  assert(ref.SendCustomDataToTerminal(large.data(), 300) == LibXR::ErrorCode::OK);
  assert(uart.tx == frame(0x0310, large, 2));
  assert(uart.tx.size() == 309);
  assert(receive(ref, uart, 0x0310, large));
  assert(ref.data_.custom_data2.data[299] == large[299]);
  assert(!receive(ref, uart, 0x0310, Bytes(301)));
  assert(ref.SendCustomDataToTerminal(large.data(), 301) == LibXR::ErrorCode::ARG_ERR);
  assert(ref.SendCustomDataToTerminal(nullptr, 1) == LibXR::ErrorCode::ARG_ERR);
  LibXR::Timebase::now += 20;
  assert(ref.SendCustomDataToTerminal(nullptr, 0) == LibXR::ErrorCode::OK);
  for (size_t length : {60U, 1U, 0U})
  {
    assert(receive(ref, uart, 0x0311, Bytes(length, 0xAB)));
    assert(ref.GetCustomControl(control));
    assert(control.length == length);
    assert(!ref.GetCustomControl(control));
    for (size_t i = 0; i < 60; ++i)
    {
      assert(control.data[i] == (i < length ? 0xAB : 0));
    }
  }
  assert(!receive(ref, uart, 0x0311, Bytes(61)));
  assert(!ref.GetCustomControl(control));

  // Invalid CRC leaves data unchanged; a following valid frame resynchronizes.
  auto corrupt = frame(0x0201, status);
  corrupt.back() ^= 1;
  load(uart, corrupt);
  ref.FindHeader();
  assert(!ref.ParseData());
  assert(ref.GetRobotID() == 9);
  auto bad_header = frame(0x0201, status);
  bad_header[4] ^= 1;
  auto good = frame(0x0201, status);
  bad_header.insert(bad_header.end(), good.begin(), good.end());
  load(uart, bad_header);
  ref.FindHeader();
  assert(ref.ParseData());
  auto truncated = good;
  truncated.pop_back();
  load(uart, truncated);
  ref.FindHeader();
  assert(!ref.ParseData());
  assert(receive(ref, uart, 0x0301, Bytes(118)));
  assert(!receive(ref, uart, 0x0301, Bytes(119)));
  assert(!receive(ref, uart, 0x0301, Bytes(5)));

  // Existing UI framing remains usable after increasing the frame capacity.
  status[0] = 1;
  assert(receive(ref, uart, 0x0201, status));
  Referee::UIFigure figure{};
  ref.FillLine(figure, "abc", Referee::UIFigureOp::UI_OP_ADD, 0,
               Referee::UIColor::UI_COLOR_WHITE, 2, 10, 20, 30, 40);
  assert(ref.SendUIFigure(1, 0x101, figure) == LibXR::ErrorCode::OK);
  assert(uart.tx.size() == 30);
  assert(uart.tx[7] == 1 && uart.tx[8] == 1);
  assert(uart.tx[13] == 'a' && uart.tx[14] == 'b' && uart.tx[15] == 'c');
  status[0] = 7;
  assert(receive(ref, uart, 0x0201, status));
  assert(receive(ref, uart, 0x020D, Bytes(14)));
  assert(ref.SetNeedBullet(1000) == LibXR::ErrorCode::OK);
  assert(ref.SetNeedBullet(2000) == LibXR::ErrorCode::ARG_ERR);
  assert(ref.data_.sentry_dec_data.buy_bullet_num == 1000);
  assert(ref.RequestRemoteBulletExchange() == LibXR::ErrorCode::OK);
  assert(ref.RequestRemoteHPExchange() == LibXR::ErrorCode::OK);
  assert(ref.RequestRemoteBulletExchange() == LibXR::ErrorCode::BUSY);
  assert(ref.RequestRemoteHPExchange() == LibXR::ErrorCode::BUSY);
  assert(ref.data_.sentry_dec_data.buy_bullet_num == 1000);
  ref.SetSwitchMode(static_cast<Referee::State>(7));
  assert(ref.data_.sentry_dec_data.current_state == 6);
  std::puts("Protocol parsing, publishing, frame encoding and boundary checks passed");
}
