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

#pragma once

#include "message.h"
#include <fty/expected.h>
#include <functional>
#include <string>

namespace messagebus {

typedef void(MessageListenerFn)(Message);
using MessageListener = std::function<MessageListenerFn>;

class MessageBus
{
public:
    virtual ~MessageBus() = default;

    /// Try a connection with message bus
    [[nodiscard]] virtual fty::Expected<void> connect() = 0;

    /// Publish message to a topic
    ///
    /// @param topic     The topic to use
    /// @param message   The message object to send
    [[nodiscard]] virtual fty::Expected<void> publish(const std::string& topic, const Message& message) = 0;

    /// Subscribe to a topic
    ///
    /// @param topic             The topic to subscribe
    /// @param messageListener   The message listener to call on message
    [[nodiscard]] virtual fty::Expected<void> subscribe(const std::string& topic, MessageListener messageListener) = 0;

    /// Unsubscribe to a topic
    ///
    /// @param topic             The topic to unsubscribe
    /// @param messageListener   The message listener to remove from topic
    [[nodiscard]] virtual fty::Expected<void> unsubscribe(const std::string& topic) = 0;

    /// Send request to a queue
    ///
    /// @param requestQueue    The queue to use
    /// @param message         The message to send
    [[nodiscard]] virtual fty::Expected<void> sendRequest(const std::string& requestQueue, const Message& message) = 0;

    /// Send request to a queue and receive response to a specific listener
    ///
    /// @param requestQueue    The queue to use
    /// @param message         The message to send
    /// @param messageListener The listener where to receive response (on queue set to reply to field)
    [[nodiscard]] virtual fty::Expected<void> sendRequest(
        const std::string& requestQueue, const Message& message, MessageListener messageListener) = 0;

    /// Send a reply to a queue
    ///
    /// @param replyQueue      The queue to use
    /// @param message         The message to send
    [[nodiscard]] virtual fty::Expected<void> sendReply(const std::string& replyQueue, const Message& message) = 0;

    /// Receive message from queue
    ///
    /// @param queue             The queue where receive message
    /// @param messageListener   The message listener to use for this queue
    [[nodiscard]] virtual fty::Expected<void> receive(const std::string& queue, MessageListener messageListener) = 0;

    /// Send request to a queue and wait to receive response
    ///
    /// @param requestQueue    The queue to use
    /// @param message         The message to send
    /// @param receiveTimeOut  Wait for response until timeout is reach
    ///
    /// @return message as response
    [[nodiscard]] virtual fty::Expected<Message> request(
        const std::string& requestQueue, const Message& message, int receiveTimeOut) = 0;

protected:
    MessageBus() = default;
};

//======================================================================================================================
// HELPERS
//======================================================================================================================

/// @brief Generate a random uuid
///
/// @return uuid
std::string generateUuid();

/// @brief Generate a random clientName
///
/// @param clientName prefix for client Name
///
/// @return client Name
std::string getClientId(const std::string& prefix);

/// @brief Malamute implementation
///
/// @param clientName prefix for client Name
///
/// @return client Name
MessageBus* MlmMessageBus(const std::string& endpoint, const std::string& clientName);
} // namespace messagebus
