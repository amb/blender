/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2016 by Mike Erwin.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#include "GPU_glew.h"

#include "GPU_shader_interface.h"
#include "GPU_vertex_buffer.h"

#include "gpu_vertex_format_private.h"

#include "gl_batch.hh"
#include "gl_context.hh"

#include "gl_vertex_array.hh"

using namespace blender::gpu;

/* -------------------------------------------------------------------- */
/** \name Vertex Array Bindings
 * \{ */

/* Returns enabled vertex pointers as a bitflag (one bit per attrib). */
static uint16_t vbo_bind(const GPUShaderInterface *interface,
                         const GPUVertFormat *format,
                         uint v_first,
                         uint v_len,
                         const bool use_instancing)
{
  uint16_t enabled_attrib = 0;
  const uint attr_len = format->attr_len;
  uint stride = format->stride;
  uint offset = 0;
  GLuint divisor = (use_instancing) ? 1 : 0;

  for (uint a_idx = 0; a_idx < attr_len; a_idx++) {
    const GPUVertAttr *a = &format->attrs[a_idx];

    if (format->deinterleaved) {
      offset += ((a_idx == 0) ? 0 : format->attrs[a_idx - 1].sz) * v_len;
      stride = a->sz;
    }
    else {
      offset = a->offset;
    }

    const GLvoid *pointer = (const GLubyte *)0 + offset + v_first * stride;
    const GLenum type = convert_comp_type_to_gl(static_cast<GPUVertCompType>(a->comp_type));

    for (uint n_idx = 0; n_idx < a->name_len; n_idx++) {
      const char *name = GPU_vertformat_attr_name_get(format, a, n_idx);
      const GPUShaderInput *input = GPU_shaderinterface_attr(interface, name);

      if (input == NULL) {
        continue;
      }

      enabled_attrib |= (1 << input->location);

      if (a->comp_len == 16 || a->comp_len == 12 || a->comp_len == 8) {
        BLI_assert(a->fetch_mode == GPU_FETCH_FLOAT);
        BLI_assert(a->comp_type == GPU_COMP_F32);
        for (int i = 0; i < a->comp_len / 4; i++) {
          glEnableVertexAttribArray(input->location + i);
          glVertexAttribDivisor(input->location + i, divisor);
          glVertexAttribPointer(
              input->location + i, 4, type, GL_FALSE, stride, (const GLubyte *)pointer + i * 16);
        }
      }
      else {
        glEnableVertexAttribArray(input->location);
        glVertexAttribDivisor(input->location, divisor);

        switch (a->fetch_mode) {
          case GPU_FETCH_FLOAT:
          case GPU_FETCH_INT_TO_FLOAT:
            glVertexAttribPointer(input->location, a->comp_len, type, GL_FALSE, stride, pointer);
            break;
          case GPU_FETCH_INT_TO_FLOAT_UNIT:
            glVertexAttribPointer(input->location, a->comp_len, type, GL_TRUE, stride, pointer);
            break;
          case GPU_FETCH_INT:
            glVertexAttribIPointer(input->location, a->comp_len, type, stride, pointer);
            break;
        }
      }
    }
  }
  return enabled_attrib;
}

/* Update the Attrib Binding of the currently bound VAO. */
void GLVertArray::update_bindings(const GLuint vao,
                                  const GPUBatch *batch,
                                  const GPUShaderInterface *interface,
                                  const int base_instance)
{
  uint16_t attr_mask = interface->enabled_attr_mask;

  glBindVertexArray(vao);

  /* Reverse order so first VBO'S have more prevalence (in term of attribute override). */
  for (int v = GPU_BATCH_VBO_MAX_LEN - 1; v > -1; v--) {
    GPUVertBuf *vbo = batch->verts[v];
    if (vbo) {
      GPU_vertbuf_use(vbo);
      attr_mask &= ~vbo_bind(interface, &vbo->format, 0, vbo->vertex_len, false);
    }
  }

  for (int v = GPU_BATCH_INST_VBO_MAX_LEN - 1; v > -1; v--) {
    GPUVertBuf *vbo = batch->inst[v];
    if (vbo) {
      GPU_vertbuf_use(vbo);
      attr_mask &= ~vbo_bind(interface, &vbo->format, base_instance, vbo->vertex_len, true);
    }
  }

  if (attr_mask != 0 && GLEW_ARB_vertex_attrib_binding) {
    for (uint16_t mask = 1, a = 0; a < 16; a++, mask <<= 1) {
      if (attr_mask & mask) {
        GLContext *ctx = static_cast<GLContext *>(GPU_context_active_get());
        /* This replaces glVertexAttrib4f(a, 0.0f, 0.0f, 0.0f, 1.0f); with a more modern style.
         * Fix issues for some drivers (see T75069). */
        glBindVertexBuffer(a, ctx->default_attr_vbo_, (intptr_t)0, (intptr_t)0);
        glEnableVertexAttribArray(a);
        glVertexAttribFormat(a, 4, GL_FLOAT, GL_FALSE, 0);
        glVertexAttribBinding(a, a);
      }
    }
  }

  if (batch->elem) {
    /* Binds the index buffer. This state is also saved in the VAO. */
    GPU_indexbuf_use(batch->elem);
  }
}

/** \} */
