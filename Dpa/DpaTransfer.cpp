/**
 * Copyright 2015-2017 MICRORISC s.r.o.
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

#include "DpaTransfer.h"
#include "DpaTransaction.h"

#include "unexpected_packet_type.h"
#include "unexpected_command.h"
#include "unexpected_peripheral.h"

#include "IqrfLogging.h"

DpaTransfer::DpaTransfer()
  : m_status(kCreated), m_messageToBeProcessed(false), m_sentMessage(nullptr),
  m_expectedDurationMs(200), m_timeoutMs(200), m_currentCommunicationMode(kStd), m_dpaTransaction(nullptr)
{
}

DpaTransfer::DpaTransfer(DpaTransaction* dpaTransaction, IqrfRfCommunicationMode comMode, int32_t timeout)
  : m_status(kCreated), m_messageToBeProcessed(false), m_sentMessage(nullptr),
  m_expectedDurationMs(timeout), m_timeoutMs(timeout), m_currentCommunicationMode(comMode), m_dpaTransaction(dpaTransaction)
{
}

DpaTransfer::~DpaTransfer()
{
  delete m_sentMessage;
}

void DpaTransfer::ProcessSentMessage(const DpaMessage& sentMessage)
{
  TRC_ENTER("");

  if (m_status != kCreated)
  {
    throw std::logic_error("Sent message already set.");
  }

  // current request status is set as sent
  if (sentMessage.NodeAddress() == COORDINATOR_ADDRESS) {
    SetStatus(kSentCoordinator);
  }
  else {
    SetStatus(kSent);
  }

  // message itself is destroyed after being sent
  delete m_sentMessage;
  // creating object to hold new request
  m_sentMessage = new DpaMessage(sentMessage);

  // setting default timeout, no estimation yet
  SetTimingForCurrentTransfer();
  
  TRC_LEAVE("");
}

void DpaTransfer::MessageReceived(bool flg) {
  // there were delays sometime before processing causing timeout and not processing response at all
  m_messageToBeProcessed = flg;
}

void DpaTransfer::ProcessReceivedMessage(const DpaMessage& receivedMessage)
{
  TRC_ENTER("");
  // direction
  auto messageDirection = receivedMessage.MessageDirection();

  // is transfer in progress?
  if (!IsInProgressStatus(m_status)) {
    m_messageToBeProcessed = false;
    return;
  }
  // yes
  else {
    // no request is expected
    if (messageDirection != DpaMessage::kResponse && messageDirection != DpaMessage::kConfirmation) {
      // clear flag after processing
      m_messageToBeProcessed = false;
      throw unexpected_packet_type("Response is expected.");
    }
    // same as sent request
    if (receivedMessage.NodeAddress() != m_sentMessage->NodeAddress()) {
      // clear flag after processing
      m_messageToBeProcessed = false;
      throw unexpected_peripheral("Different node address than in sent message.");
    }
    // same as sent request
    if (receivedMessage.PeripheralType() != m_sentMessage->PeripheralType()) {
      // clear flag after processing
      m_messageToBeProcessed = false;
      throw unexpected_peripheral("Different peripheral type than in sent message.");
    }
    // same as sent request
    if ((receivedMessage.PeripheralCommand() & ~0x80) != m_sentMessage->PeripheralCommand()) {
      // clear flag after processing
      m_messageToBeProcessed = false;
      throw unexpected_command("Different peripheral command than in sent message.");
    }
  }

  if (messageDirection == DpaMessage::kConfirmation) {
    if (m_dpaTransaction) {
      m_dpaTransaction->processConfirmationMessage(receivedMessage);
    }

    ProcessConfirmationMessage(receivedMessage);
    TRC_INF("Confirmation processed.");
  }
  else {
    if (m_dpaTransaction) {
      m_dpaTransaction->processResponseMessage(receivedMessage);
    }

    ProcessResponseMessage(receivedMessage);
    TRC_INF("Response processed.");
  }

  // clear flag after processing
  m_messageToBeProcessed = false;
  TRC_LEAVE("");
}

void DpaTransfer::ProcessConfirmationMessage(const DpaMessage& confirmationMessage)
{
  if (confirmationMessage.NodeAddress() == DpaMessage::kBroadCastAddress) {
    SetStatus(kConfirmationBroadcast);
  }
  else {
    SetStatus(kConfirmation);
  }

  m_confirmationMessage = confirmationMessage;

  // setting timeout based on the confirmation
  SetTimingForCurrentTransfer(EstimatedTimeout(confirmationMessage));
}

void DpaTransfer::ProcessResponseMessage(const DpaMessage& responseMessage)
{
  // if there is a request to coordinator then after receiving response it is allowed to send another
  if (m_status == kSentCoordinator) {
    // done, next request gets ready 
    SetStatus(kProcessed);
  }
  else {
    // only if there is not infinite timeout
    if (m_expectedDurationMs != 0) {
      SetStatus(kReceivedResponse);
      // adjust timing before allowing next request
      SetTimingForCurrentTransfer(EstimatedTimeout(responseMessage));
    }
    // infinite timeout
    else {
      // done, next request gets ready 
      SetStatus(kProcessed);
    }
  }

  m_responseMessage = responseMessage;
}

int32_t DpaTransfer::EstimatedTimeout(const DpaMessage& receivedMessage)
{
  // direction
  auto direction = receivedMessage.MessageDirection();

  // double check
  if (direction != DpaMessage::kConfirmation && direction != DpaMessage::kResponse) {
    throw std::invalid_argument("Parameter is not a received message type.");
  }

  // confirmation
  if (direction == DpaMessage::kConfirmation) {
    auto iFace = receivedMessage.DpaPacket().DpaResponsePacket_t.DpaMessage.IFaceConfirmation;

    // save for later use with response
    m_hops = iFace.Hops;
    m_timeslotLength = iFace.TimeSlotLength;
    m_hopsResponse = iFace.HopsResponse;

    // lp
    if (m_currentCommunicationMode == kLp) {
      return EstimateLpTimeout(m_hops, m_timeslotLength, m_hopsResponse);
    }
    // std
    return EstimateStdTimeout(m_hops, m_timeslotLength, m_hopsResponse);
  }

  // response
  if (direction == DpaMessage::kResponse) {
    // lp
    if (m_currentCommunicationMode == kLp) {
      return EstimateLpTimeout(m_hops, m_timeslotLength, m_hopsResponse, 
        receivedMessage.GetLength() - (sizeof(TDpaIFaceHeader) + 2));
    }
    // std
    return EstimateStdTimeout(m_hops, m_timeslotLength, m_hopsResponse, 
      receivedMessage.GetLength() - (sizeof(TDpaIFaceHeader) + 2));
  }
}

int32_t DpaTransfer::EstimateStdTimeout(uint8_t hopsRequest, uint8_t timeslotReq, uint8_t hopsResponse, int8_t responseDataLength)
{
  TRC_ENTER("");
  int32_t responseTimeSlotLengthMs;

  auto estimatedTimeoutMs = (hopsRequest + 1) * timeslotReq * 10;

  // estimation from confirmation 
  if (responseDataLength == -1) {
    if (timeslotReq == 20) {
      responseTimeSlotLengthMs = 200;
    }
    else {
      // worst case
      responseTimeSlotLengthMs = 60;
    }
  }
  // correction of the estimation from response 
  else {
    TRC_DBG("PData length of the received response: " << PAR((int)responseDataLength));
    if (responseDataLength >= 0 && responseDataLength < 16)
    {
      responseTimeSlotLengthMs = 40;
    }
    else if (responseDataLength >= 16 && responseDataLength <= 39)
    {
      responseTimeSlotLengthMs = 50;
    }
    else if (responseDataLength > 39)
    {
      responseTimeSlotLengthMs = 60;
    }
    TRC_DBG("Correction of the response timeout: " << PAR(responseTimeSlotLengthMs));
  }

  estimatedTimeoutMs += (hopsResponse + 1) * responseTimeSlotLengthMs + m_safetyTimeoutMs;

  TRC_INF("Estimated STD timeout: " << PAR(estimatedTimeoutMs));
  TRC_LEAVE("");
  return estimatedTimeoutMs;
}

int32_t DpaTransfer::EstimateLpTimeout(uint8_t hopsRequest, uint8_t timeslotReq, uint8_t hopsResponse, int8_t responseDataLength)
{
  TRC_ENTER("");
  int32_t responseTimeSlotLengthMs;

  auto estimatedTimeoutMs = (hopsRequest + 1) * timeslotReq * 10;

  // estimation from confirmation 
  if (responseDataLength == -1) {
    if (timeslotReq == 20) {
      responseTimeSlotLengthMs = 200;
    }
    else {
      // worst case
      responseTimeSlotLengthMs = 110;
    }
  }
  // correction of the estimation from response 
  else {
    TRC_DBG("PData length of the received response: " << PAR((int)responseDataLength));
    if (responseDataLength >= 0 && responseDataLength < 11)
    {
      responseTimeSlotLengthMs = 80;
    }
    else if (responseDataLength >= 11 && responseDataLength <= 33)
    {
      responseTimeSlotLengthMs = 90;
    }
    else if (responseDataLength >= 34 && responseDataLength <= 56)
    {
      responseTimeSlotLengthMs = 100;
    }
    else if (responseDataLength > 56)
    {
      responseTimeSlotLengthMs = 110;
    }
    TRC_DBG("Correction of the response timeout: " << PAR(responseTimeSlotLengthMs));
  }

  estimatedTimeoutMs += (hopsResponse + 1) * responseTimeSlotLengthMs + m_safetyTimeoutMs;

  TRC_INF("Estimated LP timeout: " << PAR(estimatedTimeoutMs));
  TRC_LEAVE("");
  return estimatedTimeoutMs;
}

void DpaTransfer::SetTimingForCurrentTransfer(int32_t estimatedTimeMs)
{
  TRC_ENTER("");

  // waiting forever
  if (m_timeoutMs == 0) {
    m_expectedDurationMs = m_timeoutMs;
    TRC_INF("Expected duration to wait :" << PAR(m_expectedDurationMs));
    return;
  }

  // adjust time to wait before allowing next request to go the iqrf network
  if (m_status == kReceivedResponse) {
    //adjust new timing based on length of PData in response
    m_expectedDurationMs = estimatedTimeMs;
    TRC_INF("New expected duration to wait :" << PAR(m_expectedDurationMs));
    return;
  }

  // estimation done
  if (estimatedTimeMs >= 0) {
    // either default timeout is 200 or user sets lower time than estimated
    if (m_timeoutMs < estimatedTimeMs) {
      // in both cases use estimation from confirmation
      m_timeoutMs = estimatedTimeMs;
    }
    // set new duration
    // there is also case when user sets higher than estimation then user choice is set
    m_expectedDurationMs = m_timeoutMs;
    TRC_INF("Expected duration to wait :" << PAR(m_expectedDurationMs));
  }

  // start time when dpa request is sent and rerun again when confirmation is received
  m_startTime = std::chrono::system_clock::now();
  TRC_INF("Transfer status: started");

  TRC_LEAVE("");
}

DpaTransfer::DpaTransferStatus DpaTransfer::ProcessStatus() {
  TRC_ENTER("");

  // changes m_status, does not care about remains
  // todo: refactor and rename - two functions
  CheckTimeout();

  TRC_LEAVE(PAR(m_status));
  return m_status;
}

int32_t DpaTransfer::CheckTimeout()
{
  int32_t remains(0);

  if (m_status == kCreated) {
    TRC_INF("Transfer status: created");
    return remains;
  }

  if (m_status == kAborted) {
    TRC_INF("Transfer status: aborted");
    return remains;
  }

  bool timingFinished(false);

  // infinite (0) is out of this statement
  if (m_expectedDurationMs > 0) {
    // passed time from sent request
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - m_startTime);
    remains = m_expectedDurationMs - duration.count();
    TRC_INF("Time to wait: " << PAR(remains));

    // already over?
    timingFinished = remains < 0;
  }

  // not yet set and yes time is over
  // processed or timeouted can be set only after finished timing
  if (m_status != kProcessed && m_status != kTimeout) {
    if (timingFinished) {
      // double check that there is no message yet to be processed
      // this allows not to exit transfer before actual processing
      if (!m_messageToBeProcessed) {
        // and we have received confirmation for broadcast or response
        if (m_status == kConfirmationBroadcast || m_status == kReceivedResponse) {
          SetStatus(kProcessed);
          TRC_INF("Transfer status: processed");
        }
        else {
          SetStatus(kTimeout);
          TRC_INF("Transfer status: timeout");
        }
      }
    }
  }

  // time to wait
  return remains;
}

bool DpaTransfer::IsInProgress() {
  return IsInProgressStatus(ProcessStatus());
}

bool DpaTransfer::IsInProgress(int32_t& expectedDuration) {
  expectedDuration = CheckTimeout();
  return IsInProgressStatus(m_status);
}

bool DpaTransfer::IsInProgressStatus(DpaTransferStatus status) {
  switch (status)
  {
  case kSent:
  case kSentCoordinator:
  case kConfirmation:
  case kConfirmationBroadcast:
  case kReceivedResponse:
    return true;
  // kCreated, kProcessed, kTimeout, kAbort, kError
  default:
    return false;
  }
}

void DpaTransfer::Abort() {
  std::lock_guard<std::mutex> lck(m_statusMutex);
  m_status = kAborted;
}

void DpaTransfer::SetStatus(DpaTransfer::DpaTransferStatus status)
{
  TRC_ENTER("");
  {
    std::lock_guard<std::mutex> lck(m_statusMutex);
    m_status = status;
  }
  TRC_LEAVE("");
}
