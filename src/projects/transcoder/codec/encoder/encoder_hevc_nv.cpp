//==============================================================================
//
//  Transcoder
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "encoder_hevc_nv.h"

#include <unistd.h>

#include "../../transcoder_gpu.h"
#include "../../transcoder_private.h"

bool EncoderHEVCxNV::SetCodecParams()
{
	_codec_context->framerate = ::av_d2q((GetRefTrack()->GetFrameRate() > 0) ? GetRefTrack()->GetFrameRate() : GetRefTrack()->GetEstimateFrameRate(), AV_TIME_BASE);
	_codec_context->bit_rate = GetRefTrack()->GetBitrate();
	_codec_context->rc_min_rate = _codec_context->rc_max_rate = _codec_context->bit_rate;
	_codec_context->rc_buffer_size = static_cast<int>(_codec_context->bit_rate / 2);
	_codec_context->sample_aspect_ratio = (AVRational){1, 1};
	_codec_context->ticks_per_frame = 2;
	_codec_context->time_base = ::av_inv_q(::av_mul_q(_codec_context->framerate, (AVRational){_codec_context->ticks_per_frame, 1}));
	_codec_context->max_b_frames = 0;
	_codec_context->pix_fmt = (AVPixelFormat)GetSupportedFormat();
	_codec_context->width = GetRefTrack()->GetWidth();
	_codec_context->height = GetRefTrack()->GetHeight();
	
	// KeyFrame Interval By Time
	if(GetRefTrack()->GetKeyFrameIntervalTypeByConfig() == cmn::KeyFrameIntervalType::TIME)
	{
		// When inserting a keyframe based on time, set the GOP value to 10 seconds.
		_codec_context->gop_size = (int32_t)(GetRefTrack()->GetFrameRate() * 10);
		::av_opt_set(_codec_context->priv_data, "forced-idr", "1", 0);

		_force_keyframe_timer.Start(GetRefTrack()->GetKeyFrameInterval());
	}
	// KeyFrame Interval By Frame
	if(GetRefTrack()->GetKeyFrameIntervalTypeByConfig() == cmn::KeyFrameIntervalType::FRAME)
	{
		_codec_context->gop_size = (GetRefTrack()->GetKeyFrameInterval() == 0) ? (_codec_context->framerate.num / _codec_context->framerate.den) : GetRefTrack()->GetKeyFrameInterval();
	}

	// Preset
	if (GetRefTrack()->GetPreset() == "slower")
	{
		::av_opt_set(_codec_context->priv_data, "preset", "p7", 0);
	}
	else if (GetRefTrack()->GetPreset() == "slow")
	{
		::av_opt_set(_codec_context->priv_data, "preset", "p6", 0);
	}
	else if (GetRefTrack()->GetPreset() == "medium")
	{
		::av_opt_set(_codec_context->priv_data, "preset", "p5", 0);
	}
	else if (GetRefTrack()->GetPreset() == "fast")
	{
		::av_opt_set(_codec_context->priv_data, "preset", "p4", 0);
	}
	else if (GetRefTrack()->GetPreset() == "faster")
	{
		::av_opt_set(_codec_context->priv_data, "preset", "p3", 0);
	}
	else
	{
		// Default
		::av_opt_set(_codec_context->priv_data, "preset", "p7", 0);
	}
	
	::av_opt_set(_codec_context->priv_data, "tune", "ull", 0);
	::av_opt_set(_codec_context->priv_data, "rc", "cbr", 0);

	_bitstream_format = cmn::BitstreamFormat::H265_ANNEXB;
	
	_packet_type = cmn::PacketType::NALU;

	return true;
}

// Notes.
// - B-frame must be disabled. because, WEBRTC does not support B-Frame.
bool EncoderHEVCxNV::Configure(std::shared_ptr<MediaTrack> context)
{
	if (TranscodeEncoder::Configure(context) == false)
	{
		return false;
	}

	auto codec_id = GetCodecID();

	const AVCodec *codec = ::avcodec_find_encoder_by_name("hevc_nvenc");
	if (codec == nullptr)
	{
		logte("Could not find encoder: %d (%s)", codec_id, ::avcodec_get_name(codec_id));
		return false;
	}

	_codec_context = ::avcodec_alloc_context3(codec);
	if (_codec_context == nullptr)
	{
		logte("Could not allocate codec context for %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	_codec_context->hw_device_ctx = ::av_buffer_ref(TranscodeGPU::GetInstance()->GetDeviceContext(cmn::MediaCodecModuleId::NVENC, context->GetCodecDeviceId()));
	if(_codec_context->hw_device_ctx == nullptr)
	{
		logte("Could not allocate hw device context for %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	if (SetCodecParams() == false)
	{
		logte("Could not set codec parameters for %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	auto hw_device_ctx = TranscodeGPU::GetInstance()->GetDeviceContext(cmn::MediaCodecModuleId::NVENC, GetRefTrack()->GetCodecDeviceId());
	if(hw_device_ctx == nullptr)
	{
		logte("Could not get hw device context for %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	// Assign HW device context to encoder
	if(ffmpeg::Conv::SetHwDeviceCtxOfAVCodecContext(_codec_context, hw_device_ctx) == false)
	{
		logte("Could not set hw device context for %s (%d)", ::avcodec_get_name(GetCodecID()), GetCodecID());
		return false;
	}


	// Assign HW frames context to encoder
	if(ffmpeg::Conv::SetHWFramesCtxOfAVCodecContext(_codec_context) == false)
	{
		logte("Could not set hw frames context for %s (%d)", ::avcodec_get_name(GetCodecID()), GetCodecID());
		return false;
	}

	if (::avcodec_open2(_codec_context, codec, nullptr) < 0)
	{
		logte("Could not open codec: %s (%d)", codec->name, codec->id);
		return false;
	}

	// Generates a thread that reads and encodes frames in the input_buffer queue and places them in the output queue.
	try
	{
		_kill_flag = false;

		_codec_thread = std::thread(&EncoderHEVCxNV::CodecThread, this);
		pthread_setname_np(_codec_thread.native_handle(), ov::String::FormatString("Enc%sNV", avcodec_get_name(GetCodecID())).CStr());
	}
	catch (const std::system_error &e)
	{
		logte("Failed to start encoder thread.");
		_kill_flag = true;

		return false;
	}

	return true;
}
