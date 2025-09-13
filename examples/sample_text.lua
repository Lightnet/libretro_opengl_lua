local gl = require("gl")

function render(frame_count, width, height, hw_render)
   
   gl.bind_framebuffer(hw_render)
   gl.clear(0.3, 0.4, 0.5, 1.0)
   gl.viewport(width, height)

   gl.use_program()
   local angle = frame_count / 100.0
   local cos_angle = math.cos(angle)
   local sin_angle = math.sin(angle)
   local mvp = {
      cos_angle, -sin_angle, 0, 0,
      sin_angle, cos_angle, 0, 0,
      0, 0, 1, 0,
      0, 0, 0, 1
   }
   gl.draw(mvp)
   local cos_angle2 = cos_angle * 0.5
   local sin_angle2 = sin_angle * 0.5
   local mvp2 = {
      cos_angle2, -sin_angle2, 0, 0,
      sin_angle2, cos_angle2, 0, 0,
      0, 0, 1, 0,
      0.4, 0.4, 0.2, 1
   }
   gl.draw(mvp2)

   gl.cleanup()

   gl.draw_text(10, 50, "Frame: " .. frame_count)
   
   -- gl.draw_test_quad(10, 40) --conflict other build
end