//
// Created by stephane bourque on 2021-08-29.
//

#include "RESTAPI_configurations_list_handler.h"

#include "framework/Utils.h"
#include "RESTAPI_ProvObjects.h"
#include "StorageService.h"
#include "framework/RESTAPI_errors.h"

namespace OpenWifi{
    void RESTAPI_configurations_list_handler::DoGet() {
        if(!QB_.Select.empty()) {
            auto DevUUIDS = Utils::Split(QB_.Select);
            ProvObjects::DeviceConfigurationVec Configs;
            for(const auto &i:DevUUIDS) {
                ProvObjects::DeviceConfiguration E;
                if(Storage()->ConfigurationDB().GetRecord("id",i,E)) {
                    Configs.push_back(E);
                } else {
                    return BadRequest(RESTAPI::Errors::UnknownId + " (" + i + ")");
                }
            }
            return ReturnObject("configurations", Configs);
        } else if(QB_.CountOnly) {
            Poco::JSON::Object  Answer;
            auto C = Storage()->ConfigurationDB().Count();
            return ReturnCountOnly(C);
        } else {
            ProvObjects::DeviceConfigurationVec Configs;
            Storage()->ConfigurationDB().GetRecords(QB_.Offset,QB_.Limit,Configs);

            if(QB_.AdditionalInfo) {
                Poco::JSON::Array   ObjArray;
                for(const auto &i:Configs) {
                    Poco::JSON::Object  Obj;
                    i.to_json(Obj);
                    Poco::JSON::Object  EI;

                    if(!i.managementPolicy.empty()) {
                        Poco::JSON::Object  PolObj;
                        ProvObjects::ManagementPolicy Policy;
                        if(Storage()->PolicyDB().GetRecord("id",i.managementPolicy,Policy)) {
                            PolObj.set( "name", Policy.info.name);
                            PolObj.set( "description", Policy.info.description);
                        }
                        EI.set("managementPolicy",PolObj);
                    }
                    Obj.set("extendedInfo", EI);
                    ObjArray.add(Obj);
                }
                Poco::JSON::Object  Answer;
                Answer.set("locations",ObjArray);
                return ReturnObject(Answer);
            }
            return ReturnObject("configurations", Configs);
        }
    }
}