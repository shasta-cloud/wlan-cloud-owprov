//
//	License type: BSD 3-Clause License
//	License copy: https://github.com/Telecominfraproject/wlan-cloud-ucentralgw/blob/master/LICENSE
//
//	Created by Stephane Bourque on 2021-03-04.
//	Arilia Wireless Inc.
//


#include "RESTAPI_venue_handler.h"
#include "RESTAPI/RESTAPI_ProvObjects.h"
#include "StorageService.h"
#include "Poco/JSON/Parser.h"
#include "Daemon.h"
#include "framework/RESTAPI_errors.h"
#include "framework/CIDRUtils.h"
#include "RESTAPI/RESTAPI_db_helpers.h"

namespace OpenWifi{

    void RESTAPI_venue_handler::DoGet() {
        std::string UUID = GetBinding("uuid", "");
        ProvObjects::Venue Existing;
        if(UUID.empty() || !DB_.GetRecord("id",UUID,Existing)) {
            return NotFound();
        }

        Poco::JSON::Object Answer;
        if(QB_.AdditionalInfo)
            AddVenueExtendedInfo(Existing, Answer);

        Existing.to_json(Answer);
        ReturnObject(Answer);
    }

    void RESTAPI_venue_handler::DoDelete() {
        std::string UUID = GetBinding("uuid", "");
        ProvObjects::Venue Existing;
        if(UUID.empty() || !DB_.GetRecord("id",UUID,Existing)) {
            return NotFound();
        }

        if(!Existing.children.empty() || !Existing.devices.empty()) {
            return BadRequest(RESTAPI::Errors::StillInUse);
        }

        if(!Existing.contact.empty())
            Storage()->ContactDB().DeleteInUse("id",Existing.contact,Storage()->VenueDB().Prefix(),UUID);
        if(!Existing.location.empty())
            Storage()->LocationDB().DeleteInUse("id",Existing.location,Storage()->VenueDB().Prefix(),UUID);
        if(!Existing.managementPolicy.empty())
            Storage()->PolicyDB().DeleteInUse("id",Existing.managementPolicy,Storage()->VenueDB().Prefix(),UUID);
        if(!Existing.deviceConfiguration.empty())
            Storage()->ConfigurationDB().DeleteInUse("id",Existing.deviceConfiguration,Storage()->VenueDB().Prefix(),UUID);
        if(!Existing.parent.empty())
            Storage()->VenueDB().DeleteChild("id",Existing.parent,UUID);
        if(!Existing.entity.empty())
            Storage()->EntityDB().DeleteVenue("id",Existing.entity,UUID);
        DB_.DeleteRecord("id",UUID);
        return OK();
    }

    void RESTAPI_venue_handler::DoPost() {
        std::string UUID = GetBinding("uuid", "");
        if(UUID.empty()) {
            return BadRequest(RESTAPI::Errors::MissingUUID);
        }

        auto Obj = ParseStream();
        ProvObjects::Venue NewObject;
        if (!NewObject.from_json(Obj)) {
            return BadRequest(RESTAPI::Errors::InvalidJSONDocument);
        }

        if(NewObject.parent.empty() && NewObject.entity.empty()) {
            return BadRequest(RESTAPI::Errors::ParentOrEntityMustBeSet);
        }

        if(!NewObject.parent.empty() && !NewObject.entity.empty()) {
            return BadRequest(RESTAPI::Errors::NotBoth);
        }

        if(!NewObject.parent.empty() && !DB_.Exists("id",NewObject.parent)) {
            return BadRequest(RESTAPI::Errors::VenueMustExist);
        }

        if(NewObject.entity == EntityDB::RootUUID()) {
            return BadRequest(RESTAPI::Errors::ValidNonRootUUID);
        }

        if(!NewObject.entity.empty() && !Storage()->EntityDB().Exists("id",NewObject.entity)) {
            return BadRequest(RESTAPI::Errors::EntityMustExist);
        }

        if(!NewObject.contact.empty() && !Storage()->ContactDB().Exists("id",NewObject.contact)) {
            return BadRequest(RESTAPI::Errors::ContactMustExist);
        }

        if(!NewObject.location.empty() && !Storage()->LocationDB().Exists("id",NewObject.location)) {
            return BadRequest(RESTAPI::Errors::LocationMustExist);
        }

        if(!NewObject.managementPolicy.empty() && !Storage()->PolicyDB().Exists("id",NewObject.managementPolicy)) {
            return BadRequest(RESTAPI::Errors::UnknownManagementPolicyUUID);
        }

        if(!NewObject.deviceConfiguration.empty() && !Storage()->ConfigurationDB().Exists("id",NewObject.deviceConfiguration)) {
            return BadRequest(RESTAPI::Errors::UnknownManagementPolicyUUID);
        }

        if(!NewObject.sourceIP.empty() && CIDR::ValidateIpRanges(NewObject.sourceIP)) {
            return BadRequest(RESTAPI::Errors::InvalidIPRanges);
        }

        NewObject.children.clear();
        NewObject.info.modified = NewObject.info.created = std::time(nullptr);
        NewObject.info.id = Daemon()->CreateUUID() ;
        for(auto &i:NewObject.info.notes)
            i.createdBy = UserInfo_.userinfo.email;

        if(DB_.CreateShortCut(NewObject)) {
            ProvObjects::Venue  NewRecord;
            DB_.GetRecord("id",NewObject.info.id,NewRecord);
            Poco::JSON::Object  Answer;
            NewRecord.to_json(Answer);
            return ReturnObject(Answer);
        }
        InternalError(RESTAPI::Errors::RecordNotCreated);
    }

    void RESTAPI_venue_handler::DoPut() {
        std::string UUID = GetBinding("uuid", "");
        ProvObjects::Venue Existing;
        if(UUID.empty() || !DB_.GetRecord("id",UUID,Existing)) {
            return NotFound();
        }

        auto RawObject = ParseStream();
        ProvObjects::Venue NewObject;
        if (!NewObject.from_json(RawObject)) {
            return BadRequest(RESTAPI::Errors::InvalidJSONDocument);
        }

        for(auto &i:NewObject.info.notes) {
            i.createdBy = UserInfo_.userinfo.email;
            Existing.info.notes.insert(Existing.info.notes.begin(),i);
        }

        AssignIfPresent(RawObject, "name", Existing.info.name);
        AssignIfPresent(RawObject, "description", Existing.info.description);
        AssignIfPresent(RawObject, "rrm",Existing.rrm);

        if(RawObject->has("sourceIP")) {
            if(!NewObject.sourceIP.empty() && !CIDR::ValidateIpRanges(NewObject.sourceIP)) {
                return BadRequest(RESTAPI::Errors::InvalidIPRanges);
            }
            Existing.sourceIP = NewObject.sourceIP;
        }

        std::string MoveEntity;
        bool MovingEntity=false;
        if(AssignIfPresent(RawObject, "entity", MoveEntity)) {
            if(!MoveEntity.empty() && !Storage()->EntityDB().Exists("id",MoveEntity)) {
                return BadRequest(RESTAPI::Errors::EntityMustExist);
            }
            MovingEntity = MoveEntity != Existing.entity;
        }

        std::string MoveVenue;
        bool MovingVenue=false;
        if(AssignIfPresent(RawObject, "venue", MoveVenue)) {
            if(!MoveVenue.empty() && !Storage()->VenueDB().Exists("id",MoveVenue)) {
                return BadRequest(RESTAPI::Errors::VenueMustExist);
            }
            MovingVenue = MoveVenue != Existing.parent;
        }

        std::string MoveLocation;
        bool MovingLocation=false;
        if(AssignIfPresent(RawObject,"location",MoveLocation)) {
            if(!MoveLocation.empty() && !Storage()->LocationDB().Exists("id",MoveLocation)) {
                return BadRequest(RESTAPI::Errors::LocationMustExist);
            }
            MovingLocation = MoveLocation!=Existing.location;
        }

        std::string MoveContact;
        bool MovingContact=false;
        if(AssignIfPresent(RawObject,"contact",MoveContact)) {
            if(!MoveContact.empty() && !Storage()->ContactDB().Exists("id",MoveContact)) {
                return BadRequest(RESTAPI::Errors::ContactMustExist);
            }
            MovingContact = MoveContact!=Existing.contact;
        }

        std::string MovePolicy;
        bool MovingPolicy=false;
        if(AssignIfPresent(RawObject,"managementPolicy",MovePolicy)) {
            if(!MovePolicy.empty() && !Storage()->PolicyDB().Exists("id",MovePolicy)) {
                return BadRequest(RESTAPI::Errors::UnknownManagementPolicyUUID);
            }
            MovingPolicy = MovePolicy != Existing.managementPolicy;
        }

        std::string MoveConfiguration;
        bool MovingConfiguration=false;
         if(AssignIfPresent(RawObject,"deviceConfiguration",MoveConfiguration)) {
             if(!MoveConfiguration.empty() && !Storage()->ConfigurationDB().Exists("id",MoveConfiguration)) {
                return BadRequest(RESTAPI::Errors::ConfigurationMustExist);
            }
            MovingConfiguration = MoveConfiguration != Existing.deviceConfiguration;
        }

        Existing.info.modified = std::time(nullptr);
        if(Storage()->VenueDB().UpdateRecord("id", UUID, Existing)) {
            if(MovingContact) {
                if(!Existing.contact.empty())
                    Storage()->ContactDB().DeleteInUse("id",Existing.contact,DB_.Prefix(),Existing.info.id);
                if(!MoveContact.empty())
                    Storage()->ContactDB().AddInUse("id", MoveContact, DB_.Prefix(), Existing.info.id);
                Existing.contact = MoveContact;
            }
            if(MovingEntity) {
                if(!Existing.entity.empty())
                    Storage()->EntityDB().DeleteVenue("id", Existing.entity, Existing.info.id);
                if(!MoveEntity.empty())
                    Storage()->EntityDB().AddVenue("id",MoveEntity,Existing.info.id);
                Existing.entity = MoveEntity;
            }
            if(MovingVenue) {
               if(!Existing.parent.empty())
                   DB_.DeleteChild("id",Existing.parent,Existing.info.id);
               if(!MoveVenue.empty())
                   DB_.AddChild("id", MoveVenue, Existing.info.id);
               Existing.parent = MoveVenue;
            }
            if(MovingLocation) {
                if(!Existing.location.empty())
                    Storage()->LocationDB().DeleteInUse("id", Existing.location, DB_.Prefix(), Existing.info.id);
                if(!MoveLocation.empty())
                    Storage()->LocationDB().AddInUse("id", MoveLocation, DB_.Prefix(), Existing.info.id);
                Existing.location = MoveLocation;
            }
            if(MovingPolicy) {
                if(!Existing.managementPolicy.empty())
                    Storage()->PolicyDB().DeleteInUse("id", Existing.managementPolicy, DB_.Prefix(), Existing.info.id);
                if(!MovePolicy.empty())
                    Storage()->PolicyDB().AddInUse("id", MovePolicy, DB_.Prefix(), Existing.info.id);
                Existing.managementPolicy = MovePolicy;
            }
            if(MovingConfiguration) {
                if(!Existing.deviceConfiguration.empty())
                    Storage()->ConfigurationDB().DeleteInUse("id", Existing.deviceConfiguration, DB_.Prefix(), Existing.info.id);
                if(!MoveConfiguration.empty())
                    Storage()->ConfigurationDB().AddInUse("id", MoveConfiguration, DB_.Prefix(), Existing.info.id);
                Existing.deviceConfiguration = MoveConfiguration;
            }

            DB_.UpdateRecord("id",Existing.info.id, Existing);
            ProvObjects::Venue AddedRecord;
            DB_.GetRecord("id",UUID,AddedRecord);
            Poco::JSON::Object  Answer;
            AddedRecord.to_json(Answer);
            return ReturnObject(Answer);
        }
        InternalError(RESTAPI::Errors::RecordNotUpdated);
    }
}