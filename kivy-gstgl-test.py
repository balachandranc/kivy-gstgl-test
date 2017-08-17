import kivy

from kivy.app import App
from kivy.uix.image import Image
from kivy.clock import Clock
from kivy.core.window import Window
from kivy.graphics.texture import Texture
from kivy.graphics.cgl import *

import gi
gi.require_version('Gst', '1.0')
gi.require_version('GstGL', '1.0')
from gi.repository import Gst, GstGL

Gst.init(None)

import ctypes

glstuff = ctypes.CDLL('./glstuff.so')
glstuff.get_texture_id_from_buffer.restype = ctypes.c_uint

texture_map = {}

class GstGLTest(App):
    def new_sample(self, appsink):
        sample = self.appsink.emit('pull-sample')
        buf = sample.get_buffer()
        
        tex_id = glstuff.get_texture_id_from_buffer(ctypes.c_void_p(hash(buf)))
        
        if not tex_id == self.old_tex_id:
            self.old_tex_id = tex_id
            
            texture = texture_map.get(tex_id)

            if not texture:
                texture = Texture(1920, 1080, self.texture_target, colorfmt = 'rgba', texid = tex_id)
                texture.flip_vertical()
                texture_map[tex_id] = texture
                
            self.image.texture = texture
            
    def init_gst(self, dt):
        self.old_tex_id = None
        
        # Ewwww. Kivy doesn't expose the constant I need, so stealing if from here.
        self.texture_target = Texture.create(size = (1, 2)).target

        self.pipeline = Gst.parse_launch('videotestsrc pattern=0 ! video/x-raw, width=1920, height=1080, framerate=60/1 ! queue max-size-buffers=3 ! glupload ! appsink caps="video/x-raw(memory:GLMemory), format=RGBA" emit-signals=true name=appsink')

        sdl2_window_pointer = ctypes.c_void_p(Window.get_sdl2_window_pointer())
        
        glstuff.init_gl_appsink_from_sdl_and_pipeline(
            sdl2_window_pointer, 
            ctypes.c_void_p(hash(self.pipeline))
        )
        
        self.appsink = self.pipeline.get_by_name('appsink')
        self.appsink.connect('new-sample', self.new_sample)
        
        self.pipeline.set_state(Gst.State.PLAYING)
    
    def on_stop(self):
        glstuff.stop_pipeline_and_fix_context(ctypes.c_void_p(hash(self.pipeline)))
    
    def build(self):
        Clock.schedule_once(self.init_gst, 0)
        
        self.image = Image()        
        return self.image

GstGLTest().run()
