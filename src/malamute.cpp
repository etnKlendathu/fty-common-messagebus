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

#include "malamute.h"
#include "fty/messagebus/message.h"
#include <czmq.h>
#include <fty/fty-log.h>
#include <malamute.h>
#include <thread>

namespace messagebus {

static std::string popStr(zmsg_t* msg)
{
    if (char* str = zmsg_popstr(msg)) {
        std::string ret = str;
        zstr_free(&str);
        return ret;
    }
    return {};
}

static Message _fromZmsg(zmsg_t* msg)
{
    Message message;

    if (zmsg_size(msg)) {
        std::string key = popStr(msg);
        if (key == "__METADATA_START") {
            while (!(key = popStr(msg)).empty()) {
                if (key == "__METADATA_END") {
                    break;
                }
                message.metaData().emplace(key, popStr(msg));
            }
        } else {
            message.userData().emplace_back(key);
        }
        while (!(key = popStr(msg)).empty()) {
            message.userData().emplace_back(key);
        }
    }
    return message;
}

static zmsg_t* _toZmsg(const Message& message)
{
    zmsg_t* msg = zmsg_new();

    zmsg_addstr(msg, "__METADATA_START");
    for (const auto& pair : message.metaData()) {
        zmsg_addstr(msg, pair.first.c_str());
        zmsg_addstr(msg, pair.second.c_str());
    }
    zmsg_addstr(msg, "__METADATA_END");

    for (const auto& item : message.userData()) {
        zmsg_addstr(msg, item.c_str());
    }

    return msg;
}

MessageBusMalamute::MessageBusMalamute(const std::string& endpoint, const std::string& clientName)
    : m_client(mlm_client_new(), &MessageBusMalamute::freeClient)
    , m_clientName(clientName)
    , m_endpoint(endpoint)
{
}

MessageBusMalamute::~MessageBusMalamute()
{
    m_running = false;
    if (m_listener.joinable()) {
        m_listener.join();
    }
}

void MessageBusMalamute::freeClient(mlm_client_t* client)
{
    mlm_client_destroy(&client);
}

fty::Expected<void> MessageBusMalamute::connect()
{
    if (mlm_client_connect(m_client.get(), m_endpoint.c_str(), 1000, m_clientName.c_str()) == -1) {
        return fty::unexpected("Failed to connect to Malamute server.");
    }

    logTrace() << m_clientName << "connected to Malamute server";
    m_running = true;
    m_listener = std::thread(&MessageBusMalamute::threadListener, this);
    return {};
}

fty::Expected<void> MessageBusMalamute::publish(const std::string& topic, const Message& message)
{
    if (m_publishTopic.empty()) {
        m_publishTopic = topic;
        if (mlm_client_set_producer(m_client.get(), m_publishTopic.c_str()) == -1) {
            return fty::unexpected("Failed to set producer on Malamute connection.");
        }

        logTrace() << m_clientName << "- registered as stream producter on '" << m_publishTopic.c_str() << "'";
    }

    if (topic != m_publishTopic) {
        return fty::unexpected("MessageBusMalamute requires publishing to declared topic.");
    }

    zmsg_t* msg = _toZmsg(message);
    logTrace() << m_clientName << "- publishing on topic '" << m_publishTopic << "'";
    if (mlm_client_send(m_client.get(), topic.c_str(), &msg) == -1) {
        return fty::unexpected("MessageBusMalamute, cannot publish message");
    }
    return {};
}

fty::Expected<void> MessageBusMalamute::subscribe(const std::string& topic, MessageListener messageListener)
{
    if (mlm_client_set_consumer(m_client.get(), topic.c_str(), "") == -1) {
        return fty::unexpected("Failed to set consumer on Malamute connection.");
    }

    m_subscriptions.emplace(topic, messageListener);
    logTrace() << m_clientName << "- subscribed to topic '" << topic << "'";
    return {};
}

fty::Expected<void> MessageBusMalamute::unsubscribe(const std::string& topic)
{
    auto iterator = m_subscriptions.find(topic);

    if (iterator == m_subscriptions.end()) {
        return fty::unexpected("Trying to unsubscribe on non-subscribed topic.");
    }

    // Our current Malamute version is too old...
    logWarn() << m_clientName << "- mlm_client_remove_consumer() not implemented";

    m_subscriptions.erase(iterator);
    logTrace() << m_clientName << "- unsubscribed to topic '" << topic << "'";
    return {};
}

fty::Expected<void> MessageBusMalamute::sendRequest(const std::string& requestQueue, const Message& message)
{
    std::string to      = requestQueue;
    std::string subject = requestQueue;

    auto iterator = message.metaData().find(Message::CORRELATION_ID);
    if (iterator == message.metaData().end() || iterator->second.empty()) {
        logWarn() << m_clientName << "- request should have a correlation id";
    }

    iterator = message.metaData().find(Message::REPLY_TO);
    if (iterator == message.metaData().end() || iterator->second.empty()) {
        logWarn() << m_clientName << "- request should have a reply to field";
    }

    iterator = message.metaData().find(Message::TO);
    if (iterator == message.metaData().end() || iterator->second.empty()) {
        logWarn() << m_clientName << "- request should have a to field";
    } else {
        to      = iterator->second;
        subject = requestQueue;
    }

    zmsg_t* msg = _toZmsg(message);

    // Todo: Check error code after sendto
    if (mlm_client_sendto(m_client.get(), to.c_str(), subject.c_str(), nullptr, 200, &msg) == -1) {
        return fty::unexpected("MessageBusMalamute, cannot send a message");
    }
    return {};
}

fty::Expected<void> MessageBusMalamute::sendRequest(
    const std::string& requestQueue, const Message& message, MessageListener messageListener)
{
    auto iterator = message.metaData().find(Message::REPLY_TO);
    if (iterator == message.metaData().end() || iterator->second.empty()) {
        return fty::unexpected("Request must have a reply to queue.");
    }

    std::string queue(iterator->second);
    if (auto res = receive(queue, messageListener); !res) {
        return fty::unexpected(res.error());
    }

    return sendRequest(requestQueue, message);
}

fty::Expected<void> MessageBusMalamute::sendReply(const std::string& replyQueue, const Message& message)
{
    auto iterator = message.metaData().find(Message::CORRELATION_ID);
    if (iterator == message.metaData().end() || iterator->second == "") {
        return fty::unexpected("Reply must have a correlation id.");
    }

    iterator = message.metaData().find(Message::TO);
    if (iterator == message.metaData().end() || iterator->second.empty()) {
        logWarn() << m_clientName << "- request should have a to field";
    }

    zmsg_t* msg = _toZmsg(message);
    if (mlm_client_sendto(m_client.get(), iterator->second.c_str(), replyQueue.c_str(), nullptr, 200, &msg) == -1) {
        return fty::unexpected("MessageBusMalamute, cannot send a message");
    }
    return {};
}

fty::Expected<void> MessageBusMalamute::receive(const std::string& queue, MessageListener messageListener)
{
    auto iterator = m_subscriptions.find(queue);
    if (iterator != m_subscriptions.end()) {
        return fty::unexpected("Already have queue map to listener");
    }

    m_subscriptions.emplace(queue, messageListener);
    logTrace() << m_clientName << "- receive from queue '" << queue << "'";
    return {};
}

fty::Expected<Message> MessageBusMalamute::request(
    const std::string& requestQueue, const Message& message, int receiveTimeOut)
{

    auto iterator = message.metaData().find(Message::CORRELATION_ID);
    if (iterator == message.metaData().end() || iterator->second.empty()) {
        return fty::unexpected("Request must have a correlation id.");
    }

    m_syncUuid = iterator->second;
    iterator   = message.metaData().find(Message::TO);
    if (iterator == message.metaData().end() || iterator->second.empty()) {
        return fty::unexpected("Request must have a to field.");
    }

    Message msg(message);
    // Adding metadata timeout.
    msg.metaData().emplace(Message::TIMEOUT, std::to_string(receiveTimeOut));

    std::unique_lock<std::mutex> lock(m_cv_mtx);
    msg.metaData().emplace(Message::REPLY_TO, m_clientName);
    zmsg_t* msgMlm = _toZmsg(msg);

    // Todo: Check error code after sendto
    if (mlm_client_sendto(m_client.get(), iterator->second.c_str(), requestQueue.c_str(), nullptr, 200, &msgMlm) ==
        -1) {
        return fty::unexpected("MessageBusMalamute, cannot send a message");
    }

    if (m_cv.wait_for(lock, std::chrono::seconds(receiveTimeOut)) == std::cv_status::timeout) {
        throw MessageBusException("Request timed out.");
    } else {
        return m_syncResponse;
    }
}

void MessageBusMalamute::threadListener()
{
    zpoller_t* poller = zpoller_new(mlm_client_msgpipe(m_client.get()), nullptr);
    while (m_running) {
        if (zpoller_wait(poller, 100)) {
            zmsg_t* message = mlm_client_recv(m_client.get());
            if (message) {

                const char* subject = mlm_client_subject(m_client.get());
                const char* from    = mlm_client_sender(m_client.get());
                const char* command = mlm_client_command(m_client.get());

                onMessage(command, from, subject, message);
                zmsg_destroy(&message);
            }
        }
    }
    zpoller_destroy(&poller);
}

void MessageBusMalamute::onMessage(
    const std::string& command, const std::string& from, const std::string& subject, zmsg_t* msg)
{
    Message message = _fromZmsg(msg);

    if (command == "MAILBOX DELIVER") {
        if (!m_syncUuid.empty()) {
            auto it = message.metaData().find(Message::CORRELATION_ID);
            if (it != message.metaData().end()) {
                if (m_syncUuid == it->second) {
                    std::unique_lock<std::mutex> lock(m_cv_mtx);
                    m_syncResponse = message;
                    m_cv.notify_one();
                    m_syncUuid   = "";
                    return;
                }
            }
        }
    } else if (command != "STREAM DELIVER"){
        logError() << m_clientName << "- unknown malamute pattern '" << command << "' from '" << from << "' subject '" << subject << "'";
        return;
    }

    auto iterator = m_subscriptions.find(subject);
    if (iterator != m_subscriptions.end()) {
        try {
            (iterator->second)(message);
        } catch (const std::exception& e) {
            logError() << "Error in listener of queue '" << iterator->first << "': '" << e.what() << "'";
        } catch (...) {
            logError() << "Error in listener of queue '" << iterator->first << "': 'unknown error'";
        }
    } else {
        logWarn() << "Message skipped";
    }
}

} // namespace messagebus
