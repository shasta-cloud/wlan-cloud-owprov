//
// Created by stephane bourque on 2021-10-02.
//

#pragma once

#include "framework/SubSystemServer.h"

namespace OpenWifi {

    class TagServer : public SubSystemServer, Poco::Runnable {
    public:

        typedef std::map<std::string,uint32_t>  DictMap;
        typedef std::map<std::string,DictMap>   EntityToDict;

        static auto instance() {
            static auto instance_ = new TagServer;
            return instance_;
        }

        int Start() override;
        void Stop() override;
        void run() override;

    private:
        Poco::Thread                Worker_;
        std::atomic_bool            Running_ = false;
        EntityToDict                E2D_;

        TagServer() noexcept:
            SubSystemServer("TagServer", "TAGS", "tags")
            {
            }
    };

    inline auto TagServer() { return TagServer::instance(); }

}
