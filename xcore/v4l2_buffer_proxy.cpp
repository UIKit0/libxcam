/*
 * v4l2_buffer_proxy.cpp - v4l2 buffer proxy
 *
 *  Copyright (c) 2014-2015 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Wind Yuan <feng.yuan@intel.com>
 */

#include "v4l2_buffer_proxy.h"
#include "v4l2_device.h"

namespace XCam {
V4l2Buffer::V4l2Buffer (const struct v4l2_buffer &buf, const struct v4l2_format &format)
{
    _buf = buf;
    _format = format;
}

V4l2Buffer::~V4l2Buffer ()
{
}

V4l2BufferProxy::V4l2BufferProxy (SmartPtr<V4l2Buffer> &buf, SmartPtr<V4l2Device> &device)
    : _buf (buf)
    , _device (device)
{
    VideoBufferInfo info;
    struct timeval ts = buf->get_buf().timestamp;

    xcam_mem_clear (&info);
    v4l2_format_to_video_info (buf->get_format(), info);
    set_video_info (info);
    set_timestamp (XCAM_TIMEVAL_2_USEC (ts));
}

V4l2BufferProxy::~V4l2BufferProxy ()
{
    XCAM_ASSERT (_buf.ptr());
    if (_device.ptr())
        _device->queue_buffer (_buf);
    XCAM_LOG_DEBUG ("v4l2 buffer released");
}

void
V4l2BufferProxy::v4l2_format_to_video_info (
    const struct v4l2_format &format, VideoBufferInfo &info)
{
    xcam_mem_clear (&info);

    info.format = format.fmt.pix.pixelformat;
    info.width = format.fmt.pix.width;
    info.height = format.fmt.pix.height;
    info.size = format.fmt.pix.sizeimage;
    switch (format.fmt.pix.pixelformat) {
    case V4L2_PIX_FMT_NV12:  // 420
    case V4L2_PIX_FMT_NV21:
        info.components = 2;
        info.strides [0] = format.fmt.pix.bytesperline * 2 / 3;
        info.strides [1] = info.strides [0];
        info.offsets[0] = 0;
        info.offsets[1] = info.strides [0] * format.fmt.pix.height;
        break;
    case V4L2_PIX_FMT_YUV422P: // 422 Planar
        info.components = 3;
        info.strides [0] = format.fmt.pix.bytesperline / 2;
        info.strides [1] = info.strides [0] / 2 ;
        info.strides [2] = info.strides [0] / 2 ;
        info.offsets[0] = 0;
        info.offsets[1] = info.strides [0] * format.fmt.pix.height;
        info.offsets[2] = info.offsets[1] + info.strides [1] * format.fmt.pix.height;
        break;
    case V4L2_PIX_FMT_YUYV: // 422
        info.components = 1;
        info.strides [0] = format.fmt.pix.bytesperline;
        info.offsets[0] = 0;
        break;
    default:
        XCAM_LOG_WARNING (
            "unknown v4l2 format(%s) to video info",
            xcam_fourcc_to_string (format.fmt.pix.pixelformat));
        break;
    }

}

const struct v4l2_buffer &
V4l2BufferProxy::get_v4l2_buf () const
{
    XCAM_ASSERT (_buf.ptr());
    return _buf->get_buf ();
}

uint8_t *
V4l2BufferProxy::map ()
{
    const struct v4l2_buffer & v4l2_buf = get_v4l2_buf ();
    if (v4l2_buf.memory == V4L2_MEMORY_DMABUF)
        return NULL;
    return (uint8_t *)(v4l2_buf.m.userptr);
}

bool
V4l2BufferProxy::unmap ()
{
    return true;
}

};
