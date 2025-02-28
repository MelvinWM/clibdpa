/**
 * Copyright 2017 IQRF Tech s.r.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "IqrfCdcChannel.h"
#include "IqrfSpiChannel.h"
#include "DpaHandler2.h"
#include "IqrfTrace.h"
#include "IqrfTraceHex.h"
#include <iostream>
#include <chrono>
#include <thread>

using namespace std;
using namespace iqrf;

void asynchronousMessageHandler(const DpaMessage& message) {
  TRC_INFORMATION("Received as async msg: " << endl 
    << MEM_HEX((unsigned char *)message.DpaPacketData(), message.GetLength()));
}

int main( int argc, char** argv ) {
  //TRC_START( "log.txt", iqrf::TrcLevel::Debug, 1000000 ); //tracing to ./log.txt
  TRC_START( "", iqrf::TrcLevel::Debug, 1000000 ); //tracing to cout
  TRC_FUNCTION_ENTER( "" );

  // COM interface
  string portNameWinCdc = "COM4";
  string serviceId = "";
  //string portNameLinuxCdc = "/dev/ttyACM0";
  string portNameLinuxSpi = "/dev/spidev0.0";
  spi_iqrf_config_struct spiCfg;
  spiCfg.powerEnableGpioPin = 1;

  IDpaHandler2 *dpaHandler = nullptr;

  try {
    TRC_INFORMATION( "Creating DPA handler" );
    // IQRF CDC interface
    IChannel *cdc = new IqrfCdcChannel(portNameWinCdc);

    // DPA handler
    dpaHandler = new DpaHandler2(cdc);

    // Register async. messages handler
    dpaHandler->registerAsyncMessageHandler( serviceId, &asynchronousMessageHandler );

    // default iqrf communication mode is standard 
    dpaHandler->setRfCommunicationMode( IDpaTransaction2::kStd );
    // Define and set FRC timing
    IDpaTransaction2::TimingParams frcTiming;
    frcTiming.bondedNodes = 2;
    frcTiming.discoveredNodes = 2;
    frcTiming.frcResponseTime = IDpaTransaction2::FrcResponseTime::k20620Ms;
    dpaHandler->setTimingParams( frcTiming );
  }
  catch ( std::exception &e ) {
    CATCH_EXC_TRC_WAR( std::exception, e, "Cannot create DpaHandler Interface: " );
    return 2;
  }

  if ( dpaHandler ) {
    try {
      // DPA message
      DpaMessage dpaRequest;

      // Pulse LEDR at [C]
      dpaRequest.DpaPacket().DpaRequestPacket_t.NADR = 0x00;
      dpaRequest.DpaPacket().DpaRequestPacket_t.PNUM = PNUM_LEDR;
      dpaRequest.DpaPacket().DpaRequestPacket_t.PCMD = CMD_LED_PULSE;
      dpaRequest.DpaPacket().DpaRequestPacket_t.HWPID = HWPID_DoNotCheck;
      // Set request data length      
      dpaRequest.SetLength( sizeof( TDpaIFaceHeader ));
      // Send DPA request
      cout << "Pulse LEDR\r\n";
      auto dt = dpaHandler->executeDpaTransaction( dpaRequest, 1000 );
      auto res = dt->get();
      int err = res->getErrorCode();
      auto s = res->getErrorString();
      const uint8_t *buf = res->getResponse().DpaPacket().Buffer;
      int sz = res->getResponse().GetLength();
      for ( int i = 0; i < sz; i++ )
        cout << std::hex << (int)buf[i] << ' ';

      // Set FRC params 
      dpaRequest.DpaPacket().DpaRequestPacket_t.PNUM = PNUM_FRC;
      dpaRequest.DpaPacket().DpaRequestPacket_t.PCMD = CMD_FRC_SET_PARAMS;
      dpaRequest.DpaPacket().DpaRequestPacket_t.DpaMessage.PerFrcSetParams_RequestResponse.FRCresponseTime = IDpaTransaction2::FrcResponseTime::k20620Ms;
      // Set request data length      
      dpaRequest.SetLength( sizeof( TDpaIFaceHeader ) + sizeof( TPerFrcSetParams_RequestResponse ) );
      cout << "\r\nSet FRC params\r\n";
      dt = dpaHandler->executeDpaTransaction( dpaRequest, 1000 );
      res = dt->get();
      err = res->getErrorCode();
      s = res->getErrorString();
      buf = res->getResponse().DpaPacket().Buffer;
      sz = res->getResponse().GetLength();
      for ( int i = 0; i < sz; i++ )
        cout << std::hex << (int)buf[i] << ' ';

      // Send FRC
      dpaRequest.DpaPacket().DpaRequestPacket_t.PNUM = PNUM_FRC;
      dpaRequest.DpaPacket().DpaRequestPacket_t.PCMD = CMD_FRC_SEND;
      dpaRequest.DpaPacket().DpaRequestPacket_t.DpaMessage.PerFrcSend_Request.FrcCommand = 0xc0;
      // Set request data length      
      dpaRequest.SetLength( sizeof( TDpaIFaceHeader ) + 3);
      cout << "\r\nSend FRC\r\n";
      dt = dpaHandler->executeDpaTransaction( dpaRequest, 10000 );
      res = dt->get();
      err = res->getErrorCode();
      s = res->getErrorString();
      buf = res->getResponse().DpaPacket().Buffer;
      sz = res->getResponse().GetLength();
      for ( int i = 0; i < sz; i++ )
        cout << std::hex << (int)buf[i] << ' ';

      /*
      // Discovery
      dpaRequest.DpaPacket().DpaRequestPacket_t.PNUM = PNUM_COORDINATOR;
      dpaRequest.DpaPacket().DpaRequestPacket_t.PCMD = CMD_COORDINATOR_DISCOVERY;
      dpaRequest.DpaPacket().DpaRequestPacket_t.DpaMessage.PerCoordinatorDiscovery_Request.TxPower = 0x07;
      dpaRequest.DpaPacket().DpaRequestPacket_t.DpaMessage.PerCoordinatorDiscovery_Request.MaxAddr = 0xfe;
      dpaRequest.SetLength( sizeof( TDpaIFaceHeader ) + sizeof( TPerCoordinatorDiscovery_Request ) );
      dt = dpaHandler->executeDpaTransaction( dpaRequest, 500 );
      res = dt->get();
      err = res->getErrorCode();
      s = res->getErrorString();
      buf = res->getResponse().DpaPacket().Buffer;
      sz = res->getResponse().GetLength();
      for ( int i = 0; i < sz; i++ )
        cout << std::hex << (int)buf[i] << ' ';
      */

      this_thread::sleep_for( chrono::seconds( 1 ) );
    }
    catch ( std::exception& e ) {
      CATCH_EXC_TRC_WAR(std::exception, e, "Error in ExecuteDpaTransaction: ");
    }
  }
  else {
    TRC_ERROR( "Dpa interface is not working" );
  }

  this_thread::sleep_for( chrono::seconds( 1 ) );
  TRC_INFORMATION( "Clean after yourself" );
  delete dpaHandler;

  TRC_FUNCTION_LEAVE( "" );
  TRC_STOP

  return 0;
}
