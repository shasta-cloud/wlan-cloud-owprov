//
// Created by stephane bourque on 2022-01-11.
//

#pragma once

#include "framework/RESTAPI_Handler.h"
#include "RESTObjects/RESTAPI_SecurityObjects.h"

namespace OpenWifi::SDK::Sec {

    namespace User {
        bool Exists(RESTAPIHandler *client, const Types::UUID_t & User);
        bool Get(RESTAPIHandler *client, const Types::UUID_t & User, SecurityObjects::UserInfo & UserInfo);
    }

    namespace Subscriber {
        bool Exists(RESTAPIHandler *client, const Types::UUID_t & User);
        bool Get(RESTAPIHandler *client, const Types::UUID_t & User, SecurityObjects::UserInfo & UserInfo);
        bool Delete(RESTAPIHandler *client, const Types::UUID_t & User);
        bool Search(RESTAPIHandler *client, const std::string &OperatorId, const std::string &Name,
                    const std::string &EMail, SecurityObjects::UserInfoList &Users);
    }

}

