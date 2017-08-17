/*
 * GStreamer
 * Copyright (C) 2009 Julien Isorce <julien.isorce@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include <windows.h>
#endif

#include <GL/gl.h>
#include "SDL2/SDL.h"
#include "SDL2/SDL_opengl.h"

#ifndef WIN32
#include <GL/glx.h>
#include "SDL2/SDL_syswm.h"
#include <gst/gl/x11/gstgldisplay_x11.h>
#endif

#define GST_USE_UNSTABLE_API

#include <gst/gst.h>
#include <gst/gl/gl.h>

struct glstuff_instance_data {
    GstGLContext *gst_gl_context;
    GstGLDisplay *gst_gl_display;
    Display *sdl_display;
    Window sdl_win;
#ifdef WIN32
    HGLRC sdl_gl_context;
    HDC sdl_dc;
#else
    SDL_SysWMinfo wm_info;
    GLXContext sdl_gl_context;
#endif
};

unsigned int
get_texture_id_from_buffer (GstBuffer * buf)
{
  GstMapInfo mapinfo;
  unsigned int texture = 0;

  if (!gst_buffer_map (buf, &mapinfo, GST_MAP_READ | GST_MAP_GL)) {
    g_warning ("Failed to map the video buffer");
    return texture;
  }

  texture = (unsigned int) mapinfo.data[0];

  gst_buffer_unmap (buf, &mapinfo);

  return texture;
}

static gboolean
sync_bus_call (GstBus * bus, GstMessage * msg, struct glstuff_instance_data *data)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_NEED_CONTEXT:
    {
      const gchar *context_type;

      gst_message_parse_context_type (msg, &context_type);
      g_print ("got need context %s\n", context_type);

      if (g_strcmp0 (context_type, GST_GL_DISPLAY_CONTEXT_TYPE) == 0) {
        GstContext *display_context =
            gst_context_new (GST_GL_DISPLAY_CONTEXT_TYPE, TRUE);
        gst_context_set_gl_display (display_context, data->gst_gl_display);
        gst_element_set_context (GST_ELEMENT (msg->src), display_context);
        return TRUE;
      } else if (g_strcmp0 (context_type, "gst.gl.app_context") == 0) {
        GstContext *app_context = gst_context_new ("gst.gl.app_context", TRUE);
        GstStructure *s = gst_context_writable_structure (app_context);
        gst_structure_set (s, "context", GST_GL_TYPE_CONTEXT, data->gst_gl_context,
            NULL);
        gst_element_set_context (GST_ELEMENT (msg->src), app_context);
        return TRUE;
      }
      break;
    }
    default:
      break;
  }
  return FALSE;
}



void
init_gl_appsink_from_sdl_and_pipeline (SDL_Window *sdl_window, GstPipeline *pipeline)
{
  struct glstuff_instance_data data;

#ifndef WIN32
  SDL_SysWMinfo info;
#endif

  GstBus *bus = NULL;
  const gchar *platform;

  /* retrieve and turn off sdl opengl context */
#ifdef WIN32
  data.sdl_gl_context = wglGetCurrentContext ();
  data.sdl_dc = wglGetCurrentDC ();
  wglMakeCurrent (0, 0);
  platform = "wgl";
  data.gst_gl_display = gst_gl_display_new ();
#else
  SDL_VERSION (&info.version);
  SDL_GetWindowWMInfo (sdl_window, &info);
  data.wm_info = info;
  data.sdl_display = info.info.x11.display;
  data.sdl_win = info.info.x11.window;
  data.sdl_gl_context = glXGetCurrentContext ();
  glXMakeCurrent (data.sdl_display, None, 0);
  platform = "glx";
  data.gst_gl_display =
      (GstGLDisplay *) gst_gl_display_x11_new_with_display (data.sdl_display);
#endif

  data.gst_gl_context =
      gst_gl_context_new_wrapped (data.gst_gl_display, (guintptr) data.sdl_gl_context,
      gst_gl_platform_from_string (platform), GST_GL_API_OPENGL);


  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch (bus);
  gst_bus_enable_sync_message_emission (bus);
  g_signal_connect (bus, "sync-message", G_CALLBACK (sync_bus_call), &data);
  gst_object_unref (bus);

  /* NULL to PAUSED state pipeline to make sure the gst opengl context is created and
   * shared with the sdl one */
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PAUSED);

  /* turn on back sdl opengl context */
#ifdef WIN32
  wglMakeCurrent (data.sdl_dc, data.sdl_gl_context);
#else
  glXMakeCurrent (data.sdl_display, data.sdl_win, data.sdl_gl_context);
#endif


  g_object_set_data(G_OBJECT(pipeline), "glstuff-instance-data", &data);
  return;
}

void
stop_pipeline_and_fix_context(GstPipeline *pipeline) {
   struct glstuff_instance_data *data = 
        (struct glstuff_instance_data *) g_object_get_data(G_OBJECT(pipeline), "glstuff-instance-data");
    
  /* before to deinitialize the gst-gl-opengl context,
   * no shared context (here the sdl one) must be current
   */
#ifdef WIN32
  wglMakeCurrent (0, 0);
#else
  glXMakeCurrent (data->sdl_display, data->sdl_win, data->sdl_gl_context);
#endif

  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_NULL);

  /* turn on back sdl opengl context */
#ifdef WIN32
  wglMakeCurrent (data->sdl_dc, data->sdl_gl_context);
#else
  glXMakeCurrent (data->sdl_display, None, 0);
#endif
}
