/*
 * main.cpp - test
 *
 *  Copyright (c) 2014 Intel Corporation
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

#include "device_manager.h"
#include "atomisp_device.h"
#include "isp_controller.h"
#include "isp_image_processor.h"
#include "x3a_analyzer_simple.h"
#if HAVE_IA_AIQ
#include "x3a_analyzer_aiq.h"
#endif
#if HAVE_LIBCL
#include "cl_image_processor.h"
#endif

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include "test_common.h"


using namespace XCam;

class MainDeviceManager
    : public DeviceManager
{
public:
    MainDeviceManager ()
        : _file (NULL)
        , _save_file (false)
        , _interval (1)
        , _frame_count (0)
    {}

    ~MainDeviceManager () {
        close_file ();
    }

    void enable_save_file (bool enable) {
        _save_file = enable;
    }
    void set_interval (uint32_t inteval) {
        _interval = inteval;
    }

protected:
    virtual void handle_message (SmartPtr<XCamMessage> &msg);
    virtual void handle_buffer (SmartPtr<VideoBuffer> &buf);

private:
    void open_file ();
    void close_file ();

    FILE      *_file;
    bool       _save_file;
    uint32_t   _interval;
    uint32_t   _frame_count;
};

void
MainDeviceManager::handle_message (SmartPtr<XCamMessage> &msg)
{
    XCAM_UNUSED (msg);
}

void
MainDeviceManager::handle_buffer (SmartPtr<VideoBuffer> &buf)
{
    if (!_save_file)
        return ;

    if ((_frame_count++ % _interval) != 0)
        return;

    const VideoBufferInfo & frame_info = buf->get_video_info ();
    uint8_t *frame = buf->map ();
    // only for NV12
    uint32_t size = XCAM_ALIGN_UP(frame_info.width, 2) * XCAM_ALIGN_UP(frame_info.height, 2) * 3 / 2;
    if (frame == NULL)
        return;
    open_file ();
    if (fwrite (frame, size, 1, _file) <= 0) {
        XCAM_LOG_WARNING ("write frame failed.");
    }
}

void
MainDeviceManager::open_file ()
{
    if (_file)
        return;
    _file = fopen (DEFAULT_SAVE_FILE_NAME, "wb");
}

void
MainDeviceManager::close_file ()
{
    if (_file)
        fclose (_file);
    _file = NULL;
}

#define V4L2_CAPTURE_MODE_STILL   0x2000
#define V4L2_CAPTURE_MODE_VIDEO   0x4000
#define V4L2_CAPTURE_MODE_PREVIEW 0x8000


static Mutex g_mutex;
static Cond  g_cond;
static bool  g_stop = false;

void dev_stop_handler(int sig)
{
    XCAM_UNUSED (sig);

    SmartLock locker (g_mutex);
    g_stop = true;
    g_cond.broadcast ();

    //exit(0);
}

void print_help (const char *bin_name)
{
    printf ("Usage: %s [-a analyzer]\n"
            "\t -a analyzer  specify a analyzer\n"
            "\t              select from [simple, aiq], default is [simple]\n"
            "\t -m mem_type  specify video memory type\n"
            "\t              mem_type select from [dma, mmap], default is [mmap]\n"
            "\t -s           save file to %s\n"
            "\t -n interval  save file on every [interval] frame\n"
            "\t -c           process image with cl kernel\n"
            "\t -h           help\n"
            , bin_name
            , DEFAULT_SAVE_FILE_NAME);
}

int main (int argc, char *argv[])
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    SmartPtr<MainDeviceManager> device_manager = new MainDeviceManager;
    SmartPtr<V4l2Device> device;
    SmartPtr<V4l2SubDevice> event_device;
    SmartPtr<IspController> isp_controller;
    SmartPtr<X3aAnalyzer> analyzer;
    SmartPtr<ImageProcessor> processor;
#if HAVE_LIBCL
    SmartPtr<CLImageProcessor> cl_processor;
#endif
    bool have_cl_processor = false;
    enum v4l2_memory v4l2_mem_type = V4L2_MEMORY_MMAP;
    const char *bin_name = argv[0];
    int opt;

    while ((opt =  getopt(argc, argv, "sca:n:m:h")) != -1) {
        switch (opt) {
        case 'a': {
            if (!strcmp (optarg, "simple"))
                analyzer = new X3aAnalyzerSimple ();
#if HAVE_IA_AIQ
            else if (!strcmp (optarg, "aiq"))
                analyzer = new X3aAnalyzerAiq (isp_controller, DEFAULT_CPF_FILE);
#endif
            else {
                print_help (bin_name);
                return -1;
            }
            break;
        }

        case 'm': {
            if (!strcmp (optarg, "dma"))
                v4l2_mem_type = V4L2_MEMORY_DMABUF;
            else if (!strcmp (optarg, "mmap"))
                v4l2_mem_type = V4L2_MEMORY_MMAP;
            else
                print_help (bin_name);
            break;
        }

        case 's':
            device_manager->enable_save_file (true);
            break;
        case 'n':
            device_manager->set_interval (atoi(optarg));
            break;
        case 'c':
            have_cl_processor = true;
            break;
        case 'h':
            print_help (bin_name);
            return 0;

        default:
            print_help (bin_name);
            return -1;
        }
    }

    if (!device.ptr ())
        device = new AtomispDevice (DEFAULT_CAPTURE_DEVICE);
    if (!event_device.ptr ())
        event_device = new V4l2SubDevice (DEFAULT_EVENT_DEVICE);
    if (!isp_controller.ptr ())
        isp_controller = new IspController (device);
    if (!processor.ptr ())
        processor = new IspImageProcessor (isp_controller);
    //if (!analyzer.ptr())
    //    analyzer = new X3aAnalyzerSimple ();

    signal(SIGINT, dev_stop_handler);

    device->set_sensor_id (0);
    device->set_capture_mode (V4L2_CAPTURE_MODE_VIDEO);
    //device->set_mem_type (V4L2_MEMORY_DMABUF);
    device->set_mem_type (v4l2_mem_type);
    device->set_buffer_count (8);
    device->set_framerate (25, 1);
    ret = device->open ();
    CHECK (ret, "device(%s) open failed", device->get_device_name());
    ret = device->set_format (1920, 1080, V4L2_PIX_FMT_NV12, V4L2_FIELD_NONE, 1920 * 2);
    CHECK (ret, "device(%s) set format failed", device->get_device_name());

    ret = event_device->open ();
    CHECK (ret, "event device(%s) open failed", event_device->get_device_name());
    int event = V4L2_EVENT_ATOMISP_3A_STATS_READY;
    ret = event_device->subscribe_event (event);
    CHECK_CONTINUE (
        ret,
        "device(%s) subscribe event(%d) failed",
        event_device->get_device_name(), event);
    event = V4L2_EVENT_FRAME_SYNC;
    ret = event_device->subscribe_event (event);
    CHECK_CONTINUE (
        ret,
        "device(%s) subscribe event(%d) failed",
        event_device->get_device_name(), event);

    device_manager->set_capture_device (device);
    device_manager->set_event_device (event_device);
    device_manager->set_isp_controller (isp_controller);
    if (analyzer.ptr())
        device_manager->set_analyzer (analyzer);
    device_manager->add_image_processor (processor);
#if HAVE_LIBCL
    if (have_cl_processor) {
        cl_processor = new CLImageProcessor ();
        device_manager->add_image_processor (cl_processor);
    }
#endif
    ret = device_manager->start ();
    CHECK (ret, "device manager start failed");

    // wait for interruption
    {
        SmartLock locker (g_mutex);
        while (!g_stop)
            g_cond.wait (g_mutex);
    }

    ret = device_manager->stop();
    CHECK_CONTINUE (ret, "device manager stop failed");
    device->close ();
    event_device->close ();

    return 0;
}
