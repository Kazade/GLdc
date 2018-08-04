#include <string.h>
#include <stdio.h>

#include <dc/pvr.h>
#include <dc/vec3f.h>
#include <dc/video.h>

#include "../include/gl.h"
#include "../include/glkos.h"

#include "private.h"

static pvr_poly_cxt_t GL_CONTEXT;

pvr_poly_cxt_t* getPVRContext() {
    return &GL_CONTEXT;
}


/* We can't just use the GL_CONTEXT for this state as the two
 * GL states are combined, so we store them separately and then
 * calculate the appropriate PVR state from them. */
static GLenum CULL_FACE = GL_BACK;
static GLenum FRONT_FACE = GL_CCW;
static GLboolean CULLING_ENABLED = GL_FALSE;

static int _calc_pvr_face_culling() {
    if(!CULLING_ENABLED) {
        return PVR_CULLING_NONE;
    } else {
        if(CULL_FACE == GL_BACK) {
            return (FRONT_FACE == GL_CW) ? PVR_CULLING_CCW : PVR_CULLING_CW;
        } else {
            return (FRONT_FACE == GL_CCW) ? PVR_CULLING_CCW : PVR_CULLING_CW;
        }
    }
}

static GLenum DEPTH_FUNC = GL_LESS;
static GLboolean DEPTH_TEST_ENABLED = GL_FALSE;

static int _calc_pvr_depth_test() {
    if(!DEPTH_TEST_ENABLED) {
        return PVR_DEPTHCMP_ALWAYS;
    }

    switch(DEPTH_FUNC) {
        case GL_NEVER:
            return PVR_DEPTHCMP_NEVER;
        case GL_LESS:
            return PVR_DEPTHCMP_GEQUAL;
        case GL_EQUAL:
            return PVR_DEPTHCMP_EQUAL;
        case GL_LEQUAL:
            return PVR_DEPTHCMP_GREATER;
        case GL_GREATER:
            return PVR_DEPTHCMP_LEQUAL;
        case GL_NOTEQUAL:
            return PVR_DEPTHCMP_NOTEQUAL;
        case GL_GEQUAL:
            return PVR_DEPTHCMP_LESS;
        break;
        case GL_ALWAYS:
        default:
            return PVR_DEPTHCMP_ALWAYS;
    }
}

static GLenum BLEND_SFACTOR = GL_ONE;
static GLenum BLEND_DFACTOR = GL_ZERO;
static GLboolean BLEND_ENABLED = GL_FALSE;

GLboolean isBlendingEnabled() {
    return BLEND_ENABLED;
}

static int _calcPVRBlendFactor(GLenum factor) {
    switch(factor) {
    case GL_ZERO:
        return PVR_BLEND_ZERO;
    case GL_SRC_ALPHA:
        return PVR_BLEND_SRCALPHA;
    case GL_DST_COLOR:
        return PVR_BLEND_DESTCOLOR;
    case GL_DST_ALPHA:
        return PVR_BLEND_DESTALPHA;
    case GL_ONE_MINUS_DST_COLOR:
        return PVR_BLEND_INVDESTCOLOR;
    case GL_ONE_MINUS_SRC_ALPHA:
        return PVR_BLEND_INVSRCALPHA;
    case GL_ONE_MINUS_DST_ALPHA:
        return PVR_BLEND_INVDESTALPHA;
    case GL_ONE:
        return PVR_BLEND_ONE;
    default:
        fprintf(stderr, "Invalid blend mode: %d\n", factor);
        return PVR_BLEND_ONE;
    }
}

static void _updatePVRBlend(pvr_poly_cxt_t* context) {
    if(BLEND_ENABLED) {
        context->gen.alpha = PVR_ALPHA_ENABLE;
        context->blend.src = _calcPVRBlendFactor(BLEND_SFACTOR);
        context->blend.dst = _calcPVRBlendFactor(BLEND_DFACTOR);
        context->blend.src_enable = PVR_BLEND_DISABLE;
        context->blend.dst_enable = PVR_BLEND_DISABLE;
    } else {
        context->gen.alpha = PVR_ALPHA_DISABLE;
        context->blend.src = PVR_BLEND_ONE;
        context->blend.dst = PVR_BLEND_ZERO;
        context->blend.src_enable = PVR_BLEND_DISABLE;
        context->blend.dst_enable = PVR_BLEND_DISABLE;
    }
}

static GLboolean TEXTURES_ENABLED = GL_FALSE;

void updatePVRTextureContext(pvr_poly_cxt_t* context, TextureObject *tx1) {
    if(!TEXTURES_ENABLED) {
        context->txr2.enable = context->txr.enable = PVR_TEXTURE_DISABLE;
        return;
    }

    context->txr2.enable = PVR_TEXTURE_DISABLE;
    context->txr2.alpha = PVR_TXRALPHA_DISABLE;

    if(tx1) {
        context->txr.enable = PVR_TEXTURE_ENABLE;
        context->txr.filter = tx1->filter;
        context->txr.mipmap_bias = PVR_MIPBIAS_NORMAL;
        context->txr.width = tx1->width;
        context->txr.height = tx1->height;
        context->txr.base = tx1->data;
        context->txr.format = tx1->color;
        context->txr.env = tx1->env;
        context->txr.uv_flip = PVR_UVFLIP_NONE;
        context->txr.uv_clamp = tx1->uv_clamp;
        context->txr.alpha = PVR_TXRALPHA_ENABLE;
    } else {
        context->txr.enable = PVR_TEXTURE_DISABLE;
    }
}

static GLboolean LIGHTING_ENABLED = GL_FALSE;
static GLboolean LIGHT_ENABLED[MAX_LIGHTS];

GLboolean isLightingEnabled() {
    return LIGHTING_ENABLED;
}

GLboolean isLightEnabled(unsigned char light) {
    return LIGHT_ENABLED[light & 0xF];
}

static GLfloat CLEAR_COLOUR[3];

void initContext() {
    memset(&GL_CONTEXT, 0, sizeof(pvr_poly_cxt_t));

    GL_CONTEXT.list_type = PVR_LIST_OP_POLY;
    GL_CONTEXT.fmt.color = PVR_CLRFMT_ARGBPACKED;
    GL_CONTEXT.fmt.uv = PVR_UVFMT_32BIT;
    GL_CONTEXT.gen.color_clamp = PVR_CLRCLAMP_DISABLE;

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
            TEXTURES_ENABLED = GL_TRUE;
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
            GL_CONTEXT.gen.clip_mode = PVR_USERCLIP_INSIDE;
        } break;
        case GL_LIGHTING: {
            LIGHTING_ENABLED = GL_TRUE;
        } break;
        case GL_FOG:
            GL_CONTEXT.gen.fog_type = PVR_FOG_TABLE;
        break;
        case GL_LIGHT0:
        case GL_LIGHT1:
        case GL_LIGHT2:
        case GL_LIGHT3:
        case GL_LIGHT4:
        case GL_LIGHT5:
        case GL_LIGHT6:
        case GL_LIGHT7:
            LIGHT_ENABLED[cap & 0xF] = GL_TRUE;
        break;
        case GL_NEARZ_CLIPPING_KOS:
            enableClipping(GL_TRUE);
        break;
    default:
        break;
    }
}

GLAPI void APIENTRY glDisable(GLenum cap) {
    switch(cap) {
        case GL_TEXTURE_2D: {
            TEXTURES_ENABLED = GL_FALSE;
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
            GL_CONTEXT.gen.clip_mode = PVR_USERCLIP_DISABLE;
        } break;
        case GL_LIGHTING: {
            LIGHTING_ENABLED = GL_FALSE;
        } break;
        case GL_FOG:
            GL_CONTEXT.gen.fog_type = PVR_FOG_DISABLE;
        break;
        case GL_LIGHT0:
        case GL_LIGHT1:
        case GL_LIGHT2:
        case GL_LIGHT3:
        case GL_LIGHT4:
        case GL_LIGHT5:
        case GL_LIGHT6:
        case GL_LIGHT7:
            LIGHT_ENABLED[cap & 0xF] = GL_FALSE;
        break;
        case GL_NEARZ_CLIPPING_KOS:
            enableClipping(GL_FALSE);
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

}

GLAPI void APIENTRY glClearDepth(GLfloat depth) {

}

GLAPI void APIENTRY glDepthMask(GLboolean flag) {
    GL_CONTEXT.depth.write = (flag == GL_TRUE) ? PVR_DEPTHWRITE_ENABLE : PVR_DEPTHWRITE_DISABLE;
    GL_CONTEXT.depth.comparison = _calc_pvr_depth_test();
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

/* Shading - Flat or Goraud */
GLAPI void APIENTRY glShadeModel(GLenum mode) {
    GL_CONTEXT.gen.shading = (mode == GL_SMOOTH) ? PVR_SHADE_GOURAUD : PVR_SHADE_FLAT;
}

/* Blending */
GLAPI void APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor) {
    BLEND_SFACTOR = sfactor;
    BLEND_DFACTOR = dfactor;
    _updatePVRBlend(&GL_CONTEXT);
}

void glAlphaFunc(GLenum func, GLclampf ref) {
    ;
}

void glLineWidth(GLfloat width) {
    ;
}

void glPolygonOffset(GLfloat factor, GLfloat units) {
    ;
}

void glGetTexParameteriv(GLenum target, GLenum pname, GLint *params) {
    ;
}

void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    ;
}

void glPixelStorei(GLenum pname, GLint param) {
    ;
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
    PVRTileClipCommand *c = aligned_vector_extend(&activePolyList()->vector, 1);

    GLint miny, maxx, maxy;
    GLsizei gl_scissor_width = CLAMP(width, 0, vid_mode->width);
    GLsizei gl_scissor_height = CLAMP(height, 0, vid_mode->height);

    /* force the origin to the lower left-hand corner of the screen */
    miny = (vid_mode->height - gl_scissor_height) - y;
    maxx = (gl_scissor_width + x);
    maxy = (gl_scissor_height + miny);

    /* load command structure while mapping screen coords to TA tiles */
    c->flags = PVR_CMD_USERCLIP;
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
        return GL_CONTEXT.gen.clip_mode == PVR_USERCLIP_INSIDE;
    case GL_CULL_FACE:
        return CULLING_ENABLED;
    case GL_LIGHTING:
        return LIGHTING_ENABLED;
    case GL_BLEND:
        return BLEND_ENABLED;
    }

    return GL_FALSE;
}

void APIENTRY glGetIntegerv(GLenum pname, GLint *params) {
    switch(pname) {
        case GL_MAX_LIGHTS:
            *params = MAX_LIGHTS;
        break;
        case GL_TEXTURE_BINDING_2D:
            *params = getBoundTexture()->index;
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
    default:
        _glKosThrowError(GL_INVALID_ENUM, "glGetIntegerv");
        _glKosPrintError();
        break;
    }
}

const GLbyte *glGetString(GLenum name) {
    switch(name) {
        case GL_VENDOR:
            return "KallistiOS / Kazade";

        case GL_RENDERER:
            return "PowerVR2 CLX2 100mHz";

        case GL_VERSION:
            return "GLdc 1.x";

        case GL_EXTENSIONS:
            return "GL_ARB_framebuffer_object, GL_ARB_multitexture, GL_ARB_texture_rg";
    }

    return "GL_KOS_ERROR: ENUM Unsupported\n";
}
