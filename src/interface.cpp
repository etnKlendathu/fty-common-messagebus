/*  =========================================================================
    fty_common_messagebus_interface - class description

    Copyright (C) 2014 - 2020 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    =========================================================================
*/

#include "fty/messagebus/interface.h"
#include "malamute.h"
#include <uuid/uuid.h>
#include <chrono>
#include <ctime>

namespace messagebus {

const std::string Message::REPLY_TO       = "_replyTo";
const std::string Message::CORRELATION_ID = "_correlationId";
const std::string Message::FROM           = "_from";
const std::string Message::TO             = "_to";
const std::string Message::SUBJECT        = "_subject";
const std::string Message::STATUS         = "_status";
const std::string Message::TIMEOUT        = "_timeout";

Message::Message(const MetaData& metaData, const UserData& userData)
    : m_metadata(metaData)
    , m_data(userData)
{
}

MetaData& Message::metaData()
{
    return m_metadata;
}

UserData& Message::userData()
{
    return m_data;
}

const MetaData& Message::metaData() const
{
    return m_metadata;
}
const UserData& Message::userData() const
{
    return m_data;
}

bool Message::isOnError() const
{
    bool returnValue = false;
    auto iterator    = m_metadata.find(Message::STATUS);
    if (iterator != m_metadata.end() && STATUS_KO == iterator->second) {
        returnValue = true;
    }
    return returnValue;
}

std::string generateUuid()
{
    uuid_t uuid;
    uuid_generate(uuid);
    std::array<char, 37> buf;
    uuid_unparse(uuid, buf.data());
    return std::string(buf.data());
}

std::string getClientId(const std::string& prefix)
{
    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
    std::string clientId = prefix + "-" + std::to_string(ms.count());
    return clientId;
}

MessageBus* MlmMessageBus(const std::string& endpoint, const std::string& clientName)
{
    return new messagebus::MessageBusMalamute(endpoint, clientName);
}
} // namespace messagebus
