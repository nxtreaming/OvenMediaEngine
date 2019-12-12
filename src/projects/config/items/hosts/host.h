//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "applications/applications.h"
#include "domain/domain.h"
#include "listen/listen.h"
#include "origins/origins.h"

namespace cfg
{
	enum class HostType
	{
		Unknown,
		Origin,
		Edge
	};

	struct Host : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetName, _name)
		CFG_DECLARE_GETTER_OF(GetType, _type)
		CFG_DECLARE_REF_GETTER_OF(GetTypeName, _typeName)

		CFG_DECLARE_REF_GETTER_OF(GetIp, _ip)

		CFG_DECLARE_REF_GETTER_OF(GetDomain, _domain)
		CFG_DECLARE_REF_GETTER_OF(GetListen, _listen)
		CFG_DECLARE_REF_GETTER_OF(GetApplicationList, _applications.GetApplicationList())

	protected:
		void MakeParseList() override
		{
			RegisterValue("Name", &_name);
			RegisterValue("Type", &_typeName, nullptr, [this]() -> bool {
				_type = HostType::Unknown;

				if (_typeName == "origin")
				{
					_type = HostType::Origin;
				}
				else if (_typeName == "edge")
				{
					_type = HostType::Edge;
				}

				return _type != HostType::Unknown;
			});

			RegisterValue("IP", &_ip);

			RegisterValue<Optional>("Domain", &_domain);
			RegisterValue("Listen", &_listen);
			RegisterValue<CondOptional>("Origins", &_origins, [this]() -> bool {
				return (_type == HostType::Edge);
			});
			RegisterValue<CondOptional>("Applications", &_applications, [this]() -> bool {
				return (_type == HostType::Origin);
			});
		}

		ov::String _name;
		ov::String _typeName;
		HostType _type;

		ov::String _ip;

		Domain _domain;
		Listen _listen;
		Origins _origins;
		Applications _applications;
	};
}  // namespace cfg
