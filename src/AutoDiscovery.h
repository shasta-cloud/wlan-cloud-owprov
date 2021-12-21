//
// Created by stephane bourque on 2021-09-29.
//

#pragma once

#include "framework/MicroService.h"
#include "framework/OpenWifiTypes.h"

namespace OpenWifi {
    class AutoDiscovery : public SubSystemServer, Poco::Runnable {
    public:

        static auto instance() {
            static auto instance_ = new AutoDiscovery;
            return instance_;
        }

        int Start() override;
        void Stop() override;
        void run() override;
        void ConnectionReceived( const std::string & Key, const std::string & Message);

    private:
        Poco::Thread                Worker_;
        std::atomic_bool            Running_ = false;
        int                         ConnectionWatcherId_=0;
        Types::StringPairQueue      NewConnections_;

        AutoDiscovery() noexcept:
            SubSystemServer("AutoDiscovery", "AUTO-DISCOVERY", "discovery")
            {
            }
    };

    inline auto AutoDiscovery() { return AutoDiscovery::instance(); }

}

