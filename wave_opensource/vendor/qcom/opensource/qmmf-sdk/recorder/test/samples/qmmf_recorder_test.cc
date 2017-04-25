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

#define TAG "RecorderTest"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <utils/Log.h>
#include <utils/String8.h>
#include <assert.h>
#include <system/graphics.h>
#include <QCamera3VendorTags.h>

#include "recorder/test/samples/qmmf_recorder_test.h"
#include "recorder/test/samples/qmmf_recorder_test_wav.h"
#include "recorder/test/samples/qmmf_recorder_test_amr.h"

//#define DEBUG
#define TEST_INFO(fmt, args...)  ALOGD(fmt, ##args)
#define TEST_ERROR(fmt, args...) ALOGE(fmt, ##args)
#ifdef DEBUG
#define TEST_DBG  TEST_INFO
#else
#define TEST_DBG(...) ((void)0)
#endif

using namespace qcamera;

static const char* kDefaultAudioFilenamePrefix =
    "/data/qmmf_recorder_test_audio";

RecorderTest::RecorderTest() :
            camera_id_(0),
            session_enabled_(false) {
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  static_info_.clear();
  use_display = 0;
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

RecorderTest::~RecorderTest() {
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

status_t RecorderTest::Connect() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  RecorderCb recorder_status_cb;
  recorder_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { RecorderCallbackHandler(event_type, event_data,
      event_data_size); };

  auto ret = recorder_.Connect(recorder_status_cb);
  TEST_INFO("%s:%s: Exit", TAG, __func__);

  return ret;
}

status_t RecorderTest::Disconnect() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  auto ret = recorder_.Disconnect();
  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}


status_t RecorderTest::GetCurrentAFMode(int32_t& mode) {
  CameraMetadata meta;

  auto status = recorder_.GetCameraParam(camera_id_, meta);
  if (NO_ERROR == status) {
     if (meta.exists(ANDROID_CONTROL_AF_MODE)) {
        mode = meta.find(ANDROID_CONTROL_AF_MODE).data.i32[0];
        TEST_DBG("current af mode(%d)",mode);
     } else {
        TEST_ERROR("auto focus not support\n");
        return NAME_NOT_FOUND;
     }
  } else {
    TEST_ERROR("Fail to get camera param, %s, %d\n", strerror(errno), status);
    return BAD_VALUE;
  }

  return NO_ERROR;
}

status_t RecorderTest::ToggleAFMode(const AfMode& af_mode) {
  CameraMetadata meta;
  uint8_t mode;
  status_t ret = NO_ERROR;
  int32_t current_mode = 0;

  switch(af_mode){
    case AfMode::kOff:
      mode = ANDROID_CONTROL_AF_MODE_OFF;
      break;
    case AfMode::kAuto:
      mode = ANDROID_CONTROL_AF_MODE_AUTO;
      break;
    case AfMode::kMacro:
      mode = ANDROID_CONTROL_AF_MODE_MACRO;
      break;
    case AfMode::kContinousVideo:
      mode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
      break;
    case AfMode::kContinuousPicture:
      mode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE;
      break;
    case AfMode::kEdof:
      mode = ANDROID_CONTROL_AF_MODE_EDOF;
      break;
    default:
      TEST_ERROR("Focuse -> Focus mode(%d) not supported! \n",af_mode);
      return BAD_VALUE;
  }

  ret = GetCurrentAFMode(current_mode);
  if(NO_ERROR != ret) {
     TEST_ERROR("Fail to get current focus mode\n");
     return ret;
  } else {
    if (current_mode != static_cast<int32_t>(mode)) {
      TEST_ERROR("current focus mode (%d),update focus mode to (%d)now!",
                current_mode,mode);
      meta.update(ANDROID_CONTROL_AF_MODE, &mode, 1);
      auto status = recorder_.SetCameraParam(camera_id_, meta);
      if (NO_ERROR != status) {
         TEST_ERROR("Fail to set focus mode param\n");
         ret = status;
      }
    }
  }

 return ret;
}

int32_t RecorderTest::ToggleNR() {
  CameraMetadata meta;
  auto status = recorder_.GetCameraParam(camera_id_, meta);
  if (NO_ERROR == status) {
    if (meta.exists(ANDROID_NOISE_REDUCTION_MODE)) {
      uint8_t mode = meta.find(ANDROID_NOISE_REDUCTION_MODE).data.u8[0];
      nr_modes_iter it = supported_nr_modes_.begin();
      nr_modes_iter next;
      while (it != supported_nr_modes_.end()) {
        if ((*it).first == mode) {
          it++;
          if (it == supported_nr_modes_.end()) {
            next = supported_nr_modes_.begin();
          } else {
            next = it;
          }
          meta.update(ANDROID_NOISE_REDUCTION_MODE, &next->first, 1);
          status = recorder_.SetCameraParam(camera_id_, meta);
          if (NO_ERROR != status) {
            ALOGE("%s:%s Failed to apply: %s\n",
                  TAG, __func__, next->second.c_str());
          }
          break;
        } else {
          it++;
        }
      }
    }
  }

  return status;
}

std::string RecorderTest::GetCurrentNRMode() {
  CameraMetadata meta;
  std::string ret(FEATURE_NOT_AVAILABLE);
  auto status = recorder_.GetCameraParam(camera_id_, meta);
  if (NO_ERROR == status) {
    if (meta.exists(ANDROID_NOISE_REDUCTION_MODE)) {
      uint8_t mode = meta.find(ANDROID_NOISE_REDUCTION_MODE).data.u8[0];
      for (auto it : supported_nr_modes_) {
        if ((it).first == mode) {
          ret = (it).second;
          break;
        }
      }
    }
  }

  return ret;
}

void RecorderTest::InitSupportedNRModes() {
  camera_metadata_entry_t entry;

  if (static_info_.exists(
      ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES)) {
    entry = static_info_.find(
        ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES);
    for (uint32_t i = 0 ; i < entry.count; i++) {
      switch(entry.data.u8[i]) {
        case ANDROID_NOISE_REDUCTION_MODE_OFF:
          supported_nr_modes_.insert(std::make_pair(entry.data.u8[i], "Off"));
          break;
        case ANDROID_NOISE_REDUCTION_MODE_FAST:
          supported_nr_modes_.insert(std::make_pair(entry.data.u8[i], "Fast"));
          break;
        case ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY:
          supported_nr_modes_.insert(std::make_pair(entry.data.u8[i],
                                                    "High quality"));
          break;
        case ANDROID_NOISE_REDUCTION_MODE_MINIMAL:
          supported_nr_modes_.insert(std::make_pair(entry.data.u8[i],
                                                    "Minimal"));
          break;
        case ANDROID_NOISE_REDUCTION_MODE_ZERO_SHUTTER_LAG:
          supported_nr_modes_.insert(std::make_pair(entry.data.u8[i], "ZSL"));
          break;
        default:
          ALOGE("%s:%s Invalid NR mode: %d\n", TAG, __func__,
                entry.data.u8[i]);
      }
    }
  }
}

int32_t RecorderTest::ToggleVHDR() {
  CameraMetadata meta;
  auto status = recorder_.GetCameraParam(camera_id_, meta);
  if (NO_ERROR == status) {
    if (meta.exists(QCAMERA3_VIDEO_HDR_MODE)) {
      int32_t mode = meta.find(QCAMERA3_VIDEO_HDR_MODE).data.i32[0];
      vhdr_modes_iter it = supported_hdr_modes_.begin();
      vhdr_modes_iter next;
      while (it != supported_hdr_modes_.end()) {
        if ((*it).first == mode) {
          it++;
          if (it == supported_hdr_modes_.end()) {
            next = supported_hdr_modes_.begin();
          } else {
            next = it;
          }
          meta.update(QCAMERA3_VIDEO_HDR_MODE, &next->first, 1);
          status = recorder_.SetCameraParam(camera_id_, meta);
          if (NO_ERROR != status) {
            ALOGE("%s:%s Failed to apply: %s\n",
                  TAG, __func__, next->second.c_str());
          }
          break;
        } else {
          it++;
        }
      }
    }
  }

  return status;
}

std::string RecorderTest::GetCurrentVHDRMode() {
  CameraMetadata meta;
  std::string ret(FEATURE_NOT_AVAILABLE);
  auto status = recorder_.GetCameraParam(camera_id_, meta);
  if (NO_ERROR == status) {
    if (meta.exists(QCAMERA3_VIDEO_HDR_MODE)) {
      int32_t mode = meta.find(QCAMERA3_VIDEO_HDR_MODE).data.i32[0];
      for (auto it : supported_hdr_modes_) {
        if ((it).first == mode) {
          ret = (it).second;
          break;
        }
      }
    } else {
      //In case camera didn't set default check if there are any
      //modes supported. In case there are set the first available.
      if (!supported_hdr_modes_.empty()) {
        vhdr_modes_iter start = supported_hdr_modes_.begin();
        meta.update(QCAMERA3_VIDEO_HDR_MODE, &start->first, 1);
        status = recorder_.SetCameraParam(camera_id_, meta);
        if (NO_ERROR != status) {
          ALOGE("%s:%s Failed to apply: %s\n",
                TAG, __func__, start->second.c_str());
        }
        ret = start->second;
      }
    }
  }

  return ret;
}

void RecorderTest::InitSupportedVHDRModes() {
  camera_metadata_entry_t entry;

  if (static_info_.exists(QCAMERA3_AVAILABLE_VIDEO_HDR_MODES)) {
    entry = static_info_.find(QCAMERA3_AVAILABLE_VIDEO_HDR_MODES);
    for (uint32_t i = 0 ; i < entry.count; i++) {
      switch(entry.data.i32[i]) {
        case QCAMERA3_VIDEO_HDR_MODE_OFF:
          supported_hdr_modes_.insert(std::make_pair(entry.data.i32[i], "Off"));
          break;
        case QCAMERA3_VIDEO_HDR_MODE_ON:
          supported_hdr_modes_.insert(std::make_pair(entry.data.i32[i], "On"));
          break;
        default:
          ALOGE("%s:%s Invalid VHDR mode: %d\n", TAG, __func__,
                entry.data.i32[i]);
      }
    }
  }
}



int32_t RecorderTest::ToggleIR() {
  CameraMetadata meta;
  auto status = recorder_.GetCameraParam(camera_id_, meta);
  if (NO_ERROR == status) {
    if (meta.exists(QCAMERA3_IR_MODE)) {
      int32_t mode = meta.find(QCAMERA3_IR_MODE).data.i32[0];
      ir_modes_iter it = supported_ir_modes_.begin();
      ir_modes_iter next;
      while (it != supported_ir_modes_.end()) {
        if ((*it).first == mode) {
          it++;
          if (it == supported_ir_modes_.end()) {
            next = supported_ir_modes_.begin();
          } else {
            next = it;
          }
          meta.update(QCAMERA3_IR_MODE, &next->first, 1);
          status = recorder_.SetCameraParam(camera_id_, meta);
          if (NO_ERROR != status) {
            ALOGE("%s:%s Failed to apply: %s\n",
                  TAG, __func__, next->second.c_str());
          }
          break;
        } else {
          it++;
        }
      }
    }
  }

  return status;
}

void RecorderTest::InitSupportedBinningCorrectionModes() {
  camera_metadata_entry_t entry;

  if (static_info_.exists(QCAMERA3_AVAILABLE_BINNING_CORRECTION_MODES)) {
    entry = static_info_.find(QCAMERA3_AVAILABLE_BINNING_CORRECTION_MODES);
    for (uint32_t i = 0 ; i < entry.count; i++) {
      switch(entry.data.i32[i]) {
        case QCAMERA3_BINNING_CORRECTION_MODE_OFF:
          supported_bc_modes_.insert(std::make_pair(entry.data.i32[i], "Off"));
          break;
        case QCAMERA3_BINNING_CORRECTION_MODE_ON:
          supported_bc_modes_.insert(std::make_pair(entry.data.i32[i], "On"));
          break;
        default:
          ALOGE("%s:%s Invalid binning correction mode: %d\n", TAG, __func__,
                entry.data.i32[i]);
      }
    }
  }
}

int32_t RecorderTest::ToggleBinningCorrectionMode() {
  CameraMetadata meta;
  auto status = recorder_.GetCameraParam(camera_id_, meta);
  if (NO_ERROR == status) {
    if (meta.exists(QCAMERA3_BINNING_CORRECTION_MODE)) {
      int32_t mode = meta.find(QCAMERA3_BINNING_CORRECTION_MODE).data.i32[0];
      bc_modes_iter it = supported_bc_modes_.begin();
      bc_modes_iter next;
      while (it != supported_bc_modes_.end()) {
        if ((*it).first == mode) {
          it++;
          if (it == supported_bc_modes_.end()) {
            next = supported_bc_modes_.begin();
          } else {
            next = it;
          }
          meta.update(QCAMERA3_BINNING_CORRECTION_MODE, &next->first, 1);
          status = recorder_.SetCameraParam(camera_id_, meta);
          if (NO_ERROR != status) {
            ALOGE("%s:%s Failed to apply: %s\n",
                  TAG, __func__, next->second.c_str());
          }
          break;
        } else {
          it++;
        }
      }
    }
  }

  return status;
}

std::string RecorderTest::GetCurrentBinningCorrectionMode() {
  CameraMetadata meta;
  std::string ret(FEATURE_NOT_AVAILABLE);
  auto status = recorder_.GetCameraParam(camera_id_, meta);
  if (NO_ERROR == status) {
    if (meta.exists(QCAMERA3_BINNING_CORRECTION_MODE)) {
      int32_t mode = meta.find(QCAMERA3_BINNING_CORRECTION_MODE).data.i32[0];
      for (auto it : supported_bc_modes_) {
        if ((it).first == mode) {
          ret = (it).second;
          break;
        }
      }
    } else {
      //In case camera didn't set default check if there are any
      //modes supported. In case there are set the first available.
      if (!supported_bc_modes_.empty()) {
        bc_modes_iter start = supported_bc_modes_.begin();
        meta.update(QCAMERA3_BINNING_CORRECTION_MODE, &start->first, 1);
        status = recorder_.SetCameraParam(camera_id_, meta);
        if (NO_ERROR != status) {
          ALOGE("%s:%s Failed to apply: %s\n",
                TAG, __func__, start->second.c_str());
        }
        ret = start->second;
      }
    }
  }

  return ret;
}

int32_t RecorderTest::SetBinningCorrectionMode(const bool& mode) {
  CameraMetadata meta;
  auto ret = recorder_.GetCameraParam(camera_id_, meta);
  assert(ret == NO_ERROR);

  int32_t binning_correction_mode = QCAMERA3_BINNING_CORRECTION_MODE_OFF;
  if(mode) { //Enabled
    binning_correction_mode = QCAMERA3_BINNING_CORRECTION_MODE_ON;
  }
  meta.update(QCAMERA3_BINNING_CORRECTION_MODE, &binning_correction_mode, 1);
  ret = recorder_.SetCameraParam(camera_id_, meta);
  return ret;
}

int32_t RecorderTest::ChooseCamera() {

  // TODO - propagate num cameras
  int num_cameras = 3;
  int input;

  do {
    printf("\n");
    printf("****** Current Camera is 0 - %d *******\n", camera_id_);
    printf("    Choose Camera: 0 - %d\n", num_cameras-1);
    printf("\n");
    scanf("%d", &input);
  } while ((input > num_cameras - 1) || (input < 0));
  camera_id_ = input;

  return 0;
}

int32_t RecorderTest::SetAntibandingMode() {

  int input;
  uint8_t mode;
  CameraMetadata meta;

  printf("\n");
  printf("****** Antibanding mode *******\n" );
  printf("  1. Off\n" );
  printf("  2. 50Hz\n" );
  printf("  3. 60Hz\n" );
  printf("  4. Auto\n" );
  printf("  0. exit\n");
  scanf("%d", &input);
  auto status = recorder_.GetCameraParam(camera_id_, meta);

  switch(input) {
    case 1:
      mode = ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF;
      break;
    case 2:
      mode = ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ;
      break;
    case 3:
      mode = ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ;
      break;
    case 4:
      mode = ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO;
      break;
    default:
      return 0;
      break;
  }

  meta.update(ANDROID_CONTROL_AE_ANTIBANDING_MODE, &mode, 1);
  status = recorder_.SetCameraParam(camera_id_, meta);
  if (NO_ERROR != status) {
    TEST_ERROR("%s:%s Failed to apply: ANDROID_CONTROL_AE_ANTIBANDING_MODE\n",
                TAG, __func__);
    return status;
  }

  return 0;
}

std::string RecorderTest::GetCurrentIRMode() {
  CameraMetadata meta;
  std::string ret(FEATURE_NOT_AVAILABLE);
  auto status = recorder_.GetCameraParam(camera_id_, meta);
  if (NO_ERROR == status) {
    if (meta.exists(QCAMERA3_IR_MODE)) {
      int32_t mode = meta.find(QCAMERA3_IR_MODE).data.i32[0];
      for (auto it : supported_ir_modes_) {
        if ((it).first == mode) {
          ret = (it).second;
          break;
        }
      }
    }  else {
      //In case camera didn't set default check if there are any
      //modes supported. In case there are set the first available.
      if (!supported_ir_modes_.empty()) {
        ir_modes_iter start = supported_ir_modes_.begin();
        meta.update(QCAMERA3_IR_MODE, &start->first, 1);
        status = recorder_.SetCameraParam(camera_id_, meta);
        if (NO_ERROR != status) {
          ALOGE("%s:%s Failed to apply: %s\n",
                TAG, __func__, start->second.c_str());
        }
        ret = start->second;
      }
    }
  }

  return ret;
}

void RecorderTest::InitSupportedIRModes() {
  camera_metadata_entry_t entry;

  if (static_info_.exists(QCAMERA3_IR_AVAILABLE_MODES)) {
    entry = static_info_.find(QCAMERA3_IR_AVAILABLE_MODES);
    for (uint32_t i = 0 ; i < entry.count; i++) {
      switch(entry.data.i32[i]) {
        case QCAMERA3_IR_MODE_OFF:
          supported_ir_modes_.insert(std::make_pair(entry.data.i32[i], "Off"));
          break;
        case QCAMERA3_IR_MODE_ON:
          supported_ir_modes_.insert(std::make_pair(entry.data.i32[i], "On"));
          break;
        default:
          ALOGE("%s:%s Invalid IR mode: %d\n", TAG, __func__,
                entry.data.i32[i]);
      }
    }
  }
}

status_t RecorderTest::StartCamera() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  CameraStartParam camera_params;
  memset(&camera_params, 0x0, sizeof camera_params);
  camera_params.zsl_mode            = false;
  camera_params.zsl_queue_depth     = 10;
  camera_params.zsl_width           = 3840;
  camera_params.zsl_height          = 2160;
  camera_params.frame_rate          = 30;
  camera_params.flags               = 0x0;

  auto ret = recorder_.StartCamera(camera_id_, camera_params);
  if(ret != 0) {
      ALOGE("%s:%s StartCamera Failed!!", TAG, __func__);
  }

  ret = recorder_.GetDefaultCaptureParam(camera_id_, static_info_);
  if (NO_ERROR != ret) {
    ALOGE("%s:%s Unable to query default capture parameters!\n",
          TAG, __func__);
  } else {
    InitSupportedNRModes();
    InitSupportedVHDRModes();
    InitSupportedIRModes();
    InitSupportedBinningCorrectionModes();
  }

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return 0;
}

status_t RecorderTest::StopCamera() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  auto ret = recorder_.StopCamera(camera_id_);
  if(ret != 0) {
    ALOGE("%s:%s StopCamera Failed!!", TAG, __func__);
  }

  static_info_.clear();
  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return 0;
}

status_t RecorderTest::TakeSnapshotWithConfig(const SnapshotInfo&
                                          snapshot_info) {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  int32_t ret = 0;
  camera_metadata_entry_t entry;
  CameraMetadata meta;
  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  assert(ret == 0);

  ImageParam image_param;
  memset(&image_param, 0x0, sizeof image_param);

  image_param.width = snapshot_info.width;
  image_param.height = snapshot_info.height;

  switch(snapshot_info.type) {
    case SnapshotType::kNone:
      TEST_INFO("Snapshot format(%d) is not correct\n",snapshot_info.type);
      break;
    case SnapshotType::kJpeg:
      // Check Supported JPEG snapshot resolutions.
      if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
        entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
        for (uint32_t i = 0 ; i < entry.count; i += 4) {
        if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
           if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
               entry.data.i32[i+3]) {
             if ( (0 == snapshot_info.width)
               && (0 == snapshot_info.height) ){
               image_param.width = (uint32_t)entry.data.i32[i+1];
               image_param.height = (uint32_t)entry.data.i32[i+2];
               TEST_INFO("Snapshot JPEG size %dx%d is support\n",
                       image_param.width,image_param.height);
               break;
             }
             if ( (image_param.width == (uint32_t)entry.data.i32[i+1])
               && (image_param.height == (uint32_t)entry.data.i32[i+2]) ){
                TEST_INFO("Snapshot size %dx%d is support\n",
                        image_param.width,image_param.height);
                break;
             }
           }
         }
       }
     }

     image_param.image_format  = ImageFormat::kJPEG;
     image_param.image_quality = 95;
     break;
   case SnapshotType::kRawYuv:
     // Check available raw YUV resolutions.
     if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
       entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
       for (uint32_t i = 0 ; i < entry.count; i += 4) {
         if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
           if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
             entry.data.i32[i+3]) {
             TEST_INFO("%s:%s:(%d) Supported Raw YUV:(%d)x(%d)", TAG,
                    __func__, i, entry.data.i32[i+1], entry.data.i32[i+2]);
             if ( (0 == snapshot_info.width)
               && (0 == snapshot_info.height) ){
               image_param.width = (uint32_t)entry.data.i32[i+1];
               image_param.height = (uint32_t)entry.data.i32[i+2];
               TEST_INFO("RAW YUV size %dx%d\n is support\n",
                 image_param.width,image_param.height);
               break;
              }
              if ( (image_param.width == (uint32_t)entry.data.i32[i+1])
                 && (image_param.height == (uint32_t)entry.data.i32[i+2]) ){
                 TEST_INFO("RAW YUV size %dx%d is support\n",
                          image_param.width,image_param.height);
              }
            }
          }
        }
      }

      image_param.image_format = ImageFormat::kNV12;
      break;
    case SnapshotType::kRawRdi:
      if (meta.exists(ANDROID_SCALER_AVAILABLE_RAW_SIZES)) {
          entry = meta.find(ANDROID_SCALER_AVAILABLE_RAW_SIZES);
        for (uint32_t i = 0 ; i < entry.count; i += 2) {
          TEST_INFO("%s:%s: (%d) Supported RAW RDI W(%d):H(%d)\n", TAG,
                __func__, i, entry.data.i32[i+0], entry.data.i32[i+1]);
          image_param.width = (uint32_t)entry.data.i32[i+0];
          image_param.height = (uint32_t)entry.data.i32[i+1];
          break;
        }
      }

      image_param.image_format = ImageFormat::kBayerRDI;
      break;
    default:
      TEST_INFO("Snapshot format(%d) is not correct\n",snapshot_info.type);
      return BAD_VALUE;
  }

  if (snapshot_info.type  != SnapshotType::kNone) {
    ImageCaptureCb cb = [&] (uint32_t camera_id_, uint32_t image_count,
                             BufferDescriptor buffer, MetaData meta_data)
          { SnapshotCb(camera_id_, image_count, buffer, meta_data); };

    assert(ret == NO_ERROR);

    std::vector<CameraMetadata> meta_array;
    for (uint32_t i = 0; i < snapshot_info.count; i++) {
      meta_array.push_back(meta);
    }

    TEST_INFO("CaptureImage size %dx%d images count %d\n",
              image_param.width,image_param.height,snapshot_info.count);
    ret = recorder_.CaptureImage(camera_id_, image_param, snapshot_info.count,
                                 meta_array, cb);
    if(ret != 0) {
      ALOGE("%s:%s CaptureImage Failed", TAG, __func__);
    }
  }

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderTest::TakeSnapshot() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  int32_t ret = 0;
  int32_t input;
  camera_metadata_entry_t entry;
  CameraMetadata meta;
  ret = recorder_.GetDefaultCaptureParam(camera_id_, meta);
  assert(ret == 0);
  uint32_t num_images = 1;

  do {
    printf("\n");
    printf("****** Take Snapshot *******\n" );
    printf("  1. JPEG - 4K\n" );
    printf("  2. RAW:YUV - 1080p \n" );
    printf("  3. RAW:BAYER \n" );
    printf("  4. JPEG Burst (3 frames) - 1080p \n" );
    printf("  0. exit \n");
    printf("\n");
    printf("Enter option:\n");
    scanf("%d", &input);

    uint32_t w, h;
    ImageParam image_param;
    memset(&image_param, 0x0, sizeof image_param);
    switch(input) {
      case 0:
        break;
      case 1:
        image_param.width         = 3840;
        image_param.height        = 2160;
        image_param.image_format  = ImageFormat::kJPEG;
        image_param.image_quality = 95;
        break;
      case 2:
        // Check available raw YUV resolutions.
        if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
          entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
          for (uint32_t i = 0 ; i < entry.count; i += 4) {
            if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
              if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
                  entry.data.i32[i+3]) {
                TEST_INFO("%s:%s:(%d) Supported Raw YUV:(%d)x(%d)", TAG,
                    __func__, i, entry.data.i32[i+1], entry.data.i32[i+2]);
              }
            }
          }
        }
        image_param.width        = 1920;
        image_param.height       = 1080;
        image_param.image_format = ImageFormat::kNV12;
        break;
      case 3:
        if (meta.exists(ANDROID_SCALER_AVAILABLE_RAW_SIZES)) {
          entry = meta.find(ANDROID_SCALER_AVAILABLE_RAW_SIZES);
          if (entry.count < 2) {
            printf("ANDROID_SCALER_AVAILABLE_RAW_SIZES count is wrong\n");
            input = 0;
            break;
          }
          for (uint32_t i = 0 ; i < entry.count; i += 2) {
            w = entry.data.i32[i+0];
            h = entry.data.i32[i+1];
            TEST_INFO("%s:%s: (%d) Supported RAW RDI W(%d):H(%d)", TAG,
                __func__, i, w, h);
          }
        } else {
          printf("ANDROID_SCALER_AVAILABLE_RAW_SIZES not found\n");
          input = 0;
          break;
        }
        image_param.width        = w; // 5344
        image_param.height       = h; // 4016
        image_param.image_format = ImageFormat::kBayerRDI;
        break;
      default:
         printf("Wrong value entered(%d)\n", input);
         input = 0;
         break;
      case 4:
        // Check available raw YUV resolutions.
        if (meta.exists(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS)) {
          entry = meta.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
          for (uint32_t i = 0 ; i < entry.count; i += 4) {
            if (HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED == entry.data.i32[i]) {
              if (ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT ==
                  entry.data.i32[i+3]) {
                TEST_INFO("%s:%s:(%d) Supported Raw YUV:(%d)x(%d)", TAG,
                    __func__, i, entry.data.i32[i+1], entry.data.i32[i+2]);
              }
            }
          }
        }
        image_param.width        = 1920;
        image_param.height       = 1080;
        image_param.image_format = ImageFormat::kJPEG;
        num_images = 30;
        break;
    }

    if (input != 0) {
      ImageCaptureCb cb = [&] (uint32_t camera_id_, uint32_t image_count,
                               BufferDescriptor buffer, MetaData meta_data)
          { SnapshotCb(camera_id_, image_count, buffer, meta_data); };

      assert(ret == NO_ERROR);

      uint8_t awb_mode = ANDROID_CONTROL_AWB_MODE_INCANDESCENT;
      ret = meta.update(ANDROID_CONTROL_AWB_MODE, &awb_mode, 1);
      assert(ret == NO_ERROR);

      if (!sessions_.size()) {
        /* we have only capture stream which will by default disable WB and lead
           to broken picture */
        uint8_t intent = ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW;
        ret = meta.update(ANDROID_CONTROL_CAPTURE_INTENT, &intent, 1);
        assert(ret == NO_ERROR);
      }

      std::vector<CameraMetadata> meta_array;
      for (uint32_t i = 0; i < num_images; i++) {
        meta_array.push_back(meta);
      }
      ret = recorder_.CaptureImage(camera_id_, image_param, num_images, meta_array, cb);
      if(ret != 0) {
        ALOGE("%s:%s CaptureImage Failed!!", TAG, __func__);
      }
      input = 0;
    }

  } while(input);

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

// This session has two YUV video tracks 4K and 1080p.
status_t RecorderTest::Session4KAnd1080pYUVTracks() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *yuv_4k_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.width      = 3840;
  info.height     = 2160;
  info.track_id   = 1;
  info.track_type = TrackType::kVideoYUV;
  info.session_id = session_id;
  info.camera_id = camera_id_;

  ret = yuv_4k_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(yuv_4k_track);

  TestTrack *yuv_1080p_track = new TestTrack(this);
  memset(&info, 0x0, sizeof info);
  info.width      = 1920;
  info.height     = 1080;
  info.track_id   = 2;
  info.track_type = TrackType::kVideoYUV;
  info.session_id = session_id;
  info.camera_id = camera_id_;

  ret = yuv_1080p_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(yuv_1080p_track);

  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

// This session has one 4K video encode track
status_t RecorderTest::Session4KEncTrack(const TrackType& track_type) {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  TestTrack *video_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.width      = 3840;
  info.height     = 2160;
  info.track_id   = 1;
  info.track_type = track_type;
  info.session_id = session_id;
  info.camera_id = camera_id_;

  ret = video_track->SetUp(info);
  assert(ret == 0);

  std::vector<TestTrack*> tracks;
  tracks.push_back(video_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

// This session has one 1080p video encode and one AAC Audio track.
status_t RecorderTest::Session1080pEncTrack(const TrackType& track_type) {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *enc_1080p_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.width      = 1920;
  info.height     = 1080;
  info.track_id   = 1;
  info.track_type = track_type;
  info.session_id = session_id;
  info.camera_id = camera_id_;

  ret = enc_1080p_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(enc_1080p_track);

  TestTrack *audio_aac_track = new TestTrack(this);
  memset(&info, 0x0, sizeof info);
  info.track_id   = 101;
  info.track_type = TrackType::kAudioAAC;
  info.session_id = session_id;
  info.camera_id = camera_id_;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_aac_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_aac_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

// In this test case session has one 1080p video encode and one 1080p YUV track.
status_t RecorderTest::Session1080pEnc1080YUV(const TrackType& track_type) {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *enc_1080p_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.width      = 1920;
  info.height     = 1080;
  info.track_id   = 1;
  info.track_type = track_type;
  info.session_id = session_id;
  info.camera_id = camera_id_;

  ret = enc_1080p_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(enc_1080p_track);

  TestTrack *yuv_1080p_track = new TestTrack(this);
  memset(&info, 0x0, sizeof info);
  info.width      = 1920;
  info.height     = 1080;
  info.track_id   = 2;
  info.track_type = TrackType::kVideoYUV;
  info.session_id = session_id;
  info.camera_id = camera_id_;

  ret = yuv_1080p_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(yuv_1080p_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

// In this test case session has one 4K video HEVC and one 1080p YUV track.
status_t RecorderTest::Session4KHEVCAnd1080pYUVTracks(const TrackType&
                                                      track_type) {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *enc_1080p_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.width      = 3840;
  info.height     = 2160;
  info.track_id   = 1;
  info.track_type = track_type;
  info.session_id = session_id;
  info.camera_id = camera_id_;

  ret = enc_1080p_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(enc_1080p_track);

  TestTrack *yuv_1080p_track = new TestTrack(this);
  memset(&info, 0x0, sizeof info);
  info.width      = 1920;
  info.height     = 1080;
  info.track_id   = 2;
  info.track_type = TrackType::kVideoYUV;
  info.session_id = session_id;
  info.camera_id = camera_id_;

  ret = yuv_1080p_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(yuv_1080p_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

// This session has one 4K YUV and one 1080p video encode track
status_t RecorderTest::Session4KYUVAnd1080pEncTracks(const TrackType&
                                                        track_type) {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *yuv_4k_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.width      = 3840;
  info.height     = 2160;
  info.track_id   = 1;
  info.track_type = TrackType::kVideoYUV;
  info.session_id = session_id;
  info.camera_id = camera_id_;

  ret = yuv_4k_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(yuv_4k_track);

  TestTrack *enc_1080p_track = new TestTrack(this);
  memset(&info, 0x0, sizeof info);
  info.width      = 1920;
  info.height     = 1080;
  info.track_id   = 2;
  info.track_type = track_type;
  info.session_id = session_id;
  info.camera_id = camera_id_;

  ret = enc_1080p_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(enc_1080p_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

// This session has two 1080p video encode tracks.
status_t RecorderTest::SessionTwo1080pEncTracks(const TrackType& track_type) {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *enc_1080p_track1 = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.width      = 1920;
  info.height     = 1080;
  info.track_id   = 1;
  info.track_type = track_type;
  info.session_id = session_id;
  info.camera_id = camera_id_;

  ret = enc_1080p_track1->SetUp(info);
  assert(ret == 0);
  tracks.push_back(enc_1080p_track1);

  TestTrack *enc_1080p_track2 = new TestTrack(this);
  memset(&info, 0x0, sizeof info);
  info.width      = 1920;
  info.height     = 1080;
  info.track_id   = 2;
  info.track_type = track_type;
  info.session_id = session_id;
  info.camera_id = camera_id_;

  ret = enc_1080p_track2->SetUp(info);
  assert(ret == 0);
  tracks.push_back(enc_1080p_track2);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

// This session has one 720P LPM track
status_t RecorderTest::Session720pLPMTrack(const TrackType& track_type) {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *yuv_720p_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.width      = 1280;
  info.height     = 720;
  info.track_id   = 1;
  info.track_type = track_type;
  info.session_id = session_id;
  info.camera_id = camera_id_;
  info.low_power_mode = true;

  ret = yuv_720p_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(yuv_720p_track);

  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

// This session has one 1080p Encode and one 1080p LPM tracks
status_t RecorderTest::Session1080pEnc1080pLPMTracks(const TrackType& track_type) {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *enc_1080p_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.width      = 1920;
  info.height     = 1080;
  info.track_id   = 1;
  info.track_type = track_type;
  info.session_id = session_id;
  info.low_power_mode = false;

  ret = enc_1080p_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(enc_1080p_track);

  TestTrack *yuv_1080p_track = new TestTrack(this);
  memset(&info, 0x0, sizeof info);
  info.width          = 1920;
  info.height         = 1080;
  info.track_id       = 2;
  info.track_type     = TrackType::kVideoYUV;
  info.session_id     = session_id;
  info.low_power_mode = true;
  info.camera_id = camera_id_;

  ret = yuv_1080p_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(yuv_1080p_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderTest::CreateAudioPCMTrack() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_pcm_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.track_id   = 101;
  info.track_type = TrackType::kAudioPCM;
  info.session_id = session_id;
  info.camera_id = camera_id_;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_pcm_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_pcm_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderTest::CreateAudio2PCMTrack() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_pcm_track1 = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.track_id   = 101;
  info.track_type = TrackType::kAudioPCM;
  info.session_id = session_id;
  info.camera_id = camera_id_;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_pcm_track1->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_pcm_track1);

  TestTrack *audio_pcm_track2 = new TestTrack(this);
  info.track_id   = 102;

  ret = audio_pcm_track2->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_pcm_track2);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderTest::CreateAudioSCOTrack() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_sco_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.track_id   = 101;
  info.track_type = TrackType::kAudioPCM;
  info.session_id = session_id;
  info.camera_id = camera_id_;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBlueToothSCO);

  ret = audio_sco_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_sco_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderTest::CreateAudioPCMSCOTrack() {
  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_pcm_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.track_id   = 101;
  info.track_type = TrackType::kAudioPCM;
  info.session_id = session_id;
  info.camera_id = camera_id_;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_pcm_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_pcm_track);

  TestTrack *audio_sco_track = new TestTrack(this);
  info.track_id   = 102;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBlueToothSCO);

  ret = audio_sco_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_sco_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderTest::CreateAudioA2DPTrack() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_a2dp_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.track_id   = 101;
  info.track_type = TrackType::kAudioPCM;
  info.session_id = session_id;
  info.camera_id = camera_id_;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBlueToothA2DP);

  ret = audio_a2dp_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_a2dp_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderTest::CreateAudioPCMA2DPTrack() {
  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_pcm_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.track_id   = 101;
  info.track_type = TrackType::kAudioPCM;
  info.session_id = session_id;
  info.camera_id = camera_id_;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_pcm_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_pcm_track);

  TestTrack *audio_a2dp_track = new TestTrack(this);
  info.track_id   = 102;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBlueToothA2DP);

  ret = audio_a2dp_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_a2dp_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderTest::CreateAudioAACTrack() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_aac_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.track_id   = 101;
  info.track_type = TrackType::kAudioAAC;
  info.session_id = session_id;
  info.camera_id = camera_id_;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_aac_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_aac_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderTest::CreateAudio2AACTrack() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_aac_track1 = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.track_id   = 101;
  info.track_type = TrackType::kAudioAAC;
  info.session_id = session_id;
  info.camera_id = camera_id_;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_aac_track1->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_aac_track1);

  TestTrack *audio_aac_track2 = new TestTrack(this);
  info.track_id   = 102;

  ret = audio_aac_track2->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_aac_track2);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderTest::CreateAudioPCMAACTrack() {
  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_pcm_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.track_id   = 101;
  info.track_type = TrackType::kAudioPCM;
  info.session_id = session_id;
  info.camera_id = camera_id_;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_pcm_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_pcm_track);

  TestTrack *audio_aac_track = new TestTrack(this);
  info.track_id   = 102;
  info.track_type = TrackType::kAudioAAC;

  ret = audio_aac_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_aac_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderTest::CreateAudioAMRTrack() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_amr_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.track_id   = 101;
  info.track_type = TrackType::kAudioAMR;
  info.session_id = session_id;
  info.camera_id = camera_id_;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_amr_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_amr_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderTest::CreateAudio2AMRTrack() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_amr_track1 = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.track_id   = 101;
  info.track_type = TrackType::kAudioAMR;
  info.session_id = session_id;
  info.camera_id = camera_id_;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_amr_track1->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_amr_track1);

  TestTrack *audio_amr_track2 = new TestTrack(this);
  info.track_id   = 102;

  ret = audio_amr_track2->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_amr_track2);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderTest::CreateAudioPCMAMRTrack() {
  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_pcm_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.track_id   = 101;
  info.track_type = TrackType::kAudioPCM;
  info.session_id = session_id;
  info.camera_id = camera_id_;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_pcm_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_pcm_track);

  TestTrack *audio_amr_track = new TestTrack(this);
  info.track_id   = 102;
  info.track_type = TrackType::kAudioAMR;

  ret = audio_amr_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_amr_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderTest::CreateAudioG711Track() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_g711_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.track_id   = 101;
  info.track_type = TrackType::kAudioG711;
  info.session_id = session_id;
  info.camera_id = camera_id_;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_g711_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_g711_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderTest::CreateAudio2G711Track() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_g711_track1 = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.track_id   = 101;
  info.track_type = TrackType::kAudioG711;
  info.session_id = session_id;
  info.camera_id = camera_id_;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_g711_track1->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_g711_track1);

  TestTrack *audio_g711_track2 = new TestTrack(this);
  info.track_id   = 102;

  ret = audio_g711_track2->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_g711_track2);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderTest::CreateAudioPCMG711Track() {
  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *audio_pcm_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.track_id   = 101;
  info.track_type = TrackType::kAudioPCM;
  info.session_id = session_id;
  info.camera_id = camera_id_;
  info.device_id = static_cast<DeviceId>(AudioDeviceId::kBuiltIn);

  ret = audio_pcm_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_pcm_track);

  TestTrack *audio_g711_track = new TestTrack(this);
  info.track_id   = 102;
  info.track_type = TrackType::kAudioG711;

  ret = audio_g711_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(audio_g711_track);
  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

// This session has one RDI track with sensor resolution.
status_t RecorderTest::SessionRDITrack() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id, raw_width = 0, raw_height = 0;

  CameraMetadata meta;
  camera_metadata_entry_t entry;
  recorder_.GetDefaultCaptureParam(camera_id_, meta);

  if (meta.exists(ANDROID_SCALER_AVAILABLE_RAW_SIZES)) {
    entry = meta.find(ANDROID_SCALER_AVAILABLE_RAW_SIZES);
    for (uint32_t i = 0 ; i < entry.count; i += 2) {
      raw_width = entry.data.i32[i+0];
      raw_height = entry.data.i32[i+1];
      break;
    }
  }
  assert(raw_width != 0 && raw_height != 0);

  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  TestTrack *rdi_track = new TestTrack(this);
  TrackInfo info;
  memset(&info, 0x0, sizeof info);
  info.width      = raw_width;
  info.height     = raw_height;
  info.track_id   = 1;
  info.track_type = TrackType::kVideoRDI;
  info.session_id = session_id;
  info.camera_id = camera_id_;
  info.low_power_mode = false;

  ret = rdi_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(rdi_track);

  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

// This session has 1080P YUV video track with Display Enabled.
status_t RecorderTest::Session1080pYUVTrackWithDisplay() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;
  TrackInfo info;

  TestTrack *yuv_1080p_track = new TestTrack(this);
  memset(&info, 0x0, sizeof info);
  info.width      = 1920;
  info.height     = 1080;
  info.track_id   = 1;
  info.track_type = TrackType::kVideoYUV;
  info.session_id = session_id;
  info.fps = 30;

  ret = yuv_1080p_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(yuv_1080p_track);

  sessions_.insert(std::make_pair(session_id, tracks));

  use_display = 1;
  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

// 1080P YUV video track with Display Enabled in recorder service.
status_t RecorderTest::Session1080pYUVTrackWithPreview() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  auto ret = recorder_.CreateSession(session_status_cb, &session_id);
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;
  TrackInfo info;

  TestTrack *yuv_1080p_track = new TestTrack(this);
  memset(&info, 0x0, sizeof info);
  info.width      = 1920;
  info.height     = 1080;
  info.track_id   = 1;
  info.track_type = TrackType::kVideoPreview;
  info.session_id = session_id;

  ret = yuv_1080p_track->SetUp(info);
  assert(ret == 0);
  tracks.push_back(yuv_1080p_track);

  sessions_.insert(std::make_pair(session_id, tracks));

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderTest::StartSession() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  session_iter_ it = sessions_.begin();

  // Prepare tracks: setup files to dump track data, event etc.
  for (auto track : it->second) {
    track->Prepare();
    TrackType type = track->GetTrackType();
    if ( (type == TrackType::kVideoYUV)
        || (type == TrackType::kVideoRDI)
        || (type == TrackType::kVideoAVC)
        || (type == TrackType::kVideoHEVC)
        || (type == TrackType::kVideoPreview) ) {
      session_enabled_ = true;
      if (use_display == 1) {
        auto ret = track->StartDisplay(DisplayType::kPrimary);
        if(ret != 0) {
          ALOGE("%s:%s StartDisplay Failed!!", TAG, __func__);
        }
      }
    }
  }
  uint32_t session_id = it->first;
  auto result = recorder_.StartSession(session_id);
  assert(result == NO_ERROR);

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  return NO_ERROR;
}

status_t RecorderTest::StopSession() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  session_iter_ it = sessions_.begin();

  uint32_t session_id = it->first;
  auto result = recorder_.StopSession(session_id, true /*flush buffers*/);
  assert(result == NO_ERROR);

  for (auto track : it->second) {
    if (use_display == 1) {
      auto ret = track->StopDisplay(DisplayType::kPrimary);
      if(ret != 0) {
        ALOGE("%s:%s StopDisplay Failed!!", TAG, __func__);
      }
    }
    track->CleanUp();
    TrackType type = track->GetTrackType();
    if ( (type == TrackType::kVideoYUV)
        || (type == TrackType::kVideoRDI)
        || (type == TrackType::kVideoAVC)
        || (type == TrackType::kVideoHEVC)
        || (type == TrackType::kVideoPreview) ) {
      session_enabled_ = false;
    }
  }
  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return NO_ERROR;
}

status_t RecorderTest::PauseSession() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  session_iter_ it = sessions_.begin();
  uint32_t session_id = it->first;
  auto ret = recorder_.PauseSession(session_id);
  assert(ret == 0);
  TEST_INFO("%s:%s: Exit", TAG, __func__);

  return NO_ERROR;
}

status_t RecorderTest::ResumeSession() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  session_iter_ it = sessions_.begin();
  uint32_t session_id = it->first;
  auto ret = recorder_.ResumeSession(session_id);
  assert(ret == 0);
  TEST_INFO("%s:%s: Exit", TAG, __func__);

  return NO_ERROR;
}

status_t RecorderTest::SetParams() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  int32_t ret = 0;
  session_iter_ it = sessions_.begin();
  uint32_t session_id = it->first;
  CodecParamType param_type;

  uint32_t input;
  uint32_t value;
  do {
    printf("\n");
    printf("****** Set Dynamic Codec Param *******\n" );
    printf("  1. bitrate \n" );
    printf("  2. framerate \n" );
    printf("  3. insert-idr \n" );
    printf("  4. idr interval \n" );
    printf("  5. ltr mark \n" );
    printf("  6. ltr use \n" );
    printf("  0. exit \n");
    printf("\n");
    printf("Enter set param option\n");
    scanf("%d", &input);

    switch(input) {
      case 0:
        break;
      case 1:
        printf("Enter bitrate value\n");
        scanf("%d", &value);
        param_type = CodecParamType::kBitRateType;
        ret = recorder_.SetVideoTrackParam(session_id, 1, param_type, &value,
                                              sizeof(value));
        break;
      case 2:
        printf("Enter fps value\n");
        scanf("%d", &value);
        param_type = CodecParamType::kFrameRateType;
        ret = recorder_.SetVideoTrackParam(session_id, 1, param_type, &value,
                                              sizeof(value));
        break;
      case 3:
        printf("Insert idr frame\n");
        param_type = CodecParamType::kInsertIDRType;
        ret = recorder_.SetVideoTrackParam(session_id, 1, param_type, &value,
                                              sizeof(value));
        break;
      case 4:
        printf("Enter number of P frame value\n");
        scanf("%d", &value);
        param_type = CodecParamType::kIDRIntervalType;
        VideoEncIdrInterval idr_interval;
        idr_interval.num_pframes = value;
        idr_interval.num_bframes = 0;
        idr_interval.idr_period = 0;
        ret = recorder_.SetVideoTrackParam(session_id, 1, param_type,
                                           &idr_interval, sizeof(idr_interval));
        break;
      case 5:
        printf("Enter ltr mark id value\n");
        scanf("%d", &value);
        param_type = CodecParamType::kMarkLtrType;
        ret = recorder_.SetVideoTrackParam(session_id, 1, param_type, &value,
                                              sizeof(value));
        break;
      case 6:
        printf("Enter ltr use id value\n");
        scanf("%d", &value);
        param_type = CodecParamType::kUseLtrType;
        VideoEncLtrUse ltr_use;
        ltr_use.id = value;
        ltr_use.frame = 5;
        ret = recorder_.SetVideoTrackParam(session_id, 1, param_type, &ltr_use,
                                              sizeof(ltr_use));
        break;
      default:
         printf("Wrong value entered(%d)\n", input);
         input = 0;
    }
    if(input) {
      assert(ret == 0);
    }
  } while(input);

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderTest::EnableOverlay() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  int32_t ret = 0;
  // Enable overlay on all existing video tracks.
  session_iter_ it = sessions_.begin();
  for (auto track : it->second) {
    TrackType type = track->GetTrackType();
    if ( (type == TrackType::kVideoYUV)
        || (type == TrackType::kVideoAVC)
        || (type == TrackType::kVideoHEVC)
        || (type == TrackType::kVideoPreview) ) {
      track->EnableOverlay();
      assert(ret == 0);
    }
  }
  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t RecorderTest::DisableOverlay() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  session_iter_ it = sessions_.begin();
  for (auto track : it->second) {
    TrackType type = track->GetTrackType();
    if ( (type == TrackType::kVideoYUV)
        || (type == TrackType::kVideoAVC)
        || (type == TrackType::kVideoHEVC)
        || (type == TrackType::kVideoPreview) ) {
      track->DisableOverlay();
    }
  }
  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return NO_ERROR;
}

status_t RecorderTest::DeleteSession() {

  TEST_INFO("%s:%s: Enter", TAG, __func__);
  session_iter_ it = sessions_.begin();
  uint32_t session_id = it->first;
  // Delete all the tracks associated to session.
  status_t ret;
  for (auto track : it->second) {
      assert(track != nullptr);
      if (track->GetTrackType() == TrackType::kAudioPCM ||
          track->GetTrackType() == TrackType::kAudioAAC ||
          track->GetTrackType() == TrackType::kAudioAMR ||
          track->GetTrackType() == TrackType::kAudioG711) {
        ret = recorder_.DeleteAudioTrack(session_id, track->GetTrackId());
      } else {
        ret = recorder_.DeleteVideoTrack(session_id, track->GetTrackId());
      }
      assert(ret == 0);
      delete track;
      track = nullptr;
  }
  // Once all tracks are deleted successfully delete session.
  ret = recorder_.DeleteSession(session_id);
  sessions_.erase(it);
  use_display = 0;

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return 0;
}

void RecorderTest::SnapshotCb(uint32_t camera_id,
                              uint32_t image_sequence_count,
                              BufferDescriptor buffer, MetaData meta_data) {

  TEST_INFO("%s:%s Enter ", TAG, __func__);
  String8 file_path;
  const char* ext_str;

  if (meta_data.meta_flag  &
      static_cast<uint32_t>(MetaParamType::kCamBufMetaData)) {
    CameraBufferMetaData cam_buf_meta = meta_data.cam_buffer_meta_data;
    TEST_DBG("%s:%s: format(0x%x)", TAG, __func__, cam_buf_meta.format);
    TEST_DBG("%s:%s: num_planes=%d", TAG, __func__, cam_buf_meta.num_planes);
    for (uint8_t i = 0; i < cam_buf_meta.num_planes; ++i) {
      TEST_DBG("%s:%s: plane[%d]:stride(%d)", TAG, __func__, i,
          cam_buf_meta.plane_info[i].stride);
      TEST_DBG("%s:%s: plane[%d]:scanline(%d)", TAG, __func__, i,
          cam_buf_meta.plane_info[i].scanline);
      TEST_DBG("%s:%s: plane[%d]:width(%d)", TAG, __func__, i,
          cam_buf_meta.plane_info[i].width);
      TEST_DBG("%s:%s: plane[%d]:height(%d)", TAG, __func__, i,
          cam_buf_meta.plane_info[i].height);
    }

    switch (cam_buf_meta.format) {
      case BufferFormat::kNV12:
      ext_str = "nv12";
      break;
      case BufferFormat::kNV21:
      ext_str = "nv21";
      break;
      case BufferFormat::kBLOB:
      ext_str = "jpg";
      break;
      case BufferFormat::kRAW10:
      ext_str = "raw10";
      break;
      case BufferFormat::kRAW16:
      ext_str = "raw16";
      break;
      default:
      assert(0);
      break;
    }
    file_path.appendFormat("/data/snapshot_%u.%s", image_sequence_count,
        ext_str);
    DumpFrameToFile(buffer, cam_buf_meta, file_path);
  }
  // Return buffer back to recorder service.
  recorder_.ReturnImageCaptureBuffer(camera_id, buffer);

  TEST_INFO("%s:%s Exit", TAG, __func__);
}

void RecorderTest::RecorderCallbackHandler(EventType event_type,
                                           void *event_data,
                                           size_t event_data_size) {
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

void RecorderTest::SessionCallbackHandler(EventType event_type,
                                          void *event_data,
                                          size_t event_data_size) {
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  TEST_INFO("%s:%s: Exit", TAG, __func__);
}

// This function dumps YUV, JPEG and RAW frames to file.
status_t RecorderTest::DumpFrameToFile(BufferDescriptor& buffer,
                                       CameraBufferMetaData& meta_data,
                                       String8& file_path) {
  size_t written_len = 0;
  FILE *file = fopen(file_path.string(), "w+");
  if (!file) {
    ALOGE("%s:%s: Unable to open file(%s)", TAG, __func__,
        file_path.string());
    return -1;
  }
  // JPEG, RAW & NV12UBWC
  if ( (meta_data.format == BufferFormat::kBLOB)
      || (meta_data.format == BufferFormat::kRAW10)
      || (meta_data.format == BufferFormat::kNV12UBWC) ){
    written_len = fwrite(buffer.data, sizeof(uint8_t), buffer.size, file);
  } else {
    // YUV
    uint32_t offset = 0;
    for (uint32_t i = 0; i < meta_data.num_planes; ++i) {
      void* data = static_cast<void*>((static_cast<uint8_t*>(buffer.data)
                      + offset));
      written_len += fwrite(data, sizeof(uint8_t),
          meta_data.plane_info[i].width *
          meta_data.plane_info[i].height, file);
      TEST_DBG("%s:%s: plane(%d) written_len = %d", TAG, __func__, i,
          written_len);
      offset += meta_data.plane_info[i].stride
                    * meta_data.plane_info[i].scanline;
    }
  }
  TEST_DBG("%s:%s: total written_len = %d", TAG, __func__, written_len);
  TEST_INFO("%s:%s: Buffer(0x%p) Size(%u) Stored@(%s)\n", TAG, __func__,
      buffer.data, written_len, file_path.string());

  return NO_ERROR;
}

int32_t RecorderTest::RunFromConfig(int32_t argc, char *argv[])
{
  ALOGD("%s: Enter ",__func__);

  int32_t ret;

  if(strcmp(argv[1], "-c")) {
    ALOGD("Usage: %s -c config.txt",argv[0]);
    return -1;
  }

  TestInitParams params;
  std::vector<TrackInfo> infos;
  ret = ParseConfig(argv[2], &params, &infos);
  if(ret != 0) {
    return ret;
  }
  printf("%s Connect - Start\n",__func__);

  // Connect - Start
  RecorderCb recorder_status_cb;
  recorder_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { RecorderCallbackHandler(event_type, event_data,
      event_data_size); };

  ret = recorder_.Connect(recorder_status_cb);

  if (NO_ERROR  != ret) {
    ALOGE("%s:%s Connect Failed!!", TAG, __func__);
    return ret;
  }
  // Connect - End

  printf("%s StartCamera\n",__func__);
  // StartCamera - Begin
  // TODO: this parameters to be configured from config file
  // once the proper lower layer support for zsl is added
  CameraStartParam camera_params;
  memset(&camera_params, 0x0, sizeof camera_params);
  camera_params.zsl_mode            = false;
  camera_params.zsl_queue_depth     = 10;
  camera_params.zsl_width           = 3840;
  camera_params.zsl_height          = 2160;
  camera_params.frame_rate          = params.camera_fps;
  camera_params.flags               = 0x0;

  ret = recorder_.StartCamera(camera_id_, camera_params);
  if(ret != 0) {
    ALOGE("%s:%s StartCamera Failed!!", TAG, __func__);
    return ret;
  }

  ret = recorder_.GetDefaultCaptureParam(camera_id_, static_info_);
  if (NO_ERROR != ret) {
    ALOGE("%s:%s Unable to query default capture parameters!\n",
           TAG, __func__);
    return ret;
  }

  InitSupportedNRModes();
  InitSupportedVHDRModes();
  InitSupportedIRModes();

  // StartCamera - End
  printf("%s Create session and add track\n",__func__);

  // Create session and add track
  if (params.numStream != infos.size()) {
    ALOGE("%s:%s Number of streams and params provided not equal!!", TAG, __func__);
    return BAD_VALUE;
  }

  // Session for encoder tracks
  SessionCb session_status_cb;
  session_status_cb.event_cb = [&] ( EventType event_type, void *event_data,
      size_t event_data_size) { SessionCallbackHandler(event_type,
      event_data, event_data_size); };

  uint32_t session_id;
  ret = recorder_.CreateSession(session_status_cb, &session_id);
  if(ret != 0) {
    ALOGE("%s:%s CreateSession Failed!!", TAG, __func__);
    return ret;
  }
  TEST_INFO("%s:%s: sessions_id = %d", TAG, __func__, session_id);

  std::vector<TestTrack*> tracks;

  for(uint32_t i=1; i <= params.numStream; i++) {
    TestTrack *video_track = new TestTrack(this);
    TrackInfo track_info = infos[i-1];
    track_info.track_id = i;
    track_info.session_id = session_id;
    track_info.camera_id = camera_id_;
    ret = video_track->SetUp(track_info);
    assert(ret == 0);
    tracks.push_back(video_track);

    if(track_info.track_type == TrackType::kAudioAAC) {
       //Test audio AAC track
       //TODO: To be removed when support added in config file
       TestTrack *audio_aac_track = new TestTrack(this);
       TrackInfo info;
       memset(&info, 0x0, sizeof info);
       info.track_id   = 101;
       info.track_type = TrackType::kAudioAAC;
       info.session_id = session_id;
       info.camera_id = camera_id_;

       ret = audio_aac_track->SetUp(info);
       assert(ret == 0);
       tracks.push_back(audio_aac_track);
    }
  }

  printf("%s StartSession\n",__func__);

  // StartSession - Begin
  // Prepare tracks: setup files to dump track data, event etc.
  for (uint32_t i=0;i < tracks.size();i++) {
    tracks[i]->Prepare();
    TrackType type = tracks[i]->GetTrackType();
    if ( (type == TrackType::kVideoYUV)
      || (type == TrackType::kVideoRDI)
      || (type == TrackType::kVideoAVC)
      || (type == TrackType::kVideoHEVC)
      || (type == TrackType::kVideoPreview) ) {
      session_enabled_ = true;
    }
  }

  ret = recorder_.StartSession(session_id);
  assert(ret == NO_ERROR);
  // StartSession - End

  // Setting binning correction off by default
  if (GetCurrentBinningCorrectionMode() != FEATURE_NOT_AVAILABLE) {
    ret = SetBinningCorrectionMode(false);
    assert (ret == NO_ERROR);
  }

  // TNR & SHDR - Start
  CameraMetadata meta;
  auto status = recorder_.GetCameraParam(camera_id_, meta);
  if (NO_ERROR == status) {
    if (meta.exists(ANDROID_NOISE_REDUCTION_MODE)) {
      if (params.tnr) {
        const uint8_t tnrMode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
        ALOGI("%s:%s Selecting TNR mode to %s \n",
          TAG, __func__,"High quality");
        meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnrMode, 1);
      } else {
        const uint8_t tnrMode = ANDROID_NOISE_REDUCTION_MODE_OFF;
        ALOGI("%s:%s Selecting TNR mode to %s \n",TAG, __func__,"Off");
        meta.update(ANDROID_NOISE_REDUCTION_MODE, &tnrMode, 1);
      }
      status = recorder_.SetCameraParam(camera_id_, meta);
      if (NO_ERROR != status) {
        ALOGE("%s:%s Failed to apply: TNR/VHDR\n",TAG, __func__);
        return status;
      }
    }
  }
  // TODO: This value is still under discussion and verification
  PARAMETER_SETTLE_INTERVAL(2);

  status = recorder_.GetCameraParam(camera_id_, meta);
  if (NO_ERROR == status) {
    if (meta.exists(QCAMERA3_VIDEO_HDR_MODE)) {
      if (params.vhdr) {
        const int32_t vhdrMode = QCAMERA3_VIDEO_HDR_MODE_ON;
        ALOGI("%s:%s Selecting sHDR mode to %s \n",TAG, __func__,"On");
        meta.update(QCAMERA3_VIDEO_HDR_MODE, &vhdrMode, 1);
      } else {
        const int32_t vhdrMode = QCAMERA3_VIDEO_HDR_MODE_OFF;
        ALOGI("%s:%s Selecting sHDR mode to %s \n",TAG, __func__,"Off");
        meta.update(QCAMERA3_VIDEO_HDR_MODE, &vhdrMode, 1);
      }
      status = recorder_.SetCameraParam(camera_id_, meta);
      if (NO_ERROR != status) {
        ALOGE("%s:%s Failed to apply: TNR/VHDR\n",TAG, __func__);
        return status;
      }
    } else {
      //In case camera didn't set default turn on HDR if user requested
      if ((!supported_hdr_modes_.empty()) && (params.vhdr)) {
        const int32_t vhdrMode = QCAMERA3_VIDEO_HDR_MODE_ON;
        ALOGI("%s:%s Selecting sHDR mode to %s \n",TAG, __func__,"On");
        meta.update(QCAMERA3_VIDEO_HDR_MODE, &vhdrMode, 1);
        status = recorder_.SetCameraParam(camera_id_, meta);
        if (NO_ERROR != status) {
          ALOGE("%s:%s Failed to apply SHDR\n", TAG, __func__);
          return status;
        }
      }
    }
    // TODO: This value is still under discussion and verification
    PARAMETER_SETTLE_INTERVAL(2);

  }
  // TNR/SHDR - End

  if (params.binning_correct) {
    ret = SetBinningCorrectionMode(true);
    assert (ret == NO_ERROR);
  }

  if (params.af_mode != AfMode::kNone) {
     printf("%s toggle auto focus mode\n",__func__);
     status = ToggleAFMode(params.af_mode);
     if (NO_ERROR != status) {
       TEST_ERROR("failed to toggle focus mode, status = %d\n", status);
       if (BAD_VALUE == status) {
         return BAD_VALUE;
       }
     }
  }

  if (params.snapshot_info.type != SnapshotType::kNone) {

     printf("%s waiting for 2 seconds for exposure to settle\n",__func__);
     // sleep required to settle the exposure before taking snapshot.
     // This app does not provide interactive feedback to user
     // about the exposure
     PARAMETER_SETTLE_INTERVAL(AEC_SETTLE_INTERVAL);
     TEST_INFO("start to take snapshot\n");
     ret = TakeSnapshotWithConfig(params.snapshot_info);
     if (NO_ERROR != ret) {
        printf("failed to take snapshot with config\n");
        assert(ret == NO_ERROR);
     }
  }

  printf("%s Keep recording for %ds time\n",__func__,params.recordTime);
  // Keep recording for the given time
  sleep(params.recordTime);

  printf("%s StopSession\n",__func__);

  // StopSession - Begin
  ret = recorder_.StopSession(session_id, true /*flush buffers*/);
  assert(ret == NO_ERROR);

  for (uint32_t i=0;i < tracks.size();i++) {
    tracks[i]->CleanUp();
    TrackType type = tracks[i]->GetTrackType();
    if ( (type == TrackType::kVideoYUV)
         || (type == TrackType::kVideoRDI)
         || (type == TrackType::kVideoAVC)
         || (type == TrackType::kVideoHEVC)
         || (type == TrackType::kVideoPreview) ) {
      session_enabled_ = false;
    }
  }
  // StopSession - End
  printf("%s DeleteSession\n",__func__);

  // DeleteSession - Begin
  // Delete all the tracks associated to session.
  for (uint32_t i=0;i < tracks.size();i++) {
    if (tracks[i]->GetTrackType() == TrackType::kAudioPCM ||
          tracks[i]->GetTrackType() == TrackType::kAudioAAC ||
          tracks[i]->GetTrackType() == TrackType::kAudioAMR ||
          tracks[i]->GetTrackType() == TrackType::kAudioG711) {
      ret = recorder_.DeleteAudioTrack(session_id, tracks[i]->GetTrackId());
    } else {
      ret = recorder_.DeleteVideoTrack(session_id, tracks[i]->GetTrackId());
    }
    assert(ret == 0);
    delete tracks[i];
    tracks[i] = nullptr;
  }
  // Once all tracks are deleted successfully delete session.
  ret = recorder_.DeleteSession(session_id);

  // DeleteSession - End
  printf("%s StopCamera\n",__func__);

  // StopCamera - Begin
  ret = recorder_.StopCamera(camera_id_);
  if(ret != 0) {
    ALOGE("%s:%s StopCamera Failed!!", TAG, __func__);
    return ret;
  }
  static_info_.clear();
  // StopCamera - End
  printf("%s Disconnect\n",__func__);

  // Disconnect - Begin
  ret = recorder_.Disconnect();
  if(ret != 0) {
    ALOGE("%s:%s StopCamera Failed!!", TAG, __func__);
    return ret;
  }
  // Disconnect - End

  ALOGD("%s: Exit ",__func__);
  return ret;
}

void RecorderTest::printInitParameterAndTtrackInfo(const TestInitParams&
                                 initParams,const TrackInfo& track_info) {
  printf("\ninitParams.camera_id = %d\n", initParams.camera_id);
  printf("\ninitParams.camera_fps = %d\n", initParams.camera_fps);
  printf("initParams.numStream = %d\n", initParams.numStream);
  printf("initParams.snapshot_info.type = %d\n",
          initParams.snapshot_info.type);
  printf("initParams.snapshot_info.width = %d\n",
          initParams.snapshot_info.width);
  printf("initParams.snapshot_info.height = %d\n",
          initParams.snapshot_info.height);
  printf("initParams.snapshot_info.count = %d\n",
          initParams.snapshot_info.count);
  printf("initParams.af_mode = %d\n", initParams.af_mode);
  printf("TrackInfo.track_type = %d\n", track_info.track_type);
  printf("TrackInfo.camera_id = %d\n", track_info.camera_id);
  printf("TrackInfo.fps = %d\n", track_info.fps);
  printf("TrackInfo.width = %d\n", track_info.width);
  printf("TrackInfo.height = %d\n\n", track_info.height);
}

int32_t RecorderTest::ParseConfig(char *fileName, TestInitParams* initParams,
                                  std::vector<TrackInfo>* infos) {
  FILE *fp;
  TrackInfo track_info;
  memset(&track_info, 0x0, sizeof(track_info));
  bool isStreamReadCompleted = false;
  const int MAX_LINE = 128;
  char line[MAX_LINE];
  char value[50];
  char key[25];
  uint32_t id = 0;

  if(!(fp = fopen(fileName,"r"))) {
    ALOGE("failed to open config file: %s", fileName);
    return -1;
  }

  while(fgets(line,MAX_LINE-1,fp)) {
    if((line[0] == '\n') || (line[0] == '/') || line[0] == ' ')
      continue;
    strtok(line, "\n");
    memset(value, 0x0, sizeof(value));
    memset(key, 0x0, sizeof(key));
    if(isStreamReadCompleted) {
      memset(&track_info, 0x0, sizeof(track_info));
      isStreamReadCompleted = false;
    }
    int len = strlen(line);
    int i,j = 0;

    //This assumes new stream params always start with #
    if(!strcspn(line,"#")) {
      id++;
      continue;
     }


    if((id > 0) && (id > initParams->numStream)) {
      break;
    }

    int pos = strcspn(line,":");
    for(i = 0; i< pos; i++){
      if(line[i] != ' ') {
        key[j] = line[i];
        j++;
      }
    }

    key[j] = '\0';
    j = 0;
    for(i = pos+1; i< len; i++) {
      if(line[i] != ' ') {
        value[j] = line[i];
        j++;
      }
    }
    value[j] = '\0';

    if(!strncmp("CameraID", key, strlen("CameraID"))) {
      initParams->camera_id = atoi(value);
    } else if(!strncmp("CameraFPS", key, strlen("CameraFPS"))) {
      initParams->camera_fps = atoi(value);
    } else if(!strncmp("SnapshotType", key, strlen("SnapshotType"))) {
      if(!strncmp("None", value, strlen("None"))) {
        initParams->snapshot_info.type= SnapshotType::kNone;
      } else if(!strncmp("JPEG", value, strlen("JPEG"))) {
        initParams->snapshot_info.type= SnapshotType::kJpeg;
      } else if(!strncmp("RAWYUV", value, strlen("RAWYUV"))) {
        initParams->snapshot_info.type= SnapshotType::kRawYuv;
      } else if(!strncmp("RAWRDI", value, strlen("RAWRDI"))) {
        initParams->snapshot_info.type= SnapshotType::kRawRdi;
      } else {
        ALOGE("%s: Unknown SnapshotType(%s)", __func__, value);
        goto READ_FAILED;
      }
    } else if(!strncmp("SnapshotWidth", key, strlen("SnapshotWidth"))) {
      initParams->snapshot_info.width = atoi(value);
    } else if(!strncmp("SnapshotHeight", key, strlen("SnapshotHeight"))) {
      initParams->snapshot_info.height = atoi(value);
    } else if(!strncmp("SnapshotCount", key, strlen("SnapshotCount"))) {
      initParams->snapshot_info.count = atoi(value);
    } else if(!strncmp("AFMode", key, strlen("AFMode"))) {
      if(!strncmp("None", value, strlen("None"))) {
        initParams->af_mode = AfMode::kNone;
      } else if(!strncmp("Off", value, strlen("Off"))) {
        initParams->af_mode = AfMode::kOff;
      } else if(!strncmp("AUTO", value, strlen("AUTO"))) {
        initParams->af_mode = AfMode::kAuto;
      } else if(!strncmp("MACRO", value, strlen("MACRO"))) {
        initParams->af_mode = AfMode::kMacro;
      } else if(!strncmp("CVAF", value, strlen("CVAF"))) {
        initParams->af_mode = AfMode::kContinousVideo;
      } else if(!strncmp("CPAF", value, strlen("CPAF"))) {
        initParams->af_mode = AfMode::kContinuousPicture;
      } else {
        ALOGE("%s: Unknown AFMode(%s)", __func__, value);
        goto READ_FAILED;
      }
    } else if(!strncmp("RecordingTime", key, strlen("RecordingTime"))) {
      initParams->recordTime = atoi(value);
    } else if(!strncmp("NumStream", key, strlen("NumStream"))) {
      if(atoi(value) <= 0) {
        ALOGE ("%s Number of stream can not be %d", __func__,
                atoi (value));
        goto READ_FAILED;
      }
      initParams->numStream = atoi(value);
    } else if(!strncmp("VHDR", key, strlen("VHDR"))) {
      initParams->vhdr = atoi(value)?true:false;
    } else if(!strncmp("TNR", key, strlen("TNR"))) {
      initParams->tnr = atoi(value)?true:false;
    } else if(!strncmp("BinningCorrect", key, strlen("BinningCorrect"))) {
      initParams->binning_correct = atoi(value)?true:false;
    } else if(!strncmp("Width", key, strlen("Width"))) {
      track_info.width = atoi(value);
    } else if(!strncmp("Height", key, strlen("Height"))) {
      track_info.height = atoi(value);
    } else if(!strncmp("FPS", key, strlen("FPS"))) {
      track_info.fps = atoi(value);
    } else if(!strncmp("Bitrate", key, strlen("Bitrate"))) {
      track_info.bitrate = atoi(value);
    } else if(!strncmp("TrackType", key, strlen("TrackType"))) {
      if(!strncmp("AVC", value, strlen("AVC"))) {
        track_info.track_type = TrackType::kVideoAVC;
      } else if(!strncmp("HEVC", value, strlen("HEVC"))) {
        track_info.track_type = TrackType::kVideoHEVC;
      } else if(!strncmp("YUV", value, strlen("YUV"))) {
        track_info.track_type = TrackType::kVideoYUV;
      } else if(!strncmp("RAW", value, strlen("RAW"))) {
        track_info.track_type = TrackType::kVideoRDI;
      } else if(!strncmp("Preview", value, strlen("Preview"))) {
        track_info.track_type = TrackType::kVideoPreview;
      } else {
        ALOGE("%s: Unknown Video CodecType(%s)", __func__, value);
        goto READ_FAILED;
      }
    } else if(!strncmp("CamLowPowerMode", key, strlen("CamLowPowerMode"))) {
      track_info.low_power_mode = atoi(value) ? true : false;
      isStreamReadCompleted = true;
    } else {
      ALOGE("Unknown Key %s found in %s", key, fileName);
      goto READ_FAILED;
    }
    if (isStreamReadCompleted) {
      camera_id_ = initParams->camera_id;
      track_info.camera_id = initParams->camera_id;
      infos->push_back(track_info);
      printInitParameterAndTtrackInfo(*initParams,track_info);
    }
  }

  if (initParams->numStream > infos->size()) {
    ALOGE("%s: Insufficient stream parameter for total stream count(%d/%d)",
           __func__, infos->size(), initParams->numStream);
    goto READ_FAILED;
  }

  fclose(fp);
  return 0;
READ_FAILED:
  fclose(fp);
  return -1;
}

int32_t RecorderTest::RunAutoMode() {
  ALOGD("%s: Enter ",__func__);

  auto ret = Connect();
  if (NO_ERROR  != ret) {
    ALOGE("%s:%s Connect Failed!!", TAG, __func__);
    return ret;
  }

  ret = StartCamera();
  if (NO_ERROR  != ret) {
    ALOGE("%s:%s StartCamera Failed!!", TAG, __func__);
    return ret;
  }

  ret = Session4KEncTrack(TrackType::kVideoAVC);
  if (NO_ERROR  != ret) {
    ALOGE("%s:%s Session4KEncTrack Failed!!", TAG, __func__);
    return ret;
  }

  ret = StartSession();
  if (NO_ERROR  != ret) {
      ALOGE("%s:%s Session4KEncTrack Failed!!", TAG, __func__);
      return ret;
  }

  // Record video for 5 sec
  sleep(5);

  ret = StopSession();
  if (NO_ERROR  != ret) {
      ALOGE("%s:%s StopSession Failed!!", TAG, __func__);
      return ret;
  }

  ret = DeleteSession();
  if (NO_ERROR  != ret) {
      ALOGE("%s:%s DeleteSession Failed!!", TAG, __func__);
      return ret;
  }

  ret = StopCamera();
  if (NO_ERROR  != ret) {
      ALOGE("%s:%s StopCamera Failed!!", TAG, __func__);
      return ret;
  }

  ret = Disconnect();
  if (NO_ERROR  != ret) {
      ALOGE("%s:%s Disconnect Failed!!", TAG, __func__);
      return ret;
  }

  ALOGD("%s: Exit ",__func__);
  return ret;
}

TestTrack::TestTrack(RecorderTest* recorder_test)
    : file_fd_(-1), recorder_test_(recorder_test), num_yuv_frames_(0),
      display_started_(0) {
  TEST_DBG("%s:%s: Enter", TAG, __func__);
  memset(&track_info_, 0x0, sizeof track_info_);
  TEST_DBG("%s:%s: Exit", TAG, __func__);
}

TestTrack::~TestTrack() {
  TEST_DBG("%s:%s: Enter", TAG, __func__);
  if (file_fd_ > 0) {
    close(file_fd_);
  }
  TEST_DBG("%s:%s: Exit", TAG, __func__);
}

status_t TestTrack::SetUp(TrackInfo& track_info) {

  TEST_DBG("%s:%s: Enter", TAG, __func__);
  int32_t ret = NO_ERROR;
  assert(recorder_test_ != nullptr);

  if ( (track_info.track_type == TrackType::kVideoAVC)
      || (track_info.track_type == TrackType::kVideoHEVC)
      || (track_info.track_type == TrackType::kVideoRDI)
      || (track_info.track_type == TrackType::kVideoYUV)
      || (track_info.track_type == TrackType::kVideoPreview) ) {
    uint32_t fps = track_info.fps;
    uint32_t bitrate = track_info.bitrate;
    // Create Video Track.
    VideoTrackCreateParam video_track_param;
    memset(&video_track_param, 0x0, sizeof video_track_param);
    video_track_param.camera_id   = track_info.camera_id;
    video_track_param.width       = track_info.width;
    video_track_param.height      = track_info.height;

    if (fps != 0)
      video_track_param.frame_rate  = fps;
    else
      video_track_param.frame_rate  = 30;
    if(track_info.track_type == TrackType::kVideoPreview)
      video_track_param.out_device  = 0x02;
    else
      video_track_param.out_device  = 0x01;
    video_track_param.low_power_mode  = track_info.low_power_mode;

    switch (track_info.track_type) {
      case TrackType::kVideoAVC:
      video_track_param.format_type = VideoFormat::kAVC;
      video_track_param.codec_param.avc.idr_interval = 1;
      if(bitrate != 0)
        video_track_param.codec_param.avc.bitrate      = bitrate;
      else
        video_track_param.codec_param.avc.bitrate      = 10000000;
      video_track_param.codec_param.avc.profile = AVCProfileType::kHigh;
      video_track_param.codec_param.avc.level   = AVCLevelType::kLevel3;
      video_track_param.codec_param.avc.ratecontrol_type =
          VideoRateControlType::kMaxBitrate;
      video_track_param.codec_param.avc.qp_params.enable_init_qp = true;
      video_track_param.codec_param.avc.qp_params.init_qp.init_IQP = 27;
      video_track_param.codec_param.avc.qp_params.init_qp.init_PQP = 28;
      video_track_param.codec_param.avc.qp_params.init_qp.init_BQP = 28;
      video_track_param.codec_param.avc.qp_params.init_qp.init_QP_mode = 0x7;
      video_track_param.codec_param.avc.qp_params.enable_qp_range = true;
      video_track_param.codec_param.avc.qp_params.qp_range.min_QP = 10;
      video_track_param.codec_param.avc.qp_params.qp_range.max_QP = 51;
      video_track_param.codec_param.avc.qp_params.enable_qp_IBP_range = true;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.min_IQP = 10;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.max_IQP = 51;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.min_PQP = 10;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.max_PQP = 51;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.min_BQP = 10;
      video_track_param.codec_param.avc.qp_params.qp_IBP_range.max_BQP = 51;
      video_track_param.codec_param.avc.ltr_count = 4;
      video_track_param.codec_param.avc.insert_aud_delimiter = true;
      break;
      case TrackType::kVideoHEVC:
      video_track_param.format_type = VideoFormat::kHEVC;
      video_track_param.codec_param.hevc.idr_interval = 1;
      if (bitrate != 0)
        video_track_param.codec_param.hevc.bitrate      = bitrate;
      else
        video_track_param.codec_param.hevc.bitrate      = 10000000;
      video_track_param.codec_param.hevc.profile = HEVCProfileType::kMain;
      video_track_param.codec_param.hevc.level   = HEVCLevelType::kLevel3;
      video_track_param.codec_param.hevc.ratecontrol_type =
          VideoRateControlType::kMaxBitrate;
      video_track_param.codec_param.hevc.qp_params.enable_init_qp = true;
      video_track_param.codec_param.hevc.qp_params.init_qp.init_IQP = 27;
      video_track_param.codec_param.hevc.qp_params.init_qp.init_PQP = 28;
      video_track_param.codec_param.hevc.qp_params.init_qp.init_BQP = 28;
      video_track_param.codec_param.hevc.qp_params.init_qp.init_QP_mode = 0x7;
      video_track_param.codec_param.hevc.qp_params.enable_qp_range = true;
      video_track_param.codec_param.hevc.qp_params.qp_range.min_QP = 10;
      video_track_param.codec_param.hevc.qp_params.qp_range.max_QP = 51;
      video_track_param.codec_param.hevc.qp_params.enable_qp_IBP_range = true;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.min_IQP = 10;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.max_IQP = 51;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.min_PQP = 10;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.max_PQP = 51;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.min_BQP = 10;
      video_track_param.codec_param.hevc.qp_params.qp_IBP_range.max_BQP = 51;
      video_track_param.codec_param.hevc.ltr_count = 4;
      break;
      case TrackType::kVideoYUV:
      case TrackType::kVideoPreview:
      video_track_param.format_type = VideoFormat::kYUV;
      break;
      case TrackType::kVideoRDI:
      video_track_param.format_type = VideoFormat::kBayerRDI;
      break;
      default:
      break;
    }

    TrackCb video_track_cb;
    video_track_cb.data_cb = [&] (uint32_t track_id,
        std::vector<BufferDescriptor> buffers, std::vector<MetaData>
        meta_buffers) { TrackDataCB(track_id, buffers, meta_buffers); };

    video_track_cb.event_cb = [&] (uint32_t track_id, EventType event_type,
        void *event_data, size_t data_size) { TrackEventCB(track_id,
        event_type, event_data, data_size); };

    ret = recorder_test_->GetRecorder().CreateVideoTrack(track_info.session_id,
              track_info.track_id, video_track_param, video_track_cb);
    assert(ret == 0);
  } else {
    // Create AudioTrack
    AudioTrackCreateParam audio_track_params;
    memset(&audio_track_params, 0x0, sizeof audio_track_params);
    audio_track_params.in_devices_num = 0;
    audio_track_params.in_devices[audio_track_params.in_devices_num++] =
        track_info.device_id;
    audio_track_params.sample_rate = 48000;
    audio_track_params.channels    = 1;
    audio_track_params.bit_depth   = 16;
    audio_track_params.out_device  = 0;
    audio_track_params.flags       = 0;

    switch (track_info.track_type) {
      case TrackType::kAudioPCM:
        audio_track_params.format = AudioFormat::kPCM;
        break;
      case TrackType::kAudioAAC:
        audio_track_params.format = AudioFormat::kAAC;
        audio_track_params.codec_params.aac.format = AACFormat::kADTS;
        audio_track_params.codec_params.aac.mode = AACMode::kAALC;
        break;
      case TrackType::kAudioAMR:
        audio_track_params.format = AudioFormat::kAMR;
        audio_track_params.codec_params.amr.isWAMR = false;
        audio_track_params.sample_rate = 8000;
        break;
      case TrackType::kAudioG711:
        audio_track_params.format = AudioFormat::kG711;
        audio_track_params.codec_params.g711.mode = G711Mode::kALaw;
        audio_track_params.sample_rate = 8000;
        break;
      default:
        assert(0);
        break;
    }
    TrackCb audio_track_cb;
    audio_track_cb.data_cb =
        [this] (uint32_t track_id, std::vector<BufferDescriptor> buffers,
                std::vector<MetaData> meta_buffers)
                -> void {
          TrackDataCB(track_id, buffers, meta_buffers);
        };

    audio_track_cb.event_cb =
        [this] (uint32_t track_id, EventType event_type, void *event_data,
                size_t event_data_size) -> void {
          TrackEventCB(track_id, event_type, event_data, event_data_size);
        };

    ret = recorder_test_->GetRecorder().CreateAudioTrack(track_info.session_id,
              track_info.track_id, audio_track_params, audio_track_cb);
    assert(ret == NO_ERROR);

    switch (track_info.track_type) {
      case TrackType::kAudioPCM:
      case TrackType::kAudioG711:
        // Configure .wav output.
        ret = wav_output_.Configure(kDefaultAudioFilenamePrefix,
                                    track_info.track_id, audio_track_params);
        assert(ret == NO_ERROR);
        break;
      case TrackType::kAudioAAC:
        // Configure .aac output.
        ret = aac_output_.Configure(kDefaultAudioFilenamePrefix,
                                    track_info.track_id, audio_track_params);
        assert(ret == NO_ERROR);
        break;
      case TrackType::kAudioAMR:
        // Configure .amr output.
        ret = amr_output_.Configure(kDefaultAudioFilenamePrefix,
                                    track_info.track_id, audio_track_params);
        assert(ret == NO_ERROR);
        break;
      default:
        assert(0);
        break;
    }
  }
  track_info_ = track_info;

  TEST_DBG("%s:%s: Exit", TAG, __func__);
  return ret;
}

// Set up file to dump track data.
status_t TestTrack::Prepare() {

  TEST_DBG("%s:%s: Enter", TAG, __func__);
  int32_t ret = NO_ERROR;
#ifdef DUMP_BITSTREAM
  if ( (track_info_.track_type == TrackType::kVideoAVC)
     || (track_info_.track_type == TrackType::kVideoHEVC) ) {
    String8 bitstream_filepath;
    const char* type_string = (track_info_.track_type == TrackType::kVideoAVC)
         ? "h264":"h265";
    String8 extn(type_string);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    bitstream_filepath.appendFormat("/data/track_%d_%dx%d_%lu.%s",
        track_info_.track_id, track_info_.width, track_info_.height,
        tv.tv_sec, extn.string());
    file_fd_ = open(bitstream_filepath.string(), O_CREAT | O_WRONLY | O_TRUNC,
        0655);
    assert(file_fd_ >= 0);
    TEST_INFO("%s:%s: file(%s) opened successfully!!", TAG, __func__,
        bitstream_filepath.string());
  }
#endif
  if (track_info_.track_type == TrackType::kAudioPCM ||
      track_info_.track_type == TrackType::kAudioG711) {
    ret = wav_output_.Open();
    assert(ret == NO_ERROR);
  } else if (track_info_.track_type == TrackType::kAudioAAC) {
    ret = aac_output_.Open();
    assert(ret == NO_ERROR);
  } else if (track_info_.track_type == TrackType::kAudioAMR) {
    ret = amr_output_.Open();
    assert(ret == NO_ERROR);
  }
  TEST_DBG("%s:%s: Exit", TAG, __func__);
  return ret;
}

// Clean up file.
status_t TestTrack::CleanUp() {

  TEST_DBG("%s:%s: Enter", TAG, __func__);
  int32_t ret = NO_ERROR;
  switch (track_info_.track_type) {
    case TrackType::kVideoAVC:
    case TrackType::kVideoHEVC:
#ifdef DUMP_BITSTREAM
    if(file_fd_ > 0) {
      close(file_fd_);
      file_fd_ = -1;
    }
#endif
    break;
    case TrackType::kAudioPCM:
    case TrackType::kAudioG711:
    wav_output_.Close();
    break;
    case TrackType::kAudioAAC:
    aac_output_.Close();
    break;
    case TrackType::kAudioAMR:
    amr_output_.Close();
    break;
    default:
    break;
  }
  TEST_DBG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t TestTrack::EnableOverlay() {

  TEST_DBG("%s:%s: Enter", TAG, __func__);
  int32_t ret = 0;
  OverlayParam object_params;
  // Create Static Image type overlay.
  memset(&object_params, 0x0, sizeof object_params);
  object_params.type = OverlayType::kStaticImage;
  object_params.location = OverlayLocationType::kBottomRight;
  std::string str("/etc/overlay_test.rgba");
  str.copy(object_params.image_info.image_location, str.length());
  object_params.image_info.width  = 451;
  object_params.image_info.height = 109;

  uint32_t object_id;
  assert(recorder_test_ != nullptr);
  ret = recorder_test_->GetRecorder().CreateOverlayObject(track_info_.track_id,
                                                          object_params,
                                                          &object_id);
  assert(ret == 0);

  ret = recorder_test_->GetRecorder().SetOverlay(track_info_.track_id,
                                                 object_id);
  assert(ret == 0);
  // One track can have multiple types of overlay.
  overlay_ids_.push_back(object_id);

  // Create Date & Time type overlay.
  memset(&object_params, 0x0, sizeof object_params);
  object_params.type = OverlayType::kDateType;
  object_params.location = OverlayLocationType::kBottomLeft;
  object_params.color    = 0x202020FF; //Dark Gray
  object_params.date_time.time_format = OverlayTimeFormatType::kHHMMSS_AMPM;
  object_params.date_time.date_format = OverlayDateFormatType::kMMDDYYYY;

  uint32_t date_time_id;
  ret = recorder_test_->GetRecorder().CreateOverlayObject(track_info_.track_id,
                                                          object_params,
                                                          &date_time_id);
  assert(ret == 0);

  ret = recorder_test_->GetRecorder().SetOverlay(track_info_.track_id,
                                                 date_time_id);
  assert(ret == 0);
  // One track can have multiple types of overlay.
  overlay_ids_.push_back(date_time_id);

  // Create BoundingBox type overlay.
  memset(&object_params, 0x0, sizeof object_params);
  object_params.type  = OverlayType::kBoundingBox;
  object_params.color = 0x33CC00FF; //Light Green
  // Dummy coordinates for test purpose.
  object_params.bounding_box.start_x = 100;
  object_params.bounding_box.start_y = 200;
  object_params.bounding_box.width   = 1900;
  object_params.bounding_box.height  = 200;
  std::string bb_text("Test BBox..");
  bb_text.copy(object_params.bounding_box.box_name, bb_text.length());

  uint32_t bbox_id;
  ret = recorder_test_->GetRecorder().CreateOverlayObject(track_info_.track_id,
                                                          object_params,
                                                          &bbox_id);
  assert(ret == 0);
  ret = recorder_test_->GetRecorder().SetOverlay(track_info_.track_id, bbox_id);
  assert(ret == 0);
  overlay_ids_.push_back(bbox_id);

  // Create UserText type overlay.
  memset(&object_params, 0x0, sizeof object_params);
  object_params.type = OverlayType::kUserText;
  object_params.location = OverlayLocationType::kTopRight;
  object_params.color = 0x189BF2FF; //Light Blue
  std::string user_text("Simple User Text For Testing!!");
  user_text.copy(object_params.user_text, user_text.length());

  uint32_t user_text_id;
  ret = recorder_test_->GetRecorder().CreateOverlayObject(track_info_.track_id,
                                                          object_params,
                                                          &user_text_id);
  assert(ret == 0);
  ret = recorder_test_->GetRecorder().SetOverlay(track_info_.track_id,
                                                 user_text_id);
  assert(ret == 0);
  overlay_ids_.push_back(user_text_id);

  // Create PrivacyMask type overlay.
  memset(&object_params, 0x0, sizeof object_params);
  object_params.type = OverlayType::kPrivacyMask;
  object_params.color = 0xFF9933FF; //Fill mask with color.
  // Dummy coordinates for test purpose.
  object_params.bounding_box.start_x = 600;
  object_params.bounding_box.start_y = 200;
  object_params.bounding_box.width   = 1920/3;
  object_params.bounding_box.height  = 1080/3;

  uint32_t privacy_mask_id;
  ret = recorder_test_->GetRecorder().CreateOverlayObject(track_info_.track_id,
                                                          object_params,
                                                          &privacy_mask_id);
  assert(ret == 0);
  ret = recorder_test_->GetRecorder().SetOverlay(track_info_.track_id,
                                                 privacy_mask_id);
  assert(ret == 0);
  overlay_ids_.push_back(privacy_mask_id);
  TEST_DBG("%s:%s: Exit", TAG, __func__);
  return ret;
}

status_t TestTrack::DisableOverlay() {

  TEST_DBG("%s:%s: Enter", TAG, __func__);
  int32_t ret = 0;
  assert(recorder_test_ != nullptr);
  for (auto overlay_id : overlay_ids_) {
    ret = recorder_test_->GetRecorder().RemoveOverlay(GetTrackId(), overlay_id);
    assert(ret == 0);
    ret = recorder_test_->GetRecorder().DeleteOverlayObject(GetTrackId(),
                                                            overlay_id);
    assert(ret == 0);
  }
  overlay_ids_.clear();
  TEST_DBG("%s:%s: Exit", TAG, __func__);
  return ret;
}

void TestTrack::TrackEventCB(uint32_t track_id, EventType event_type,
                             void *event_data, size_t event_data_size) {

  TEST_DBG("%s:%s: Enter", TAG, __func__);
  TEST_DBG("%s:%s: Exit", TAG, __func__);
}

void TestTrack::TrackDataCB(uint32_t track_id, std::vector<BufferDescriptor>
                            buffers, std::vector<MetaData> meta_buffers) {

  TEST_DBG("%s:%s: Enter track_id(%dd)", TAG, __func__, track_id);
  assert (recorder_test_ != nullptr);
  int32_t ret = 0;

  switch (track_info_.track_type) {
    case TrackType::kAudioPCM:
    case TrackType::kAudioG711:
      for (const BufferDescriptor& buffer : buffers) {
        ret = wav_output_.Write(buffer);
        assert(ret == 0);
      }
    break;
    case TrackType::kAudioAAC:
      for (const BufferDescriptor& buffer : buffers) {
        if (buffer.flag & static_cast<uint32_t>(BufferFlags::kFlagEOS))
          break;
        ret = aac_output_.Write(buffer);
        assert(ret == 0);
      }
    break;
    case TrackType::kAudioAMR:
      for (const BufferDescriptor& buffer : buffers) {
        if (buffer.flag & static_cast<uint32_t>(BufferFlags::kFlagEOS))
          break;
        ret = amr_output_.Write(buffer);
        assert(ret == 0);
      }
    break;
    case TrackType::kVideoYUV:
    case TrackType::kVideoRDI:
      for (uint32_t i = 0; i < meta_buffers.size(); ++i) {
        MetaData meta_data = meta_buffers[i];
        if (meta_data.meta_flag &
            static_cast<uint32_t>(MetaParamType::kCamBufMetaData)) {
          CameraBufferMetaData cam_buf_meta = meta_data.cam_buffer_meta_data;
          TEST_DBG("%s:%s: format=%d", TAG, __func__, cam_buf_meta.format);
          TEST_DBG("%s:%s: num_planes=%d", TAG, __func__,
              cam_buf_meta.num_planes);
          for (uint8_t i = 0; i < cam_buf_meta.num_planes; ++i) {
            TEST_DBG("%s:%s: plane[%d]:stride(%d)", TAG, __func__, i,
                cam_buf_meta.plane_info[i].stride);
            TEST_DBG("%s:%s: plane[%d]:scanline(%d)", TAG, __func__, i,
                cam_buf_meta.plane_info[i].scanline);
            TEST_DBG("%s:%s: plane[%d]:width(%d)", TAG, __func__, i,
                cam_buf_meta.plane_info[i].width);
            TEST_DBG("%s:%s: plane[%d]:height(%d)", TAG, __func__, i,
                cam_buf_meta.plane_info[i].height);
          }
          #ifdef DUMP_YUV_FRAMES
          // Dump every 200th Frame.
          ++num_yuv_frames_;
          if (num_yuv_frames_ == 200) {
            const char *ext = track_info_.track_type ==  TrackType::kVideoRDI ?
                "raw" : "yuv";
            String8 file_path;
            file_path.appendFormat("/data/track_%d_%dx%d_%lld.%s",
                track_info_.track_id, cam_buf_meta.plane_info[0].width,
                cam_buf_meta.plane_info[0].height, buffers[i].timestamp, ext);
            recorder_test_->DumpFrameToFile(buffers[i], cam_buf_meta,
                                            file_path);
            num_yuv_frames_ = 0;
          }
          #endif
          PushFrameToDisplay(buffers[i], cam_buf_meta);
        }
      }
    break;
    case TrackType::kVideoAVC:
    case TrackType::kVideoHEVC:
      for (uint32_t i = 0; i < meta_buffers.size(); ++i) {
        MetaData meta_data = meta_buffers[i];
        if (meta_data.meta_flag &
            static_cast<uint32_t>(MetaParamType::kVideoFrameType)) {
          TEST_DBG("%s:%s: frame_type=%d", TAG, __func__,
                   meta_data.video_frame_type_info);
        }
      }
      #ifdef DUMP_BITSTREAM
      // Dump AVC/HEVC bitstream data
      DumpBitStream(buffers);
      #endif
    break;
    default:
    break;
  }
  // Return buffers back to service.
  ret = recorder_test_->GetRecorder().ReturnTrackBuffer(track_info_.session_id,
                                                        track_id, buffers);
  assert(ret == 0);
  TEST_DBG("%s:%s: Exit", TAG, __func__);
}

#ifdef DUMP_BITSTREAM
status_t TestTrack::DumpBitStream(std::vector<BufferDescriptor>& buffers) {

  TEST_DBG("%s:%s: Enter", TAG, __func__);
  for (auto& iter : buffers) {
    if (file_fd_ > 0) {
      uint32_t exp_size = iter.size;
      TEST_DBG("%s BitStream buffer data(0x%p):size(%d):ts(%lld):flag(0x%x)"
        ":buf_id(%d):capacity(%d)", __func__, iter.data, iter.size,
         iter.timestamp, iter.flag, iter.buf_id, iter.capacity);

      uint32_t written_length = write(file_fd_, iter.data, iter.size);
      TEST_DBG("%s: written_length(%d)", __func__, written_length);
      if (written_length != exp_size) {
        TEST_ERROR("%s:%s: Bad Write error (%d) %s", TAG, __func__, errno,
        strerror(errno));
      }
    } else {
      TEST_ERROR("%s:%s File is not open fd = %d", TAG, __func__, file_fd_);
      return -1;
    }
    if (iter.flag & static_cast<uint32_t>(BufferFlags::kFlagEOS)) {
      TEST_INFO("%s:%s EOS Last buffer!", TAG, __func__);
      close(file_fd_);
      file_fd_ = -1;
    }
  }
  TEST_DBG("%s:%s: Exit", TAG, __func__);
  return NO_ERROR;
}
#endif

void TestTrack::DisplayCallbackHandler(DisplayEventType event_type,
    void *event_data, size_t event_data_size) {
  TEST_DBG("%s:%s Enter ", TAG, __func__);
  TEST_DBG("%s:%s Exit ", TAG, __func__);
}

void TestTrack::DisplayVSyncHandler(int64_t time_stamp) {
  TEST_DBG("%s:%s: Enter", TAG, __func__);
  TEST_DBG("%s:%s: Exit", TAG, __func__);
}

status_t TestTrack::StartDisplay(DisplayType display_type) {
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  int32_t res = 0;
  SurfaceConfig surface_config;
  DisplayCb  display_status_cb;

  display_= new Display();
  assert(display_ != nullptr);

  res = display_->Connect();
  assert(res == 0);

  display_status_cb.EventCb = [&] ( DisplayEventType event_type,
      void *event_data, size_t event_data_size) { DisplayCallbackHandler
      (event_type, event_data, event_data_size); };

  display_status_cb.VSyncCb = [&] ( int64_t time_stamp)
      { DisplayVSyncHandler(time_stamp); };

  res = display_->CreateDisplay(display_type, display_status_cb);
  assert(res == 0);

  memset(&surface_config, 0x0, sizeof surface_config);

  surface_config.width = track_info_.width;
  surface_config.height = track_info_.height;
  surface_config.format = SurfaceFormat::kFormatYCbCr420SemiPlanarVenus;
  surface_config.buffer_count = 1;
  surface_config.cache = 0;
  surface_config.use_buffer = 1;
  res = display_->CreateSurface(surface_config, &surface_id_);
  assert(res == 0);

  display_started_ = 1;

  surface_param_.src_rect = { 0.0, 0.0, (float)track_info_.width,
      (float)track_info_.height };
  surface_param_.dst_rect = { 0.0, 0.0, (float)track_info_.width,
      (float)track_info_.height };
  surface_param_.surface_blending =
      SurfaceBlending::kBlendingCoverage;
  surface_param_.surface_flags.cursor = 0;
  surface_param_.frame_rate = track_info_.fps;
  surface_param_.z_order = 0;
  surface_param_.solid_fill_color = 0;
  surface_param_.surface_transform.rotation = 0.0f;
  surface_param_.surface_transform.flip_horizontal = 0;
  surface_param_.surface_transform.flip_vertical = 0;

  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return res;
}

status_t TestTrack::StopDisplay(DisplayType display_type) {
  TEST_INFO("%s:%s: Enter", TAG, __func__);
  int32_t res = 0;

  if (display_started_ == 1) {
    display_started_ = 0;
    res = display_->DestroySurface(surface_id_);
    if (res != 0) {
      TEST_ERROR("%s:%s DestroySurface Failed!!", TAG, __func__);
    }

    res = display_->DestroyDisplay(display_type);
    if (res != 0) {
      TEST_ERROR("%s:%s DestroyDisplay Failed!!", TAG, __func__);
    }
    res = display_->Disconnect();

    if (display_ != nullptr) {
      TEST_INFO("%s:%s: DELETE display_:%p", TAG, __func__, display_);
      delete display_;
      display_ = nullptr;
    }
  }
  TEST_INFO("%s:%s: Exit", TAG, __func__);
  return res;
}

status_t TestTrack::PushFrameToDisplay(BufferDescriptor& buffer,
    CameraBufferMetaData& meta_data) {
  if (display_started_ == 1) {
    int32_t ret;
    surface_buffer_.plane_info[0].ion_fd = buffer.fd;
    surface_buffer_.buf_id = 0;
    surface_buffer_.format = SurfaceFormat::kFormatYCbCr420SemiPlanarVenus;
    surface_buffer_.plane_info[0].stride = meta_data.plane_info[0].stride;
    surface_buffer_.plane_info[0].size = buffer.size;
    surface_buffer_.plane_info[0].width = meta_data.plane_info[0].width;
    surface_buffer_.plane_info[0].height = meta_data.plane_info[0].height;
    surface_buffer_.plane_info[0].offset = 0;
    surface_buffer_.plane_info[0].buf = buffer.data;

    ret = display_->QueueSurfaceBuffer(surface_id_, surface_buffer_,
        surface_param_);
    if (ret != 0) {
      TEST_ERROR("%s:%s QueueSurfaceBuffer Failed!!", TAG, __func__);
      return ret;
    }

    ret = display_->DequeueSurfaceBuffer(surface_id_, surface_buffer_);
    if (ret != 0) {
      TEST_ERROR("%s:%s DequeueSurfaceBuffer Failed!!", TAG, __func__);
    }
  }
  return NO_ERROR;
}

void CmdMenu::PrintMenu() {
  printf("\n\n=========== QMMF RECORDER TEST MENU ===================\n\n");

  printf(" Recorder Test Application commands \n");
  printf(" ---------------------------------\n");
  printf("   %c. Connect\n", CmdMenu::CONNECT_CMD);
  printf("   %c. Disconnect\n", CmdMenu::DISCONNECT_CMD);
  printf("   %c. Choose camera\n", CmdMenu::CHOOSE_CAMERA_CMD);
  printf("   %c. Start Camera\n", CmdMenu::START_CAMERA_CMD);
  printf("   %c. Stop Camera\n", CmdMenu::STOP_CAMERA_CMD);
  printf("   %c. Create Session: (4K YUV + 1080 YUV)\n",
      CmdMenu::CREATE_YUV_SESSION_CMD);
  printf("   %c. Create Session: (4K Enc AVC)\n",
      CmdMenu::CREATE_4KENC_AVC_SESSION_CMD);
  printf("   %c. Create Session: (4K Enc HEVC)\n",
      CmdMenu::CREATE_4KENC_HEVC_SESSION_CMD);
  printf("   %c. Create Session: (1080p Enc AVC)\n",
      CmdMenu::CREATE_1080pENC_AVC_SESSION_CMD);
  printf("   %c. Create Session: (1080p Enc HEVC)\n",
      CmdMenu::CREATE_1080pENC_HEVC_SESSION_CMD);
  printf("   %c. Create Session: (4K YUV + 1080p Enc AVC)\n",
    CmdMenu::CREATE_4KYUV_1080pENC_SESSION_CMD);
  printf("   %c. Create Session: (Two 1080p Enc AVC)\n",
    CmdMenu::CREATE_TWO_1080pENC_SESSION_CMD);
  printf("   %c. Create Session: (1080p Enc AVC + 1080 YUV)\n",
    CmdMenu::CREATE_1080pENC_AVC_1080YUV_SESSION_CMD);
  printf("   %c. Create Session: (4K Enc HEVC + 1080 YUV)\n",
    CmdMenu::CREATE_4KHEVC_AVC_1080YUV_SESSION_CMD);
  printf("   %c. Create Session: (720p LPM YUV)\n",
    CmdMenu::CREATE_720pLPM_SESSION_CMD);
  printf("   %c. Create Session: (1080p Enc AVC + 1080 LPM YUV)\n",
      CmdMenu::CREATE_1080pENC_AVC_1080LPM_SESSION_CMD);
  printf("   %c. Create Session: (RDI)\n",
      CmdMenu::CREATE_RDI_SESSION_CMD);
  printf("   %c. Create Session: (PCM mono,16,48KHz)\n",
      CmdMenu::CREATE_PCM_AUD_SESSION_CMD);
  printf("   %c. Create Session: (PCM mono,16,48KHz + PCM mono,16,48KHz)\n",
      CmdMenu::CREATE_2PCM_AUD_SESSION_CMD);
  printf("   %c. Create Session: (SCO mono,16,48KHz)\n",
      CmdMenu::CREATE_SCO_AUD_SESSION_CMD);
  printf("   %c. Create Session: (PCM mono,16,48KHz + SCO mono,16,48KHz)\n",
      CmdMenu::CREATE_PCM_SCO_AUD_SESSION_CMD);
#ifdef ENABLE_A2DP_USECASE
  printf("   %c. Create Session: (A2DP mono,16,48KHz)\n",
      CmdMenu::CREATE_A2DP_AUD_SESSION_CMD);
  printf("   %c. Create Session: (PCM mono,16,48KHz + A2DP mono,16,48KHz)\n",
      CmdMenu::CREATE_PCM_A2DP_AUD_SESSION_CMD);
#endif
  printf("   %c. Create Session: (AAC mono)\n",
      CmdMenu::CREATE_AAC_AUD_SESSION_CMD);
  printf("   %c. Create Session: (AAC mono + AAC mono)\n",
      CmdMenu::CREATE_2AAC_AUD_SESSION_CMD);
  printf("   %c. Create Session: (PCM mono,16,48KHz + AAC mono)\n",
    CmdMenu::CREATE_PCM_AAC_AUD_SESSION_CMD);
  printf("   %c. Create Session: (AMR mono)\n",
      CmdMenu::CREATE_AMR_AUD_SESSION_CMD);
  printf("   %c. Create Session: (AMR mono + AMR mono)\n",
      CmdMenu::CREATE_2AMR_AUD_SESSION_CMD);
  printf("   %c. Create Session: (PCM mono,16,8KHz + AMR mono)\n",
      CmdMenu::CREATE_PCM_AMR_AUD_SESSION_CMD);
  printf("   %c. Create Session: (G711 mono)\n",
      CmdMenu::CREATE_G7ll_AUD_SESSION_CMD);
  printf("   %c. Create Session: (G711 mono + G711 mono)\n",
      CmdMenu::CREATE_2G7ll_AUD_SESSION_CMD);
  printf("   %c. Create Session: (PCM mono,16,8KHz + G711 mono)\n",
      CmdMenu::CREATE_PCM_G7ll_AUD_SESSION_CMD);
  printf("   %c. Create Session: (1080p YUV with Display)\n",
      CmdMenu::CREATE_YUV_SESSION_DISPLAY_CMD);
  printf("   %c. Create Session: (1080p YUV with Preview)\n",
      CmdMenu::CREATE_YUV_SESSION_PREVIEW_CMD);
  printf("   %c. Start Session\n", CmdMenu::START_SESSION_CMD);
  printf("   %c. Stop Session\n", CmdMenu::STOP_SESSION_CMD);
  printf("   %c. Take Snapshot\n", CmdMenu::TAKE_SNAPSHOT_CMD);
  printf("   %c. Set Dynamic Codec Param \n", CmdMenu::SET_PARAM_CMD);
  printf("   %c. Pause Session\n", CmdMenu::PAUSE_SESSION_CMD);
  printf("   %c. Resume Session\n", CmdMenu::RESUME_SESSION_CMD);
  printf("   %c. Enable Overlay\n", CmdMenu::ENABLE_OVERLAY_CMD);
  printf("   %c. Disable Overlay\n", CmdMenu::DISABLE_OVERLAY_CMD);
  printf("   %c. Delete Session\n", CmdMenu::DELETE_SESSION_CMD);
  if (ctx_.session_enabled_) {
    printf("   %c. NR mode: %s\n", CmdMenu::NOISE_REDUCTION_CMD,
           ctx_.GetCurrentNRMode().c_str());
    printf("   %c. VHDR: %s\n", CmdMenu::VIDEO_HDR_CMD,
           ctx_.GetCurrentVHDRMode().c_str());

    printf("   %c. IR: %s\n", CmdMenu::IR_MODE_CMD,
           ctx_.GetCurrentIRMode().c_str());
  }
  printf("   %c. Set Antibanding mode\n", CmdMenu::SET_ANTIBANDING_MODE_CMD);
  printf("   %c. Exit\n", CmdMenu::EXIT_CMD);
  printf("\n   Choice: ");
}

CmdMenu::Command CmdMenu::GetCommand(bool& is_print_menu) {
  if (is_print_menu) {
    PrintMenu();
    is_print_menu = false;
  }
  return CmdMenu::Command(static_cast<CmdMenu::CommandType>(getchar()));
}

int main(int argc,char *argv[]) {

  TEST_INFO("%s:%s: Enter", TAG, __func__);

  RecorderTest test_context;

  if(argc > 1) {
    if(strcmp(argv[1], "--auto") == 0) {
      return test_context.RunAutoMode();
    }
    return test_context.RunFromConfig(argc, argv);
  }

  CmdMenu cmd_menu(test_context);
  bool is_print_menu = true;
  int32_t exit_test = false;

  while (!exit_test) {

    CmdMenu::Command command = cmd_menu.GetCommand(is_print_menu);
    switch (command.cmd) {

      case CmdMenu::CONNECT_CMD: {
        test_context.Connect();
      }
      break;
      case CmdMenu::DISCONNECT_CMD: {
        test_context.Disconnect();
      }
      break;
      case CmdMenu::START_CAMERA_CMD: {
        test_context.StartCamera();
      }
      break;
      case CmdMenu::STOP_CAMERA_CMD: {
        test_context.StopCamera();
      }
      break;
      case CmdMenu::CREATE_YUV_SESSION_CMD: {
        test_context.Session4KAnd1080pYUVTracks();
      }
      break;
      case CmdMenu::CREATE_4KENC_AVC_SESSION_CMD: {
        test_context.Session4KEncTrack(TrackType::kVideoAVC);
      }
      break;
      case CmdMenu::CREATE_4KENC_HEVC_SESSION_CMD: {
        test_context.Session4KEncTrack(TrackType::kVideoHEVC);
      }
      break;
      case CmdMenu::CREATE_1080pENC_AVC_SESSION_CMD: {
        test_context.Session1080pEncTrack(TrackType::kVideoAVC);
      }
      break;
      case CmdMenu::CREATE_1080pENC_AVC_1080YUV_SESSION_CMD: {
        test_context.Session1080pEnc1080YUV(TrackType::kVideoAVC);
      }
      break;
      case CmdMenu::CREATE_4KHEVC_AVC_1080YUV_SESSION_CMD: {
        test_context.Session4KHEVCAnd1080pYUVTracks(TrackType::kVideoAVC);
      }
      break;
      case CmdMenu::CREATE_1080pENC_HEVC_SESSION_CMD: {
        test_context.Session1080pEncTrack(TrackType::kVideoHEVC);
      }
      break;
      case CmdMenu::CREATE_4KYUV_1080pENC_SESSION_CMD: {
        test_context.Session4KYUVAnd1080pEncTracks(TrackType::kVideoAVC);
      }
      break;
      case CmdMenu::CREATE_TWO_1080pENC_SESSION_CMD: {
        test_context.SessionTwo1080pEncTracks(TrackType::kVideoAVC);
      }
      break;
      case CmdMenu::CREATE_720pLPM_SESSION_CMD: {
        test_context.Session720pLPMTrack(TrackType::kVideoYUV);
      }
      break;
      case CmdMenu::CREATE_1080pENC_AVC_1080LPM_SESSION_CMD: {
        test_context.Session1080pEnc1080pLPMTracks(TrackType::kVideoAVC);
      }
      break;
      case CmdMenu::CREATE_PCM_AUD_SESSION_CMD: {
          test_context.CreateAudioPCMTrack();
      }
      break;
      case CmdMenu::CREATE_2PCM_AUD_SESSION_CMD: {
          test_context.CreateAudio2PCMTrack();
      }
      break;
      case CmdMenu::CREATE_SCO_AUD_SESSION_CMD: {
          test_context.CreateAudioSCOTrack();
      }
      break;
      case CmdMenu::CREATE_PCM_SCO_AUD_SESSION_CMD: {
          test_context.CreateAudioPCMSCOTrack();
      }
      break;
#ifdef ENABLE_A2DP_USECASE
      case CmdMenu::CREATE_A2DP_AUD_SESSION_CMD: {
          test_context.CreateAudioA2DPTrack();
      }
      break;
      case CmdMenu::CREATE_PCM_A2DP_AUD_SESSION_CMD: {
          test_context.CreateAudioPCMA2DPTrack();
      }
      break;
#endif
      case CmdMenu::CREATE_AAC_AUD_SESSION_CMD: {
          test_context.CreateAudioAACTrack();
      }
      break;
      case CmdMenu::CREATE_2AAC_AUD_SESSION_CMD: {
          test_context.CreateAudio2AACTrack();
      }
      break;
      case CmdMenu::CREATE_PCM_AAC_AUD_SESSION_CMD: {
          test_context.CreateAudioPCMAACTrack();
      }
      break;
      case CmdMenu::CREATE_AMR_AUD_SESSION_CMD: {
          test_context.CreateAudioAMRTrack();
      }
      break;
      case CmdMenu::CREATE_2AMR_AUD_SESSION_CMD: {
          test_context.CreateAudio2AMRTrack();
      }
      break;
      case CmdMenu::CREATE_PCM_AMR_AUD_SESSION_CMD: {
          test_context.CreateAudioPCMAMRTrack();
      }
      break;
      case CmdMenu::CREATE_G7ll_AUD_SESSION_CMD: {
          test_context.CreateAudioG711Track();
      }
      break;
      case CmdMenu::CREATE_2G7ll_AUD_SESSION_CMD: {
          test_context.CreateAudio2G711Track();
      }
      break;
      case CmdMenu::CREATE_PCM_G7ll_AUD_SESSION_CMD: {
          test_context.CreateAudioPCMG711Track();
      }
      break;
      case CmdMenu::CREATE_RDI_SESSION_CMD: {
          test_context.SessionRDITrack();
      }
      break;
      case CmdMenu::CREATE_YUV_SESSION_DISPLAY_CMD: {
        test_context.Session1080pYUVTrackWithDisplay();
      }
      break;
      case CmdMenu::CREATE_YUV_SESSION_PREVIEW_CMD: {
        test_context.Session1080pYUVTrackWithPreview();
      }
      break;
      case CmdMenu::START_SESSION_CMD: {
        test_context.StartSession();
      }
      break;
      case CmdMenu::STOP_SESSION_CMD: {
        test_context.StopSession();
      }
      break;
      case CmdMenu::TAKE_SNAPSHOT_CMD: {
        test_context.TakeSnapshot();
      }
      break;
      case CmdMenu::SET_PARAM_CMD: {
        test_context.SetParams();
      }
      break;
      case CmdMenu::PAUSE_SESSION_CMD: {
        test_context.PauseSession();
      }
      break;
      case CmdMenu::RESUME_SESSION_CMD: {
        test_context.ResumeSession();
      }
      break;
      case CmdMenu::ENABLE_OVERLAY_CMD: {
        test_context.EnableOverlay();
      }
      break;
      case CmdMenu::DISABLE_OVERLAY_CMD: {
        test_context.DisableOverlay();
      }
      break;
      case CmdMenu::DELETE_SESSION_CMD: {
        test_context.DeleteSession();
      }
      break;
      case CmdMenu::NOISE_REDUCTION_CMD: {
        test_context.ToggleNR();
      }
      break;
      case CmdMenu::VIDEO_HDR_CMD: {
        test_context.ToggleVHDR();
      }
      break;
      case CmdMenu::IR_MODE_CMD: {
        test_context.ToggleIR();
      }
      break;
      case CmdMenu::CHOOSE_CAMERA_CMD: {
        test_context.ChooseCamera();
      }
      break;
      case CmdMenu::SET_ANTIBANDING_MODE_CMD: {
        test_context.SetAntibandingMode();
      }
      break;
      // TODO: To be exposed to end user once
      // 1080p@90FPS usecase is enabled through Menu
      case CmdMenu::BINNING_CORRECTION_CMD: {
        test_context.ToggleBinningCorrectionMode();
      }
      break;
      case CmdMenu::EXIT_CMD: {
        exit_test = true;
      }
      break;
      case CmdMenu::NEXT_CMD: {
        is_print_menu = true;
      }
      break;
      default:
        break;
    }
  }
  return 0;
}
