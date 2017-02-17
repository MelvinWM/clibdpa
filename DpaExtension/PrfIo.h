#pragma once

#include "DpaTask.h"
#include <chrono>
#include <vector>
#include <map>
#include <tuple>

class PrfIo : public DpaTask
{
public:
  enum class Cmd {
    DIRECTION = CMD_IO_DIRECTION,
    SET = CMD_IO_SET,
    GET = CMD_IO_GET
  };

  enum class Port {
    PORTA = PNUM_IO_PORTA,
    //TRISA = PNUM_IO_TRISA,
    PORTB = PNUM_IO_PORTB,
    //TRISB = PNUM_IO_TRISB,
    PORTC = PNUM_IO_PORTC,
    //TRISC = PNUM_IO_TRISC,
    PORTE = PNUM_IO_PORTE,
    //TRISE = PNUM_IO_TRISE,
    WPUB = PNUM_IO_WPUB,
    WPUE = PNUM_IO_WPUE,
    DELAY = 0xff
  };

  //typedef std::tuple<Port, uint8_t, uint8_t> Triplet;
  typedef std::vector<std::tuple<Port, uint8_t, uint8_t>> DirectionAndSet;

  static const std::string PRF_NAME;

  PrfIo();
  PrfIo(uint16_t address);
  virtual ~PrfIo();

  void parseResponse(const DpaMessage& response) override;
  void parseCommand(const std::string& command) override;
  const std::string& encodeCommand() const override;

  //cmd methods
  void directionCommand(DirectionAndSet directions);
  void directionCommand(Port port, uint8_t bit, bool direction);
  void setCommand(DirectionAndSet settings);
  void setCommand(Port port, uint8_t bit, bool val);
  void getCommand();
  void getCommand(Port port, uint8_t bit);

  //aux methods
  static void addDirection(DirectionAndSet& das, Port port, uint8_t bits, uint8_t dirs);
  static void addTriplet(DirectionAndSet& das, Port port, uint8_t bits, uint8_t dirs);
  static void addDelay(DirectionAndSet& das, uint16_t delay);

  static Port parsePort(const std::string& port);
  static const std::string& encodePort(Port port);

  //get methods
  uint8_t getInput(Port port) const;
  bool getInput(Port port, uint8_t bit) const;

  Cmd getCmd() const;
  void setCmd(Cmd cmd);

private:
  typedef std::chrono::duration<unsigned long, std::ratio<2097, 1000>> milis2097;
  typedef std::chrono::duration<unsigned long, std::ratio<32768, 1000000>> micros32768;

  Cmd m_cmd = Cmd::GET;

  uint8_t m_PORTA = 0;
  uint8_t m_PORTB = 0;
  uint8_t m_PORTC = 0;
  uint8_t m_PORTE = 0;
  uint8_t m_WPUB = 0;
  uint8_t m_WPUE = 0;

};
