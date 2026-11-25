#include "test_helpers.hpp"

#include <thread>

using Error = LibXR::ErrorCode;

static void identify(Referee& ref, LibXR::UART& uart, uint8_t id)
{
  Bytes status(17);
  status[0] = id;
  assert(receive(ref, uart, 0x0201, status));
}

static void sentry_feedback(Referee& ref, LibXR::UART& uart, uint8_t bullet, uint8_t hp)
{
  Bytes payload(14);
  const uint32_t VALUE = (static_cast<uint32_t>(bullet) << 11) |
                         (static_cast<uint32_t>(hp) << 15);
  for (int i = 0; i < 4; ++i) { payload[i] = (VALUE >> (8 * i)) & 0xFF; }
  assert(receive(ref, uart, 0x020D, payload));
}

static void check_sentry_requests()
{
  LibXR::UART uart;
  Referee ref(uart);
  identify(ref, uart, 7);
  assert(ref.RequestRemoteBulletExchange() == Error::BUSY);
  sentry_feedback(ref, uart, 0, 0);
  assert(ref.SetNeedBullet(100) == Error::OK);
  assert(ref.RequestRemoteBulletExchange() == Error::OK);
  assert(ref.RequestRemoteBulletExchange() == Error::BUSY);
  assert(ref.RequestRemoteHPExchange() == Error::OK);
  assert(ref.data_.sentry_dec_data.buy_bullet_num == 100);

  uart.next_write_result = Error::TIMEOUT;
  assert(ref.SendSentryPack() == Error::TIMEOUT);
  const Bytes FIRST_PAYLOAD(uart.tx.begin() + 7, uart.tx.end() - 2);
  assert(ref.GetDecisionProgress().remote_bullet_pending);
  sentry_feedback(ref, uart, 0, 0);
  assert(ref.RequestRemoteBulletExchange() == Error::BUSY);
  assert(ref.SendSentryPack() == Error::OK);
  assert(Bytes(uart.tx.begin() + 7, uart.tx.end() - 2) == FIRST_PAYLOAD);
  sentry_feedback(ref, uart, 1, 1);
  assert(!ref.GetDecisionProgress().remote_bullet_pending);
  assert(!ref.GetDecisionProgress().remote_hp_pending);

  // Concurrent callers cannot combine two requests into an illegal +2 transition.
  std::atomic<int> accepted{0};
  std::vector<std::thread> callers;
  for (int i = 0; i < 8; ++i)
  {
    callers.emplace_back([&]()
    {
      const auto RESULT = ref.RequestRemoteBulletExchange();
      if (RESULT == Error::OK) { ++accepted; }
      else { assert(RESULT == Error::BUSY); }
    });
  }
  for (auto& caller : callers) { caller.join(); }
  assert(accepted == 1);
  assert(ref.GetDecisionProgress().remote_bullet_count == 2);
  assert(ref.SendSentryPack() == Error::OK);
  sentry_feedback(ref, uart, 2, 1);
  assert(!ref.GetDecisionProgress().remote_bullet_pending);
  sentry_feedback(ref, uart, 15, 15);
  assert(ref.RequestRemoteBulletExchange() == Error::ARG_ERR);
  assert(ref.RequestRemoteHPExchange() == Error::ARG_ERR);

  Bytes game(11);
  game[0] = 0x41;
  assert(receive(ref, uart, 0x0001, game));
  assert(ref.ResetDecisionSession() == Error::BUSY);
  game[0] = 0x11;
  assert(receive(ref, uart, 0x0001, game));
  assert(ref.GetDecisionProgress().remote_bullet_count == 0);
  assert(ref.RequestRemoteBulletExchange() == Error::BUSY);
  sentry_feedback(ref, uart, 0, 0);
  assert(ref.RequestRemoteBulletExchange() == Error::OK);
  LibXR::Timebase::now += 3000;
  assert(ref.SendSentryPack() == Error::BUSY);
  assert(ref.GetDecisionProgress().remote_bullet_pending);
}

static void check_radar_requests()
{
  LibXR::UART uart;
  Referee ref(uart);
  identify(ref, uart, 9);
  assert(receive(ref, uart, 0x020E, {2, 4, 0, 0, 0, 0, 0, 0}));
  Referee::RadarDecisionData request{};
  request.radar_cmd = 2;
  assert(ref.SendRadarDecision(request) == Error::ARG_ERR);
  request.radar_cmd = 1;
  request.armor_fortify_cmd = 2;
  assert(ref.SendRadarDecision(request) == Error::ARG_ERR);
  assert(ref.GetDecisionProgress().radar_count == 0);
  request.armor_fortify_cmd = 1;
  request.fortify_center_x = 123;
  uart.next_write_result = Error::TIMEOUT;
  assert(ref.SendRadarDecision(request) == Error::TIMEOUT);
  request.radar_cmd = 2;
  assert(ref.SendRadarDecision(request) == Error::BUSY);
  request.radar_cmd = 1;
  request.fortify_center_x = 124;
  assert(ref.SendRadarDecision(request) == Error::BUSY);
  request.fortify_center_x = 123;
  assert(ref.SendRadarDecision(request) == Error::OK);
  assert(ref.GetDecisionProgress().radar_pending);
  assert(ref.GetDecisionProgress().fortify_pending);
  assert(receive(ref, uart, 0x020E, {2, 4, 0, 0, 0, 0, 0, 0}));
  assert(ref.GetDecisionProgress().radar_pending);
  // Opportunity consumption is evidence of progress, not an invented packet ACK.
  assert(receive(ref, uart, 0x020E, {1, 2, 0, 0, 0, 0, 0, 0}));
  assert(!ref.GetDecisionProgress().radar_pending);
  assert(!ref.GetDecisionProgress().fortify_pending);
  request.radar_cmd = 2;
  request.armor_fortify_cmd = 2;
  assert(ref.SendRadarDecision(request) == Error::OK);
  LibXR::Timebase::now += 3000;
  assert(ref.SendRadarDecision(request) == Error::BUSY);
  assert(ref.GetDecisionProgress().radar_pending);
  assert(ref.SendStudentCmd(Referee::CMDID::REF_STDNT_CMD_ID_RADAR_CMD,
                            9, 0x8080, request) == Error::ARG_ERR);
}

static void check_addresses_and_maps()
{
  for (uint16_t id : {1, 3, 4, 5, 6, 101, 103, 104, 105, 106})
  {
    uint16_t client = 0;
    assert(Referee::TryGetClientID(id, client));
    assert(client == id + 0x100);
  }
  for (uint16_t id : {0, 2, 7, 9, 10, 11, 100, 102, 107, 109, 110, 111, 65535})
  {
    uint16_t client = 123;
    assert(!Referee::TryGetClientID(id, client));
    assert(client == 0 && Referee::GetClientID(id) == 0);
  }
  LibXR::UART uart;
  Referee ref(uart);
  identify(ref, uart, 1);
  Referee::UIFigure figure{};
  assert(ref.SendUIFigure(1, 0x107, figure) == Error::ARG_ERR);
  assert(ref.SendUIFigure(1, 0x165, figure) == Error::ARG_ERR);
  assert(ref.SendUIFigure(3, 0x103, figure) == Error::ARG_ERR);
  assert(ref.SendUIFigure(1, 0x101, figure) == Error::OK);
  const uint32_t SENT_AT = LibXR::Timebase::now;
  assert(ref.SendUIFigure(1, 0x101, figure) == Error::OK);
  assert(static_cast<uint32_t>(LibXR::Timebase::now - SENT_AT) >= 34);

  Referee::MapCommandMessage click{};
  assert(!ref.GetMapCommand(click));
  Bytes map(12);
  map[10] = 0x67;
  map[11] = 1;
  assert(receive(ref, uart, 0x0303, map));
  assert(ref.GetMapCommand(click) && click.command.cmd_source == 0x167);
  const auto FIRST = click;
  assert(!ref.GetMapCommand(click));
  LibXR::Timebase::now += 100;
  assert(receive(ref, uart, 0x0303, map));
  assert(ref.GetMapCommand(click));
  assert(click.receive_sequence == FIRST.receive_sequence + 1);
  assert(click.received_at_ms == FIRST.received_at_ms + 100);

  Referee::MapRobotData positions{};
  assert(ref.SendMapRobotData(positions) == Error::ARG_ERR);
  identify(ref, uart, 9);
  assert(ref.SendMapRobotData(positions) == Error::OK);
  assert(ref.SendMapRobotData(positions) == Error::BUSY);
  LibXR::Timebase::now += 200;
  assert(ref.SendMapRobotData(positions) == Error::OK);
  assert(ref.SendUIFigure(9, 0x103, figure) == Error::OK);
  identify(ref, uart, 7);
  Referee::SentryPosition path{};
  path.intention = 3;
  path.sender_id = 107;
  assert(ref.SendMapPath(path) == Error::ARG_ERR);
  path.sender_id = 7;
  assert(ref.SendMapPath(path) == Error::OK);
  assert(ref.SendMapPath(path) == Error::BUSY);
  LibXR::Timebase::now += 1000;
  assert(ref.SendMapPath(path) == Error::OK);
}

static void check_text_and_online_state()
{
  LibXR::UART uart;
  Referee ref(uart);
  assert(ref.GetStatus() == Referee::Status::OFFLINE);
  for (uint16_t id : {0x0205, 0x0302, 0x0304, 0x0306, 0x0309, 0x0F01, 0x0F02})
  {
    assert(!receive(ref, uart, id, Bytes(12)));
    assert(ref.GetStatus() == Referee::Status::OFFLINE);
    assert(ref.SendFrame(static_cast<Referee::CommandID>(id), nullptr, 0) == Error::ARG_ERR);
  }
  identify(ref, uart, 1);
  assert(ref.GetStatus() == Referee::Status::RUNNING);
  LibXR::Timebase::now += 3000;
  assert(!receive(ref, uart, 0xFFFF, {}));
  assert(ref.GetStatus() == Referee::Status::OFFLINE);
  Bytes status(17);
  status[0] = 1;
  load(uart, frame(0x0201, status));
  ref.FindHeader();
  assert(ref.GetStatus() == Referee::Status::OFFLINE);
  assert(ref.ParseData());
  assert(ref.GetStatus() == Referee::Status::RUNNING);

  Referee::UICharacter character{};
  assert(ref.FillCharacter(character, "TXT", Referee::UIFigureOp::UI_OP_ADD,
                            0, Referee::UIColor::UI_COLOR_WHITE, 20, 2, 10, 10,
                            "ASCII") == Error::OK);
  assert(ref.SendUICharacter(1, 0x101, character) == Error::OK);
  assert(ref.FillCharacter(character, "TXT", Referee::UIFigureOp::UI_OP_ADD,
                            0, Referee::UIColor::UI_COLOR_WHITE, 20, 2, 10, 10,
                            "\xE8\xBF\x99") == Error::ARG_ERR);
  assert(ref.SendUICharacter(1, 0x101, character) == Error::ARG_ERR);
  const char16_t TEXT[] = {0x8FD9, 0xD83D, 0xDE00};
  assert(ref.SendMapText(0x165, TEXT, 3, Referee::TextByteOrder::LITTLE_ENDIAN_ORDER)
         == Error::ARG_ERR);
  assert(ref.SendMapText(0x101, TEXT, 3, Referee::TextByteOrder::LITTLE_ENDIAN_ORDER)
         == Error::OK);
  assert(uart.tx[11] == 0xD9 && uart.tx[12] == 0x8F);
  assert(uart.tx[13] == 0x3D && uart.tx[14] == 0xD8);
  LibXR::Timebase::now += 334;
  assert(ref.SendMapText(0x101, TEXT, 3, Referee::TextByteOrder::BIG_ENDIAN_ORDER)
         == Error::OK);
  assert(uart.tx[11] == 0x8F && uart.tx[12] == 0xD9);
  assert(ref.SendMapText(0x101, TEXT, 2, Referee::TextByteOrder::LITTLE_ENDIAN_ORDER)
         == Error::ARG_ERR);
  Referee::CustomData2 fixed{};
  assert(ref.SendCustomDataToTerminalFixed(fixed) == Error::OK);
  assert(uart.tx.size() == 309);
  assert(ref.SendCustomDataToTerminal(fixed.data, 12) == Error::BUSY);
  LibXR::Timebase::now += 20;
  assert(ref.SendCustomDataToTerminal(fixed.data, 12) == Error::OK);
  assert(uart.tx.size() == 21);
}

int main()
{
  check_sentry_requests();
  check_radar_requests();
  check_addresses_and_maps();
  check_text_and_online_state();
  std::puts("Request ordering, retries, concurrency, addresses, maps and text passed");
}
