// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_EVENTGROUPINFO_HPP
#define VSOMEIP_EVENTGROUPINFO_HPP

#include <chrono>
#include <list>
#include <memory>
#include <set>

#include <boost/asio/ip/address.hpp>

#include <vsomeip/export.hpp>
#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint_definition;
class event;

class eventgroupinfo {
public:
    struct target_t {
        std::shared_ptr<endpoint_definition> endpoint_;
        std::chrono::high_resolution_clock::time_point expiration_;

        bool operator==(const target_t &_other) const {
            return (endpoint_ == _other.endpoint_);
        }
    };

    VSOMEIP_EXPORT eventgroupinfo();
    VSOMEIP_EXPORT eventgroupinfo(major_version_t _major, ttl_t _ttl);
    VSOMEIP_EXPORT ~eventgroupinfo();

    VSOMEIP_EXPORT major_version_t get_major() const;
    VSOMEIP_EXPORT void set_major(major_version_t _major);

    VSOMEIP_EXPORT ttl_t get_ttl() const;
    VSOMEIP_EXPORT void set_ttl(ttl_t _ttl);

    VSOMEIP_EXPORT bool is_multicast() const;
    VSOMEIP_EXPORT bool get_multicast(boost::asio::ip::address &_address,
            uint16_t &_port) const;
    VSOMEIP_EXPORT void set_multicast(const boost::asio::ip::address &_address,
            uint16_t _port);

    VSOMEIP_EXPORT const std::set<std::shared_ptr<event> > get_events() const;
    VSOMEIP_EXPORT void add_event(std::shared_ptr<event> _event);
    VSOMEIP_EXPORT void remove_event(std::shared_ptr<event> _event);

    VSOMEIP_EXPORT const std::list<target_t> get_targets() const;
    VSOMEIP_EXPORT uint32_t get_unreliable_target_count();

    VSOMEIP_EXPORT bool add_target(const target_t &_target);
    VSOMEIP_EXPORT bool add_target(const target_t &_target, const target_t &_subscriber);
    VSOMEIP_EXPORT bool update_target(
            const std::shared_ptr<endpoint_definition> &_target,
            const std::chrono::high_resolution_clock::time_point &_expiration);
    VSOMEIP_EXPORT bool remove_target(
            const std::shared_ptr<endpoint_definition> &_target);
    VSOMEIP_EXPORT void clear_targets();

    VSOMEIP_EXPORT void add_multicast_target(const target_t &_multicast_target);
    VSOMEIP_EXPORT void clear_multicast_targets();
    VSOMEIP_EXPORT const std::list<target_t> get_multicast_targets() const;

private:
    major_version_t major_;
    ttl_t ttl_;

    boost::asio::ip::address address_;
    uint16_t port_;

    std::set<std::shared_ptr<event> > events_;
    std::list<target_t> targets_;
    std::list<target_t> multicast_targets_;
};

} // namespace vsomeip

#endif // VSOMEIP_EVENTGROUPINFO_HPP
