#include <lua.h>
#include <lauxlib.h>
#include <GL/gl.h>
#include "glsym/glsym.h"
#include "libretro.h"
#include "globals.h"
#include <math.h>

#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER

static void check_gl_error(const char *op)
{
   GLenum error;
   while ((error = glGetError()) != GL_NO_ERROR)
      fprintf(stderr, "[libretro-test]: OpenGL error after %s: %u\n", op, error);
}

static int l_gl_clear(lua_State *L)
{
   luaL_checktype(L, 1, LUA_TNUMBER);
   luaL_checktype(L, 2, LUA_TNUMBER);
   luaL_checktype(L, 3, LUA_TNUMBER);
   luaL_checktype(L, 4, LUA_TNUMBER);
   float r = (float)lua_tonumber(L, 1);
   float g = (float)lua_tonumber(L, 2);
   float b = (float)lua_tonumber(L, 3);
   float a = (float)lua_tonumber(L, 4);
   glClearColor(r, g, b, a);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
   check_gl_error("l_gl_clear");
   return 0;
}

static int l_gl_viewport(lua_State *L)
{
   luaL_checktype(L, 1, LUA_TNUMBER);
   luaL_checktype(L, 2, LUA_TNUMBER);
   int width = (int)lua_tonumber(L, 1);
   int height = (int)lua_tonumber(L, 2);
   glViewport(0, 0, width, height);
   check_gl_error("l_gl_viewport");
   return 0;
}

static int l_gl_bind_framebuffer(lua_State *L)
{
   luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
   struct retro_hw_render_callback *hw_render = (struct retro_hw_render_callback *)lua_touserdata(L, 1);
   glBindFramebuffer(RARCH_GL_FRAMEBUFFER, hw_render->get_current_framebuffer());
   check_gl_error("l_gl_bind_framebuffer");
   return 0;
}

static int l_gl_use_program(lua_State *L)
{
   glUseProgram(prog);
   glEnable(GL_DEPTH_TEST);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   int vloc = glGetAttribLocation(prog, "aVertex");
   glVertexAttribPointer(vloc, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
   glEnableVertexAttribArray(vloc);
   int cloc = glGetAttribLocation(prog, "aColor");
   glVertexAttribPointer(cloc, 4, GL_FLOAT, GL_FALSE, 0, (void*)(8 * sizeof(GLfloat)));
   glEnableVertexAttribArray(cloc);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   fprintf(stderr, "[libretro-test]: Using program, prog=%u, vbo=%u\n", prog, vbo);
   check_gl_error("l_gl_use_program");
   return 0;
}

static int l_gl_draw(lua_State *L)
{
   luaL_checktype(L, 1, LUA_TTABLE);
   int loc = glGetUniformLocation(prog, "uMVP");
   GLfloat mvp[16];
   for (int i = 0; i < 16; i++)
   {
      lua_rawgeti(L, 1, i + 1);
      if (lua_isnumber(L, -1))
         mvp[i] = (GLfloat)lua_tonumber(L, -1);
      else
         mvp[i] = 0.0f;
      lua_pop(L, 1);
   }
   glUniformMatrix4fv(loc, 1, GL_FALSE, mvp);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   fprintf(stderr, "[libretro-test]: Drawing rotating square\n");
   check_gl_error("l_gl_draw");
   return 0;
}

static int l_gl_cleanup(lua_State *L)
{
   int vloc = glGetAttribLocation(prog, "aVertex");
   int cloc = glGetAttribLocation(prog, "aColor");
   glDisableVertexAttribArray(vloc);
   glDisableVertexAttribArray(cloc);
   glUseProgram(0);
   glDisable(GL_DEPTH_TEST);
   fprintf(stderr, "[libretro-test]: Cleaning up program state\n");
   check_gl_error("l_gl_cleanup");
   return 0;
}

static int l_gl_draw_test_quad(lua_State *L)
{
   luaL_checktype(L, 1, LUA_TNUMBER);
   luaL_checktype(L, 2, LUA_TNUMBER);
   float x = (float)lua_tonumber(L, 1);
   float y = (float)lua_tonumber(L, 2);

   GLfloat vertices[] = {
      x, y,
      x + 50.0, y,
      x, y + 50.0,
      x + 50.0, y + 50.0,
      1.0, 0.0, 0.0, 1.0,
      1.0, 0.0, 0.0, 1.0,
      1.0, 0.0, 0.0, 1.0,
      1.0, 0.0, 0.0, 1.0,
   };

   glUseProgram(prog);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

   int vloc = glGetAttribLocation(prog, "aVertex");
   glVertexAttribPointer(vloc, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
   glEnableVertexAttribArray(vloc);
   int cloc = glGetAttribLocation(prog, "aColor");
   glVertexAttribPointer(cloc, 4, GL_FLOAT, GL_FALSE, 0, (void*)(8 * sizeof(GLfloat)));
   glEnableVertexAttribArray(cloc);

   int loc = glGetUniformLocation(prog, "uMVP");
   const GLfloat mvp[] = {
      2.0 / width, 0.0, 0.0, 0.0,
      0.0, -2.0 / height, 0.0, 0.0,
      0.0, 0.0, 1.0, 0.0,
      -1.0, 1.0, 0.0, 1.0,
   };
   glUniformMatrix4fv(loc, 1, GL_FALSE, mvp);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   glDisableVertexAttribArray(vloc);
   glDisableVertexAttribArray(cloc);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glUseProgram(0);
   glDisable(GL_DEPTH_TEST);
   check_gl_error("l_gl_draw_test_quad");
   return 0;
}

static int l_gl_draw_text(lua_State *L)
{
   luaL_checktype(L, 1, LUA_TNUMBER);
   luaL_checktype(L, 2, LUA_TNUMBER);
   luaL_checktype(L, 3, LUA_TSTRING);
   float x = (float)lua_tonumber(L, 1);
   float y = (float)lua_tonumber(L, 2);
   const char *text = lua_tostring(L, 3);

   if (!font_texture || !text_prog || !text_vbo)
   {
      fprintf(stderr, "[libretro-test]: Lua text rendering skipped: font_texture=%u, text_prog=%u, text_vbo=%u\n",
              font_texture, text_prog, text_vbo);
      return 0;
   }

   glUseProgram(text_prog);
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glDisable(GL_DEPTH_TEST);
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, font_texture);

   GLint tex_loc = glGetUniformLocation(text_prog, "uTexture");
   glUniform1i(tex_loc, 0);

   int loc = glGetUniformLocation(text_prog, "uMVP");
   const GLfloat mvp[] = {
      2.0f / width, 0, 0, 0,
      0, -2.0f / height, 0, 0,
      0, 0, 1, 0,
      -1, 1, 0, 1,
   };
   glUniformMatrix4fv(loc, 1, GL_FALSE, mvp);

   float sx = x, sy = y;
   float vertices[4 * 4 * strlen(text)];
   int vertex_count = 0;

   fprintf(stderr, "[libretro-test]: Rendering text: %s at (%f, %f)\n", text, x, y);
   for (const char *p = text; *p; p++)
   {
      if (*p >= 32 && *p < 128)
      {
         stbtt_aligned_quad q;
         stbtt_GetBakedQuad(cdata, font_bitmap_width, font_bitmap_height, *p - 32, &sx, &sy, &q, 1);
         fprintf(stderr, "[libretro-test]: Char '%c': x0=%f, y0=%f, x1=%f, y1=%f, s0=%f, t0=%f, s1=%f, t1=%f\n",
                 *p, q.x0, q.y0, q.x1, q.y1, q.s0, q.t0, q.s1, q.t1);

         vertices[vertex_count++] = q.x0; vertices[vertex_count++] = q.y0;
         vertices[vertex_count++] = q.s0; vertices[vertex_count++] = q.t0;
         vertices[vertex_count++] = q.x1; vertices[vertex_count++] = q.y0;
         vertices[vertex_count++] = q.s1; vertices[vertex_count++] = q.t0;
         vertices[vertex_count++] = q.x1; vertices[vertex_count++] = q.y1;
         vertices[vertex_count++] = q.s1; vertices[vertex_count++] = q.t1;
         vertices[vertex_count++] = q.x0; vertices[vertex_count++] = q.y1;
         vertices[vertex_count++] = q.s0; vertices[vertex_count++] = q.t1;
      }
   }

   glBindBuffer(GL_ARRAY_BUFFER, text_vbo);
   glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(float), vertices, GL_DYNAMIC_DRAW);

   int vloc = glGetAttribLocation(text_prog, "aVertex");
   glVertexAttribPointer(vloc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
   glEnableVertexAttribArray(vloc);
   int tloc = glGetAttribLocation(text_prog, "aTexCoord");
   glVertexAttribPointer(tloc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
   glEnableVertexAttribArray(tloc);

   glDrawArrays(GL_QUADS, 0, vertex_count / 4);

   glDisableVertexAttribArray(vloc);
   glDisableVertexAttribArray(tloc);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindTexture(GL_TEXTURE_2D, 0);
   glUseProgram(0);
   glDisable(GL_BLEND);
   glEnable(GL_DEPTH_TEST);
   check_gl_error("l_gl_draw_text");
   return 0;
}

static const struct luaL_Reg gl_funcs[] = {
   {"clear", l_gl_clear},
   {"viewport", l_gl_viewport},
   {"bind_framebuffer", l_gl_bind_framebuffer},
   {"use_program", l_gl_use_program},
   {"draw", l_gl_draw},
   {"cleanup", l_gl_cleanup},
   {"draw_test_quad", l_gl_draw_test_quad},
   {"draw_text", l_gl_draw_text},
   {NULL, NULL}
};

int luaopen_gl(lua_State *L)
{
   luaL_newlib(L, gl_funcs);
   return 1;
}