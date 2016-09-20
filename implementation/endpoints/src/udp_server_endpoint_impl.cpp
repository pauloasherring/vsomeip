// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <sstream>

#include <boost/asio/ip/multicast.hpp>


#include "../include/endpoint_definition.hpp"
#include "../include/endpoint_host.hpp"
#include "../include/udp_server_endpoint_impl.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../logging/include/logger.hpp"
#include "../../utility/include/byteorder.hpp"
#include "../../utility/include/utility.hpp"
#include "../../service_discovery/include/defines.hpp"

namespace ip = boost::asio::ip;

namespace vsomeip {

udp_server_endpoint_impl::udp_server_endpoint_impl(
        std::shared_ptr< endpoint_host > _host,
        endpoint_type _local,
        boost::asio::io_service &_io)
    : server_endpoint_impl<ip::udp_ext>(
            _host, _local, _io, VSOMEIP_MAX_UDP_MESSAGE_SIZE),
      socket_(_io, _local.protocol()),
      recv_buffer_(VSOMEIP_MAX_UDP_MESSAGE_SIZE, 0) {
    boost::system::error_code ec;

    boost::asio::socket_base::reuse_address optionReuseAddress(true);
    socket_.set_option(optionReuseAddress);

    if (_local.address().is_v4()) {
        boost::asio::ip::address_v4 its_unicast_address
            = configuration::get()->get_unicast_address().to_v4();
        boost::asio::ip::multicast::outbound_interface option(its_unicast_address);
        socket_.set_option(option);
    }

    socket_.bind(_local, ec);
    boost::asio::detail::throw_error(ec, "bind");

    boost::asio::socket_base::broadcast option(true);
    socket_.set_option(option);

#ifdef WIN32
    const char* optval("0001");
    ::setsockopt(socket_.native(), IPPROTO_IP, IP_PKTINFO,
        optval, sizeof(optval));
#else
    int optval(1);
    ::setsockopt(socket_.native(), IPPROTO_IP, IP_PKTINFO,
        &optval, sizeof(optval));
#endif
}

udp_server_endpoint_impl::~udp_server_endpoint_impl() {
}

bool udp_server_endpoint_impl::is_local() const {
    return false;
}

void udp_server_endpoint_impl::start() {
    receive();
}

void udp_server_endpoint_impl::stop() {
    std::lock_guard<std::mutex> its_lock(stop_mutex_);
    server_endpoint_impl::stop();
    if (socket_.is_open()) {
        boost::system::error_code its_error;
        socket_.close(its_error);
    }
}

void udp_server_endpoint_impl::receive() {
    std::lock_guard<std::mutex> its_lock(stop_mutex_);
    if(socket_.is_open()) {
        socket_.async_receive_from(
                boost::asio::buffer(&recv_buffer_[0], max_message_size_),
            remote_,
            std::bind(
                &udp_server_endpoint_impl::receive_cbk,
                std::dynamic_pointer_cast<
                    udp_server_endpoint_impl >(shared_from_this()),
                std::placeholders::_1,
                std::placeholders::_2,
                std::placeholders::_3
            )
        );
    }
}

void udp_server_endpoint_impl::restart() {
    receive();
}

bool udp_server_endpoint_impl::send_to(
    const std::shared_ptr<endpoint_definition> _target,
    const byte_t *_data, uint32_t _size, bool _flush) {
    std::lock_guard<std::mutex> its_lock(mutex_);
    endpoint_type its_target(_target->get_address(), _target->get_port());
    return send_intern(its_target, _data, _size, _flush);
}

void udp_server_endpoint_impl::send_queued(
        queue_iterator_type _queue_iterator) {
    message_buffer_ptr_t its_buffer = _queue_iterator->second.front();
#if 0
        std::stringstream msg;
        msg << "usei::sq(" << _queue_iterator->first.address().to_string() << ":"
            << _queue_iterator->first.port() << "): ";
        for (std::size_t i = 0; i < its_buffer->size(); ++i)
            msg << std::hex << std::setw(2) << std::setfill('0')
                << (int)(*its_buffer)[i] << " ";
        VSOMEIP_DEBUG << msg.str();
#endif
     socket_.async_send_to(
        boost::asio::buffer(*its_buffer),
        _queue_iterator->first,
        std::bind(
            &udp_server_endpoint_base_impl::send_cbk,
            shared_from_this(),
            _queue_iterator,
            std::placeholders::_1,
            std::placeholders::_2
        )
    );
}

udp_server_endpoint_impl::endpoint_type
udp_server_endpoint_impl::get_remote() const {
    return remote_;
}

bool udp_server_endpoint_impl::get_remote_address(
        boost::asio::ip::address &_address) const {
    boost::asio::ip::address its_address = remote_.address();
    if (its_address.is_unspecified()) {
        return false;
    } else {
        _address = its_address;
    }
    return true;
}

unsigned short udp_server_endpoint_impl::get_remote_port() const {
    return remote_.port();
}

bool udp_server_endpoint_impl::is_joined(const std::string &_address) const {
    return (joined_.find(_address) != joined_.end());
}

void udp_server_endpoint_impl::join(const std::string &_address) {
    try {
        if (!is_joined(_address)) {
            if (local_.address().is_v4()) {
                socket_.set_option(ip::udp_ext::socket::reuse_address(true));
                socket_.set_option(
                    boost::asio::ip::multicast::enable_loopback(false));
                socket_.set_option(boost::asio::ip::multicast::join_group(
                    boost::asio::ip::address::from_string(_address).to_v4()));
            } else if (local_.address().is_v6()) {
                socket_.set_option(ip::udp_ext::socket::reuse_address(true));
                socket_.set_option(
                    boost::asio::ip::multicast::enable_loopback(false));
                socket_.set_option(boost::asio::ip::multicast::join_group(
                    boost::asio::ip::address::from_string(_address).to_v6()));
            }
            joined_.insert(_address);
        }
    }
    catch (const std::exception &e) {
        VSOMEIP_ERROR << e.what();
    }
}

void udp_server_endpoint_impl::leave(const std::string &_address) {
    try {
        if (is_joined(_address)) {
            if (local_.address().is_v4()) {
                socket_.set_option(boost::asio::ip::multicast::leave_group(
                    boost::asio::ip::address::from_string(_address)));
            } else if (local_.address().is_v6()) {
                socket_.set_option(boost::asio::ip::multicast::leave_group(
                    boost::asio::ip::address::from_string(_address)));
            }
            joined_.erase(_address);
        }
    }
    catch (const std::exception &e) {
        VSOMEIP_ERROR << e.what();
    }
}

void udp_server_endpoint_impl::add_default_target(
        service_t _service, const std::string &_address, uint16_t _port) {
    endpoint_type its_endpoint(
            boost::asio::ip::address::from_string(_address), _port);
    default_targets_[_service] = its_endpoint;
}

void udp_server_endpoint_impl::remove_default_target(service_t _service) {
    default_targets_.erase(_service);
}

bool udp_server_endpoint_impl::get_default_target(service_t _service,
        udp_server_endpoint_impl::endpoint_type &_target) const {
    bool is_valid(false);
    auto find_service = default_targets_.find(_service);
    if (find_service != default_targets_.end()) {
        _target = find_service->second;
        is_valid = true;
    }
    return is_valid;
}

unsigned short udp_server_endpoint_impl::get_local_port() const {
    boost::system::error_code its_error;
    return socket_.local_endpoint(its_error).port();
}

// TODO: find a better way to structure the receive functions
void udp_server_endpoint_impl::receive_cbk(
        boost::system::error_code const &_error, std::size_t _bytes,
        boost::asio::ip::address const &_destination) {
#if 0
    std::stringstream msg;
    msg << "usei::rcb(" << _error.message() << "): ";
    for (std::size_t i = 0; i < _bytes; ++i)
        msg << std::hex << std::setw(2) << std::setfill('0')
            << (int) recv_buffer_[i] << " ";
    VSOMEIP_DEBUG << msg.str();
#endif
    std::shared_ptr<endpoint_host> its_host = this->host_.lock();
    if (its_host) {
        if (!_error && 0 < _bytes) {
            std::size_t remaining_bytes = _bytes;
            std::size_t i = 0;
            do {
                uint32_t current_message_size
                    = utility::get_message_size(&this->recv_buffer_[i],
                            (uint32_t) remaining_bytes);
                if (current_message_size > VSOMEIP_SOMEIP_HEADER_SIZE &&
                        current_message_size <= remaining_bytes) {
                    remaining_bytes -= current_message_size;
                    if (utility::is_request(
                            recv_buffer_[i + VSOMEIP_MESSAGE_TYPE_POS])) {
                        client_t its_client;
                        std::memcpy(&its_client,
                            &recv_buffer_[i + VSOMEIP_CLIENT_POS_MIN],
                            sizeof(client_t));
                        session_t its_session;
                        std::memcpy(&its_session,
                            &recv_buffer_[i + VSOMEIP_SESSION_POS_MIN],
                            sizeof(session_t));
                        clients_mutex_.lock();
                        clients_[its_client][its_session] = remote_;
                        clients_mutex_.unlock();
                    }
                    service_t its_service = VSOMEIP_BYTES_TO_WORD(recv_buffer_[i + VSOMEIP_SERVICE_POS_MIN],
                            recv_buffer_[i + VSOMEIP_SERVICE_POS_MAX]);
                    if (its_service != VSOMEIP_SD_SERVICE ||
                        (current_message_size > VSOMEIP_SOMEIP_HEADER_SIZE &&
                                current_message_size >= remaining_bytes)) {
                            its_host->on_message(&recv_buffer_[i], current_message_size, this, _destination);
                    } else {
                        //ignore messages for service discovery with shorter SomeIP length
                        VSOMEIP_ERROR << "Received an unreliable vSomeIP SD message with too short length field";
                    }
                    i += current_message_size;
                } else {
                    VSOMEIP_ERROR << "Received an unreliable vSomeIP message with bad length field";
                    service_t its_service = VSOMEIP_BYTES_TO_WORD(recv_buffer_[VSOMEIP_SERVICE_POS_MIN],
                            recv_buffer_[VSOMEIP_SERVICE_POS_MAX]);
                    if (its_service != VSOMEIP_SD_SERVICE) {
                        its_host->on_error(&recv_buffer_[i], (uint32_t)remaining_bytes, this);
                    }
                    remaining_bytes = 0;
                }
            } while (remaining_bytes > 0);
            restart();
        } else {
            receive();
        }
    }
}

client_t udp_server_endpoint_impl::get_client(std::shared_ptr<endpoint_definition> _endpoint) {
    endpoint_type endpoint(_endpoint->get_address(), _endpoint->get_port());
    std::lock_guard<std::mutex> its_lock(clients_mutex_);
    for (auto its_client : clients_) {
        for (auto its_session : clients_[its_client.first]) {
            if (endpoint == its_session.second) {
                // TODO: Check system byte order before convert!
                client_t client = client_t(its_client.first << 8 | its_client.first >> 8);
                return client;
            }
        }
    }
    return 0;
}

} // namespace vsomeip
