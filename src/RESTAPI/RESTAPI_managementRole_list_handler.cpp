//
// Created by stephane bourque on 2021-08-26.
//

#include "RESTAPI_managementRole_list_handler.h"
#include "StorageService.h"
#include "RESTAPI/RESTAPI_db_helpers.h"

namespace OpenWifi{
    void RESTAPI_managementRole_list_handler::DoGet() {
        return ListHandler<ManagementRoleDB>("roles", DB_, *this);
    }
}