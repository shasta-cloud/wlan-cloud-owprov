//
// Created by stephane bourque on 2022-04-06.
//

#include "RESTAPI_sub_devices_handler.h"
#include "RESTAPI/RESTAPI_db_helpers.h"
#include "sdks/SDK_sec.h"

namespace OpenWifi {

    void RESTAPI_sub_devices_handler::DoGet() {
        auto uuid = GetBinding("uuid");
        if(uuid.empty()) {
            return BadRequest(RESTAPI::Errors::MissingUUID);
        }

        ProvObjects::SubscriberDevice   SD;
        if(!DB_.GetRecord("id",uuid,SD)) {
            return NotFound();
        }

        Poco::JSON::Object  Answer;
        SD.to_json(Answer);
        return ReturnObject(Answer);
    }

    void RESTAPI_sub_devices_handler::DoDelete() {
        auto uuid = GetBinding("uuid");
        if(uuid.empty()) {
            return BadRequest(RESTAPI::Errors::MissingUUID);
        }

        if(!DB_.Exists("id",uuid)) {
            return NotFound();
        }

        DB_.DeleteRecord("id",uuid);
        return OK();
    }

    void RESTAPI_sub_devices_handler::DoPost() {

        auto RawObject = ParseStream();
        SubscriberDeviceDB::RecordName NewObject;
        if(!NewObject.from_json(RawObject)) {
            return BadRequest(RESTAPI::Errors::InvalidJSONDocument);
        }

        if( !ValidDbId(NewObject.managementPolicy, StorageService()->PolicyDB(), true , RESTAPI::Errors::UnknownManagementPolicyUUID, *this) ||
            !ValidDbId(NewObject.contact, StorageService()->OpContactDB(), true, RESTAPI::Errors::InvalidContactId, *this)                   ||
            !ValidDbId(NewObject.location, StorageService()->OpLocationDB(), true, RESTAPI::Errors::InvalidLocationId, *this)                ||
            !ValidDbId(NewObject.operatorId, StorageService()->OperatorDB(), true, RESTAPI::Errors::InvalidOperatorId, *this)                ||
            !ValidDbId(NewObject.serviceClass, StorageService()->ServiceClassDB(), true, RESTAPI::Errors::InvalidServiceClassId, *this)      ||
            !ValidSubscriberId(NewObject.subscriberId, true, *this) ||
            !ValidRRM(NewObject.rrm,*this) ||
            !ValidSerialNumber(NewObject.serialNumber,false,*this)
                ) {
            return;
        }

        ProvObjects::CreateObjectInfo(RawObject,UserInfo_.userinfo,NewObject.info);
        return ReturnCreatedObject(DB_,NewObject,*this);
    }

    void RESTAPI_sub_devices_handler::DoPut() {
        auto uuid = GetBinding("uuid");

        auto RawObject = ParseStream();
        SubscriberDeviceDB::RecordName UpdateObj;
        if(!UpdateObj.from_json(RawObject)) {
            return BadRequest(RESTAPI::Errors::InvalidJSONDocument);
        }

        SubscriberDeviceDB::RecordName  Existing;
        if(!DB_.GetRecord("id",uuid,Existing)) {
            return NotFound();
        }

        if( !ValidDbId(UpdateObj.managementPolicy, StorageService()->PolicyDB(), true , RESTAPI::Errors::UnknownManagementPolicyUUID, *this) ||
            !ValidDbId(UpdateObj.contact, StorageService()->OpContactDB(), true, RESTAPI::Errors::InvalidContactId, *this)                   ||
            !ValidDbId(UpdateObj.location, StorageService()->OpLocationDB(), true, RESTAPI::Errors::InvalidLocationId, *this)                ||
            !ValidDbId(UpdateObj.operatorId, StorageService()->OperatorDB(), true, RESTAPI::Errors::InvalidOperatorId, *this)                ||
            !ValidDbId(UpdateObj.serviceClass, StorageService()->ServiceClassDB(), true, RESTAPI::Errors::InvalidServiceClassId, *this)      ||
            !ValidSubscriberId(UpdateObj.subscriberId, true, *this) ||
            !ValidRRM(UpdateObj.rrm,*this) ||
            !ValidSerialNumber(UpdateObj.serialNumber,false,*this)
                ) {
            return;
        }

        ProvObjects::UpdateObjectInfo(RawObject,UserInfo_.userinfo,Existing.info);
        AssignIfPresent(RawObject, "deviceType", Existing.deviceType);
        AssignIfPresent(RawObject, "subscriberId", Existing.subscriberId);
        AssignIfPresent(RawObject, "location", Existing.location);
        AssignIfPresent(RawObject, "contact", Existing.contact);
        AssignIfPresent(RawObject, "managementPolicy", Existing.managementPolicy);
        AssignIfPresent(RawObject, "serviceClass", Existing.serviceClass);
        AssignIfPresent(RawObject, "qrCode", Existing.qrCode);
        AssignIfPresent(RawObject, "geoCode", Existing.geoCode);
        AssignIfPresent(RawObject, "rrm", Existing.rrm);
        AssignIfPresent(RawObject, "state", Existing.state);
        AssignIfPresent(RawObject, "locale", Existing.locale);
        AssignIfPresent(RawObject, "billingCode", Existing.billingCode);
        AssignIfPresent(RawObject, "realMacAddress", Existing.realMacAddress);

        if(RawObject->has("configuration")) {
            Existing.configuration = UpdateObj.configuration;
        }
        return ReturnUpdatedObject(DB_,Existing,*this);
    }

}