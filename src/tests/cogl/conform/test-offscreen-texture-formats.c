/*
 * Copyright (C) 2022 Intel Corporation.
 * Copyright (C) 2022 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <cogl/cogl.h>

#include "tests/cogl-test-utils.h"

static int
get_bits (uint32_t in,
          int      end,
          int      begin)
{
  int mask = (1 << (end - begin + 1)) - 1;

  return (in >> begin) & mask;
}

static int
rgb10_to_rgb8 (int rgb10)
{
  float r;

  r = rgb10 / (float) ((1 << 10) - 1);
  return (int) (r * (float) ((1 << 8) - 1));
}

static int
rgb8_to_rgb10 (int rgb8)
{
  float r;

  r = rgb8 / (float) ((1 << 8) - 1);
  return (int) (r * (float) ((1 << 10) - 1));
}

static void
test_offscreen_texture_formats_store_rgb10 (void)
{
  const int rgb10_red = 514;
  const int rgb10_green = 258;
  const int rgb10_blue = 18;
  const int rgb10_alpha = 2;
  float red;
  float green;
  float blue;
  float alpha;
  GError *error = NULL;
  CoglPixelFormat formats[] = {
    COGL_PIXEL_FORMAT_XRGB_2101010,
    COGL_PIXEL_FORMAT_ARGB_2101010_PRE,
    COGL_PIXEL_FORMAT_XBGR_2101010,
    COGL_PIXEL_FORMAT_ABGR_2101010_PRE,
    COGL_PIXEL_FORMAT_RGBA_1010102_PRE,
    COGL_PIXEL_FORMAT_BGRA_1010102_PRE,
  };
  int i;

  /* The extra fraction is there to avoid rounding inconsistencies in OpenGL
   * implementations. */
  red = (rgb10_red / (float) ((1 << 10) - 1)) + 0.00001;
  green = (rgb10_green / (float) ((1 << 10) - 1)) + 0.00001;
  blue = (rgb10_blue / (float) ((1 << 10) - 1)) + 0.00001;
  alpha = (rgb10_alpha / (float) ((1 << 2) - 1)) + 0.00001;

  /* Make sure that that the color value can't be represented using rgb8. */
  g_assert_cmpint (rgb8_to_rgb10 (rgb10_to_rgb8 (rgb10_red)), !=, rgb10_red);
  g_assert_cmpint (rgb8_to_rgb10 (rgb10_to_rgb8 (rgb10_green)), !=, rgb10_green);
  g_assert_cmpint (rgb8_to_rgb10 (rgb10_to_rgb8 (rgb10_blue)), !=, rgb10_blue);

  for (i = 0; i < G_N_ELEMENTS (formats); i++)
    {
      CoglTexture2D *tex;
      CoglOffscreen *offscreen;
      uint32_t rgb8_readback[4];
      int j, k;

      /* Allocate 2x2 to ensure we avoid any fast paths. */
      tex = cogl_texture_2d_new_with_format (test_ctx, 2, 2, formats[i]);

      offscreen = cogl_offscreen_new_with_texture (COGL_TEXTURE (tex));
      cogl_framebuffer_allocate (COGL_FRAMEBUFFER (offscreen), &error);
      g_assert_no_error (error);

      cogl_framebuffer_clear4f (COGL_FRAMEBUFFER (offscreen),
                                COGL_BUFFER_BIT_COLOR,
                                red, green, blue, alpha);

      for (j = 0; j < G_N_ELEMENTS (formats); j++)
        {
          uint32_t rgb10_readback[4];
          int alpha_out;

          cogl_framebuffer_read_pixels (COGL_FRAMEBUFFER (offscreen), 0, 0, 2, 2,
                                        formats[j],
                                        (uint8_t *) &rgb10_readback);

          for (k = 0; k < 4; k++)
            {
              int channels[3];

              switch (formats[j])
                {
                case COGL_PIXEL_FORMAT_RGBA_1010102_PRE:
                case COGL_PIXEL_FORMAT_BGRA_1010102_PRE:
                  channels[0] = get_bits (rgb10_readback[k], 31, 22);
                  channels[1] = get_bits (rgb10_readback[k], 21, 12);
                  channels[2] = get_bits (rgb10_readback[k], 11, 2);
                  alpha_out = get_bits (rgb10_readback[k], 1, 0);
                  break;
                case COGL_PIXEL_FORMAT_XRGB_2101010:
                case COGL_PIXEL_FORMAT_ARGB_2101010_PRE:
                case COGL_PIXEL_FORMAT_XBGR_2101010:
                case COGL_PIXEL_FORMAT_ABGR_2101010_PRE:
                  alpha_out = get_bits (rgb10_readback[k], 31, 30);
                  channels[0] = get_bits (rgb10_readback[k], 29, 20);
                  channels[1] = get_bits (rgb10_readback[k], 19, 10);
                  channels[2] = get_bits (rgb10_readback[k], 9, 0);
                  break;
                default:
                  g_assert_not_reached ();
                }

              if ((formats[i] & COGL_A_BIT) && (formats[j] & COGL_A_BIT))
                g_assert_cmpint (alpha_out, ==, rgb10_alpha);
              else if (!(formats[i] & COGL_A_BIT) && !(formats[j] & COGL_A_BIT))
                g_assert_cmpint (alpha_out, ==, 0x3);

              switch (formats[j])
                {
                case COGL_PIXEL_FORMAT_RGBA_1010102_PRE:
                case COGL_PIXEL_FORMAT_XRGB_2101010:
                case COGL_PIXEL_FORMAT_ARGB_2101010_PRE:
                  g_assert_cmpint (channels[0], ==, rgb10_red);
                  g_assert_cmpint (channels[1], ==, rgb10_green);
                  g_assert_cmpint (channels[2], ==, rgb10_blue);
                  break;
                case COGL_PIXEL_FORMAT_BGRA_1010102_PRE:
                case COGL_PIXEL_FORMAT_XBGR_2101010:
                case COGL_PIXEL_FORMAT_ABGR_2101010_PRE:
                  g_assert_cmpint (channels[0], ==, rgb10_blue);
                  g_assert_cmpint (channels[1], ==, rgb10_green);
                  g_assert_cmpint (channels[2], ==, rgb10_red);
                  break;
                default:
                  g_assert_not_reached ();
                }
            }
        }

      cogl_framebuffer_read_pixels (COGL_FRAMEBUFFER (offscreen), 0, 0, 2, 2,
                                    COGL_PIXEL_FORMAT_RGBX_8888,
                                    (uint8_t *) &rgb8_readback);
      for (k = 0; k < 4; k++)
        {
          uint8_t *rgb8_buf = (uint8_t *) &rgb8_readback[k];

          g_assert_cmpint (rgb8_buf[0], ==, rgb10_to_rgb8 (rgb10_red));
          g_assert_cmpint (rgb8_buf[1], ==, rgb10_to_rgb8 (rgb10_green));
          g_assert_cmpint (rgb8_buf[2], ==, rgb10_to_rgb8 (rgb10_blue));

          if (!(formats[i] & COGL_A_BIT))
            g_assert_cmpint (rgb8_buf[3], ==, 0xff);
        }

      g_object_unref (offscreen);
      cogl_object_unref (tex);
    }
}

static void
test_offscreen_texture_formats_store_rgb8 (void)
{
  CoglColor color;
  const uint8_t red = 0xab;
  const uint8_t green = 0x1f;
  const uint8_t blue = 0x50;
  const uint8_t alpha = 0x34;
  CoglPixelFormat formats[] = {
    COGL_PIXEL_FORMAT_RGBX_8888,
    COGL_PIXEL_FORMAT_RGBA_8888_PRE,
    COGL_PIXEL_FORMAT_BGRX_8888,
    COGL_PIXEL_FORMAT_BGRA_8888_PRE,
    COGL_PIXEL_FORMAT_XRGB_8888,
    COGL_PIXEL_FORMAT_ARGB_8888_PRE,
    COGL_PIXEL_FORMAT_XBGR_8888,
    COGL_PIXEL_FORMAT_ABGR_8888_PRE,
  };
  int i;

  cogl_color_init_from_4ub (&color, red, green, blue, alpha);

  for (i = 0; i < G_N_ELEMENTS (formats); i++)
    {
      CoglTexture2D *tex;
      CoglOffscreen *offscreen;
      GError *error = NULL;
      int j;

      /* Allocate 2x2 to ensure we avoid any fast paths. */
      tex = cogl_texture_2d_new_with_format (test_ctx, 2, 2, formats[i]);

      offscreen = cogl_offscreen_new_with_texture (COGL_TEXTURE (tex));
      cogl_framebuffer_allocate (COGL_FRAMEBUFFER (offscreen), &error);
      g_assert_no_error (error);

      cogl_framebuffer_clear (COGL_FRAMEBUFFER (offscreen),
                              COGL_BUFFER_BIT_COLOR,
                              &color);

      for (j = 0; j < G_N_ELEMENTS (formats); j++)
        {
          uint8_t rgba_readback[4 * 4] = {};
          int alpha_out;
          int k;

          cogl_framebuffer_read_pixels (COGL_FRAMEBUFFER (offscreen), 0, 0, 2, 2,
                                        formats[j],
                                        (uint8_t *) &rgba_readback);

          for (k = 0; k < 4; k++)
            {
              switch (formats[j])
                {
                case COGL_PIXEL_FORMAT_RGBX_8888:
                case COGL_PIXEL_FORMAT_RGBA_8888_PRE:
                  g_assert_cmpint (rgba_readback[k * 4 + 0], ==, red);
                  g_assert_cmpint (rgba_readback[k * 4 + 1], ==, green);
                  g_assert_cmpint (rgba_readback[k * 4 + 2], ==, blue);
                  alpha_out = rgba_readback[k * 4 + 3];
                  break;
                case COGL_PIXEL_FORMAT_XRGB_8888:
                case COGL_PIXEL_FORMAT_ARGB_8888_PRE:
                  alpha_out = rgba_readback[k * 4 + 0];
                  g_assert_cmpint (rgba_readback[k * 4 + 1], ==, red);
                  g_assert_cmpint (rgba_readback[k * 4 + 2], ==, green);
                  g_assert_cmpint (rgba_readback[k * 4 + 3], ==, blue);
                  break;
                case COGL_PIXEL_FORMAT_BGRX_8888:
                case COGL_PIXEL_FORMAT_BGRA_8888_PRE:
                  g_assert_cmpint (rgba_readback[k * 4 + 0], ==, blue);
                  g_assert_cmpint (rgba_readback[k * 4 + 1], ==, green);
                  g_assert_cmpint (rgba_readback[k * 4 + 2], ==, red);
                  alpha_out = rgba_readback[k * 4 + 3];
                  break;
                case COGL_PIXEL_FORMAT_XBGR_8888:
                case COGL_PIXEL_FORMAT_ABGR_8888_PRE:
                  alpha_out = rgba_readback[k * 4 + 0];
                  g_assert_cmpint (rgba_readback[k * 4 + 1], ==, blue);
                  g_assert_cmpint (rgba_readback[k * 4 + 2], ==, green);
                  g_assert_cmpint (rgba_readback[k * 4 + 3], ==, red);
                  break;
                default:
                  g_assert_not_reached ();
                }

              if ((formats[i] & COGL_A_BIT) && (formats[j] & COGL_A_BIT))
                g_assert_cmpint (alpha_out, ==, alpha);
              else if (!(formats[i] & COGL_A_BIT) && !(formats[j] & COGL_A_BIT))
                g_assert_cmpint (alpha_out, ==, 0xff);
            }
        }

      g_object_unref (offscreen);
      cogl_object_unref (tex);
    }
}

static void
test_offscreen_texture_formats_paint_rgb10 (void)
{
  const int rgb10_red = 514;
  const int rgb10_green = 258;
  const int rgb10_blue = 18;
  const int rgb10_alpha = 2;
  float red;
  float green;
  float blue;
  float alpha;
  CoglPixelFormat formats[] = {
    COGL_PIXEL_FORMAT_XRGB_2101010,
    COGL_PIXEL_FORMAT_ARGB_2101010_PRE,
    COGL_PIXEL_FORMAT_XBGR_2101010,
    COGL_PIXEL_FORMAT_ABGR_2101010_PRE,
    COGL_PIXEL_FORMAT_RGBA_1010102_PRE,
    COGL_PIXEL_FORMAT_BGRA_1010102_PRE,
  };
  int i;

  /* The extra fraction is there to avoid rounding inconsistencies in OpenGL
   * implementations. */
  red = (rgb10_red / (float) ((1 << 10 ) - 1)) + 0.00001;
  green = (rgb10_green / (float) ((1 << 10) - 1)) + 0.00001;
  blue = (rgb10_blue / (float) ((1 << 10) - 1)) + 0.00001;
  alpha = (rgb10_alpha / (float) ((1 << 2) - 1)) + 0.00001;

  /* Make sure that that the color value can't be represented using rgb8. */
  g_assert_cmpint (rgb8_to_rgb10 (rgb10_to_rgb8 (rgb10_red)), !=, rgb10_red);
  g_assert_cmpint (rgb8_to_rgb10 (rgb10_to_rgb8 (rgb10_green)), !=, rgb10_green);
  g_assert_cmpint (rgb8_to_rgb10 (rgb10_to_rgb8 (rgb10_blue)), !=, rgb10_blue);

  for (i = 0; i < G_N_ELEMENTS (formats); i++)
    {
      CoglTexture2D *tex_src;
      CoglOffscreen *offscreen_src;
      GError *error = NULL;
      int j;

      tex_src = cogl_texture_2d_new_with_format (test_ctx, 2, 2, formats[i]);
      offscreen_src = cogl_offscreen_new_with_texture (COGL_TEXTURE (tex_src));
      cogl_framebuffer_allocate (COGL_FRAMEBUFFER (offscreen_src), &error);
      g_assert_no_error (error);

      for (j = 0; j < G_N_ELEMENTS (formats); j++)
        {
          CoglTexture2D *tex_dst;
          CoglOffscreen *offscreen_dst;
          CoglPipeline *pipeline;
          uint32_t rgb10_readback[4];
          int k;

          tex_dst = cogl_texture_2d_new_with_format (test_ctx, 2, 2, formats[j]);
          offscreen_dst = cogl_offscreen_new_with_texture (COGL_TEXTURE (tex_dst));
          cogl_framebuffer_allocate (COGL_FRAMEBUFFER (offscreen_dst), &error);
          g_assert_no_error (error);

          cogl_framebuffer_clear4f (COGL_FRAMEBUFFER (offscreen_src),
                                    COGL_BUFFER_BIT_COLOR,
                                    red, green, blue, alpha);

          pipeline = cogl_pipeline_new (test_ctx);
          cogl_pipeline_set_blend (pipeline,
                                   "RGBA = ADD (SRC_COLOR, 0)", NULL);
          cogl_pipeline_set_layer_texture (pipeline, 0, tex_src);
          cogl_framebuffer_draw_rectangle (COGL_FRAMEBUFFER (offscreen_dst),
                                           pipeline,
                                           -1.0, -1.0, 1.0, 1.0);
          cogl_object_unref (pipeline);

          cogl_framebuffer_read_pixels (COGL_FRAMEBUFFER (offscreen_dst),
                                        0, 0, 2, 2, formats[j],
                                        (uint8_t *) &rgb10_readback);

          for (k = 0; k < 4; k++)
            {
              int channels[3];
              int alpha_out;

              switch (formats[j])
                {
                case COGL_PIXEL_FORMAT_RGBA_1010102_PRE:
                case COGL_PIXEL_FORMAT_BGRA_1010102_PRE:
                  channels[0] = get_bits (rgb10_readback[k], 31, 22);
                  channels[1] = get_bits (rgb10_readback[k], 21, 12);
                  channels[2] = get_bits (rgb10_readback[k], 11, 2);
                  alpha_out = get_bits (rgb10_readback[k], 1, 0);
                  break;
                case COGL_PIXEL_FORMAT_XRGB_2101010:
                case COGL_PIXEL_FORMAT_ARGB_2101010_PRE:
                case COGL_PIXEL_FORMAT_XBGR_2101010:
                case COGL_PIXEL_FORMAT_ABGR_2101010_PRE:
                  alpha_out = get_bits (rgb10_readback[k], 31, 30);
                  channels[0] = get_bits (rgb10_readback[k], 29, 20);
                  channels[1] = get_bits (rgb10_readback[k], 19, 10);
                  channels[2] = get_bits (rgb10_readback[k], 9, 0);
                  break;
                default:
                  g_assert_not_reached ();
                }

              if ((formats[i] & COGL_A_BIT) && (formats[j] & COGL_A_BIT))
                g_assert_cmpint (alpha_out, ==, rgb10_alpha);
              else if (!(formats[i] & COGL_A_BIT) && !(formats[j] & COGL_A_BIT))
                g_assert_cmpint (alpha_out, ==, 0x3);

              switch (formats[j])
                {
                case COGL_PIXEL_FORMAT_RGBA_1010102_PRE:
                case COGL_PIXEL_FORMAT_XRGB_2101010:
                case COGL_PIXEL_FORMAT_ARGB_2101010_PRE:
                  g_assert_cmpint (channels[0], ==, rgb10_red);
                  g_assert_cmpint (channels[1], ==, rgb10_green);
                  g_assert_cmpint (channels[2], ==, rgb10_blue);
                  break;
                case COGL_PIXEL_FORMAT_BGRA_1010102_PRE:
                case COGL_PIXEL_FORMAT_XBGR_2101010:
                case COGL_PIXEL_FORMAT_ABGR_2101010_PRE:
                  g_assert_cmpint (channels[0], ==, rgb10_blue);
                  g_assert_cmpint (channels[1], ==, rgb10_green);
                  g_assert_cmpint (channels[2], ==, rgb10_red);
                  break;
                default:
                  g_assert_not_reached ();
                }
            }

          g_object_unref (offscreen_dst);
          cogl_object_unref (tex_dst);
        }

      g_object_unref (offscreen_src);
      cogl_object_unref (tex_src);
    }
}

static void
test_offscreen_texture_formats_paint_rgb8 (void)
{
  CoglColor color;
  const uint8_t red = 0xab;
  const uint8_t green = 0x1f;
  const uint8_t blue = 0x50;
  const uint8_t alpha = 0x34;
  CoglPixelFormat formats[] = {
    COGL_PIXEL_FORMAT_RGBX_8888,
    COGL_PIXEL_FORMAT_RGBA_8888_PRE,
    COGL_PIXEL_FORMAT_BGRX_8888,
    COGL_PIXEL_FORMAT_BGRA_8888_PRE,
    COGL_PIXEL_FORMAT_XRGB_8888,
    COGL_PIXEL_FORMAT_ARGB_8888_PRE,
    COGL_PIXEL_FORMAT_XBGR_8888,
    COGL_PIXEL_FORMAT_ABGR_8888_PRE,
  };
  int i;

  cogl_color_init_from_4ub (&color, red, green, blue, alpha);

  for (i = 0; i < G_N_ELEMENTS (formats); i++)
    {
      CoglTexture2D *tex_src;
      CoglOffscreen *offscreen_src;
      GError *error = NULL;
      int j;

      tex_src = cogl_texture_2d_new_with_format (test_ctx, 2, 2, formats[i]);
      offscreen_src = cogl_offscreen_new_with_texture (COGL_TEXTURE (tex_src));
      cogl_framebuffer_allocate (COGL_FRAMEBUFFER (offscreen_src), &error);
      g_assert_no_error (error);

      for (j = 0; j < G_N_ELEMENTS (formats); j++)
        {
          CoglTexture2D *tex_dst;
          CoglOffscreen *offscreen_dst;
          CoglPipeline *pipeline;
          uint8_t rgba_readback[4 * 4] = {};
          int k;

          tex_dst = cogl_texture_2d_new_with_format (test_ctx, 2, 2, formats[j]);
          offscreen_dst = cogl_offscreen_new_with_texture (COGL_TEXTURE (tex_dst));
          cogl_framebuffer_allocate (COGL_FRAMEBUFFER (offscreen_dst), &error);
          g_assert_no_error (error);

          cogl_framebuffer_clear (COGL_FRAMEBUFFER (offscreen_src),
                                  COGL_BUFFER_BIT_COLOR,
                                  &color);

          pipeline = cogl_pipeline_new (test_ctx);
          cogl_pipeline_set_blend (pipeline,
                                   "RGBA = ADD (SRC_COLOR, 0)", NULL);
          cogl_pipeline_set_layer_texture (pipeline, 0, tex_src);
          cogl_framebuffer_draw_rectangle (COGL_FRAMEBUFFER (offscreen_dst),
                                           pipeline,
                                           -1.0, -1.0, 1.0, 1.0);
          cogl_object_unref (pipeline);

          cogl_framebuffer_read_pixels (COGL_FRAMEBUFFER (offscreen_dst),
                                        0, 0, 2, 2, formats[j],
                                        (uint8_t *) &rgba_readback);

          for (k = 0; k < 4; k++)
            {
              int alpha_out;

              switch (formats[j])
                {
                case COGL_PIXEL_FORMAT_RGBX_8888:
                case COGL_PIXEL_FORMAT_RGBA_8888_PRE:
                  g_assert_cmpint (rgba_readback[k * 4 + 0], ==, red);
                  g_assert_cmpint (rgba_readback[k * 4 + 1], ==, green);
                  g_assert_cmpint (rgba_readback[k * 4 + 2], ==, blue);
                  alpha_out = rgba_readback[k * 4 + 3];
                  break;
                case COGL_PIXEL_FORMAT_XRGB_8888:
                case COGL_PIXEL_FORMAT_ARGB_8888_PRE:
                  alpha_out = rgba_readback[k * 4 + 0];
                  g_assert_cmpint (rgba_readback[k * 4 + 1], ==, red);
                  g_assert_cmpint (rgba_readback[k * 4 + 2], ==, green);
                  g_assert_cmpint (rgba_readback[k * 4 + 3], ==, blue);
                  break;
                case COGL_PIXEL_FORMAT_BGRX_8888:
                case COGL_PIXEL_FORMAT_BGRA_8888_PRE:
                  g_assert_cmpint (rgba_readback[k * 4 + 0], ==, blue);
                  g_assert_cmpint (rgba_readback[k * 4 + 1], ==, green);
                  g_assert_cmpint (rgba_readback[k * 4 + 2], ==, red);
                  alpha_out = rgba_readback[k * 4 + 3];
                  break;
                case COGL_PIXEL_FORMAT_XBGR_8888:
                case COGL_PIXEL_FORMAT_ABGR_8888_PRE:
                  alpha_out = rgba_readback[k * 4 + 0];
                  g_assert_cmpint (rgba_readback[k * 4 + 1], ==, blue);
                  g_assert_cmpint (rgba_readback[k * 4 + 2], ==, green);
                  g_assert_cmpint (rgba_readback[k * 4 + 3], ==, red);
                  break;
                default:
                  g_assert_not_reached ();
                }

              if ((formats[i] & COGL_A_BIT) && (formats[j] & COGL_A_BIT))
                g_assert_cmpint (alpha_out, ==, alpha);
              else if (!(formats[i] & COGL_A_BIT) && !(formats[j] & COGL_A_BIT))
                g_assert_cmpint (alpha_out, ==, 0xff);
            }

          g_object_unref (offscreen_dst);
          cogl_object_unref (tex_dst);
        }

      g_object_unref (offscreen_src);
      cogl_object_unref (tex_src);
    }
}

COGL_TEST_SUITE (
  g_test_add_func ("/offscreen/texture-formats/store-rgb10",
                   test_offscreen_texture_formats_store_rgb10);
  g_test_add_func ("/offscreen/texture-formats/store-8",
                   test_offscreen_texture_formats_store_rgb8);
  g_test_add_func ("/offscreen/texture-formats/paint-rgb10",
                   test_offscreen_texture_formats_paint_rgb10);
  g_test_add_func ("/offscreen/texture-formats/paint-rgb8",
                   test_offscreen_texture_formats_paint_rgb8);
)