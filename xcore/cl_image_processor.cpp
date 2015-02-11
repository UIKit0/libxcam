/*
 * cl_image_processor.cpp - CL image processor
 *
 *  Copyright (c) 2015 Intel Corporation
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
#include "cl_image_processor.h"
#include "cl_context.h"
#include "cl_device.h"
#include "cl_image_handler.h"
#include "drm_display.h"

namespace XCam {

CLImageProcessor::CLImageProcessor ()
    : ImageProcessor ("CLImageProcessor")
{
    _context = CLDevice::instance ()->get_context ();
    XCAM_ASSERT (_context.ptr());
    XCAM_LOG_DEBUG ("CLImageProcessor constructed");
}

CLImageProcessor::~CLImageProcessor ()
{
    XCAM_LOG_DEBUG ("CLImageProcessor destructed");
}

bool
CLImageProcessor::can_process_result (SmartPtr<X3aResult> &result)
{
    XCAM_UNUSED (result);
    return false;
}

XCamReturn
CLImageProcessor::apply_3a_results (X3aResultList &results)
{
    XCAM_UNUSED (results);
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
CLImageProcessor::apply_3a_result (SmartPtr<X3aResult> &result)
{
    XCAM_UNUSED (result);
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
CLImageProcessor::process_buffer (SmartPtr<VideoBuffer> &input, SmartPtr<VideoBuffer> &output)
{
    SmartPtr<DrmBoBuffer> drm_bo_in, drm_bo_out;
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    SmartPtr<DrmDisplay> display = DrmDisplay::instance ();

    drm_bo_in = display->convert_to_drm_bo_buf (display, input);
    XCAM_FAIL_RETURN (
        WARNING,
        drm_bo_in.ptr (),
        XCAM_RETURN_ERROR_MEM,
        "CL image processor can't handle this buffer, maybe type error");

    if (_handlers.empty()) {
        ret = create_handlers ();
    }

    for (ImageHandlerList::iterator i_handler = _handlers.begin ();
            i_handler != _handlers.end ();  ++i_handler)
    {
        ret = (*i_handler)->execute (drm_bo_in, drm_bo_out);
        XCAM_FAIL_RETURN (
            WARNING,
            ret == XCAM_RETURN_NO_ERROR,
            ret,
            "CL image handler(%s) execute buffer failed", (*i_handler)->get_name());
        drm_bo_in = drm_bo_out;
    }

    output = drm_bo_out;
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
CLImageProcessor::create_handlers ()
{
    //SmartPtr<CLImageHandler> new_handler;
    //SmartPtr<CLImageKernel> image_kernel;

    // new_kernel = new CLImageKernel (_context, "sample");
    // new_kernel->load_from_source ();
    // XCAM_ASSERT (new_kernel->is_valid ());
    // new_handler = new CLImageHandler ("sample");
    // new_handler->add_kernel  (new_kernel);
    //_handlers.push_back (new_handler);

    return XCAM_RETURN_ERROR_CL;
}

};