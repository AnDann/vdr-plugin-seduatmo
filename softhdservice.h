///
///	@file softhddev_service.h @brief software HD device service header file.
///
///	Copyright (c) 2012 by durchflieger.  All Rights Reserved.
///
///	Contributor(s):
///
///	License: AGPLv3
///
///	This program is free software: you can redistribute it and/or modify
///	it under the terms of the GNU Affero General Public License as
///	published by the Free Software Foundation, either version 3 of the
///	License.
///
///	This program is distributed in the hope that it will be useful,
///	but WITHOUT ANY WARRANTY; without even the implied warranty of
///	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
///	GNU Affero General Public License for more details.
///
///	$Id: softhdservice.h,v 1.2 2012/11/13 08:58:11 wendel Exp $
//////////////////////////////////////////////////////////////////////////////

#pragma once

#define ATMO1_GRAB_SERVICE	"SoftHDDevice-AtmoGrabService-v1.1" 

struct SoftHDDevice_AtmoGrabService_v1_1_t
{
    // rewuest/reply data

    int width;
    int height;

    // reply data

    int size;

    void* img;
};
