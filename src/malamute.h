/*  =========================================================================
    fty_common_messagebus_malamute - class description

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
#include "fty/messagebus/exception.h"
#include "fty/messagebus/interface.h"
#include "fty/messagebus/message.h"
#include <condition_variable>
#include <fty/expected.h>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>

struct _zmsg_t;
typedef _zmsg_t zmsg_t;
// struct _zsock_t;
// typedef _zsock_t zsock_t;
struct _mlm_client_t;
typedef _mlm_client_t mlm_client_t;
// struct _zactor_t;
// typedef _zactor_t zactor_t;

namespace messagebus {

// typedef void(MalamuteMessageListenerFn)(const char*, const char*, zmsg_t**);
// using MalamuteMessageListener = std::function<MalamuteMessageListenerFn>;

class MessageBusMalamute : public MessageBus
{
public:
    MessageBusMalamute(const std::string& endpoint, const std::string& clientName);
    ~MessageBusMalamute() override;

    fty::Expected<void> connect() override;

    // Async topic
    fty::Expected<void> publish(const std::string& topic, const Message& message) override;
    fty::Expected<void> subscribe(const std::string& topic, MessageListener messageListener) override;
    fty::Expected<void> unsubscribe(const std::string& topic) override;

    // Async queue
    fty::Expected<void> sendRequest(const std::string& requestQueue, const Message& message) override;
    fty::Expected<void> sendRequest(
        const std::string& requestQueue, const Message& message, MessageListener messageListener) override;
    fty::Expected<void> sendReply(const std::string& replyQueue, const Message& message) override;
    fty::Expected<void> receive(const std::string& queue, MessageListener messageListener) override;

    // Sync queue
    fty::Expected<Message> request(
        const std::string& requestQueue, const Message& message, int receiveTimeOut) override;

private:
    static void freeClient(mlm_client_t* client);
    void        threadListener();
    void        onMessage(const std::string& command, const std::string& from, const std::string& subject, zmsg_t* msg);

private:
    std::unique_ptr<mlm_client_t, decltype(&freeClient)> m_client;
    std::string                                          m_clientName;
    std::string                                          m_endpoint;
    std::string                                          m_publishTopic;
    std::thread                                          m_listener;
    bool                                                 m_running = false;
    std::map<std::string, MessageListener>               m_subscriptions;
    std::condition_variable                              m_cv;
    std::mutex                                           m_cv_mtx;
    Message                                              m_syncResponse;
    std::string                                          m_syncUuid;
};
} // namespace messagebus
