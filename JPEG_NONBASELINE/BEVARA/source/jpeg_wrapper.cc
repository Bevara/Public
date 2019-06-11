// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// @file file_histogram.cc
/// This example demonstrates loading, running and scripting a very simple NaCl
/// module.  To load the NaCl module, the browser first looks for the
/// CreateModule() factory method (at the end of this file).  It calls
/// CreateModule() once to load the module code from your .nexe.  After the
/// .nexe code is loaded, CreateModule() is not called again.
///
/// Once the .nexe code is loaded, the browser than calls the CreateInstance()
/// method on the object returned by CreateModule().  It calls CreateInstance()
/// each time it encounters an <embed> tag that references your NaCl module.
///
/// The browser can talk to your NaCl module via the postMessage() Javascript
/// function.  When you call postMessage() on your NaCl module from the browser,
/// this becomes a call to the HandleMessage() method of your pp::Instance
/// subclass.  You can send messages back to the browser by calling the
/// PostMessage() method on your pp::Instance.  Note that these two methods
/// (postMessage() in Javascript and PostMessage() in C++) are asynchronous.
/// This means they return immediately - there is no waiting for the message
/// to be handled.  This has implications in your program design, particularly
/// when mutating property values that are exposed to both the browser and the
/// NaCl module.

#include <algorithm>
#include <deque>
#include <string>
#include <stdio.h>
#include <cstring>

#include "jpeg_dec.h"

#include "ppapi/cpp/var_dictionary.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace {

const uint32_t kBlack = 0xff000000u;

}  // namespace
using namespace std;

/// The Instance class.  One of these exists for each instance of your NaCl
/// module on the web page.  The browser will ask the Module object to create
/// a new Instance for each occurence of the <embed> tag that has these
/// attributes:
///     type="application/x-nacl"
///     src="file_histogram.nmf"
class FileHistogramInstance : public pp::Instance {
 public:
  /// The constructor creates the plugin-side instance.
  /// @param[in] instance the handle to the browser-side plugin instance.
  explicit FileHistogramInstance(PP_Instance instance)
      : pp::Instance(instance),
        callback_factory_(this),
        flushing_(false) {
  }
  virtual ~FileHistogramInstance() {
  }

 private:

  unsigned char* img;


  /// Handler for messages coming in from the browser via postMessage().  The
  /// @a var_message can contain anything: a JSON string; a string that encodes
  /// method names and arguments; etc.
  ///
  /// In this case, we only handle <code>pp::VarArrayBuffer</code>s. When we
  /// receive one, we compute and display a histogram based on its contents.
  ///
  /// @param[in] var_message The message posted by the browser.
  virtual void HandleMessage(const pp::Var& var_message) {

    if (var_message.is_string()){
		  string message = var_message.AsString();
		  if (message == "Export_as_PPM"){
			SaveRawImage();
		  }
	 }
    else if (var_message.is_array_buffer()) {
      pp::VarArrayBuffer buffer(var_message);

      jpg_complete_setup();
      DecodeImage(buffer);
      jpg_complete_shutdown();
      DrawImage();
    }

    
  }

void SaveRawImage(){
	 pp::VarDictionary dictionary;

	 // construct PPM header
	 std::string ppm_header = "P6 " + to_string(imgWidth) + " " + to_string(imgHeight) + " 255\n";

	 // concat PPM header and raw RGB data
	 pp::VarArrayBuffer buffer((imgWidth*imgHeight*3)+ppm_header.length()+1);
	 char* data = static_cast<char*>(buffer.Map());
	 memcpy(data, ppm_header.c_str(), ppm_header.length()+1);
	 memcpy(data+ ppm_header.length(), outbuf, (imgWidth*imgHeight*3));

	 // send to be downloaded
	 dictionary.Set(pp::Var("command"), pp::Var("ppmBlobReady"));
	 dictionary.Set(pp::Var("array"), buffer);
	 PostMessage(dictionary); 
  }



  void DecodeImage(pp::VarArrayBuffer& buffer) {
	pp::Var var_reply;

	uint32_t buffer_size = buffer.ByteLength();
	if (buffer_size == 0)
	  return;
	char* buffer_data = static_cast<char*>(buffer.Map());
    
	decodeJPEG(buffer_data, buffer_size);
	sizeJpeg_.set_width(imgWidth);
	sizeJpeg_.set_height(imgHeight);
	pp::VarDictionary dictionary;
	dictionary.Set(pp::Var("command"), pp::Var("setSize"));
	dictionary.Set(pp::Var("width"), pp::Var(imgWidth));
	dictionary.Set(pp::Var("height"), pp::Var(imgHeight));
	PostMessage(dictionary);
  }

  void simple_unpack(unsigned char* rgba, unsigned char* rgb, const int count) {
    for(int i=0; i<count; ++i) {
        for(int j=0; j<3; ++j) {
            rgba[j] = rgb[j];
        }
        rgba += 4;
        rgb  += 3;
    }
  }


void DrawImage() {
  pp::ImageData image_data = MakeBlankImageData(sizeJpeg_);
	img = outbuf;
	unsigned char* image_buffer = static_cast<unsigned char*>(image_data.data());

	const int count = imgWidth*imgHeight;

	simple_unpack(image_buffer, img, count);

    if (!flushing_)
      PaintAndFlush(&image_data);
    else
      paint_queue_.push_back(image_data);
  }

  /// Create and return a blank (all-black) <code>pp::ImageData</code> of the
  /// given <code>size</code>.
  pp::ImageData MakeBlankImageData(const pp::Size& size) {
    const bool init_to_zero = false;
    pp::ImageData image_data = pp::ImageData(this,
                                             PP_IMAGEDATAFORMAT_RGBA_PREMUL,
                                             size,
                                             init_to_zero);
    uint32_t* image_buffer = static_cast<uint32_t*>(image_data.data());
    for (int i = 0; i < size.GetArea(); ++i)
       image_buffer[i] = kBlack;
    return image_data;
  }

  void PaintAndFlush(pp::ImageData* image_data) {
    assert(!flushing_);
    graphics_2d_context_.ReplaceContents(image_data);
    graphics_2d_context_.Flush(
        callback_factory_.NewCallback(&FileHistogramInstance::DidFlush));
    flushing_ = true;
  }

  /// The callback that gets invoked when a flush completes. This is bound to a
  /// <code>CompletionCallback</code> and passed as a parameter to
  /// <code>Flush</code>.
  void DidFlush(int32_t error_code) {
    flushing_ = false;
    // If there are no images in the queue, we're done for now.
    if (paint_queue_.empty())
      return;
    // Otherwise, pop the next image off the queue and draw it.
    pp::ImageData image_data = paint_queue_.front();
    paint_queue_.pop_front();
    PaintAndFlush(&image_data);
  }

  virtual void DidChangeView(const pp::View& view) {
    if (size_ != view.GetRect().size()) {
      size_ = view.GetRect().size();
      const bool is_always_opaque = true;
      graphics_2d_context_ = pp::Graphics2D(this, view.GetRect().size(),
                                            is_always_opaque);
      BindGraphics(graphics_2d_context_);
      // The images in our queue are the wrong size, so we won't paint them.
      // We'll only draw the most recently computed histogram.
      paint_queue_.clear();
      DrawImage();
    }
  }

  pp::Graphics2D graphics_2d_context_;
  pp::CompletionCallbackFactory<FileHistogramInstance> callback_factory_;

  /// A queue of images to paint. We must maintain a queue because we can not
  /// call pp::Graphics2D::Flush while a Flush is already pending.
  std::deque<pp::ImageData> paint_queue_;

  /// The size of our rectangle in the DOM, as of the last time DidChangeView
  /// was called.
  pp::Size size_;
  pp::Size sizeJpeg_;

  /// true iff we are flushing.
  bool flushing_;
};

/// The Module class.  The browser calls the CreateInstance() method to create
/// an instance of your NaCl module on the web page.  The browser creates a new
/// instance for each <embed> tag with type="application/x-nacl".
class FileHistogramModule : public pp::Module {
 public:
  FileHistogramModule() : pp::Module() {}
  virtual ~FileHistogramModule() {}

  /// Create and return a FileHistogramInstance object.
  /// @param[in] instance The browser-side instance.
  /// @return the plugin-side instance.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new FileHistogramInstance(instance);
  }
};

namespace pp {
/// Factory function called by the browser when the module is first loaded.
/// The browser keeps a singleton of this module.  It calls the
/// CreateInstance() method on the object you return to make instances.  There
/// is one instance per <embed> tag on the page.  This is the main binding
/// point for your NaCl module with the browser.
Module* CreateModule() {
  return new FileHistogramModule();
}
}  // namespace pp
