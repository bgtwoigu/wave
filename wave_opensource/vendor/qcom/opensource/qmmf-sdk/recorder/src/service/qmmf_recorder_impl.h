/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#pragma once

#include <camera/CameraMetadata.h>
#include <utils/KeyedVector.h>

#include "recorder/src/client/qmmf_recorder_service_intf.h"
#include "recorder/src/service/qmmf_recorder_common.h"
#include "recorder/src/service/qmmf_audio_source.h"
#include "recorder/src/service/qmmf_audio_encoder_core.h"
#include "recorder/src/service/qmmf_camera_source.h"
#include "recorder/src/service/qmmf_encoder_core.h"
#include "recorder/src/service/qmmf_remote_cb.h"

namespace qmmf {

namespace recorder {

using namespace android;

class RecorderImpl {
 public:

  static RecorderImpl* CreateRecorder();

  ~RecorderImpl();

  status_t Connect(const sp<RemoteCallBack>& remote_cb);

  status_t Disconnect();

  status_t StartCamera(const uint32_t camera_id, const CameraStartParam &param,
                       bool enable_result_cb = false);

  status_t StopCamera(const uint32_t camera_id);

  status_t CreateSession(uint32_t *session_id);

  status_t DeleteSession(const uint32_t session_id);

  status_t StartSession(const uint32_t session_id);

  status_t StopSession(const uint32_t session_id, bool do_flush);

  status_t PauseSession(const uint32_t session_id);

  status_t ResumeSession(const uint32_t session_id);

  status_t CreateAudioTrack(const uint32_t session_id,
                            const uint32_t track_id,
                            const AudioTrackCreateParam& param);

  status_t CreateVideoTrack(const uint32_t session_id,
                            const uint32_t track_id,
                            const VideoTrackCreateParam& param);

  status_t DeleteAudioTrack(const uint32_t session_id,
                            const uint32_t track_id);

  status_t DeleteVideoTrack(const uint32_t session_id,
                            const uint32_t track_id);

  status_t ReturnTrackBuffer(const uint32_t session_id,
                             const uint32_t track_id,
                             std::vector<BnBuffer> &buffers);

  status_t SetAudioTrackParam(const uint32_t session_id,
                              const uint32_t track_id,
                              CodecParamType type,
                              void *param,
                              size_t param_size);

  status_t SetVideoTrackParam(const uint32_t session_id,
                              const uint32_t track_id,
                              CodecParamType type,
                              void *param,
                              size_t param_size);

  status_t CaptureImage(const uint32_t camera_id, const ImageParam &param,
                        const uint32_t num_images,
                        const std::vector<CameraMetadata> &meta);

  status_t ConfigImageCapture(const uint32_t camera_id,
                              const ImageCaptureConfig &config);

  status_t CancelCaptureImage(const uint32_t camera_id);

  status_t ReturnImageCaptureBuffer(const uint32_t camera_id,
                                    const int32_t buffer_id);

  status_t SetCameraParam(const uint32_t camera_id, const CameraMetadata &meta);

  status_t GetCameraParam(const uint32_t camera_id, CameraMetadata &meta);

  status_t GetDefaultCaptureParam(const uint32_t camera_id,
                                  CameraMetadata &meta);

  status_t CreateOverlayObject(const uint32_t track_id,
                               OverlayParam *param,
                               uint32_t *overlay_id);

  status_t DeleteOverlayObject(const uint32_t track_id,
                               const uint32_t overlay_id);

  status_t GetOverlayObjectParams(const uint32_t track_id,
                                  const uint32_t overlay_id,
                                  OverlayParam &param);

  status_t UpdateOverlayObjectParams(const uint32_t track_id,
                                     const uint32_t overlay_id,
                                     OverlayParam *param);

  status_t SetOverlayObject(const uint32_t track_id,
                            const uint32_t overlay_id);

  status_t RemoveOverlayObject(const uint32_t track_id,
                               const uint32_t overlay_id);

  status_t CreateMultiCamera(const std::vector<uint32_t> camera_ids,
                             uint32_t *virtual_camera_id);

  status_t ConfigureMultiCamera(const uint32_t virtual_camera_id,
                                const uint32_t type,
                                const void *param,
                                const uint32_t param_size);

  // Data callback handlers.
  void VideoTrackBufferCallback(uint32_t track_id,
                                std::vector<BnBuffer>& buffers,
                                std::vector<MetaData>& meta_buffers);

  void AudioTrackBufferCallback(uint32_t track_id,
                                std::vector<BnBuffer>& buffers,
                                std::vector<MetaData>& meta_buffers);

  void SnapshotCallback(uint32_t camera_id, uint32_t count, BnBuffer& buffer,
                        MetaData& meta_data);

  void CameraResultCallback(uint32_t camera_id, const CameraMetadata &result);

 private:

  bool IsSessionIdValid(const uint32_t session_id);

  bool IsSessionValid(const uint32_t session_id);

  bool IsSessionStarted(const uint32_t session_id);

  bool IsTrackValid(const uint32_t session_id, const uint32_t track_id);

  typedef struct TrackInfo {
    uint32_t         track_id;
    TrackType        type;
    VideoTrackParams video_params;
    AudioTrackParams audio_params;
    //TODO: Add union and pack AudioTrack params.
  } TrackInfo;

  uint32_t            unique_id_;
  CameraSource*       camera_source_;
  EncoderCore*        encoder_core_;
  AudioSource*        audio_source_;
  AudioEncoderCore*   audio_encoder_core_;
  Vector<uint32_t>    session_ids_;
  sp<RemoteCallBack>  remote_cb_;

  DefaultKeyedVector<uint32_t, Vector<TrackInfo> > sessions_;
  DefaultKeyedVector<uint32_t, bool> sessions_state_;
  /**Not allowed */
  RecorderImpl();
  RecorderImpl(const RecorderImpl&);
  RecorderImpl& operator=(const RecorderImpl&);
  static RecorderImpl* instance_;

};

}; // namespace recorder

}; //namespace qmmf
