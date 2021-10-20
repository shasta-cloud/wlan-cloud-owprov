//
// Created by stephane bourque on 2021-08-26.
//

#include "RESTAPI_managementRole_handler.h"

#include "framework/RESTAPI_protocol.h"
#include "RESTAPI_ProvObjects.h"
#include "StorageService.h"
#include "Poco/JSON/Parser.h"
#include "Daemon.h"
#include "Poco/StringTokenizer.h"
#include "framework/RESTAPI_errors.h"
#include "RESTAPI/RESTAPI_db_helpers.h"

namespace OpenWifi{

    void RESTAPI_managementRole_handler::DoGet() {
        ProvObjects::ManagementRole   Existing;
        std::string UUID = GetBinding(RESTAPI::Protocol::ID,"");
        if(UUID.empty() || !DB_.GetRecord(RESTAPI::Protocol::ID,UUID,Existing)) {
            return NotFound();
        }

        Poco::JSON::Object  Answer;
        std::string Arg;
        if(HasParameter("expandInUse",Arg) && Arg=="true") {
            Storage::ExpandedListMap    M;
            std::vector<std::string>    Errors;
            Poco::JSON::Object  Inner;
            if(Storage()->ExpandInUse(Existing.inUse,M,Errors)) {
                for(const auto &[type,list]:M) {
                    Poco::JSON::Array   ObjList;
                    for(const auto &i:list.entries) {
                        Poco::JSON::Object  O;
                        i.to_json(O);
                        ObjList.add(O);
                    }
                    Inner.set(type,ObjList);
                }
            }
            Answer.set("entries", Inner);
            return ReturnObject(Answer);
        }

        if(QB_.AdditionalInfo)
            AddManagementRoleExtendedInfo(Existing,Answer);
        Existing.to_json(Answer);
        ReturnObject(Answer);
    }

    void RESTAPI_managementRole_handler::DoDelete() {
        ProvObjects::ManagementRole   Existing;
        std::string UUID = GetBinding(RESTAPI::Protocol::ID,"");
        if(UUID.empty() || !DB_.GetRecord(RESTAPI::Protocol::ID,UUID,Existing)) {
            return NotFound();
        }

        bool Force=false;
        std::string Arg;
        if(HasParameter("force",Arg) && Arg=="true")
            Force=true;

        if(!Force && !Existing.inUse.empty()) {
            return BadRequest(RESTAPI::Errors::StillInUse);
        }

        if(!Existing.managementPolicy.empty())
            Storage()->PolicyDB().DeleteInUse("id",Existing.managementPolicy,DB_.Prefix(),Existing.info.id);

        if(Storage()->RolesDB().DeleteRecord("id", Existing.info.id)) {
            return OK();
        }
        InternalError(RESTAPI::Errors::CouldNotBeDeleted);
    }

    void RESTAPI_managementRole_handler::DoPost() {
        std::string UUID = GetBinding(RESTAPI::Protocol::ID,"");
        if(UUID.empty()) {
            return BadRequest(RESTAPI::Errors::MissingUUID);
        }

        auto Obj = ParseStream();
        ProvObjects::ManagementRole NewObject;
        if (!NewObject.from_json(Obj)) {
            return BadRequest(RESTAPI::Errors::InvalidJSONDocument);
        }

        if(!NewObject.managementPolicy.empty() && !Storage()->PolicyDB().Exists("id",NewObject.managementPolicy)) {
            return BadRequest(RESTAPI::Errors::UnknownManagementPolicyUUID);
        }
        NewObject.info.id = Daemon()->CreateUUID();
        NewObject.info.created = NewObject.info.modified = std::time(nullptr);

        std::string f{RESTAPI::Protocol::ID};

        if(DB_.CreateRecord(NewObject)) {
            if(!NewObject.managementPolicy.empty())
                Storage()->PolicyDB().AddInUse("id", NewObject.managementPolicy, DB_.Prefix(), NewObject.info.id);
            Poco::JSON::Object Answer;
            ProvObjects::ManagementRole Role;
            DB_.GetRecord("id", NewObject.info.id,Role);
            Role.to_json(Answer);
            return ReturnObject(Answer);
        }
        InternalError(RESTAPI::Errors::RecordNotCreated);
    }

    void RESTAPI_managementRole_handler::DoPut() {
        ProvObjects::ManagementRole   Existing;
        std::string UUID = GetBinding(RESTAPI::Protocol::ID,"");
        if(UUID.empty() || !DB_.GetRecord(RESTAPI::Protocol::ID,UUID,Existing)) {
            return NotFound();
        }

        auto RawObject = ParseStream();
        ProvObjects::ManagementRole NewObject;
        if(!NewObject.from_json(RawObject)) {
            return BadRequest(RESTAPI::Errors::InvalidJSONDocument);
        }

        for(auto &i:NewObject.info.notes) {
            i.createdBy = UserInfo_.userinfo.email;
            Existing.info.notes.insert(Existing.info.notes.begin(),i);
        }

        AssignIfPresent(RawObject, "name", Existing.info.name);
        AssignIfPresent(RawObject, "description", Existing.info.description);

        std::string NewPolicy,OldPolicy = Existing.managementPolicy;
        AssignIfPresent(RawObject, "managementPolicy", NewPolicy);
        if(!NewPolicy.empty() && !Storage()->PolicyDB().Exists("id",NewPolicy)) {
            return BadRequest(RESTAPI::Errors::UnknownManagementPolicyUUID);
        }

        if(!NewPolicy.empty())
            Existing.managementPolicy = NewPolicy;

        if(DB_.UpdateRecord("id",UUID,Existing)) {
            if(!OldPolicy.empty())
                Storage()->PolicyDB().DeleteInUse("id",OldPolicy,DB_.Prefix(),UUID);
            if(!NewPolicy.empty())
                Storage()->PolicyDB().AddInUse("id",NewPolicy,DB_.Prefix(),UUID);

            ProvObjects::ManagementRole NewRecord;

            DB_.GetRecord("id", UUID, NewRecord);
            Poco::JSON::Object  Answer;
            NewRecord.to_json(Answer);
            return ReturnObject(Answer);
        }
        InternalError(RESTAPI::Errors::RecordNotUpdated);
    }
}