//
//	License type: BSD 3-Clause License
//	License copy: https://github.com/Telecominfraproject/wlan-cloud-ucentralgw/blob/master/LICENSE
//
//	Created by Stephane Bourque on 2021-03-04.
//	Arilia Wireless Inc.
//

#pragma once

#include "StorageService.h"
#include "framework/MicroService.h"

namespace OpenWifi {

    class RESTAPI_inventory_list_handler : public RESTAPIHandler {
    public:
        RESTAPI_inventory_list_handler(const RESTAPIHandler::BindingMap &bindings, Poco::Logger &L, RESTAPI_GenericServer & Server, uint64_t TransactionId, bool Internal)
        : RESTAPIHandler(bindings, L,
                         std::vector<std::string>{
            Poco::Net::HTTPRequest::HTTP_GET,
            Poco::Net::HTTPRequest::HTTP_OPTIONS},
            Server,
            TransactionId,
            Internal){}
        static const std::list<const char *> PathName() { return std::list<const char *>{"/api/v1/inventory"}; };
    private:
        InventoryDB     &DB_=StorageService()->InventoryDB();
        void DoGet() final;
        void DoPost() final {};
        void DoPut() final {};
        void DoDelete() final {};

        void SendList(const ProvObjects::InventoryTagVec & Tags, bool SerialOnly);
    };
}