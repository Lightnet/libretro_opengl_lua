#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "glsym/glsym.h"
#include "libretro.h"
#include "globals.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
static struct retro_hw_render_callback hw_render;

#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0

#define BASE_WIDTH 320
#define BASE_HEIGHT 240
#define MAX_WIDTH 2048
#define MAX_HEIGHT 2048

unsigned width = BASE_WIDTH;
unsigned height = BASE_HEIGHT;
GLuint prog = 0;
GLuint vbo = 0;
GLuint text_prog = 0;
GLuint text_vbo = 0;
GLuint font_texture = 0;
stbtt_bakedchar cdata[96]; // ASCII 32..126
unsigned char *font_bitmap = NULL;
int font_bitmap_width = 512;
int font_bitmap_height = 512;

static lua_State *L;
static bool use_lua = false;
static retro_environment_t environ_cb;

static const char *vertex_shader[] = {
   "uniform mat4 uMVP;",
   "attribute vec2 aVertex;",
   "attribute vec4 aColor;",
   "varying vec4 color;",
   "void main() {",
   "  gl_Position = uMVP * vec4(aVertex, 0.0, 1.0);",
   "  color = aColor;",
   "}",
};

static const char *fragment_shader[] = {
   "varying vec4 color;",
   "void main() {",
   "  gl_FragColor = color;",
   "}",
};

static const char *text_vertex_shader[] = {
   "uniform mat4 uMVP;",
   "attribute vec2 aVertex;",
   "attribute vec2 aTexCoord;",
   "varying vec2 vTexCoord;",
   "void main() {",
   "  gl_Position = uMVP * vec4(aVertex, 0.0, 1.0);",
   "  vTexCoord = aTexCoord;",
   "}",
};

static const char *text_fragment_shader[] = {
   "varying vec2 vTexCoord;",
   "uniform sampler2D uTexture;",
   "void main() {",
   "  gl_FragColor = texture2D(uTexture, vTexCoord);",
   "}",
};

static void check_gl_error(const char *op)
{
   GLenum error;
   while ((error = glGetError()) != GL_NO_ERROR)
      fprintf(stderr, "[libretro-test]: OpenGL error after %s: %u\n", op, error);
}

static void compile_program(void)
{
   prog = glCreateProgram();
   GLuint vert = glCreateShader(GL_VERTEX_SHADER);
   GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);

   glShaderSource(vert, ARRAY_SIZE(vertex_shader), vertex_shader, 0);
   glShaderSource(frag, ARRAY_SIZE(fragment_shader), fragment_shader, 0);
   glCompileShader(vert);
   glCompileShader(frag);

   GLint success;
   glGetShaderiv(vert, GL_COMPILE_STATUS, &success);
   if (!success) {
      char info_log[512];
      glGetShaderInfoLog(vert, 512, NULL, info_log);
      fprintf(stderr, "[libretro-test]: Vertex shader compilation failed: %s\n", info_log);
   }
   glGetShaderiv(frag, GL_COMPILE_STATUS, &success);
   if (!success) {
      char info_log[512];
      glGetShaderInfoLog(frag, 512, NULL, info_log);
      fprintf(stderr, "[libretro-test]: Fragment shader compilation failed: %s\n", info_log);
   }

   glAttachShader(prog, vert);
   glAttachShader(prog, frag);
   glLinkProgram(prog);
   glGetProgramiv(prog, GL_LINK_STATUS, &success);
   if (!success) {
      char info_log[512];
      glGetProgramInfoLog(prog, 512, NULL, info_log);
      fprintf(stderr, "[libretro-test]: Square program linking failed: %s\n", info_log);
   }
   glDeleteShader(vert);
   glDeleteShader(frag);
   fprintf(stderr, "[libretro-test]: Square program compiled, prog = %u\n", prog);
   check_gl_error("compile_program");
}

static void compile_text_program(void)
{
   text_prog = glCreateProgram();
   GLuint vert = glCreateShader(GL_VERTEX_SHADER);
   GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);

   glShaderSource(vert, ARRAY_SIZE(text_vertex_shader), text_vertex_shader, 0);
   glShaderSource(frag, ARRAY_SIZE(text_fragment_shader), text_fragment_shader, 0);
   glCompileShader(vert);
   glCompileShader(frag);

   GLint success;
   glGetShaderiv(vert, GL_COMPILE_STATUS, &success);
   if (!success) {
      char info_log[512];
      glGetShaderInfoLog(vert, 512, NULL, info_log);
      fprintf(stderr, "[libretro-test]: Text vertex shader compilation failed: %s\n", info_log);
   }
   glGetShaderiv(frag, GL_COMPILE_STATUS, &success);
   if (!success) {
      char info_log[512];
      glGetShaderInfoLog(frag, 512, NULL, info_log);
      fprintf(stderr, "[libretro-test]: Text fragment shader compilation failed: %s\n", info_log);
   }

   glAttachShader(text_prog, vert);
   glAttachShader(text_prog, frag);
   glLinkProgram(text_prog);
   glGetProgramiv(text_prog, GL_LINK_STATUS, &success);
   if (!success) {
      char info_log[512];
      glGetProgramInfoLog(text_prog, 512, NULL, info_log);
      fprintf(stderr, "[libretro-test]: Text program linking failed: %s\n", info_log);
   }
   glDeleteShader(vert);
   glDeleteShader(frag);
   fprintf(stderr, "[libretro-test]: Text program compiled, text_prog = %u\n", text_prog);
   check_gl_error("compile_text_program");
}

static void setup_vao(void)
{
   static const GLfloat vertex_data[] = {
      -0.5, -0.5,
      0.5, -0.5,
      -0.5, 0.5,
      0.5, 0.5,
      1.0, 1.0, 1.0, 1.0,
      1.0, 1.0, 0.0, 1.0,
      0.0, 1.0, 1.0, 1.0,
      1.0, 0.0, 1.0, 1.0,
   };

   glUseProgram(prog);
   glGenBuffers(1, &vbo);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glUseProgram(0);
   fprintf(stderr, "[libretro-test]: Square VBO created, vbo = %u\n", vbo);
   check_gl_error("setup_vao");
}

static void init_font(void)
{
   unsigned char ttf_buffer[1<<20];
   const char *system_dir = NULL;
   char font_path[512];

   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir)
      snprintf(font_path, sizeof(font_path), "%s/Kenney Pixel.ttf", system_dir);
   else
      snprintf(font_path, sizeof(font_path), "Kenney Pixel.ttf");

   FILE *f = fopen(font_path, "rb");
   if (!f)
   {
      fprintf(stderr, "[libretro-test]: Failed to load font: %s\n", font_path);
      return;
   }
   size_t n = fread(ttf_buffer, 1, sizeof(ttf_buffer), f);
   if (n == 0)
   {
      fprintf(stderr, "[libretro-test]: Failed to read font data from: %s\n", font_path);
      fclose(f);
      return;
   }
   fclose(f);

   font_bitmap = (unsigned char *)malloc(font_bitmap_width * font_bitmap_height);
   int result = stbtt_BakeFontBitmap(ttf_buffer, 0, 32.0, font_bitmap, font_bitmap_width, font_bitmap_height, 32, 96, cdata);
   if (result < 0)
   {
      fprintf(stderr, "[libretro-test]: Failed to bake font bitmap, result: %d\n", result);
      free(font_bitmap);
      font_bitmap = NULL;
      return;
   }

   // Log texture data
   int non_zero = 0;
   for (int i = 0; i < font_bitmap_width * font_bitmap_height; i++) {
      if (font_bitmap[i] > 0) non_zero++;
   }
   fprintf(stderr, "[libretro-test]: Font bitmap non-zero pixels: %d\n", non_zero);

   glGenTextures(1, &font_texture);
   glBindTexture(GL_TEXTURE_2D, font_texture);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, font_bitmap_width, font_bitmap_height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, font_bitmap);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glBindTexture(GL_TEXTURE_2D, 0);
   fprintf(stderr, "[libretro-test]: Font texture created, font_texture = %u\n", font_texture);
   check_gl_error("init_font");
}

static void draw_test_quad(float x, float y)
{
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
   check_gl_error("draw_test_quad");
}

static void draw_text(float x, float y, const char *text)
{
   if (!font_texture || !text_prog || !text_vbo)
   {
      fprintf(stderr, "[libretro-test]: Text rendering skipped: font_texture=%u, text_prog=%u, text_vbo=%u\n",
              font_texture, text_prog, text_vbo);
      return;
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
   check_gl_error("draw_text");
}

static void init_lua(void)
{
   if (!L)
   {
      L = luaL_newstate();
      luaL_openlibs(L);
      extern int luaopen_gl(lua_State *);
      luaL_requiref(L, "gl", luaopen_gl, 1);
      lua_pop(L, 1);
      fprintf(stderr, "[libretro-test]: Registered gl module\n");
   }

   const char *system_dir = NULL;
   use_lua = false;
   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir)
   {
      char script_path[512];
      snprintf(script_path, sizeof(script_path), "%s/render.lua", system_dir);
      if (luaL_dofile(L, script_path) == LUA_OK)
      {
         fprintf(stderr, "[libretro-test]: Loaded Lua script from %s\n", script_path);
         use_lua = true;
      }
      else
      {
         fprintf(stderr, "[libretro-test]: Failed to load Lua script: %s\n", lua_tostring(L, -1));
         lua_pop(L, 1);
      }
   }
   else
   {
      fprintf(stderr, "[libretro-test]: Could not get system directory, trying local render.lua\n");
      if (luaL_dofile(L, "render.lua") == LUA_OK)
      {
         fprintf(stderr, "[libretro-test]: Loaded local Lua script\n");
         use_lua = true;
      }
      else
      {
         fprintf(stderr, "[libretro-test]: Failed to load local Lua script: %s\n", lua_tostring(L, -1));
         lua_pop(L, 1);
      }
   }
}

void retro_init(void)
{
   L = NULL;
   init_lua();
}

void retro_deinit(void)
{
   if (L)
   {
      lua_close(L);
      L = NULL;
   }
   if (font_bitmap)
   {
      free(font_bitmap);
      font_bitmap = NULL;
   }
   if (font_texture)
   {
      glDeleteTextures(1, &font_texture);
      font_texture = 0;
   }
   if (text_vbo)
   {
      glDeleteBuffers(1, &text_vbo);
      text_vbo = 0;
   }
   if (text_prog)
   {
      glDeleteProgram(text_prog);
      text_prog = 0;
   }
}

unsigned retro_api_version(void) { return RETRO_API_VERSION; }
void retro_set_controller_port_device(unsigned port, unsigned device) {}
void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name = "TestCore GL";
   info->library_version = "v1";
   info->need_fullpath = false;
}
void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->timing = (struct retro_system_timing) {
      .fps = 60.0,
      .sample_rate = 0.0,
   };
   info->geometry = (struct retro_game_geometry) {
      .base_width = BASE_WIDTH,
      .base_height = BASE_HEIGHT,
      .max_width = MAX_WIDTH,
      .max_height = MAX_HEIGHT,
      .aspect_ratio = 4.0 / 3.0,
   };
}

static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;
   struct retro_variable variables[] = {
      { "testgl_resolution", "Internal resolution; 320x240|360x480|480x272|512x384|512x512|640x240|640x448|640x480|720x576|800x600|960x720|1024x768|1024x1024|1280x720|1280x960|1600x1200|1920x1080|1920x1440|1920x1600|2048x2048" },
      { NULL, NULL },
   };
   bool no_rom = true;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_rom);
   cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
}

void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }

static void update_variables(void)
{
   struct retro_variable var = { .key = "testgl_resolution" };
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      char *pch;
      char str[100];
      snprintf(str, sizeof(str), "%s", var.value);
      pch = strtok(str, "x");
      if (pch) width = strtoul(pch, NULL, 0);
      pch = strtok(NULL, "x");
      if (pch) height = strtoul(pch, NULL, 0);
      fprintf(stderr, "[libretro-test]: Got size: %u x %u.\n", width, height);
   }
}

static unsigned frame_count;

void retro_run(void)
{
   bool updated = false;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_variables();

   input_poll_cb();

   glBindFramebuffer(RARCH_GL_FRAMEBUFFER, hw_render.get_current_framebuffer());
   glClearColor(0.3, 0.4, 0.5, 1.0);
   glViewport(0, 0, width, height);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
   check_gl_error("clear");

   if (use_lua)
   {
      lua_getglobal(L, "render");
      lua_pushnumber(L, frame_count);
      lua_pushnumber(L, width);
      lua_pushnumber(L, height);
      lua_pushlightuserdata(L, (void*)&hw_render);
      if (lua_pcall(L, 4, 0, 0) != LUA_OK)
      {
         fprintf(stderr, "[libretro-test]: Lua render error: %s\n", lua_tostring(L, -1));
         lua_pop(L, 1);
         use_lua = false;
      }
   }

   if (!use_lua)
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

      int loc = glGetUniformLocation(prog, "uMVP");
      float angle = frame_count / 100.0;
      float cos_angle = cosf(angle);
      float sin_angle = sinf(angle);

      const GLfloat mvp[] = {
         cos_angle, -sin_angle, 0, 0,
         sin_angle, cos_angle, 0, 0,
         0, 0, 1, 0,
         0, 0, 0, 1,
      };
      glUniformMatrix4fv(loc, 1, GL_FALSE, mvp);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

      cos_angle *= 0.5;
      sin_angle *= 0.5;
      const GLfloat mvp2[] = {
         cos_angle, -sin_angle, 0, 0.0,
         sin_angle, cos_angle, 0, 0.0,
         0, 0, 1, 0,
         0.4, 0.4, 0.2, 1,
      };
      glUniformMatrix4fv(loc, 1, GL_FALSE, mvp2);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glDisableVertexAttribArray(vloc);
      glDisableVertexAttribArray(cloc);
      glUseProgram(0);
      check_gl_error("draw_squares");

      draw_test_quad(10.0, 10.0);
      char text[32];
      snprintf(text, sizeof(text), "Frame: %u", frame_count);
      draw_text(10.0, 10.0, text);
   }

   frame_count++;
   video_cb(RETRO_HW_FRAME_BUFFER_VALID, width, height, 0);
}

static void context_reset(void)
{
   fprintf(stderr, "Context reset!\n");
   rglgen_resolve_symbols(hw_render.get_proc_address);
   compile_program();
   setup_vao();
   compile_text_program();
   init_font();
   glGenBuffers(1, &text_vbo);
   fprintf(stderr, "[libretro-test]: Text VBO created, text_vbo = %u\n", text_vbo);
   check_gl_error("context_reset");
}

static void context_destroy(void)
{
   fprintf(stderr, "Context destroy!\n");
   glDeleteBuffers(1, &vbo);
   vbo = 0;
   glDeleteProgram(prog);
   prog = 0;
   glDeleteBuffers(1, &text_vbo);
   text_vbo = 0;
   glDeleteProgram(text_prog);
   text_prog = 0;
   if (font_bitmap)
   {
      free(font_bitmap);
      font_bitmap = NULL;
   }
   glDeleteTextures(1, &font_texture);
   font_texture = 0;
}

static bool retro_init_hw_context(void)
{
   hw_render.context_type = RETRO_HW_CONTEXT_OPENGL;
   hw_render.context_reset = context_reset;
   hw_render.context_destroy = context_destroy;
   hw_render.depth = true;
   hw_render.stencil = true;
   hw_render.bottom_left_origin = true;

   if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
      return false;

   return true;
}

bool retro_load_game(const struct retro_game_info *info)
{
   update_variables();

   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      fprintf(stderr, "XRGB8888 is not supported.\n");
      return false;
   }

   if (!retro_init_hw_context())
   {
      fprintf(stderr, "HW Context could not be initialized, exiting...\n");
      return false;
   }

   init_lua();
   fprintf(stderr, "Loaded game!\n");
   return true;
}

void retro_unload_game(void) {}
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num) { return false; }
size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *data, size_t size) { return false; }
bool retro_unserialize(const void *data, size_t size) { return false; }
void *retro_get_memory_data(unsigned id) { return NULL; }
size_t retro_get_memory_size(unsigned id) { return 0; }
void retro_reset(void)
{
   init_lua();
}
void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned index, bool enabled, const char *code) {}