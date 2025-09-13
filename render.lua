local gl = require("gl")

function render(frame_count, width, height, hw_render)
    -- Clear screen
    gl.clear(0.3, 0.4, 0.5, 1.0)
    gl.viewport(width, height)
    gl.bind_framebuffer(hw_render)

    -- Setup program
    gl.use_program()

    -- First quad
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

    -- Second quad
    cos_angle = cos_angle * 0.5
    sin_angle = sin_angle * 0.5
    local mvp2 = {
        cos_angle, -sin_angle, 0, 0,
        sin_angle, cos_angle, 0, 0,
        0, 0, 1, 0,
        0.4, 0.4, 0.2, 1
    }
    gl.draw(mvp2)

    -- Cleanup
    gl.cleanup()

    gl.draw_test_quad(10, 10)
end