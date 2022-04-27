//
// Created by stephane bourque on 2022-04-01.
//

#pragma once

#include "framework/MicroService.h"
#include "StorageService.h"
#include "APConfig.h"
#include "sdks/SDK_gw.h"
#include "WebSocketClientServer.h"

namespace OpenWifi {

    static void GetRejectedLines(const Poco::JSON::Object::Ptr &Response, Types::StringVec & Warnings) {
        try {
            if(Response->has("results")) {
                auto Results = Response->get("results").extract<Poco::JSON::Object::Ptr>();
                auto Status = Results->get("status").extract<Poco::JSON::Object::Ptr>();
                auto Rejected = Status->getArray("rejected");
                std::transform(Rejected->begin(),Rejected->end(),std::back_inserter(Warnings), [](auto i) -> auto { return i.toString(); });
//                for(const auto &i:*Rejected)
                //                  Warnings.push_back(i.toString());
            }
        } catch (...) {
        }
    }

    class VenueDeviceConfigUpdater : public Poco::Runnable {
    public:
        VenueDeviceConfigUpdater(const std::string &UUID, const std::string &venue, Poco::Logger &L) :
            uuid_(UUID),
            venue_(venue),
            Logger_(L) {
        }

        void run() final {
            ProvObjects::InventoryTag   Device;
            ProvObjects::WebSocketNotification N;
            N.content.type = "configuration_update";
            started_=true;
            if(StorageService()->InventoryDB().GetRecord("id",uuid_,Device)) {
                N.content.title = "Venue Configuration Updater: " + venue_;
                std::cout << "Starting push for " << uuid_ << std::endl;
                Logger().debug(fmt::format("{}: Computing configuration.",Device.serialNumber));
                auto DeviceConfig = std::make_shared<APConfig>(Device.serialNumber, Device.deviceType, Logger(), false);
                auto Configuration = Poco::makeShared<Poco::JSON::Object>();
                Poco::JSON::Object ErrorsObj, WarningsObj;
                ProvObjects::InventoryConfigApplyResult Results;
                if (DeviceConfig->Get(Configuration)) {
                    std::ostringstream OS;
                    Configuration->stringify(OS);
                    Results.appliedConfiguration = OS.str();
                    auto Response=Poco::makeShared<Poco::JSON::Object>();
                    Logger().debug(fmt::format("{}: Pushing configuration.",Device.serialNumber));
                    if (SDK::GW::Device::Configure(nullptr, Device.serialNumber, Configuration, Response)) {
                        Logger().debug(fmt::format("{}: Configuration pushed.",Device.serialNumber));
                        GetRejectedLines(Response, Results.warnings);
                        Results.errorCode = 0;
                        Logger().information(fmt::format("{}: Updated.", Device.serialNumber));
                        updated_++;
                        N.content.success.push_back(Device.serialNumber);
                    } else {
                        Logger().information(fmt::format("{}: Not updated.", Device.serialNumber));
                        Results.errorCode = 1;
                        failed_++;
                        N.content.warnings.push_back(Device.serialNumber);
                    }
                } else {
                    Logger().debug(fmt::format("{}: Configuration is bad.",Device.serialNumber));
                    Results.errorCode = 1;
                    N.content.errors.push_back(Device.serialNumber);
                    bad_config_++;
                }
            }
            done_ = true;
            std::cout << "Done push for " << uuid_ << std::endl;
        }

        uint64_t        updated_=0, failed_=0, bad_config_=0;
        bool            started_ = false,
                        done_ = false;

    private:
        std::string     uuid_;
        std::string     venue_;
        Poco::Logger    &Logger_;
        inline Poco::Logger & Logger() { return Logger_; }
    };

    class VenueConfigUpdater: public Poco::Runnable {
    public:
        explicit VenueConfigUpdater(const std::string & VenueUUID, const SecurityObjects::UserInfo &UI, uint64_t When, Poco::Logger &L) :
            VenueUUID_(VenueUUID),
            UI_(UI),
            When_(When),
            Logger_(L)
        {

        }

        inline std::string Start() {
            JobId_ = MicroService::CreateUUID();
            Worker_.start(*this);
            return JobId_;
        }

    private:
        std::string                 VenueUUID_;
        SecurityObjects::UserInfo   UI_;
        uint64_t                    When_;
        Poco::Logger                &Logger_;
        Poco::Thread                Worker_;
        std::string                 JobId_;
        Poco::ThreadPool            Pool_{2,16,300};

        inline Poco::Logger & Logger() { return Logger_; }

        inline void run() final {

            if(When_ && When_>OpenWifi::Now())
                Poco::Thread::trySleep( (long) (When_ - OpenWifi::Now()) * 1000 );

            ProvObjects::WebSocketNotification N;
            N.content.type = "configuration_update";

            Logger().information(fmt::format("Job {} Starting.", JobId_));

            ProvObjects::Venue  Venue;
            uint64_t Updated = 0, Failed = 0 , BadConfigs = 0 ;
            if(StorageService()->VenueDB().GetRecord("id",VenueUUID_,Venue)) {
                const std::size_t MaxThreads=16;
                struct tState {
                    Poco::Thread                thr_;
                    VenueDeviceConfigUpdater    *task= nullptr;
                };

                std::array<tState,MaxThreads> Tasks;

                for(const auto &uuid:Venue.devices) {
                    auto NewTask = new VenueDeviceConfigUpdater(uuid, Venue.info.name, Logger());
                    std::cout << "Scheduling config push for " << uuid << std::endl;
                    bool found_slot = false;
                    while (!found_slot) {
                        for (auto &cur_task: Tasks) {
                            if (cur_task.task == nullptr) {
                                cur_task.task = NewTask;
                                cur_task.thr_.start(*NewTask);
                                found_slot = true;
                            }
                        }

                        //  Let's look for a slot...
                        if (!found_slot) {
                            for (auto &cur_task: Tasks) {
                                if (cur_task.task != nullptr && cur_task.task->started_) {
                                    if (cur_task.thr_.isRunning())
                                        continue;
                                    if (!cur_task.thr_.isRunning() && cur_task.task->done_) {
                                        cur_task.thr_.join();
                                        Updated += cur_task.task->updated_;
                                        Failed += cur_task.task->failed_;
                                        BadConfigs += cur_task.task->bad_config_;
                                        cur_task.task->started_ = cur_task.task->done_ = false;
                                        delete cur_task.task;
                                        cur_task.task = nullptr;
                                    }
                                }
                            }
                        }
                    }
                }
                Logger().debug("Waiting for outstanding update threads to finish.");
                bool stillTasksRunning=true;
                while(stillTasksRunning) {
                    stillTasksRunning = false;
                    for(auto &cur_task:Tasks) {
                        if(cur_task.task!= nullptr && cur_task.task->started_) {
                            if(cur_task.thr_.isRunning()) {
                                stillTasksRunning = true;
                                continue;
                            }
                            if(!cur_task.thr_.isRunning() && cur_task.task->done_) {
                                cur_task.thr_.join();
                                Updated += cur_task.task->updated_;
                                Failed += cur_task.task->failed_;
                                BadConfigs += cur_task.task->bad_config_;
                                cur_task.task->started_ = cur_task.task->done_ = false;
                                delete cur_task.task;
                                cur_task.task = nullptr;
                            }
                        }
                    }
                }

                N.content.details = fmt::format("Job {} Completed: {} updated, {} failed to update{} , {} bad configurations. ",
                                                JobId_, Updated ,Failed, BadConfigs);

            } else {
                N.content.details = fmt::format("Venue {} no longer exists.",VenueUUID_);
                Logger().warning(N.content.details);
            }

            auto Sent = WebSocketClientServer()->SendUserNotification(UI_.email,N);
            Logger().information(fmt::format("Job {} Completed: {} updated, {} failed to update , {} bad configurations. Notification was send={}",
                                             JobId_, Updated ,Failed, BadConfigs, Sent));
            delete this;
        }
    };

}