#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "../include/gl.h"
#include "../include/glext.h"
#include "../include/glkos.h"

#include "private.h"

static PolyContext GL_CONTEXT;

PolyContext *_glGetPVRContext() {
    return &GL_CONTEXT;
}

/* We can't just use the GL_CONTEXT for this state as the two
 * GL states are combined, so we store them separately and then
 * calculate the appropriate PVR state from them. */
static GLenum CULL_FACE = GL_BACK;
static GLenum FRONT_FACE = GL_CCW;
static GLboolean CULLING_ENABLED = GL_FALSE;
static GLboolean COLOR_MATERIAL_ENABLED = GL_FALSE;

static GLboolean LIGHTING_ENABLED = GL_FALSE;

/* Is the shared texture palette enabled? */
static GLboolean SHARED_PALETTE_ENABLED = GL_FALSE;

static GLboolean ALPHA_TEST_ENABLED = GL_FALSE;

static GLboolean POLYGON_OFFSET_ENABLED = GL_FALSE;

static GLboolean NORMALIZE_ENABLED = GL_FALSE;

GLboolean _glIsSharedTexturePaletteEnabled() {
    return SHARED_PALETTE_ENABLED;
}

static int _calc_pvr_face_culling() {
    if(!CULLING_ENABLED) {
        return GPU_CULLING_NONE;
    } else {
        if(CULL_FACE == GL_BACK) {
            return (FRONT_FACE == GL_CW) ? GPU_CULLING_CCW : GPU_CULLING_CW;
        } else {
            return (FRONT_FACE == GL_CCW) ? GPU_CULLING_CCW : GPU_CULLING_CW;
        }
    }
}

static GLenum DEPTH_FUNC = GL_LESS;
static GLboolean DEPTH_TEST_ENABLED = GL_FALSE;

static int _calc_pvr_depth_test() {
    if(!DEPTH_TEST_ENABLED) {
        return GPU_DEPTHCMP_ALWAYS;
    }

    switch(DEPTH_FUNC) {
        case GL_NEVER:
            return GPU_DEPTHCMP_NEVER;
        case GL_LESS:
            return GPU_DEPTHCMP_GREATER;
        case GL_EQUAL:
            return GPU_DEPTHCMP_EQUAL;
        case GL_LEQUAL:
            return GPU_DEPTHCMP_GEQUAL;
        case GL_GREATER:
            return GPU_DEPTHCMP_LESS;
        case GL_NOTEQUAL:
            return GPU_DEPTHCMP_NOTEQUAL;
        case GL_GEQUAL:
            return GPU_DEPTHCMP_LEQUAL;
        break;
        case GL_ALWAYS:
        default:
            return GPU_DEPTHCMP_ALWAYS;
    }
}

static GLenum BLEND_SFACTOR = GL_ONE;
static GLenum BLEND_DFACTOR = GL_ZERO;
static GLboolean BLEND_ENABLED = GL_FALSE;

static GLfloat OFFSET_FACTOR = 0.0f;
static GLfloat OFFSET_UNITS = 0.0f;

GLboolean _glIsNormalizeEnabled() {
    return NORMALIZE_ENABLED;
}

GLboolean _glIsBlendingEnabled() {
    return BLEND_ENABLED;
}

GLboolean _glIsAlphaTestEnabled() {
    return ALPHA_TEST_ENABLED;
}

static int _calcPVRBlendFactor(GLenum factor) {
    switch(factor) {
    case GL_ZERO:
        return GPU_BLEND_ZERO;
    case GL_SRC_ALPHA:
        return GPU_BLEND_SRCALPHA;
    case GL_DST_COLOR:
        return GPU_BLEND_DESTCOLOR;
    case GL_DST_ALPHA:
        return GPU_BLEND_DESTALPHA;
    case GL_ONE_MINUS_DST_COLOR:
        return GPU_BLEND_INVDESTCOLOR;
    case GL_ONE_MINUS_SRC_ALPHA:
        return GPU_BLEND_INVSRCALPHA;
    case GL_ONE_MINUS_DST_ALPHA:
        return GPU_BLEND_INVDESTALPHA;
    case GL_ONE:
        return GPU_BLEND_ONE;
    default:
        fprintf(stderr, "Invalid blend mode: %u\n", (unsigned int) factor);
        return GPU_BLEND_ONE;
    }
}

static void _updatePVRBlend(PolyContext* context) {
    if(BLEND_ENABLED || ALPHA_TEST_ENABLED) {
        context->gen.alpha = GPU_ALPHA_ENABLE;
    } else {
        context->gen.alpha = GPU_ALPHA_DISABLE;
    }

    context->blend.src = _calcPVRBlendFactor(BLEND_SFACTOR);
    context->blend.dst = _calcPVRBlendFactor(BLEND_DFACTOR);
}

GLboolean _glCheckValidEnum(GLint param, GLint* values, const char* func) {
    GLubyte found = 0;
    while(*values != 0) {
        if(*values == param) {
            found++;
            break;
        }
        values++;
    }

    if(!found) {
        _glKosThrowError(GL_INVALID_ENUM, func);
        _glKosPrintError();
        return GL_TRUE;
    }

    return GL_FALSE;
}

GLboolean TEXTURES_ENABLED [] = {GL_FALSE, GL_FALSE};

void _glUpdatePVRTextureContext(PolyContext *context, GLshort textureUnit) {
    const TextureObject *tx1 = (textureUnit == 0) ? _glGetTexture0() : _glGetTexture1();

    /* Disable all texturing to start with */
    context->txr.enable = GPU_TEXTURE_DISABLE;
    context->txr2.enable = GPU_TEXTURE_DISABLE;
    context->txr2.alpha = GPU_TXRALPHA_DISABLE;

    if(!TEXTURES_ENABLED[textureUnit] || !tx1) {
        return;
    }

    context->txr.alpha = (BLEND_ENABLED || ALPHA_TEST_ENABLED) ? GPU_TXRALPHA_ENABLE : GPU_TXRALPHA_DISABLE;

    GLuint filter = GPU_FILTER_NEAREST;
    GLboolean enableMipmaps = GL_FALSE;

    switch(tx1->minFilter) {
        case GL_NEAREST_MIPMAP_LINEAR:
        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_LINEAR_MIPMAP_LINEAR:
        case GL_LINEAR_MIPMAP_NEAREST:
            enableMipmaps = GL_TRUE;
        break;
    default:
        enableMipmaps = GL_FALSE;
        break;
    }

    /* FIXME: If you disable mipmaps on a compressed mipmapped texture
     * you get corruption and I don't know why, so we force mipmapping for now */
    if(tx1->isCompressed && _glIsMipmapComplete(tx1)) {
        enableMipmaps = GL_TRUE;
    }

    if(tx1->height != tx1->width){
        enableMipmaps = GL_FALSE;
    }

    if(enableMipmaps) {
        if(tx1->minFilter == GL_LINEAR_MIPMAP_NEAREST) {
            filter = GPU_FILTER_TRILINEAR1;
        } else if(tx1->minFilter == GL_LINEAR_MIPMAP_LINEAR) {
            filter = GPU_FILTER_TRILINEAR2;
        } else if(tx1->minFilter == GL_NEAREST_MIPMAP_LINEAR) {
            filter = GPU_FILTER_BILINEAR;
        } else {
            filter = GPU_FILTER_NEAREST;
        }
    } else {
        if(tx1->minFilter == GL_LINEAR && tx1->magFilter == GL_LINEAR) {
            filter = GPU_FILTER_BILINEAR;
        }
    }

    /* If we don't have complete mipmaps, and yet mipmapping was enabled, we disable texturing.
     * This is effectively what standard GL does (it renders a white texture)
     */
    if(!_glIsMipmapComplete(tx1) && enableMipmaps) {
        return;
    }

    if(tx1->data) {
        context->txr.enable = GPU_TEXTURE_ENABLE;
        context->txr.filter = filter;
        context->txr.width = tx1->width;
        context->txr.height = tx1->height;
        context->txr.mipmap = enableMipmaps;
        context->txr.mipmap_bias = tx1->mipmap_bias;

        if(enableMipmaps) {
            context->txr.base = tx1->data;
        } else {
            context->txr.base = tx1->data + tx1->baseDataOffset;
        }

        context->txr.format = tx1->color;

        if(tx1->isPaletted) {
            if(_glIsSharedTexturePaletteEnabled()) {
                TexturePalette* palette = _glGetSharedPalette(tx1->shared_bank);
                context->txr.format |= PVR_TXRFMT_8BPP_PAL(palette->bank);
            } else {
                context->txr.format |= PVR_TXRFMT_8BPP_PAL((tx1->palette) ? tx1->palette->bank : 0);
            }
        }

        context->txr.env = tx1->env;
        context->txr.uv_flip = GPU_UVFLIP_NONE;
        context->txr.uv_clamp = tx1->uv_clamp;
    }
}

GLboolean _glIsLightingEnabled() {
    return LIGHTING_ENABLED;
}

GLboolean _glIsColorMaterialEnabled() {
    return COLOR_MATERIAL_ENABLED;
}

static GLfloat CLEAR_COLOUR[3];

void _glInitContext() {
    memset(&GL_CONTEXT, 0, sizeof(PolyContext));

    GL_CONTEXT.list_type = GPU_LIST_OP_POLY;
    GL_CONTEXT.fmt.color = GPU_CLRFMT_ARGBPACKED;
    GL_CONTEXT.fmt.uv = GPU_UVFMT_32BIT;
    GL_CONTEXT.gen.color_clamp = GPU_CLRCLAMP_DISABLE;

    glClearDepth(1.0f);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glShadeModel(GL_SMOOTH);
    glClearColor(0, 0, 0, 0);

    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_FOG);
    glDisable(GL_LIGHTING);

    GLubyte i;
    for(i = 0; i < MAX_LIGHTS; ++i) {
        glDisable(GL_LIGHT0 + i);
    }
}

GLAPI void APIENTRY glEnable(GLenum cap) {
    switch(cap) {
        case GL_TEXTURE_2D:
            TEXTURES_ENABLED[_glGetActiveTexture()] = GL_TRUE;
        break;
        case GL_CULL_FACE: {
            CULLING_ENABLED = GL_TRUE;
            GL_CONTEXT.gen.culling = _calc_pvr_face_culling();
        } break;
        case GL_DEPTH_TEST: {
            DEPTH_TEST_ENABLED = GL_TRUE;
            GL_CONTEXT.depth.comparison = _calc_pvr_depth_test();
        } break;
        case GL_BLEND: {
            BLEND_ENABLED = GL_TRUE;
            _updatePVRBlend(&GL_CONTEXT);
        } break;
        case GL_SCISSOR_TEST: {
            GL_CONTEXT.gen.clip_mode = GPU_USERCLIP_INSIDE;
        } break;
        case GL_LIGHTING: {
            LIGHTING_ENABLED = GL_TRUE;
        } break;
        case GL_FOG:
            GL_CONTEXT.gen.fog_type = GPU_FOG_TABLE;
        break;
        case GL_COLOR_MATERIAL:
            COLOR_MATERIAL_ENABLED = GL_TRUE;
        break;
        case GL_SHARED_TEXTURE_PALETTE_EXT: {
            SHARED_PALETTE_ENABLED = GL_TRUE;
        }
        break;
        case GL_ALPHA_TEST: {
            ALPHA_TEST_ENABLED = GL_TRUE;
            _updatePVRBlend(&GL_CONTEXT);
        } break;
        case GL_LIGHT0:
        case GL_LIGHT1:
        case GL_LIGHT2:
        case GL_LIGHT3:
        case GL_LIGHT4:
        case GL_LIGHT5:
        case GL_LIGHT6:
        case GL_LIGHT7:
            _glEnableLight(cap & 0xF, GL_TRUE);
        break;
        case GL_NEARZ_CLIPPING_KOS:
            _glEnableClipping(GL_TRUE);
        break;
        case GL_POLYGON_OFFSET_POINT:
        case GL_POLYGON_OFFSET_LINE:
        case GL_POLYGON_OFFSET_FILL:
            POLYGON_OFFSET_ENABLED = GL_TRUE;
        break;
        case GL_NORMALIZE:
            NORMALIZE_ENABLED = GL_TRUE;
        break;
    default:
        break;
    }
}

GLAPI void APIENTRY glDisable(GLenum cap) {
    switch(cap) {
        case GL_TEXTURE_2D: {
            TEXTURES_ENABLED[_glGetActiveTexture()] = GL_FALSE;
        } break;
        case GL_CULL_FACE: {
            CULLING_ENABLED = GL_FALSE;
            GL_CONTEXT.gen.culling = _calc_pvr_face_culling();
        } break;
        case GL_DEPTH_TEST: {
            DEPTH_TEST_ENABLED = GL_FALSE;
            GL_CONTEXT.depth.comparison = _calc_pvr_depth_test();
        } break;
        case GL_BLEND:
            BLEND_ENABLED = GL_FALSE;
            _updatePVRBlend(&GL_CONTEXT);
        break;
        case GL_SCISSOR_TEST: {
            GL_CONTEXT.gen.clip_mode = GPU_USERCLIP_DISABLE;
        } break;
        case GL_LIGHTING: {
            LIGHTING_ENABLED = GL_FALSE;
        } break;
        case GL_FOG:
            GL_CONTEXT.gen.fog_type = GPU_FOG_DISABLE;
        break;
        case GL_COLOR_MATERIAL:
            COLOR_MATERIAL_ENABLED = GL_FALSE;
        break;
        case GL_SHARED_TEXTURE_PALETTE_EXT: {
            SHARED_PALETTE_ENABLED = GL_FALSE;
        }
        break;
        case GL_ALPHA_TEST: {
            ALPHA_TEST_ENABLED = GL_FALSE;
        } break;
        case GL_LIGHT0:
        case GL_LIGHT1:
        case GL_LIGHT2:
        case GL_LIGHT3:
        case GL_LIGHT4:
        case GL_LIGHT5:
        case GL_LIGHT6:
        case GL_LIGHT7:
            _glEnableLight(cap & 0xF, GL_FALSE);
        break;
        case GL_NEARZ_CLIPPING_KOS:
            _glEnableClipping(GL_FALSE);
        break;
        case GL_POLYGON_OFFSET_POINT:
        case GL_POLYGON_OFFSET_LINE:
        case GL_POLYGON_OFFSET_FILL:
            POLYGON_OFFSET_ENABLED = GL_FALSE;
        break;
        case GL_NORMALIZE:
            NORMALIZE_ENABLED = GL_FALSE;
        break;
    default:
        break;
    }
}

/* Clear Caps */
GLAPI void APIENTRY glClear(GLuint mode) {
    if(mode & GL_COLOR_BUFFER_BIT) {
        pvr_set_bg_color(CLEAR_COLOUR[0], CLEAR_COLOUR[1], CLEAR_COLOUR[2]);
    }
}

GLAPI void APIENTRY glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    if(r > 1) r = 1;
    if(g > 1) g = 1;
    if(b > 1) b = 1;
    if(a > 1) a = 1;

    CLEAR_COLOUR[0] = r * a;
    CLEAR_COLOUR[1] = g * a;
    CLEAR_COLOUR[2] = b * a;
}

/* Depth Testing */
GLAPI void APIENTRY glClearDepthf(GLfloat depth) {
    glClearDepth(depth);
}

GLAPI void APIENTRY glClearDepth(GLfloat depth) {
    /* We reverse because using invW means that farther Z == lower number */
    pvr_set_zclip(1.0f - depth);
}

GLAPI void APIENTRY glDrawBuffer(GLenum mode) {
    _GL_UNUSED(mode);

}

GLAPI void APIENTRY glReadBuffer(GLenum mode) {
    _GL_UNUSED(mode);

}

GLAPI void APIENTRY glDepthMask(GLboolean flag) {
    GL_CONTEXT.depth.write = (flag == GL_TRUE) ? GPU_DEPTHWRITE_ENABLE : GPU_DEPTHWRITE_DISABLE;
}

GLAPI void APIENTRY glDepthFunc(GLenum func) {
    DEPTH_FUNC = func;
    GL_CONTEXT.depth.comparison = _calc_pvr_depth_test();
}

/* Hints */
/* Currently Supported Capabilities:
      GL_PERSPECTIVE_CORRECTION_HINT - This will Enable  on the PVR */
GLAPI void APIENTRY glHint(GLenum target, GLenum mode) {
    if(target == GL_PERSPECTIVE_CORRECTION_HINT && mode == GL_NICEST) {
        // FIXME: enable supersampling
    }
}

/* Culling */
GLAPI void APIENTRY glFrontFace(GLenum mode) {
    FRONT_FACE = mode;
    GL_CONTEXT.gen.culling = _calc_pvr_face_culling();
}

GLAPI void APIENTRY glCullFace(GLenum mode) {
    CULL_FACE = mode;
    GL_CONTEXT.gen.culling = _calc_pvr_face_culling();
}

GLenum _glGetShadeModel() {
    return (GL_CONTEXT.gen.shading == GPU_SHADE_FLAT) ? GL_FLAT : GL_SMOOTH;
}

/* Shading - Flat or Goraud */
GLAPI void APIENTRY glShadeModel(GLenum mode) {
    GL_CONTEXT.gen.shading = (mode == GL_SMOOTH) ? GPU_SHADE_GOURAUD : GPU_SHADE_FLAT;
}

/* Blending */
GLAPI void APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor) {
    BLEND_SFACTOR = sfactor;
    BLEND_DFACTOR = dfactor;
    _updatePVRBlend(&GL_CONTEXT);
}

#define PT_ALPHA_REF 0x011c

GLAPI void APIENTRY glAlphaFunc(GLenum func, GLclampf ref) {
    GLint validFuncs[] = {
        GL_GREATER,
        0
    };

    if(_glCheckValidEnum(func, validFuncs, __func__) != 0) {
        return;
    }

    GLubyte val = (GLubyte)(ref * 255.0f);
    PVR_SET(PT_ALPHA_REF, val);
}

void glLineWidth(GLfloat width) {
    _GL_UNUSED(width);
}

void glPolygonOffset(GLfloat factor, GLfloat units) {
    OFFSET_FACTOR = factor;
    OFFSET_UNITS = units;
}

void glGetTexParameteriv(GLenum target, GLenum pname, GLint *params) {
    _GL_UNUSED(target);
    _GL_UNUSED(pname);
    _GL_UNUSED(params);
}

void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    _GL_UNUSED(red);
    _GL_UNUSED(green);
    _GL_UNUSED(blue);
    _GL_UNUSED(alpha);
}

void glPixelStorei(GLenum pname, GLint param) {
    _GL_UNUSED(pname);
    _GL_UNUSED(param);
}


/* Setup the hardware user clip rectangle.

   The minimum clip rectangle is a 32x32 area which is dependent on the tile
   size use by the tile accelerator. The PVR swithes off rendering to tiles
   outside or inside the defined rectangle dependant upon the 'clipmode'
   bits in the polygon header.

   Clip rectangles therefore must have a size that is some multiple of 32.

    glScissor(0, 0, 32, 32) allows only the 'tile' in the lower left
    hand corner of the screen to be modified and glScissor(0, 0, 0, 0)
    disallows modification to all 'tiles' on the screen.
*/
void APIENTRY glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    /*!!! FIXME: Shouldn't this be added to *all* lists? */
    PVRTileClipCommand *c = aligned_vector_extend(&_glActivePolyList()->vector, 1);

    GLint miny, maxx, maxy;

    const VideoMode* vid_mode = GetVideoMode();

    GLsizei gl_scissor_width = MAX( MIN(width, vid_mode->width), 0 );
    GLsizei gl_scissor_height = MAX( MIN(height, vid_mode->height), 0 );

    /* force the origin to the lower left-hand corner of the screen */
    miny = (vid_mode->height - gl_scissor_height) - y;
    maxx = (gl_scissor_width + x);
    maxy = (gl_scissor_height + miny);

    /* load command structure while mapping screen coords to TA tiles */
    c->flags = GPU_CMD_USERCLIP;
    c->d1 = c->d2 = c->d3 = 0;
    c->sx = CLAMP(x / 32, 0, vid_mode->width / 32);
    c->sy = CLAMP(miny / 32, 0, vid_mode->height / 32);
    c->ex = CLAMP((maxx / 32) - 1, 0, vid_mode->width / 32);
    c->ey = CLAMP((maxy / 32) - 1, 0, vid_mode->height / 32);
}

GLboolean APIENTRY glIsEnabled(GLenum cap) {
    switch(cap) {
    case GL_DEPTH_TEST:
        return DEPTH_TEST_ENABLED;
    case GL_SCISSOR_TEST:
        return GL_CONTEXT.gen.clip_mode == GPU_USERCLIP_INSIDE;
    case GL_CULL_FACE:
        return CULLING_ENABLED;
    case GL_LIGHTING:
        return LIGHTING_ENABLED;
    case GL_BLEND:
        return BLEND_ENABLED;
    case GL_POLYGON_OFFSET_POINT:
    case GL_POLYGON_OFFSET_LINE:
    case GL_POLYGON_OFFSET_FILL:
        return POLYGON_OFFSET_ENABLED;
    }

    return GL_FALSE;
}

static GLenum COMPRESSED_FORMATS [] = {
    GL_COMPRESSED_ARGB_1555_VQ_KOS,
    GL_COMPRESSED_ARGB_1555_VQ_TWID_KOS,
    GL_COMPRESSED_ARGB_4444_VQ_KOS,
    GL_COMPRESSED_ARGB_4444_VQ_TWID_KOS,
    GL_COMPRESSED_RGB_565_VQ_KOS,
    GL_COMPRESSED_RGB_565_VQ_TWID_KOS
};

static GLuint NUM_COMPRESSED_FORMATS = sizeof(COMPRESSED_FORMATS) / sizeof(GLenum);

void APIENTRY glGetBooleanv(GLenum pname, GLboolean* params) {
    GLuint enabledAttrs = *_glGetEnabledAttributes();
    GLuint activeClientTexture = _glGetActiveClientTexture();

    switch(pname) {
    case GL_TEXTURE_2D:
        *params = TEXTURES_ENABLED[_glGetActiveTexture()];
    break;
    case GL_VERTEX_ARRAY:
        *params = (enabledAttrs & VERTEX_ENABLED_FLAG) == VERTEX_ENABLED_FLAG;
    break;
    case GL_COLOR_ARRAY:
        *params = (enabledAttrs & DIFFUSE_ENABLED_FLAG) == DIFFUSE_ENABLED_FLAG;
    break;
    case GL_NORMAL_ARRAY:
        *params = (enabledAttrs & NORMAL_ENABLED_FLAG) == NORMAL_ENABLED_FLAG;
    break;
    case GL_TEXTURE_COORD_ARRAY: {
        if(activeClientTexture == 0) {
            *params = (enabledAttrs & UV_ENABLED_FLAG) == UV_ENABLED_FLAG;
        } else {
            *params = (enabledAttrs & ST_ENABLED_FLAG) == ST_ENABLED_FLAG;
        }
    } break;
    default:
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        _glKosPrintError();
    }
}

void APIENTRY glGetFloatv(GLenum pname, GLfloat* params) {
    switch(pname) {
        case GL_PROJECTION_MATRIX:
            memcpy4(params, _glGetProjectionMatrix(), sizeof(float) * 16);
        break;
        case GL_MODELVIEW_MATRIX:
            memcpy4(params, _glGetModelViewMatrix(), sizeof(float) * 16);
        break;
        case GL_POLYGON_OFFSET_FACTOR:
            *params = OFFSET_FACTOR;
        break;
        case GL_POLYGON_OFFSET_UNITS:
            *params = OFFSET_UNITS;
        break;
        default:
            _glKosThrowError(GL_INVALID_ENUM, __func__);
            _glKosPrintError();
            break;
    }
}

void APIENTRY glGetIntegerv(GLenum pname, GLint *params) {
    switch(pname) {
        case GL_MAX_LIGHTS:
            *params = MAX_LIGHTS;
        break;
        case GL_TEXTURE_BINDING_2D:
            *params = (_glGetBoundTexture()) ? _glGetBoundTexture()->index : 0;
        break;
        case GL_DEPTH_FUNC:
            *params = DEPTH_FUNC;
        break;
        case GL_BLEND_SRC:
            *params = BLEND_SFACTOR;
        break;
        case GL_BLEND_DST:
            *params = BLEND_DFACTOR;
        break;
        case GL_MAX_TEXTURE_SIZE:
            *params = MAX_TEXTURE_SIZE;
        break;
        case GL_NUM_COMPRESSED_TEXTURE_FORMATS_ARB:
            *params = NUM_COMPRESSED_FORMATS;
        break;
        case GL_ACTIVE_TEXTURE:
            *params = GL_TEXTURE0 + _glGetActiveTexture();
        break;
        case GL_CLIENT_ACTIVE_TEXTURE:
            *params = GL_TEXTURE0 + _glGetActiveClientTexture();
        break;
        case GL_COMPRESSED_TEXTURE_FORMATS_ARB: {
            GLuint i = 0;
            for(; i < NUM_COMPRESSED_FORMATS; ++i) {
                params[i] = COMPRESSED_FORMATS[i];
            }
        } break;
        case GL_TEXTURE_FREE_MEMORY_ATI:
        case GL_FREE_TEXTURE_MEMORY_KOS:
            *params = _glFreeTextureMemory();
        break;
        case GL_USED_TEXTURE_MEMORY_KOS:
            *params = _glUsedTextureMemory();
        break;
        case GL_FREE_CONTIGUOUS_TEXTURE_MEMORY_KOS:
            *params = _glFreeContiguousTextureMemory();
        break;
    default:
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        _glKosPrintError();
        break;
    }
}

const GLubyte *glGetString(GLenum name) {
    switch(name) {
        case GL_VENDOR:
            return (const GLubyte*) "KallistiOS / Kazade";

        case GL_RENDERER:
            return (const GLubyte*) "PowerVR2 CLX2 100mHz";

        case GL_VERSION:
            return (const GLubyte*) "1.2 (partial) - GLdc 1.1";

        case GL_EXTENSIONS:
            return (const GLubyte*) "GL_ARB_framebuffer_object, GL_ARB_multitexture, GL_ARB_texture_rg, GL_EXT_paletted_texture, GL_EXT_shared_texture_palette, GL_KOS_multiple_shared_palette, GL_ARB_vertex_array_bgra, GL_ARB_vertex_type_2_10_10_10_rev, GL_KOS_texture_memory_management, GL_ATI_meminfo";
    }

    return (const GLubyte*) "GL_KOS_ERROR: ENUM Unsupported\n";
}
