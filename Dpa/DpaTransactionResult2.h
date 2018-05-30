/**
* Copyright 2015-2018 MICRORISC s.r.o.
* Copyright 2018 IQRF Tech s.r.o.
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

#pragma once

#include "DpaMessage.h"
#include <chrono>

class IDpaTransactionResult2
{
public:
  enum ErrorCode {
    // transaction handling
    TRN_ERROR_BAD_RESPONSE = -7,
    TRN_ERROR_BAD_REQUEST = -6,
    TRN_ERROR_IFACE_BUSY = -5,
    TRN_ERROR_IFACE = -4,
    TRN_ERROR_ABORTED = -3,
    TRN_ERROR_IFACE_QUEUE_FULL = -2,
    TRN_ERROR_TIMEOUT = -1,
    TRN_OK = STATUS_NO_ERROR,
    // DPA response erors
    TRN_ERROR_FAIL = ERROR_FAIL,
    TRN_ERROR_PCMD = ERROR_PCMD,
    TRN_ERROR_PNUM = ERROR_PNUM,
    TRN_ERROR_ADDR = ERROR_ADDR,
    TRN_ERROR_DATA_LEN = ERROR_DATA_LEN,
    TRN_ERROR_DATA = ERROR_DATA,
    TRN_ERROR_HWPID = ERROR_HWPID,
    TRN_ERROR_NADR = ERROR_NADR,
    TRN_ERROR_IFACE_CUSTOM_HANDLER = ERROR_IFACE_CUSTOM_HANDLER,
    TRN_ERROR_MISSING_CUSTOM_DPA_HANDLER = ERROR_MISSING_CUSTOM_DPA_HANDLER,
    TRN_ERROR_USER_TO = ERROR_USER_TO,
    TRN_STATUS_CONFIRMATION = STATUS_CONFIRMATION,
    TRN_ERROR_USER_FROM = ERROR_USER_FROM
  };

  virtual int getErrorCode() const = 0;
  virtual void overrideErrorCode( ErrorCode err ) = 0;
  virtual std::string getErrorString() const = 0;
  virtual const DpaMessage& getRequest() const = 0;
  virtual const DpaMessage& getConfirmation() const = 0;
  virtual const DpaMessage& getResponse() const = 0;
  virtual const std::chrono::time_point<std::chrono::system_clock>& getRequestTs() const = 0;
  virtual const std::chrono::time_point<std::chrono::system_clock>& getConfirmationTs() const = 0;
  virtual const std::chrono::time_point<std::chrono::system_clock>& getResponseTs() const = 0;
  virtual bool isConfirmed() const = 0;
  virtual bool isResponded() const = 0;
  virtual ~IDpaTransactionResult2() {};
};

/////////////////////////////////////
// class DpaTransactionResult2
/////////////////////////////////////
class DpaTransactionResult2 : public IDpaTransactionResult2
{
private:
  /// original request creating the transaction
  DpaMessage m_request;
  /// received  confirmation
  DpaMessage m_confirmation;
  /// received  response
  DpaMessage m_response;
  /// request timestamp
  std::chrono::time_point<std::chrono::system_clock> m_request_ts;
  /// confirmation timestamp
  std::chrono::time_point<std::chrono::system_clock> m_confirmation_ts;
  /// response timestamp
  std::chrono::time_point<std::chrono::system_clock> m_response_ts;
  /// overall error code
  int m_errorCode = TRN_ERROR_ABORTED;
  /// response code
  int m_responseCode = TRN_OK;
  /// received and set response flag
  bool m_isResponded = false;
  /// received and set confirmation flag
  int m_isConfirmed = false;

public:
  DpaTransactionResult2() = delete;
  DpaTransactionResult2( const DpaMessage& request );
  int getErrorCode() const override;
  void overrideErrorCode( IDpaTransactionResult2::ErrorCode err ) override;
  std::string getErrorString() const override;
  const DpaMessage& getRequest() const override;
  const DpaMessage& getConfirmation() const override;
  const DpaMessage& getResponse() const override;
  const std::chrono::time_point<std::chrono::system_clock>& getRequestTs() const override;
  const std::chrono::time_point<std::chrono::system_clock>& getConfirmationTs() const override;
  const std::chrono::time_point<std::chrono::system_clock>& getResponseTs() const override;
  bool isConfirmed() const override;
  bool isResponded() const override;
  void setConfirmation( const DpaMessage& confirmation );
  void setResponse( const DpaMessage& response );
  void setErrorCode( int errorCode );
};