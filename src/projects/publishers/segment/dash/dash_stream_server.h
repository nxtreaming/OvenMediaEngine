//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "dash_interceptor.h"
#include "dash_packetizer.h"

#include <segment_stream/segment_stream_server.h>

class DashStreamServer : public SegmentStreamServer
{
public:
	PublisherType GetPublisherType() const noexcept override
	{
		return PublisherType::Dash;
	}

	const char *GetPublisherName() const noexcept override
	{
		return "DASH";
	}

	std::shared_ptr<SegmentStreamInterceptor> CreateInterceptor() override
	{
		return std::make_shared<DashInterceptor>();
	}

protected:
	HttpConnection ProcessRequestStream(const std::shared_ptr<HttpClient> &client,
										const ov::String &app_name, const ov::String &stream_name,
										const ov::String &file_name, const ov::String &file_ext) override;

	HttpConnection OnPlayListRequest(const std::shared_ptr<HttpClient> &client, const ov::String &app_name, const ov::String &stream_name,
									 const ov::String &file_name,
									 PlayListType play_list_type) override;

	HttpConnection OnSegmentRequest(const std::shared_ptr<HttpClient> &client, const ov::String &app_name, const ov::String &stream_name,
									const ov::String &file_name,
									SegmentType segment_type) override;
};
