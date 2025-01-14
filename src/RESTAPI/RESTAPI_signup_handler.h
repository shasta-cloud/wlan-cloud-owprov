//
// Created by stephane bourque on 2022-02-20.
//

#pragma once
#include "framework/RESTAPI_Handler.h"
#include "StorageService.h"

namespace OpenWifi {
    class RESTAPI_signup_handler : public RESTAPIHandler {
    public:
        RESTAPI_signup_handler(const RESTAPIHandler::BindingMap &bindings, Poco::Logger &L, RESTAPI_GenericServerAccounting & Server, uint64_t TransactionId, bool Internal)
                : RESTAPIHandler(bindings, L,
                                 std::vector<std::string>{
                                         Poco::Net::HTTPRequest::HTTP_POST,
                                         Poco::Net::HTTPRequest::HTTP_OPTIONS,
                                         Poco::Net::HTTPRequest::HTTP_PUT,
                                         Poco::Net::HTTPRequest::HTTP_GET,
                                         Poco::Net::HTTPRequest::HTTP_DELETE},
                                 Server,
                                 TransactionId,
                                 Internal, false, true ){}

        static auto PathName() { return std::list<std::string>{"/api/v1/signup"}; };

/*        inline bool RoleIsAuthorized(std::string & Reason) {
            if(UserInfo_.userinfo.userRole != SecurityObjects::USER_ROLE::SUBSCRIBER) {
                Reason = "User must be a subscriber";
                return false;
            }
            return true;
        }
*/
        void DoGet() final;
        void DoPost() final;
        void DoPut() final ;
        void DoDelete() final;
    private:

    };
}
