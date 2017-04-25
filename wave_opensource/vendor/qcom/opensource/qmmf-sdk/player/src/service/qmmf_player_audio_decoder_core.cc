/*
* Copyright (c) 2016, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
*       copyright notice, this list of conditions and the following
*       disclaimer in the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of The Linux Foundation nor the names of its
*       contributors may be used to endorse or promote products derived
*       from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
* ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define TAG "AudioDecoderCore"

#include <memory>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/msm_ion.h>

#include "player/src/service/qmmf_player_audio_decoder_core.h"


namespace qmmf {
namespace player {

using ::qmmf::avcodec::AVCodec;
using ::qmmf::avcodec::CodecBuffer;
using ::qmmf::avcodec::CodecParam;
using ::qmmf::avcodec::CodecPortStatus;
using ::qmmf::avcodec::PortEventType;
using ::qmmf::avcodec::PortreconfigData;
using ::qmmf::avcodec::kPortIndexInput;
using ::qmmf::avcodec::kPortIndexOutput;
using ::qmmf::avcodec::kPortALL;
using ::std::make_shared;
using ::std::shared_ptr;
using ::std::vector;


AudioDecoderCore* AudioDecoderCore::instance_ = nullptr;

AudioDecoderCore* AudioDecoderCore::CreateAudioDecoderCore() {
  if(!instance_) {
     instance_ = new AudioDecoderCore();
  if(!instance_) {
    QMMF_ERROR("%s:%s: Can't Create AudioDecoderCore Instance", TAG, __func__);
    return nullptr;
  }
  }
  QMMF_INFO("%s:%s: AudioDecoderCore Instance Created Successfully(0x%p)", TAG,
       __func__, instance_);

  return instance_;
}

AudioDecoderCore::AudioDecoderCore() : ion_device_(-1) {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  QMMF_INFO("%s:%s: Exit", TAG, __func__);
}

AudioDecoderCore::~AudioDecoderCore() {
  QMMF_INFO("%s:%s: Enter", TAG, __func__);
  if (!audio_track_decoders_.isEmpty()) {
    audio_track_decoders_.clear();
  }
  instance_ = NULL;

  if (ion_device_ > 0) {
    close(ion_device_);
    ion_device_ = -1;
  }
  QMMF_INFO("%s:%s: Exit", TAG, __func__);
}

status_t AudioDecoderCore::CreateAudioTrack(AudioTrackParams& params) {
  QMMF_DEBUG("%s:%s: Enter", TAG, __func__);

  if (ion_device_ < 0) {
    ion_device_ = open("/dev/ion", O_RDONLY);
    assert(ion_device_ >=0 );
  }
  status_t ret = NO_ERROR;

  shared_ptr<AudioTrackDecoder> audio_track_decoder =
      make_shared<AudioTrackDecoder>(ion_device_);

  if (!audio_track_decoder.get()) {
    QMMF_ERROR("%s:%s: track_id(%d) Can't instantiate AudioTrackEncoder", TAG,
        __func__, params.track_id);
    return NO_MEMORY;
  }

  ret = audio_track_decoder->ConfigureTrackDecoder(params);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: track_id(%d) AudioTrackEncoder Init failed!", TAG, __func__,
        params.track_id);
    return BAD_VALUE;
  }

  audio_track_decoders_.add(params.track_id, audio_track_decoder);
  QMMF_INFO("%s:%s: AudioTrackEncoder(0x%p) for track_id(%d) Instantiated!", TAG,
      __func__, audio_track_decoder.get(), params.track_id);

  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t AudioDecoderCore::PrepareTrackPipeline(
    uint32_t track_id,
    const shared_ptr<AudioTrackSink>& audio_track_sink) {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
    return BAD_VALUE;
  }

  shared_ptr<AudioTrackDecoder> track_decoder =
      audio_track_decoders_.valueFor(track_id);
  assert(track_decoder.get() != NULL);

  auto ret = track_decoder->PreparePipeline(audio_track_sink, track_decoder);
  if (ret != NO_ERROR) {
    QMMF_INFO("%s:%s: track_id(%d) PreparePipeline failed!", TAG, __func__,
        track_id);
    return ret;
  }

  QMMF_INFO("%s:%s: track_id(%d) PreparePipeline Successful!", TAG,
      __func__, track_id);
  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t AudioDecoderCore::DequeueTrackInputBuffer(
    uint32_t track_id,
    std::vector<AVCodecBuffer>& buffers) {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
    return BAD_VALUE;
  }

  shared_ptr<AudioTrackDecoder> track_decoder =
      audio_track_decoders_.valueFor(track_id);
  assert(track_decoder.get() != NULL);

  auto ret = track_decoder->DequeueInputBuffer(buffers);
  if (ret != NO_ERROR) {
    QMMF_INFO("%s:%s: track_id(%d) DequeueInputBuffer failed!", TAG, __func__,
        track_id);
    return ret;
  }

  QMMF_INFO("%s:%s: track_id(%d) DequeueInputBuffer Successful!", TAG,
      __func__, track_id);
  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t AudioDecoderCore::QueueTrackInputBuffer(
    uint32_t track_id,
    std::vector<AVCodecBuffer>& buffers) {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
    return BAD_VALUE;
  }

  shared_ptr<AudioTrackDecoder> track_decoder =
      audio_track_decoders_.valueFor(track_id);
  assert(track_decoder.get() != NULL);

  auto ret = track_decoder->QueueInputBuffer(buffers);
  if (ret != NO_ERROR) {
    QMMF_INFO("%s:%s: track_id(%d) QueueInputBuffer failed!", TAG, __func__,
    track_id);
  return ret;
  }

  QMMF_INFO("%s:%s: track_id(%d) QueueInputBuffer Successful!", TAG,
    __func__, track_id);
  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t AudioDecoderCore::StartTrackDecoder(uint32_t track_id) {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
    return BAD_VALUE;
  }

  shared_ptr<AudioTrackDecoder> track_decoder =
      audio_track_decoders_.valueFor(track_id);
  assert(track_decoder.get() != NULL);

  auto ret = track_decoder->StartDecoder();
  if (ret != NO_ERROR) {
    QMMF_INFO("%s:%s: track_id(%d) StartDecoder failed!", TAG, __func__,
    track_id);
  return ret;
  }

  QMMF_INFO("%s:%s: track_id(%d) StartDecoder Successful!", TAG,
    __func__, track_id);
  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t AudioDecoderCore::StopTrackDecoder(uint32_t track_id,
                                            bool do_flush) {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
    return BAD_VALUE;
  }

  shared_ptr<AudioTrackDecoder> track_decoder =
      audio_track_decoders_.valueFor(track_id);
  assert(track_decoder.get() != NULL);

  auto ret = track_decoder->StopDecoder(do_flush);
  if (ret != NO_ERROR) {
    QMMF_INFO("%s:%s: track_id(%d) StopDecoder failed!", TAG, __func__,
     track_id);
   return ret;
  }

  QMMF_INFO("%s:%s: track_id(%d) StopDecoder Successful!", TAG,
     __func__, track_id);
  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t AudioDecoderCore::PauseTrackDecoder(uint32_t track_id) {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
    return BAD_VALUE;
  }

  shared_ptr<AudioTrackDecoder> track_decoder =
      audio_track_decoders_.valueFor(track_id);
  assert(track_decoder.get() != NULL);

  auto ret = track_decoder->PauseDecoder();
  if (ret != NO_ERROR) {
    QMMF_INFO("%s:%s: track_id(%d) PauseDecoder failed!", TAG, __func__,
     track_id);
   return ret;
  }

  QMMF_INFO("%s:%s: track_id(%d) PauseDecoder Successful!", TAG,
     __func__, track_id);
  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t AudioDecoderCore::ResumeTrackDecoder(uint32_t track_id) {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
    return BAD_VALUE;
  }

  shared_ptr<AudioTrackDecoder> track_decoder =
      audio_track_decoders_.valueFor(track_id);
  assert(track_decoder.get() != NULL);

  auto ret = track_decoder->ResumeDecoder();
  if (ret != NO_ERROR) {
    QMMF_INFO("%s:%s: track_id(%d) ResumeDecoder failed!", TAG, __func__,
     track_id);
   return ret;
  }

  QMMF_INFO("%s:%s: track_id(%d) ResumeDecoder Successful!", TAG,
     __func__, track_id);
  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t AudioDecoderCore::SetAudioTrackDecoderParams(
    uint32_t track_id,
    CodecParamType param_type,
    void* param,
    uint32_t param_size) {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
    return BAD_VALUE;
  }

  shared_ptr<AudioTrackDecoder> track_decoder =
      audio_track_decoders_.valueFor(track_id);
  assert(track_decoder.get() != NULL);

  auto ret =  track_decoder->SetAudioDecoderParams(param_type, param, param_size);
  if (ret != NO_ERROR) {
    QMMF_INFO("%s:%s: track_id(%d) SetAudioDecoderParams failed!", TAG, __func__,
     track_id);
   return ret;
  }

  QMMF_INFO("%s:%s: track_id(%d) SetAudioDecoderParams Successful!", TAG,
     __func__, track_id);
  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t AudioDecoderCore::DeleteTrackDecoder(uint32_t track_id) {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_id);

  if (!isTrackValid(track_id)) {
    QMMF_ERROR("%s:%s: Invalid track_id(%d)", TAG, __func__, track_id);
    return BAD_VALUE;
  }

  shared_ptr<AudioTrackDecoder> track_decoder =
      audio_track_decoders_.valueFor(track_id);
  assert(track_decoder.get() != NULL);

  auto ret = track_decoder->DeleteDecoder();
  if (ret != NO_ERROR) {
    QMMF_INFO("%s:%s: track_id(%d) DeleteDecoder failed!", TAG, __func__,
     track_id);
   return ret;
  }

  audio_track_decoders_.removeItem(track_id);

  QMMF_INFO("%s:%s: track_id(%d) DeleteDecoder Successful!", TAG,
     __func__, track_id);
  QMMF_DEBUG("%s:%s: Exit", TAG, __func__);
  return ret;
}

bool AudioDecoderCore::isTrackValid(uint32_t track_id) {
  QMMF_INFO("%s: Number of Tracks exist = %d",__func__,
      audio_track_decoders_.size());
  assert(audio_track_decoders_.size() > 0);
  return audio_track_decoders_.indexOfKey(track_id) >= 0 ? true : false;
}

/************************* Audio Decoding ********************************/

AudioTrackDecoder::AudioTrackDecoder(int32_t ion_device)
    : ion_device_(ion_device) {
  QMMF_DEBUG("%s:%s: Enter", TAG, __func__);

  memset(&audio_track_params_, 0x0, sizeof audio_track_params_);

#ifdef DUMP_AUDIO_BITSTREAM
  file_fd_audio_ = open("/data/audio_track.bitstream",
      O_CREAT | O_WRONLY | O_TRUNC, 0655);
#endif

  QMMF_INFO("%s:%s: Exit (0x%p)", TAG, __func__, this);
}

AudioTrackDecoder::~AudioTrackDecoder() {
  QMMF_INFO("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());

   uint32_t i = 0;
   for(auto& iter : input_buffer_list_) {
      if((iter).data) {
        munmap((iter).data, (iter).frame_length);
        (iter).data = nullptr;
      }
      if((iter).fd) {
        QMMF_INFO("%s:%s track_id(%d) (iter).fd =%d Free", TAG, __func__,
                                   TrackId(), (iter).fd);
        ioctl(ion_device_, ION_IOC_FREE, &(ion_handle_data[i]));
        close((iter).fd);
        (iter).fd = -1;
      }
      i++;
  }

  input_buffer_list_.clear();
  ion_handle_data.clear();

  for(auto& iter : output_buffer_list_) {

    if((iter).pointer) {
        munmap((iter).pointer, (iter).frame_length);
        (iter).pointer = NULL;
    }
    if((iter).fd) {
        QMMF_INFO("%s:%s track_id(%d) (iter).fd =%d Free", TAG, __func__,
                                   TrackId(), (iter).fd);
        ioctl(ion_device_, ION_IOC_FREE, &((iter).handle_data.handle));
        close((iter).fd);
        (iter).fd = 0;
    }
  }

  output_buffer_list_.clear();

#ifdef DUMP_AUDIO_BITSTREAM
  close(file_fd_audio_);
#endif

  delete avcodec_;

  QMMF_DEBUG("%s:%s: Exit (0x%p)", TAG, __func__, this);
}

status_t AudioTrackDecoder::ConfigureTrackDecoder(
    AudioTrackParams& track_params) {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, track_params.track_id);
  audio_track_params_ = track_params;

  status_t ret = NO_ERROR;

  avcodec_ = new AVCodec();

  CodecParam codec_param;
  memset(&codec_param, 0x0, sizeof(codec_param));
  codec_param.audio_dec_param       = track_params.params;

  ret = avcodec_->ConfigureCodec(CodecMimeType::kMimeTypeAudioDecAAC,
                                 codec_param);
  assert(ret == NO_ERROR);
  if(ret != NO_ERROR) {
   QMMF_ERROR("%s:%s track_id(%d) Failed to configure AVCodec!", TAG, __func__,
       track_params.track_id);
   return ret;
  }

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t AudioTrackDecoder::PreparePipeline(
    const shared_ptr<AudioTrackSink>& audio_track_sink,
    const shared_ptr<AudioTrackDecoder>& audio_track_decoder) {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  assert(avcodec_ != nullptr);

  status_t ret = NO_ERROR;

  audio_track_sink_ = audio_track_sink;

  // This function will get the port buffer requirment and will allocate buffer
  AllocInputPortBufs();

  vector<BufferDescriptor> dummy_list;
  ret = avcodec_->AllocateBuffer(kPortIndexInput, 0, 0,
                                 shared_ptr<ICodecSource>(audio_track_decoder),
                                 dummy_list);
  assert(ret == NO_ERROR);
  if(ret != NO_ERROR) {
    QMMF_ERROR("%s:%s track_id(%d) AllocateBuffer Failed at input port!",
        TAG, __func__, TrackId());
  }

  // This function will get the port buffer requirment and will allocate buffer
  AllocOutputPortBufs();

  ret = avcodec_->AllocateBuffer(kPortIndexOutput, 0, 0,
                                 shared_ptr<ICodecSource>(audio_track_sink),
                                 dummy_list);
  assert(ret == NO_ERROR);
  if(ret != NO_ERROR) {
    QMMF_ERROR("%s:%s track_id(%d) AllocateBuffer Failed at Output port!",
        TAG, __func__, TrackId());
  }

  // Bitstream buffer queue
  for(auto& iter : input_buffer_list_) {
     QMMF_INFO("%s:%s: track_id(%d) Adding buffer fd(%d) to "
         "unfilled_frame_queue_", TAG, __func__,TrackId() ,
         iter.fd);
     unfilled_frame_queue_.PushBack(iter);
  }

  audio_track_sink->AddBufferList(output_buffer_list_);

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

//service will send unfilled buffer fd/pointer to the application to fill it
status_t AudioTrackDecoder::DequeueInputBuffer(
    std::vector<AVCodecBuffer>& buffers) {
  QMMF_DEBUG("%s:%s: Enter", TAG, __func__);

  int32_t size = buffers.size();

  for (int32_t i = 0; i < size; i++) {

  if(unfilled_frame_queue_.Size() <= 0) {
    QMMF_DEBUG("%s:%s track_id(%d) No Empty buffer available", TAG, __func__, TrackId());
    Mutex::Autolock autoLock(wait_for_empty_frame_lock_);
    wait_for_empty_frame_.wait(wait_for_empty_frame_lock_);
  }

  StreamBuffer iter = *unfilled_frame_queue_.Begin();
  buffers[i].fd = (iter).fd;
  buffers[i].data = (iter).data;
  buffers[i].frame_length = (iter).frame_length;
  buffers[i].buf_id = (iter).fd;

  {
    Mutex::Autolock lock(queue_lock_);
    filled_frame_queue_.PushBack(iter);
  }

  unfilled_frame_queue_.Erase(unfilled_frame_queue_.Begin());

  QMMF_DEBUG("%s:%s track_id(%d) Sending buffer(0x%p) fd(%d) to client", TAG,
     __func__, TrackId(), (iter).data, (iter).fd);

  }
  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return NO_ERROR;
}

//application will fill the data in the buffer and will send to service
status_t AudioTrackDecoder::QueueInputBuffer(
    std::vector<AVCodecBuffer>& buffers) {
  QMMF_DEBUG("%s:%s: Enter", TAG, __func__);

  int32_t size = buffers.size();

  for (int32_t i = 0; i < size; i++) {

    StreamBuffer iter = *filled_frame_queue_.Begin();

    QMMF_DEBUG("%s:%s: fd from client %d" , TAG, __func__,buffers[i].buf_id);

    BufInfo bufinfo;
    memset(&bufinfo, 0x0, sizeof bufinfo);

    bufinfo = buf_info_map.valueFor(buffers[i].buf_id);

    (iter).fd                 = buffers[i].buf_id;
    (iter).data               = bufinfo.vaddr;
    (iter).filled_length      = buffers[i].filled_length;
    (iter).frame_length       = buffers[i].frame_length;
    (iter).flags              = buffers[i].flag;
    (iter).timestamp          = buffers[i].time_stamp;

    QMMF_DEBUG("%s:%s: fd %d", TAG, __func__,buffers[i].buf_id);
    QMMF_DEBUG("%s:%s: filled_length %d", TAG, __func__,buffers[i].filled_length);
    QMMF_DEBUG("%s:%s: frame_length %d", TAG, __func__,buffers[i].frame_length);
    QMMF_DEBUG("%s:%s: vaddr 0x%p", TAG, __func__,bufinfo.vaddr);
    QMMF_DEBUG("%s:%s: flag %d", TAG, __func__,buffers[i].flag);
    QMMF_DEBUG("%s:%s: time stamp %llu", TAG, __func__,buffers[i].time_stamp);

  #ifdef DUMP_AUDIO_BITSTREAM
    write(file_fd_audio_,(iter).data,(iter).filled_length);
  #endif

    {
       Mutex::Autolock lock(queue_lock_);
       frames_to_decode_.PushBack(iter);
       wait_for_frame_.signal();
    }

    filled_frame_queue_.Erase(filled_frame_queue_.Begin());

    QMMF_DEBUG("%s:%s track_id(%d) received buffer(0x%p) fd(%d) from client", TAG,
       __func__, TrackId(), (iter).data, (iter).fd);
  }

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return NO_ERROR;
}

status_t AudioTrackDecoder::StartDecoder() {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());

  assert(avcodec_ != nullptr);
  auto ret = avcodec_->StartCodec();
  // Initial debug purpose.
  assert(ret == NO_ERROR);
  if (ret != NO_ERROR) {
   QMMF_ERROR("%s:%s: track_id(%d) StopCodec failed!", TAG, __func__,
       TrackId());
   return ret;
  }

  audio_track_sink_->StartSink();
  // Initial debug purpose.
  assert(ret == NO_ERROR);
  if (ret != NO_ERROR) {
   QMMF_ERROR("%s:%s: track_id(%d) StartSink failed!", TAG, __func__,
       TrackId());
   return ret;
  }

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t AudioTrackDecoder::StopDecoder(bool do_flush) {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  auto ret = 0;

  assert(avcodec_ != nullptr);
  if (do_flush) {
    ret = avcodec_->Flush(kPortALL);
    assert(ret == NO_ERROR);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s:%s: track_id(%d) Flush failed!", TAG, __func__,
          TrackId());
      return ret;
    }
  }

  ret = avcodec_->StopCodec();
  // Initial debug purpose.
  assert(ret == NO_ERROR);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: track_id(%d) StopCodec failed!", TAG, __func__,
        TrackId());
    return ret;
  }

  ret = audio_track_sink_->StopSink();
  // Initial debug purpose.
  assert(ret == NO_ERROR);
  if (ret != NO_ERROR) {
      QMMF_ERROR("%s:%s: track_id(%d) StopSink failed!", TAG, __func__,
          TrackId());
    return ret;
  }

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t AudioTrackDecoder::PauseDecoder() {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());

  auto ret = audio_track_sink_->PauseSink();
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: track_id(%d) PauseSink failed!", TAG, __func__,
        TrackId());
    return ret;
  }

  assert(avcodec_ != nullptr);
  ret = avcodec_->PauseCodec();
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: track_id(%d) PauseCodec failed!", TAG, __func__,
        TrackId());
    return ret;
  }

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t AudioTrackDecoder::ResumeDecoder() {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());

  auto ret = audio_track_sink_->ResumeSink();
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: track_id(%d) ResumeSink failed!", TAG, __func__,
        TrackId());
    return ret;
  }

  assert(avcodec_ != nullptr);
  ret = avcodec_->ResumeCodec();
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: track_id(%d) ResumeCodec failed!", TAG, __func__,
        TrackId());
    return ret;
  }

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

status_t AudioTrackDecoder::SetAudioDecoderParams(CodecParamType
                                                  param_type,
                                                  void* param,
                                                  uint32_t param_size) {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());

  assert(avcodec_ != nullptr);
  auto ret = avcodec_->SetParameters(param_type,param,param_size);
  // Initial debug purpose.
  assert(ret == NO_ERROR);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s:%s: track_id(%d) ResumeCodec failed!", TAG, __func__,
        TrackId());
    return ret;
  }

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;
}

 status_t AudioTrackDecoder::DeleteDecoder() {
   QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
   assert(avcodec_ != nullptr);
   auto ret = avcodec_->ReleaseBuffer();
   if (ret != NO_ERROR) {
     QMMF_ERROR("%s:%s: ReleaseBuffer failed!", TAG, __func__);
   }
   QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
   return ret;
}

// This method provides an input buffer to the AVCodec
status_t AudioTrackDecoder::GetBuffer(BufferDescriptor& stream_buffer,
                                      void* client_data) {
  QMMF_DEBUG("%s:%s: Enter track_id(%d) frames_to_decode_.Size(%d) ", TAG,
    __func__, TrackId(),frames_to_decode_.Size());

  if(frames_to_decode_.Size() <= 0) {
    QMMF_DEBUG("%s:%s track_id(%d) No Filled buffer available for AVCodec,"
        " Wait for new buffer", TAG, __func__, TrackId());
    Mutex::Autolock autoLock(wait_for_frame_lock_);
    wait_for_frame_.wait(wait_for_frame_lock_);
  }

  StreamBuffer iter = *frames_to_decode_.Begin();

  stream_buffer.fd       = (iter).fd;
  stream_buffer.data     = (iter).data;
  stream_buffer.capacity = (iter).frame_length;
  stream_buffer.size     = (iter).filled_length;
  stream_buffer.flag     = (iter).flags;
  stream_buffer.timestamp = (iter).timestamp;

  QMMF_VERBOSE("%s:%s: track_id(%d)", TAG, __func__,TrackId());

  {
    Mutex::Autolock lock(queue_lock_);
    frames_being_decoded_.PushBack(iter);
  }
  frames_to_decode_.Erase(frames_to_decode_.Begin());
  QMMF_DEBUG("%s:%s track_id(%d) Sending buffer(0x%p) fd(%d) to avcodec for"
      " decoding ", TAG, __func__, TrackId(), (iter).data, (iter).fd);

  QMMF_DEBUG("%s:%s track_id(%d) frame_length(%d) filled_length(%d) to avcodec for"
      " decoding ", TAG, __func__, TrackId(), (iter).frame_length, (iter).filled_length);

  //For EOS and stop case
  if ((iter).flags == 1)
  {
     return -1;
  }

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return NO_ERROR;
}

// This method is used by AVCodec to return buffer after decoding
status_t AudioTrackDecoder::ReturnBuffer(BufferDescriptor& stream_buffer,
                                         void* client_data) {
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());

  QMMF_VERBOSE("%s:%s: track_id(%d) frames_being_decoded_.size(%d)", TAG,
      __func__, TrackId(), frames_being_decoded_.Size());

  bool found = false;

  List<StreamBuffer>::iterator it = frames_being_decoded_.Begin();
  for (; it != frames_being_decoded_.End(); ++it) {
    if ((*it).data ==  stream_buffer.data) {
      {
        Mutex::Autolock lock(queue_lock_);
        unfilled_frame_queue_.PushBack(*it);
        wait_for_empty_frame_.signal();
      }
      found = true;
      break;
    }
  }

  assert(found == true);
  frames_being_decoded_.Erase(it);

  QMMF_VERBOSE("%s:%s: frames_being_decoded_.Size(%d)", TAG, __func__,
      frames_being_decoded_.Size());

  QMMF_DEBUG("%s:%s Exit track_id(%d)", TAG, __func__, TrackId());
  return NO_ERROR;
}

// This method is used by AVCodec to notify stop
status_t AudioTrackDecoder::NotifyPortEvent(PortEventType event_type,
                                            void* event_data) {
  QMMF_DEBUG("%s:%s Enter track_id(%d)", TAG, __func__, TrackId());


  QMMF_DEBUG("%s:%s Exit track_id(%d)", TAG, __func__, TrackId());
  return NO_ERROR;
}


//**************************** Buffer Allocation ***************************//

status_t AudioTrackDecoder::AllocInputPortBufs()
{
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  int32_t ret = 0;
  uint32_t count, size;

  assert(avcodec_ != nullptr);
  ret = avcodec_->GetBufferRequirements(kPortIndexInput,  &count, &size);
  QMMF_DEBUG("%s:%s: BufferRequirements count(%d) size(%d)", TAG, __func__,
      count, size);
  assert(ret == NO_ERROR);

  assert(ion_device_ >= 0);
  int32_t ion_type = 0x1 << ION_IOMMU_HEAP_ID;
  void *vaddr      = NULL;

  struct ion_allocation_data alloc;
  struct ion_fd_data         ion_fddata;

  for(uint32_t i = 0; i < count; i++) {

    StreamBuffer buffer;
    vaddr = NULL;
    memset(&buffer, 0x0, sizeof(buffer));
    memset(&alloc, 0x0, sizeof(ion_allocation_data));
    memset(&ion_fddata, 0x0, sizeof(ion_fddata));

    alloc.len = size;
    alloc.len = (alloc.len + 4095) & (~4095);
    alloc.align = 4096;
    alloc.flags = ION_FLAG_CACHED;
    alloc.heap_id_mask = ion_type;

    ret = ioctl(ion_device_, ION_IOC_ALLOC, &alloc);
    if (ret < 0) {
      QMMF_ERROR("%s:%s ION allocation failed!", TAG, __func__);
      goto ION_ALLOC_FAILED;
    }

    ion_fddata.handle = alloc.handle;
    ret = ioctl(ion_device_, ION_IOC_SHARE, &ion_fddata);
    if (ret < 0) {
        QMMF_ERROR("%s:%s ION map failed %s", TAG, __func__, strerror(errno));
        goto ION_MAP_FAILED;
    }

    vaddr = mmap(NULL, alloc.len, PROT_READ  | PROT_WRITE, MAP_SHARED,
                 ion_fddata.fd, 0);

    if (vaddr == MAP_FAILED) {
        QMMF_ERROR("%s:%s  ION mmap failed: %s (%d)", TAG, __func__,
            strerror(errno), errno);
        goto ION_MAP_FAILED;
    }

    buffer.fd                 =  ion_fddata.fd;
    buffer.frame_length       =  alloc.len;
    buffer.filled_length      =  0;
    buffer.data               =  vaddr;
    ion_handle_data.push_back(alloc);

    QMMF_VERBOSE("%s:%s buffer.Fd(%d)", TAG, __func__, buffer.fd);
    QMMF_VERBOSE("%s:%s buffer.frame_length(%d)", TAG, __func__, buffer.frame_length);
    QMMF_VERBOSE("%s:%s buffer.vaddr(%p)", TAG, __func__, buffer.data);

    BufInfo bufinfo;
    memset(&bufinfo,0x0,sizeof bufinfo);

    bufinfo.vaddr   = vaddr;
    bufinfo.buf_id  = ion_fddata.fd;

    buf_info_map.add(ion_fddata.fd,bufinfo);

    input_buffer_list_.push_back(buffer);
  }

  for(uint32_t j = 0; j < buf_info_map.size(); j++) {
    QMMF_VERBOSE("%s:%s: buf_info_map:idx(%d) :key(%d) :fd:%d :data:"
        "0x%p", TAG, __func__, j, buf_info_map.keyAt(j), buf_info_map[j].buf_id,
        buf_info_map[j].vaddr);
  }

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;

  ION_MAP_FAILED:
  struct ion_handle_data ionHandleData;
  memset(&ionHandleData, 0x0, sizeof(ionHandleData));
  ionHandleData.handle = ion_fddata.handle;
  ioctl(ion_device_, ION_IOC_FREE, &ionHandleData);
  ION_ALLOC_FAILED:
  QMMF_ERROR("%s:%s ION Buffer allocation failed!", TAG, __func__);
  return -1;
}

status_t AudioTrackDecoder::AllocOutputPortBufs()
{
  QMMF_DEBUG("%s:%s: Enter track_id(%d)", TAG, __func__, TrackId());
  int32_t ret = 0;
  uint32_t count, size;

  assert(avcodec_ != nullptr);
  ret = avcodec_->GetBufferRequirements(kPortIndexOutput,  &count, &size);
  QMMF_DEBUG("%s:%s: BufferRequirements count(%d) size(%d)", TAG, __func__,
      count, size);
  assert(ret == NO_ERROR);

  assert(ion_device_ >= 0);
  int32_t ion_type = 0x1 << ION_IOMMU_HEAP_ID;
  void *vaddr      = NULL;

  struct ion_allocation_data alloc;
  struct ion_fd_data         ion_fddata;

  for(uint32_t i = 0; i < count; i++) {

    CodecBuffer buffer;
    vaddr = NULL;
    memset(&buffer, 0x0, sizeof(buffer));
    memset(&alloc, 0x0, sizeof(ion_allocation_data));
    memset(&ion_fddata, 0x0, sizeof(ion_fddata));

    alloc.len = size;
    alloc.len = (alloc.len + 4095) & (~4095);
    alloc.align = 4096;
    alloc.flags = ION_FLAG_CACHED;
    alloc.heap_id_mask = ion_type;

    ret = ioctl(ion_device_, ION_IOC_ALLOC, &alloc);
    if (ret < 0) {
      QMMF_ERROR("%s:%s ION allocation failed!", TAG, __func__);
      goto ION_ALLOC_FAILED;
    }

    ion_fddata.handle = alloc.handle;
    ret = ioctl(ion_device_, ION_IOC_SHARE, &ion_fddata);
    if (ret < 0) {
        QMMF_ERROR("%s:%s ION map failed %s", TAG, __func__, strerror(errno));
        goto ION_MAP_FAILED;
    }

    vaddr = mmap(NULL, alloc.len, PROT_READ  | PROT_WRITE, MAP_SHARED,
                 ion_fddata.fd, 0);

    if (vaddr == MAP_FAILED) {
        QMMF_ERROR("%s:%s  ION mmap failed: %s (%d)", TAG, __func__,
            strerror(errno), errno);
        goto ION_MAP_FAILED;
    }

    buffer.handle_data.handle = ion_fddata.handle;
    buffer.fd                 = ion_fddata.fd;
    buffer.frame_length       = alloc.len;
    buffer.filled_length      = 0;
    buffer.pointer            = vaddr;

    QMMF_VERBOSE("%s:%s buffer.Fd(%d)", TAG, __func__, buffer.fd);
    QMMF_VERBOSE("%s:%s buffer.size(%d)", TAG, __func__, buffer.frame_length);
    QMMF_VERBOSE("%s:%s buffer.vaddr(%p)", TAG, __func__, buffer.pointer);

    output_buffer_list_.push_back(buffer);
  }

  QMMF_DEBUG("%s:%s: Exit track_id(%d)", TAG, __func__, TrackId());
  return ret;

ION_MAP_FAILED:
  struct ion_handle_data ionHandleData;
  memset(&ionHandleData, 0x0, sizeof(ionHandleData));
  ionHandleData.handle = ion_fddata.handle;
  ioctl(ion_device_, ION_IOC_FREE, &ionHandleData);
ION_ALLOC_FAILED:
  QMMF_ERROR("%s:%s ION Buffer allocation failed!", TAG, __func__);
  return -1;
}

};  // namespace player
};  // namespace qmmf
