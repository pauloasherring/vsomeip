// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_APPLICATION_IMPL_HPP
#define VSOMEIP_APPLICATION_IMPL_HPP

#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/signal_set.hpp>
#include <boost/asio/system_timer.hpp>

#include <vsomeip/export.hpp>
#include <vsomeip/application.hpp>

#include "../../routing/include/routing_manager_host.hpp"

namespace vsomeip {

class configuration;
class logger;
class routing_manager;
class routing_manager_stub;

class application_impl: public application,
        public routing_manager_host,
        public std::enable_shared_from_this<application_impl> {
public:
    VSOMEIP_EXPORT application_impl(const std::string &_name);
    VSOMEIP_EXPORT  ~application_impl();

    VSOMEIP_EXPORT void set_configuration(const std::shared_ptr<configuration> _configuration);

    VSOMEIP_EXPORT bool init();
    VSOMEIP_EXPORT void start();
    VSOMEIP_EXPORT void stop();

    // Provide services / events
    VSOMEIP_EXPORT void offer_service(service_t _service, instance_t _instance,
            major_version_t _major, minor_version_t _minor);

    VSOMEIP_EXPORT void stop_offer_service(service_t _service, instance_t _instance,
            major_version_t _major = DEFAULT_MAJOR, minor_version_t _minor = DEFAULT_MINOR);

    VSOMEIP_EXPORT void offer_event(service_t _service,
            instance_t _instance, event_t _event,
            const std::set<eventgroup_t> &_eventgroups,
            bool _is_field);
    VSOMEIP_EXPORT void stop_offer_event(service_t _service,
            instance_t _instance, event_t _event);

    // Consume services / events
    VSOMEIP_EXPORT void request_service(service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor, bool _use_exclusive_proxy);
    VSOMEIP_EXPORT void release_service(service_t _service,
            instance_t _instance);

    VSOMEIP_EXPORT void request_event(service_t _service,
            instance_t _instance, event_t _event,
            const std::set<eventgroup_t> &_eventgroups,
            bool _is_field);
    VSOMEIP_EXPORT void release_event(service_t _service,
            instance_t _instance, event_t _event);

    VSOMEIP_EXPORT void subscribe(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, major_version_t _major,
            subscription_type_e _subscription_type, event_t _event);

    VSOMEIP_EXPORT void unsubscribe(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup);

    VSOMEIP_EXPORT bool is_available(service_t _service, instance_t _instance,
            major_version_t _major = DEFAULT_MAJOR, minor_version_t _minor = DEFAULT_MINOR) const;

    VSOMEIP_EXPORT void send(std::shared_ptr<message> _message, bool _flush);

    VSOMEIP_EXPORT void notify(service_t _service, instance_t _instance,
            event_t _event, std::shared_ptr<payload> _payload) const;

    VSOMEIP_EXPORT void notify_one(service_t _service, instance_t _instance,
            event_t _event, std::shared_ptr<payload> _payload, client_t _client) const;

    VSOMEIP_EXPORT void register_state_handler(state_handler_t _handler);
    VSOMEIP_EXPORT void unregister_state_handler();

    VSOMEIP_EXPORT void register_message_handler(service_t _service,
            instance_t _instance, method_t _method, message_handler_t _handler);
    VSOMEIP_EXPORT void unregister_message_handler(service_t _service,
            instance_t _instance, method_t _method);

    VSOMEIP_EXPORT void register_availability_handler(service_t _service,
            instance_t _instance, availability_handler_t _handler,
            major_version_t _major = DEFAULT_MAJOR, minor_version_t _minor = DEFAULT_MINOR);
    VSOMEIP_EXPORT void unregister_availability_handler(service_t _service,
            instance_t _instance,
            major_version_t _major = DEFAULT_MAJOR, minor_version_t _minor = DEFAULT_MINOR);

    VSOMEIP_EXPORT void register_subscription_handler(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup, subscription_handler_t _handler);
    VSOMEIP_EXPORT void unregister_subscription_handler(service_t _service,
                instance_t _instance, eventgroup_t _eventgroup);

    VSOMEIP_EXPORT void register_subscription_error_handler(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup,
            error_handler_t _handler);
    VSOMEIP_EXPORT void unregister_subscription_error_handler(service_t _service,
                instance_t _instance, eventgroup_t _eventgroup);

    VSOMEIP_EXPORT bool is_routing() const;

    // routing_manager_host
    VSOMEIP_EXPORT const std::string & get_name() const;
    VSOMEIP_EXPORT client_t get_client() const;
    VSOMEIP_EXPORT std::shared_ptr<configuration> get_configuration() const;
    VSOMEIP_EXPORT boost::asio::io_service & get_io();

    VSOMEIP_EXPORT void on_state(state_type_e _state);
    VSOMEIP_EXPORT void on_availability(service_t _service, instance_t _instance,
            bool _is_available, major_version_t _major, minor_version_t _minor);
    VSOMEIP_EXPORT void on_message(std::shared_ptr<message> _message);
    VSOMEIP_EXPORT void on_error(error_code_e _error);
    VSOMEIP_EXPORT bool on_subscription(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, client_t _client, bool _subscribed);
    VSOMEIP_EXPORT void on_subscription_error(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, uint16_t _error);

    // service_discovery_host
    VSOMEIP_EXPORT routing_manager * get_routing_manager() const;

private:
    //
    // Types
    //
    struct sync_handler {

        sync_handler(std::function<void()> _handler) :
                    handler_(_handler),
                    is_dispatching_(false) { }

        std::function<void()> handler_;
        bool is_dispatching_;
    };

    //
    // Methods
    //
    void service();
    inline void update_session() {
        session_++;
        if (0 == session_) {
            session_++;
        }
    }

    bool is_available_unlocked(service_t _service, instance_t _instance,
                               major_version_t _major, minor_version_t _minor) const;

    typedef std::map<service_t, std::map<instance_t,  std::map<major_version_t, minor_version_t >>> available_t;
    bool are_available(available_t &_available,
                       service_t _service = ANY_SERVICE, instance_t _instance = ANY_INSTANCE,
                       major_version_t _major = ANY_MAJOR, minor_version_t _minor = ANY_MINOR) const;
    bool are_available_unlocked(available_t &_available,
                                service_t _service, instance_t _instance,
                                major_version_t _major, minor_version_t _minor) const;
    void do_register_availability_handler(service_t _service,
            instance_t _instance, availability_handler_t _handler,
            major_version_t _major, minor_version_t _minor);


    void main_dispatch();
    void dispatch();
    void invoke_handler(std::shared_ptr<sync_handler> &_handler);
    bool is_active_dispatcher(std::thread::id &_id);
    void remove_elapsed_dispatchers();

    void clear_all_handler();
    void wait_for_stop();

    void send_back_cached_event(service_t _service, instance_t _instance, event_t _event);
    void send_back_cached_eventgroup(service_t _service, instance_t _instance, eventgroup_t _eventgroup);

    //
    // Attributes
    //
    client_t client_; // unique application identifier
    session_t session_;
    std::mutex session_mutex_;

    std::mutex initialize_mutex_;
    bool is_initialized_;

    std::string name_;
    std::shared_ptr<configuration> configuration_;
    std::string file_; // configuration file
    std::string folder_; // configuration folder

    boost::asio::io_service io_;

    // Proxy to or the Routing Manager itself
    std::shared_ptr<routing_manager> routing_;

    // vsomeip state (registered / deregistered)
    state_type_e state_;

    // vsomeip state handler
    state_handler_t handler_;

    // Method/Event (=Member) handlers
    std::map<service_t,
            std::map<instance_t, std::map<method_t, message_handler_t> > > members_;
    mutable std::mutex members_mutex_;

    // Availability handlers
    std::map<service_t, std::map<instance_t, std::tuple<major_version_t, minor_version_t, availability_handler_t, bool>>> availability_;
    mutable std::mutex availability_mutex_;

    // Availability
    mutable available_t available_;

    // Subscription handlers
    std::map<service_t, std::map<instance_t, std::map<eventgroup_t, subscription_handler_t>>>
        subscription_;
    mutable std::mutex subscription_mutex_;
    std::map<service_t,
        std::map<instance_t, std::map<eventgroup_t,
        std::map<client_t, error_handler_t > > > > eventgroup_error_handlers_;
    mutable std::mutex subscription_error_mutex_;

    // Signals
    boost::asio::signal_set signals_;

    // Handlers
    mutable std::deque<std::shared_ptr<sync_handler>> handlers_;
    mutable std::mutex handlers_mutex_;

    // Dispatching
    bool is_dispatching_;
    // Dispatcher threads
    std::map<std::thread::id, std::shared_ptr<std::thread>> dispatchers_;
    // Dispatcher threads that elapsed and can be removed
    std::set<std::thread::id> elapsed_dispatchers_;
    // Dispatcher threads that blocked
    std::set<std::thread::id> blocked_dispatchers_;
    // Mutex to protect access to dispatchers_ & elapsed_dispatchers_
    std::mutex dispatcher_mutex_;
    // Condition to wakeup the dispatcher thread
    mutable std::condition_variable dispatcher_condition_;
    boost::asio::system_timer dispatcher_timer_;
    std::size_t max_dispatchers_;
    std::size_t max_dispatch_time_;

    // Workaround for destruction problem
    std::shared_ptr<logger> logger_;

    std::condition_variable stop_cv_;
    std::mutex start_stop_mutex_;
    bool stopped_;
    std::thread stop_thread_;

    bool catched_signal_;

    static uint32_t app_counter__;

    bool is_routing_manager_host_;

    // Event subscriptions
    std::mutex event_subscriptions_mutex_;
    std::map<service_t, std::map<instance_t, std::map<event_t, bool>>> event_subscriptions_;
};

} // namespace vsomeip

#endif // VSOMEIP_APPLICATION_IMPL_HPP
