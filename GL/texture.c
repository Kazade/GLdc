#include "private.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "platform.h"

#include "yalloc/yalloc.h"

/* We always leave this amount of vram unallocated to prevent
 * issues with the allocator */
#define PVR_MEM_BUFFER_SIZE (64 * 1024)

#define CLAMP_U (1<<1)
#define CLAMP_V (1<<0)

static TextureObject* TEXTURE_UNITS[MAX_GLDC_TEXTURE_UNITS] = {NULL, NULL};
static NamedArray TEXTURE_OBJECTS;
GLubyte ACTIVE_TEXTURE = 0;

static TexturePalette* SHARED_PALETTES[MAX_GLDC_SHARED_PALETTES];

static GLuint _determinePVRFormat(GLint internalFormat, GLenum type);

static GLboolean BANKS_USED[MAX_GLDC_PALETTE_SLOTS];  // Each time a 256 colour bank is used, this is set to true
static GLboolean SUBBANKS_USED[MAX_GLDC_PALETTE_SLOTS][MAX_GLDC_4BPP_PALETTE_SLOTS]; // 4 counts of the used 16 colour banks within the 256 ones

static GLenum INTERNAL_PALETTE_FORMAT = GL_RGBA8;

static void* YALLOC_BASE = NULL;
static size_t YALLOC_SIZE = 0;

static void* yalloc_alloc_and_defrag(size_t size) {
    void* ret = yalloc_alloc(YALLOC_BASE, size);

    if(!ret) {
        /* Tried to allocate, but out of room, let's try defragging
         * and repeating the alloc */
        fprintf(stderr, "Ran out of memory, defragmenting\n");
        glDefragmentTextureMemory_KOS();
        ret = yalloc_alloc(YALLOC_BASE, size);
    }

    gl_assert(ret && "Out of PVR memory!");

    return ret;
}

static TexturePalette* _initTexturePalette() {
    TexturePalette* palette = (TexturePalette*) malloc(sizeof(TexturePalette));
    gl_assert(palette);

    MEMSET4(palette, 0x0, sizeof(TexturePalette));
    palette->bank = -1;
    return palette;
}

static GLshort _glGenPaletteSlot(GLushort size) {
    GLushort i, j;

    gl_assert(size == 16 || size == 256);

    if(size == 16) {
        for(i = 0; i < MAX_GLDC_PALETTE_SLOTS; ++i) {
            for(j = 0; j < MAX_GLDC_4BPP_PALETTE_SLOTS; ++j) {
                if(!SUBBANKS_USED[i][j]) {
                    BANKS_USED[i] = GL_TRUE;
                    SUBBANKS_USED[i][j] = GL_TRUE;
                    return (i * MAX_GLDC_4BPP_PALETTE_SLOTS) + j;
                }
            }
        }
    }
    else {
        for(i = 0; i < MAX_GLDC_PALETTE_SLOTS; ++i) {
            if(!BANKS_USED[i]) {
                BANKS_USED[i] = GL_TRUE;
                for(j = 0; j < MAX_GLDC_4BPP_PALETTE_SLOTS; ++j) {
                    SUBBANKS_USED[i][j] = GL_TRUE;
                }
                return i;
            }
        }
    }

    fprintf(stderr, "GL ERROR: No palette slots remain\n");
    return -1;
}

GLushort _glFreePaletteSlots(GLushort size)
{
    GLushort i, j , slots = 0;

    gl_assert(size == 16 || size == 256);

    if(size == 16) {
        for(i = 0; i < MAX_GLDC_PALETTE_SLOTS; ++i) {
            for(j = 0; j < MAX_GLDC_4BPP_PALETTE_SLOTS; ++j) {
                if(!SUBBANKS_USED[i][j]) {
                    slots++;
                }
            }
        }
    } else {
        for(i = 0; i < MAX_GLDC_PALETTE_SLOTS; ++i) {
            if(!BANKS_USED[i]) {
                slots++;
            }
        }
    }

    return slots;
}

static void _glReleasePaletteSlot(GLshort slot, GLushort size)
{
    GLushort i;

    gl_assert(size == 16 || size == 256);

    if (size == 16) {
        GLushort bank = slot / MAX_GLDC_PALETTE_SLOTS;
        GLushort subbank = slot % MAX_GLDC_PALETTE_SLOTS;

        SUBBANKS_USED[bank][subbank] = GL_FALSE;

        for (i = 0; i < MAX_GLDC_4BPP_PALETTE_SLOTS; ++i) {
            if (SUBBANKS_USED[bank][i]) {
                return;
            }
        }
        BANKS_USED[bank] = GL_FALSE;
    }
    else {
        BANKS_USED[slot] = GL_FALSE;
        for (i = 0; i < MAX_GLDC_4BPP_PALETTE_SLOTS; ++i) {
            SUBBANKS_USED[slot][i] = GL_FALSE;
        }
    }
}

/* Linear/iterative twiddling algorithm from Marcus' tatest */
#define TWIDTAB(x) ( (x&1)|((x&2)<<1)|((x&4)<<2)|((x&8)<<3)|((x&16)<<4)| \
                     ((x&32)<<5)|((x&64)<<6)|((x&128)<<7)|((x&256)<<8)|((x&512)<<9) )
#define TWIDOUT(x, y) ( TWIDTAB((y)) | (TWIDTAB((x)) << 1) )


static void GPUTextureTwiddle8PPP(void* src, void* dst, uint32_t w, uint32_t h) {
    uint32_t x, y, yout, min, mask;

    min = MIN(w, h);
    mask = min - 1;

    uint8_t* pixels;
    uint16_t* vtex;
    pixels = (uint8_t*) src;
    vtex = (uint16_t*) dst;

    for(y = 0; y < h; y += 2) {
        yout = y;
        for(x = 0; x < w; x++) {
            int32_t idx = TWIDOUT((yout & mask) / 2, x & mask) +
                 (x / min + yout / min)*min * min / 2;

            gl_assert(idx < (w * h));
            vtex[idx] = pixels[y * w + x] | (pixels[(y + 1) * w + x] << 8);
        }
    }
}

static void GPUTextureTwiddle4PPP(void* src, void* dst, uint32_t w, uint32_t h) {
    uint32_t x, y, yout, min, mask;

    min = MIN(w, h);
    mask = min - 1;

	uint8_t* pixels;
    uint16_t* vtex;
    pixels = (uint8_t*) src;
    vtex = (uint16_t*) dst;

    for(y = 0; y < h; y += 2) {
        yout = y;
        for (x = 0; x < w; x += 2) {
            vtex[TWIDOUT((x & mask) / 2, (yout & mask) / 2) +
                (x / min + yout / min) * min * min / 4] =
            vtex[TWIDOUT((x & mask) / 2, (yout & mask) / 2) +
                (x / min + yout / min) * min * min / 4] =
                ((pixels[(x + y * w) >> 1] & 15) << 8) | ((pixels[(x + (y + 1) * w) >> 1] & 15) << 12) |
                ((pixels[(x + y * w) >> 1] >> 4) << 0) | ((pixels[(x + (y + 1) * w) >> 1] >> 4) << 4);
        }
    }
}

static void GPUTextureTwiddle16BPP(void * src, void* dst, uint32_t w, uint32_t h) {
    uint32_t x, y, yout, min, mask;

    min = MIN(w, h);
    mask = min - 1;

    uint16_t* pixels;
    uint16_t* vtex;
    pixels = (uint16_t*) src;
    vtex = (uint16_t*) dst;

    for(y = 0; y < h; y++) {
        yout = y;

        for(x = 0; x < w; x++) {
            vtex[TWIDOUT(x & mask, yout & mask) +
                 (x / min + yout / min)*min * min] = pixels[y * w + x];
        }
    }
}

TexturePalette* _glGetSharedPalette(GLshort bank) {
    gl_assert(bank >= 0 && bank < MAX_GLDC_SHARED_PALETTES);
    return SHARED_PALETTES[bank];
}
void _glSetInternalPaletteFormat(GLenum val) {
    INTERNAL_PALETTE_FORMAT = val;

    switch(INTERNAL_PALETTE_FORMAT){
        case GL_RGBA8:
            GPUSetPaletteFormat(GPU_PAL_ARGB8888);
            break;
        case GL_RGBA4:
            GPUSetPaletteFormat(GPU_PAL_ARGB4444);
            break;
         case GL_RGB5_A1:
                GPUSetPaletteFormat(GPU_PAL_ARGB1555);
                break;
         case  GL_RGB565_KOS:
             GPUSetPaletteFormat(GPU_PAL_RGB565);
            break;
         default:
            gl_assert(0);

    }
}
void _glApplyColorTable(TexturePalette* src) {
    if(!src || !src->data) {
        return;
    }

    GLushort i;
    GLushort offset = src->size * src->bank;

    for(i = 0; i < src->width; ++i) {
        GLubyte* entry = &src->data[i * 4];

        switch(INTERNAL_PALETTE_FORMAT)
        {
            case GL_RGBA8:
                GPUSetPaletteEntry(offset + i, PACK_ARGB8888(entry[3], entry[0], entry[1], entry[2]));
                break;
            case GL_RGBA4:
                GPUSetPaletteEntry(offset + i, PACK_ARGB4444(entry[3], entry[0], entry[1], entry[2]));
                break;
            case GL_RGB5_A1:
                GPUSetPaletteEntry(offset + i, PACK_ARGB1555(entry[3], entry[0], entry[1], entry[2]));
                break;
             case GL_RGB565_KOS:
                GPUSetPaletteEntry(offset + i, PACK_RGB565(entry[0], entry[1], entry[2]));
                break;
        }
    }
}

GLubyte _glGetActiveTexture() {
    return ACTIVE_TEXTURE;
}

 static GLint _determineStride(GLenum format, GLenum type) {
    switch(type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
        return (format == GL_RED || format == GL_ALPHA) ? 1 : (format == GL_RGB) ? 3 : 4;
    case GL_UNSIGNED_SHORT:
        return (format == GL_RED || format == GL_ALPHA) ? 2 : (format == GL_RGB) ? 6 : 8;
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_5_6_5_REV:
    case GL_UNSIGNED_SHORT_5_6_5_TWID_KOS:
    case GL_UNSIGNED_SHORT_5_5_5_1:
    case GL_UNSIGNED_SHORT_1_5_5_5_REV_TWID_KOS:
    case GL_UNSIGNED_SHORT_1_5_5_5_REV:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_4_4_4_4_REV_TWID_KOS:
    case GL_UNSIGNED_SHORT_4_4_4_4_REV:
        return 2;
    }

    return -1;
}

static GLuint _glGetMipmapDataOffset(const TextureObject* obj, GLuint level) {
    GLuint offset = 0;
    GLuint size = obj->height;

    if(obj->width != obj->height) {
        fprintf(stderr, "ERROR: Accessing memory location of mipmaps on non-square texture\n");
        return obj->baseDataOffset;
    }

    if(obj->isPaletted){
        switch(size >> level){
            case 1024:
            offset = 0x55558;
            break;
            case 512:
            offset = 0x15558;
            break;
            case 256:
            offset = 0x05558;
            break;
            case 128:
            offset = 0x01558;
            break;
            case 64:
            offset = 0x00558;
            break;
            case 32:
            offset = 0x00158;
            break;
            case 16:
            offset = 0x00058;
            break;
            case 8:
            offset = 0x00018;
            break;
            case 4:
            offset = 0x00008;
            break;
            case 2:
            offset = 0x00004;
            break;
            case 1:
            offset = 0x00003;
            break;
        }
    } else if(obj->isCompressed) {
        switch(size >> level){
            case 1024:
            offset = 0x15556;
            break;
            case 512:
            offset = 0x05556;
            break;
            case 256:
            offset = 0x01556;
            break;
            case 128:
            offset = 0x00556;
            break;
            case 64:
            offset = 0x00156;
            break;
            case 32:
            offset = 0x00056;
            break;
            case 16:
            offset = 0x00016;
            break;
            case 8:
            offset = 0x00006;
            break;
            case 4:
            offset = 0x00002;
            break;
            case 2:
            offset = 0x00001;
            break;
            case 1:
            offset = 0x00000;
            break;
        }
    }else {
        switch(size >> level){
            case 1024:
            offset = 0xAAAB0;
            break;
            case 512:
            offset = 0x2AAB0;
            break;
            case 256:
            offset = 0x0AAB0;
            break;
            case 128:
            offset = 0x02AB0;
            break;
            case 64:
            offset = 0x00AB0;
            break;
            case 32:
            offset = 0x002B0;
            break;
            case 16:
            offset = 0x000B0;
            break;
            case 8:
            offset = 0x00030;
            break;
            case 4:
            offset = 0x00010;
            break;
            case 2:
            offset = 0x00008;
            break;
            case 1:
            offset = 0x00006;
            break;
        }
    }

    return offset;
}

GLubyte* _glGetMipmapLocation(const TextureObject* obj, GLuint level) {
    return ((GLubyte*) obj->data) + _glGetMipmapDataOffset(obj, level);
}

GLuint _glGetMipmapLevelCount(const TextureObject* obj) {
    return 1 + floorf(log2f(MAX(obj->width, obj->height)));
}

static GLuint _glGetMipmapDataSize(TextureObject* obj) {
    /* The mipmap data size is the offset + the size of the
     * image */

    GLuint imageSize = obj->baseDataSize;
    GLuint offset = _glGetMipmapDataOffset(obj, 0);

    return imageSize + offset;
}


void _glResetSharedPalettes()
{
    uint32_t i;

    for (i=0; i < MAX_GLDC_SHARED_PALETTES;i++){

        MEMSET4(SHARED_PALETTES[i], 0x0, sizeof(TexturePalette));
        SHARED_PALETTES[i]->bank = -1;
    }

    memset((void*) BANKS_USED, 0x0, sizeof(BANKS_USED));
    memset((void*) SUBBANKS_USED, 0x0, sizeof(SUBBANKS_USED));

}

static void _glInitializeTextureObject(TextureObject* txr, unsigned int id) {
    txr->index = id;
    txr->width = txr->height = 0;
    txr->mipmap = 0;
    txr->uv_clamp = 0;
    txr->env = GPU_TXRENV_MODULATEALPHA;
    txr->data = NULL;
    txr->mipmapCount = 0;
    txr->minFilter = GL_NEAREST;
    txr->magFilter = GL_NEAREST;
    txr->palette = NULL;
    txr->isCompressed = GL_FALSE;
    txr->isPaletted = GL_FALSE;
    txr->mipmap_bias = GL_KOS_INTERNAL_DEFAULT_MIPMAP_LOD_BIAS;

    /* Not mipmapped by default */
    txr->baseDataOffset = 0;

    /* Always default to the first shared bank */
    txr->shared_bank = 0;
}

GLubyte _glInitTextures() {

    uint32_t i;

    named_array_init(&TEXTURE_OBJECTS, sizeof(TextureObject), MAX_TEXTURE_COUNT);

    // Reserve zero so that it is never given to anyone as an ID!
    named_array_reserve(&TEXTURE_OBJECTS, 0);

    // Initialize zero as an actual texture object though because apparently it is!
    _glInitializeTextureObject((TextureObject*) named_array_get(&TEXTURE_OBJECTS, 0), 0);

    for (i=0; i < MAX_GLDC_SHARED_PALETTES;i++){
        SHARED_PALETTES[i] = _initTexturePalette();
    }

    _glResetSharedPalettes();

    //memset((void*) BANKS_USED, 0x0, sizeof(BANKS_USED));
    //memset((void*) SUBBANKS_USED, 0x0, sizeof(SUBBANKS_USED));

    size_t vram_free = GPUMemoryAvailable();
    YALLOC_SIZE = vram_free - PVR_MEM_BUFFER_SIZE; /* Take all but 64kb VRAM */
    YALLOC_BASE = GPUMemoryAlloc(YALLOC_SIZE);

#ifdef __DREAMCAST__
    /* Ensure memory is aligned */
    gl_assert((uintptr_t) YALLOC_BASE % 32 == 0);
#endif

    yalloc_init(YALLOC_BASE, YALLOC_SIZE);

    gl_assert(TEXTURE_OBJECTS.element_size > 0);
    return 1;
}

TextureObject* _glGetTexture0() {
    return TEXTURE_UNITS[0];
}

TextureObject* _glGetTexture1() {
    gl_assert(1 < MAX_GLDC_TEXTURE_UNITS);
    return TEXTURE_UNITS[1];
}

TextureObject* _glGetBoundTexture() {
    gl_assert(ACTIVE_TEXTURE < MAX_GLDC_TEXTURE_UNITS);
    return TEXTURE_UNITS[ACTIVE_TEXTURE];
}

void APIENTRY glActiveTextureARB(GLenum texture) {
    TRACE();

    if(texture < GL_TEXTURE0_ARB || texture >= GL_TEXTURE0_ARB + MAX_GLDC_TEXTURE_UNITS) {
        _glKosThrowError(GL_INVALID_ENUM, "glActiveTextureARB");
        return;
    }

    ACTIVE_TEXTURE = texture & 0xF;
    gl_assert(ACTIVE_TEXTURE < MAX_GLDC_TEXTURE_UNITS);

    gl_assert(TEXTURE_OBJECTS.element_size > 0);
}

GLboolean APIENTRY glIsTexture(GLuint texture) {
    return (named_array_used(&TEXTURE_OBJECTS, texture)) ? GL_TRUE : GL_FALSE;
}

void APIENTRY glGenTextures(GLsizei n, GLuint *textures) {
    TRACE();

    gl_assert(TEXTURE_OBJECTS.element_size > 0);

    while(n--) {
        GLuint id = 0;
        TextureObject* txr = (TextureObject*) named_array_alloc(&TEXTURE_OBJECTS, &id);

        gl_assert(txr);
        gl_assert(id);  // Generated IDs must never be zero

        _glInitializeTextureObject(txr, id);


        gl_assert(txr->index == id);

        *textures = id;
        textures++;
    }

    gl_assert(TEXTURE_OBJECTS.element_size > 0);
}

void APIENTRY glDeleteTextures(GLsizei n, GLuint *textures) {
    TRACE();

    gl_assert(TEXTURE_OBJECTS.element_size > 0);

    for(GLsizei i = 0; i < n; ++i) {
        GLuint id = textures[i];
        if(id == 0) {
            /* Zero is the "default texture" and we never allow deletion of it */
            continue;
        }

        TextureObject* txr = (TextureObject*) named_array_get(&TEXTURE_OBJECTS, id);

        if(txr) {
            gl_assert(txr->index == id);

            /* Make sure we update framebuffer objects that have this texture attached */
            _glWipeTextureOnFramebuffers(id);

            for(GLuint j = 0; j < MAX_GLDC_TEXTURE_UNITS; ++j) {
                if(txr == TEXTURE_UNITS[j]) {
                    TEXTURE_UNITS[j] = NULL;
                }
            }

            if(txr->data) {
                yalloc_free(YALLOC_BASE, txr->data);
                txr->data = NULL;
            }

            if(txr->palette && txr->palette->data) {
                if (txr->palette->bank > -1) {
                    _glReleasePaletteSlot(txr->palette->bank, txr->palette->size);
                    txr->palette->bank = -1;
                }
                free(txr->palette->data);
                txr->palette->data = NULL;
            }

            if(txr->palette) {
                free(txr->palette);
                txr->palette = NULL;
            }

            named_array_release(&TEXTURE_OBJECTS, id);
            textures[i] = 0;
            txr->index = 0;
        }
    }

    gl_assert(TEXTURE_OBJECTS.element_size > 0);
}

void APIENTRY glBindTexture(GLenum  target, GLuint texture) {
    TRACE();

    GLint target_values [] = {GL_TEXTURE_2D, 0};

    if(_glCheckValidEnum(target, target_values, __func__) != 0) {
        return;
    }

    TextureObject* txr = (TextureObject*) named_array_get(&TEXTURE_OBJECTS, texture);

    /* If this didn't come from glGenTextures, then we should initialize the
        * texture the first time it's bound */
    if(!txr) {
        TextureObject* txr = named_array_reserve(&TEXTURE_OBJECTS, texture);
        _glInitializeTextureObject(txr, texture);
    }

    gl_assert(ACTIVE_TEXTURE < MAX_GLDC_TEXTURE_UNITS);
    TEXTURE_UNITS[ACTIVE_TEXTURE] = txr;
    gl_assert(TEXTURE_UNITS[ACTIVE_TEXTURE]->index == texture);

    gl_assert(TEXTURE_OBJECTS.element_size > 0);

    _glGPUStateMarkDirty();
}

void APIENTRY glTexEnvi(GLenum target, GLenum pname, GLint param) {
    TRACE();

    GLubyte failures = 0;

    GLint target_values [] = {GL_TEXTURE_ENV, GL_TEXTURE_FILTER_CONTROL_EXT, 0};
    failures += _glCheckValidEnum(target, target_values, __func__);

    gl_assert(ACTIVE_TEXTURE < MAX_GLDC_TEXTURE_UNITS);
    TextureObject* active = TEXTURE_UNITS[ACTIVE_TEXTURE];

    if(!active) {
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        return;
    }

    if(failures) {
        return;
    }
    switch(target){
        case GL_TEXTURE_ENV:
            {
            GLint pname_values [] = {GL_TEXTURE_ENV_MODE, 0};
            GLint param_values [] = {GL_MODULATE, GL_DECAL, GL_REPLACE, 0};
            failures += _glCheckValidEnum(pname, pname_values, __func__);
            failures += _glCheckValidEnum(param, param_values, __func__);

            if(failures) {
                return;
            }

            switch(param) {
                case GL_MODULATE:
                    active->env = GPU_TXRENV_MODULATEALPHA;
                break;
                case GL_DECAL:
                    active->env = GPU_TXRENV_DECAL;
                break;
                case GL_REPLACE:
                    active->env = GPU_TXRENV_REPLACE;
                break;
                default:
                    break;
                }
            }
            break;

        case GL_TEXTURE_FILTER_CONTROL_EXT:
            {
            GLint pname_values [] = {GL_TEXTURE_LOD_BIAS_EXT, 0};
            failures += _glCheckValidEnum(pname, pname_values, __func__);
            failures += (param > GL_MAX_TEXTURE_LOD_BIAS_DEFAULT || param < -GL_MAX_TEXTURE_LOD_BIAS_DEFAULT);
            if(failures) {
                return;
            }
            active->mipmap_bias = (GL_MAX_TEXTURE_LOD_BIAS_DEFAULT+1)+param; // bring to 1-15 inclusive
            }
            break;
        default:
           break;
    }

    _glGPUStateMarkDirty();
}

void APIENTRY glTexEnvf(GLenum target, GLenum pname, GLfloat param) {
    glTexEnvi(target, pname, param);
}

void APIENTRY glCompressedTexImage2DARB(GLenum target,
                                     GLint level,
                                     GLenum internalFormat,
                                     GLsizei width,
                                     GLsizei height,
                                     GLint border,
                                     GLsizei imageSize,
                                     const GLvoid *data) {
    TRACE();

    if(target != GL_TEXTURE_2D) {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        return;
    }

    GLint w = width;
    if(w < 8 || (w & -w) != w) {
        /* Width is not a power of two. Must be!*/
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return;
    }

    GLint h = height;
    if(h < 8 || (h & -h) != h) {
        /* Height is not a power of two. Must be!*/
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return;
    }

    if(level || border) {
        /* We don't support setting mipmap levels manually with compressed textures
           maybe one day */
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return;
    }

    GLboolean mipmapped = GL_FALSE;
    //GLboolean paletted = GL_FALSE;
    GLbyte *ptr = (GLbyte*)data;

    switch(internalFormat) {
        case GL_COMPRESSED_ARGB_1555_VQ_KOS:
        case GL_COMPRESSED_ARGB_1555_VQ_TWID_KOS:
        case GL_COMPRESSED_ARGB_4444_VQ_KOS:
        case GL_COMPRESSED_ARGB_4444_VQ_TWID_KOS:
        case GL_COMPRESSED_RGB_565_VQ_KOS:
        case GL_COMPRESSED_RGB_565_VQ_TWID_KOS:
        break;
        case GL_COMPRESSED_ARGB_1555_VQ_MIPMAP_KOS:
        case GL_COMPRESSED_ARGB_1555_VQ_MIPMAP_TWID_KOS:
        case GL_COMPRESSED_ARGB_4444_VQ_MIPMAP_KOS:
        case GL_COMPRESSED_ARGB_4444_VQ_MIPMAP_TWID_KOS:
        case GL_COMPRESSED_RGB_565_VQ_MIPMAP_KOS:
        case GL_COMPRESSED_RGB_565_VQ_MIPMAP_TWID_KOS:
            mipmapped = GL_TRUE;
        break;
        case GL_PALETTE4_RGB8_OES:
            glColorTableEXT(GL_TEXTURE_2D, GL_RGBA8, 16, internalFormat, GL_UNSIGNED_BYTE, data);
            ptr += 16*3;
            glTexImage2D(GL_TEXTURE_2D, level, GL_COLOR_INDEX4_EXT, width, height, border, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, ptr);
            return;

        case GL_PALETTE4_RGBA8_OES:
            glColorTableEXT(GL_TEXTURE_2D, GL_RGBA8, 16, internalFormat, GL_UNSIGNED_BYTE, data);
            ptr += 16*4;
            glTexImage2D(GL_TEXTURE_2D, level, GL_COLOR_INDEX4_EXT, width, height, border, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, ptr);
            return;

        case GL_PALETTE4_R5_G6_B5_OES:
        case GL_PALETTE4_RGBA4_OES:
        case GL_PALETTE4_RGB5_A1_OES:
            glColorTableEXT(GL_TEXTURE_2D, GL_RGBA8, 16, internalFormat, GL_UNSIGNED_BYTE, data);
            ptr += 16*2;
            glTexImage2D(GL_TEXTURE_2D, level, GL_COLOR_INDEX4_EXT, width, height, border, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, ptr);
            return;

        case GL_PALETTE8_RGB8_OES:
            glColorTableEXT(GL_TEXTURE_2D, GL_RGBA8, 256, internalFormat, GL_UNSIGNED_BYTE, data);
            ptr += 256*3;
            glTexImage2D(GL_TEXTURE_2D, level, GL_COLOR_INDEX8_EXT, width, height, border, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, ptr);
            return;


        case GL_PALETTE8_RGBA8_OES:
        //
            glColorTableEXT(GL_TEXTURE_2D, GL_RGBA8, 256, internalFormat, GL_UNSIGNED_BYTE, data);
            ptr += 256*4;
            glTexImage2D(GL_TEXTURE_2D, level, GL_COLOR_INDEX8_EXT, width, height, border, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, ptr);
            return;


        case GL_PALETTE8_RGBA4_OES:
        case GL_PALETTE8_RGB5_A1_OES:
        case GL_PALETTE8_R5_G6_B5_OES:

            glColorTableEXT(GL_TEXTURE_2D, GL_RGBA8, 256, internalFormat, GL_UNSIGNED_BYTE, data);
            ptr += 256*2;
            glTexImage2D(GL_TEXTURE_2D, level, GL_COLOR_INDEX8_EXT, width, height, border, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, ptr);
            return;

        default: {
            fprintf(stderr, "Unsupported internalFormat: %d\n", internalFormat);
            _glKosThrowError(GL_INVALID_OPERATION, __func__);
            return;
        }
    }

    gl_assert(ACTIVE_TEXTURE < MAX_GLDC_TEXTURE_UNITS);
    TextureObject* active = TEXTURE_UNITS[ACTIVE_TEXTURE];
    GLuint original_id = active->index;

    if(!active) {
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        return;
    }

    /* Set the required mipmap count */
    active->width   = width;
    active->height  = height;
    active->color   = _determinePVRFormat(
        internalFormat,
        internalFormat  /* Doesn't matter (see determinePVRFormat) */
    );
    active->mipmapCount = _glGetMipmapLevelCount(active);
    active->mipmap = (mipmapped) ? ~0 : (1 << level);  /* Set only a single bit if this wasn't mipmapped otherwise set all */
    active->isCompressed = GL_TRUE;

    /* Odds are slim new data is same size as old, so free always */
    if(active->data) {
        yalloc_free(YALLOC_BASE, active->data);
    }

    active->data = yalloc_alloc_and_defrag(imageSize);

    gl_assert(active->data);  // Debug assert

    if(!active->data) {  // Release, bail out "gracefully"
        _glKosThrowError(GL_OUT_OF_MEMORY, __func__);
        return;
    }

    if(data) {
        FASTCPY(active->data, data, imageSize);
    }

    gl_assert(original_id == active->index);

    _glGPUStateMarkDirty();
}

static GLint _cleanInternalFormat(GLint internalFormat) {
    switch (internalFormat) {
    case GL_COLOR_INDEX4_EXT:
        return GL_COLOR_INDEX4_EXT;
    case GL_COLOR_INDEX8_EXT:
        return GL_COLOR_INDEX8_EXT;
    case GL_ALPHA:
/*    case GL_ALPHA4:
    case GL_ALPHA8:
    case GL_ALPHA12:
    case GL_ALPHA16:*/
       return GL_ALPHA;
    case 1:
    case GL_LUMINANCE:
/*    case GL_LUMINANCE4:
    case GL_LUMINANCE8:
    case GL_LUMINANCE12:
    case GL_LUMINANCE16:*/
       return GL_LUMINANCE;
    case 2:
    case GL_LUMINANCE_ALPHA:
/*    case GL_LUMINANCE4_ALPHA4:
    case GL_LUMINANCE6_ALPHA2:
    case GL_LUMINANCE8_ALPHA8:
    case GL_LUMINANCE12_ALPHA4:
    case GL_LUMINANCE12_ALPHA12:
    case GL_LUMINANCE16_ALPHA16: */
       return GL_LUMINANCE_ALPHA;
/*    case GL_INTENSITY:
    case GL_INTENSITY4:
    case GL_INTENSITY8:
    case GL_INTENSITY12:
    case GL_INTENSITY16:
       return GL_INTENSITY; */
    case 3:
       return GL_RGB;
    case GL_RGB:
    /* case GL_R3_G3_B2: */
    case GL_RGB4:
    case GL_RGB5:
    case GL_RGB8:
    case GL_RGB10:
    case GL_RGB12:
    case GL_RGB16:
       return GL_RGB;
    case 4:
       return GL_RGBA;
    case GL_RGBA:
    case GL_RGBA2:
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_RGBA8:
    case GL_RGB10_A2:
    case GL_RGBA12:
    case GL_RGBA16:
        return GL_RGBA;

    /* Support ARB_texture_rg */
    case GL_RED:
/*    case GL_R8:
    case GL_R16:
    case GL_RED:
    case GL_COMPRESSED_RED: */
        return GL_RED;
/*    case GL_RG:
    case GL_RG8:
    case GL_RG16:
    case GL_COMPRESSED_RG:
        return GL_RG;*/
    default:
        return -1;
    }
}

 static GLuint _determinePVRFormat(GLint internalFormat, GLenum type) {
    /* Given a cleaned internalFormat, return the Dreamcast format
     * that can hold it
     */
    switch(internalFormat) {
        case GL_ALPHA:
        case GL_LUMINANCE:
        case GL_LUMINANCE_ALPHA:
        case GL_RGBA:
        /* OK so if we have something that requires alpha, we return 4444 unless
         * the type was already 1555 (1-bit alpha) in which case we return that
         */
            if(type == GL_UNSIGNED_SHORT_1_5_5_5_REV) {
                return GPU_TXRFMT_ARGB1555 | GPU_TXRFMT_NONTWIDDLED;
            } else if(type == GL_UNSIGNED_SHORT_1_5_5_5_REV_TWID_KOS) {
                return GPU_TXRFMT_ARGB1555 | GPU_TXRFMT_TWIDDLED;
            } else if(type == GL_UNSIGNED_SHORT_4_4_4_4_REV_TWID_KOS) {
                return GPU_TXRFMT_ARGB4444 | GPU_TXRFMT_TWIDDLED;
            } else {
                return GPU_TXRFMT_ARGB4444 | GPU_TXRFMT_NONTWIDDLED;
            }
        case GL_RED:
        case GL_RGB:
            switch(type) {
                case GL_UNSIGNED_SHORT_5_6_5_TWID_KOS:
                    return GPU_TXRFMT_RGB565 | GPU_TXRFMT_TWIDDLED;
                case GL_COMPRESSED_RGB_565_VQ_KOS:
                case GL_COMPRESSED_RGB_565_VQ_MIPMAP_KOS:
                    return GPU_TXRFMT_RGB565 | GPU_TXRFMT_NONTWIDDLED | GPU_TXRFMT_VQ_ENABLE;
                case GL_COMPRESSED_RGB_565_VQ_TWID_KOS:
                case GL_COMPRESSED_RGB_565_VQ_MIPMAP_TWID_KOS:
                    return GPU_TXRFMT_RGB565 | GPU_TXRFMT_TWIDDLED | GPU_TXRFMT_VQ_ENABLE;
                default:
                    return GPU_TXRFMT_RGB565 | GPU_TXRFMT_NONTWIDDLED;
            }
        break;
        /* Compressed and twiddled versions */
        case GL_UNSIGNED_SHORT_5_6_5_TWID_KOS:
            return GPU_TXRFMT_RGB565 | GPU_TXRFMT_TWIDDLED;
        case GL_UNSIGNED_SHORT_4_4_4_4_REV_TWID_KOS:
            return GPU_TXRFMT_ARGB4444 | GPU_TXRFMT_TWIDDLED;
        case GL_UNSIGNED_SHORT_1_5_5_5_REV_TWID_KOS:
            return GPU_TXRFMT_ARGB1555 | GPU_TXRFMT_TWIDDLED;
        case GL_COMPRESSED_RGB_565_VQ_KOS:
        case GL_COMPRESSED_RGB_565_VQ_MIPMAP_KOS:
            return GPU_TXRFMT_RGB565 | GPU_TXRFMT_NONTWIDDLED | GPU_TXRFMT_VQ_ENABLE;
        case GL_COMPRESSED_RGB_565_VQ_TWID_KOS:
        case GL_COMPRESSED_RGB_565_VQ_MIPMAP_TWID_KOS:
            return GPU_TXRFMT_RGB565 | GPU_TXRFMT_TWIDDLED | GPU_TXRFMT_VQ_ENABLE;
        case GL_COMPRESSED_ARGB_4444_VQ_TWID_KOS:
        case GL_COMPRESSED_ARGB_4444_VQ_MIPMAP_TWID_KOS:
            return GPU_TXRFMT_ARGB4444 | GPU_TXRFMT_TWIDDLED | GPU_TXRFMT_VQ_ENABLE;
        case GL_COMPRESSED_ARGB_4444_VQ_KOS:
        case GL_COMPRESSED_ARGB_4444_VQ_MIPMAP_KOS:
            return GPU_TXRFMT_ARGB4444 | GPU_TXRFMT_NONTWIDDLED | GPU_TXRFMT_VQ_ENABLE;
        case GL_COMPRESSED_ARGB_1555_VQ_KOS:
        case GL_COMPRESSED_ARGB_1555_VQ_MIPMAP_KOS:
            return GPU_TXRFMT_ARGB1555 | GPU_TXRFMT_NONTWIDDLED | GPU_TXRFMT_VQ_ENABLE;
        case GL_COMPRESSED_ARGB_1555_VQ_TWID_KOS:
        case GL_COMPRESSED_ARGB_1555_VQ_MIPMAP_TWID_KOS:
            return GPU_TXRFMT_ARGB1555 | GPU_TXRFMT_TWIDDLED | GPU_TXRFMT_VQ_ENABLE;
        case GL_COLOR_INDEX8_EXT:
            return GPU_TXRFMT_PAL8BPP | GPU_TXRFMT_TWIDDLED;
        case GL_COLOR_INDEX4_EXT:
            return GPU_TXRFMT_PAL4BPP | GPU_TXRFMT_TWIDDLED;
        default:
            return 0;
    }
}


typedef void (*TextureConversionFunc)(const GLubyte*, GLubyte*);

GL_FORCE_INLINE void _rgba8888_to_argb4444(const GLubyte* source, GLubyte* dest) {
    *((GLushort*) dest) = (source[3] & 0xF0) << 8 | (source[0] & 0xF0) << 4 | (source[1] & 0xF0) | (source[2] & 0xF0) >> 4;
}

GL_FORCE_INLINE void _rgba8888_to_rgba4444(const GLubyte* source, GLubyte* dest) {
    *((GLushort*) dest) = (source[1] & 0xF0) << 8 | (source[2] & 0xF0) << 4 | (source[0] & 0xF0) | (source[3] & 0xF0) >> 4;
}

GL_FORCE_INLINE void _rgb888_to_argb4444(const GLubyte* source, GLubyte* dest) {

    *((GLushort*) dest) = 0xF << 8 | (source[0] & 0xF0) << 4 | (source[1] & 0xF0) | (source[2] & 0xF0) >> 4;
}


GL_FORCE_INLINE void _rgba8888_to_rgba8888(const GLubyte* source, GLubyte* dest) {
    /* Noop */
    GLubyte* dst = (GLubyte*) dest;
    dst[0] = source[0];
    dst[1] = source[1];
    dst[2] = source[2];
    dst[3] = source[3];
}

GL_FORCE_INLINE void _rgba4444_to_rgba4444(const GLubyte* source, GLubyte* dest) {
    /* Noop */
    GLubyte* dst = (GLubyte*) dest;
    dst[0] = source[0];
    dst[1] = source[1];
}

GL_FORCE_INLINE void _rgba8888_to_rgb565(const GLubyte* source, GLubyte* dest) {

    *((GLushort*) dest) = ((source[0] & 0b11111000) << 8) | ((source[1] & 0b11111100) << 3) | (source[2] >> 3);
}

GL_FORCE_INLINE void _rgb888_to_rgba8888(const GLubyte* source, GLubyte* dest) {
    /* Noop */
    GLubyte* dst = (GLubyte*) dest;
    dst[0] = source[0];
    dst[1] = source[1];
    dst[2] = source[2];
    dst[3] = 255;
}

GL_FORCE_INLINE void _rgb888_to_rgba4444(const GLubyte* source, GLubyte* dest) {
    *((GLushort*) dest) = 0xF << 8 | (source[2] & 0xF0) << 4 | (source[1] & 0xF0) | (source[0] & 0xF0) >> 4;
}

GL_FORCE_INLINE void _rgb888_to_rgb565(const GLubyte* source, GLubyte* dest) {
    *((GLushort*) dest) = ((source[0] & 0b11111000) << 8) | ((source[1] & 0b11111100) << 3) | (source[2] >> 3);
}

GL_FORCE_INLINE void _rgb565_to_rgb8888(const GLubyte* source, GLubyte* dest) {
    GLushort src = *((GLushort*) source);

    dest[3] = (src & 0x1f) << 3;
    dest[2] = ((src >> 5) & 0x3f) <<2;
    dest[1] = ((src >> 11) & 0x1f) <<3;
    dest[0] = 0xff;
}

GL_FORCE_INLINE void _rgba8888_to_a000(const GLubyte* source, GLubyte* dest) {
    *((GLushort*) dest) = ((source[3] & 0b11111000) << 8);
}

GL_FORCE_INLINE void _r8_to_rgb565(const GLubyte* source, GLubyte* dest) {
    *((GLushort*) dest) = (source[0] & 0b11111000) << 8;
}

GL_FORCE_INLINE void _rgba4444_to_argb4444(const GLubyte* source, GLubyte* dest) {
    GLushort* src = (GLushort*) source;
    *((GLushort*) dest) = ((*src & 0x000F) << 12) | *src >> 4;
}

GL_FORCE_INLINE void _rgba4444_to_rgba8888(const GLubyte* source, GLubyte* dest) {
    GLushort src = *((GLushort*) source);
    GLubyte* dst = (GLubyte*) dest;

    dst[0] = (src & 0xf) << 4;
    dst[1] = ((src >> 4) & 0xf) << 4;
    dst[2] = ((src >> 8) & 0xf) << 4;
    dst[3] = (src >> 12) << 4;
}

GL_FORCE_INLINE void _rgba5551_to_rgba8888(const GLubyte* source, GLubyte* dest) {
    GLushort src = *((GLushort*) source);
    GLubyte* dst = (GLubyte*) dest;

    dst[0] = (src & 0x1f) << 3;
    dst[1] = ((src >> 5) & 0x1f) << 3;
    dst[2] = ((src >> 5) & 0x1f) << 3;
    dst[3] = (src >> 15) << 7;
}

GL_FORCE_INLINE void _i8_to_i8(const GLubyte* source, GLubyte* dest) {
    /* For indexes */
    GLubyte* dst = (GLubyte*) dest;
    *dst = *source;
}

static inline void _a8_to_argb4444(const GLubyte* source, GLubyte* dest) {
    GLushort color = *source & 0xf0;
    color |= (color >> 4);
    *((GLushort*) dest) = (color << 8) | color;
}

static TextureConversionFunc _determineConversion(GLint internalFormat, GLenum format, GLenum type) {
    switch(internalFormat) {
    case GL_ALPHA: {
        if(format == GL_ALPHA) {
            /* Dreamcast doesn't really support GL_ALPHA internally, so store as argb with each rgb value as white */
            /* Applying alpha values to all channels seems a better option*/
            return _a8_to_argb4444;
        } else if(type == GL_UNSIGNED_BYTE && format == GL_RGBA) {
            return _rgba8888_to_a000;
        } else if(type == GL_BYTE && format == GL_RGBA) {
            return _rgba8888_to_a000;
        }
    } break;
    case GL_RED: {
        if(type == GL_UNSIGNED_BYTE && format == GL_RED) {
            /* Dreamcast doesn't really support GL_RED internally, so store as rgb */
            return _r8_to_rgb565;
        }
    } break;
    case GL_RGB: {
        if(type == GL_UNSIGNED_BYTE && format == GL_RGB) {
            return _rgb888_to_rgb565;
        } else if(type == GL_UNSIGNED_BYTE && format == GL_RGBA) {
            return _rgba8888_to_rgb565;
        } else if(type == GL_BYTE && format == GL_RGB) {
            return _rgb888_to_rgb565;
        } else if(type == GL_UNSIGNED_BYTE && format == GL_RED) {
            return _r8_to_rgb565;
        }
    } break;
    case GL_RGBA: {
        if(type == GL_UNSIGNED_BYTE && format == GL_RGBA) {
            return _rgba8888_to_argb4444;
        } else if (type == GL_BYTE && format == GL_RGBA) {
            return _rgba8888_to_argb4444;
        } else if(type == GL_UNSIGNED_SHORT_4_4_4_4 && format == GL_RGBA) {
            return _rgba4444_to_argb4444;
        }
    } break;
    case GL_RGBA8:
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_RGB565_KOS:

        if(type == GL_UNSIGNED_BYTE && format == GL_RGBA) {
            return _rgba8888_to_rgba8888;
        }
        else
        if (type == GL_BYTE && format == GL_RGBA) {
            return _rgba8888_to_rgba8888;
        }
        else
        if(type == GL_UNSIGNED_BYTE && format == GL_RGB) {
            return _rgb888_to_rgba8888;
        }
        else
        if (type == GL_BYTE && format == GL_RGB) {
            return _rgb888_to_rgba8888;
        }
        else
        if(type == GL_UNSIGNED_SHORT_4_4_4_4 && format == GL_RGBA) {
            return _rgba4444_to_rgba8888;
        }
       else
        if(type == GL_UNSIGNED_BYTE && format == GL_RGBA4) {
            return _rgba4444_to_rgba8888;
        }
        else
        if(type == GL_UNSIGNED_BYTE && format == GL_RGB5_A1) {
            return _rgba5551_to_rgba8888;
        }
        else
        if(type == GL_UNSIGNED_BYTE && format == GL_RGB565_KOS) {
            return _rgb565_to_rgb8888;
        }
     break;
    case GL_COLOR_INDEX8_EXT:
        if(format == GL_COLOR_INDEX) {
            switch(type) {
                case GL_BYTE:
                case GL_UNSIGNED_BYTE:
                    return _i8_to_i8;
                default:
                    break;
            }
        }
    break;
    default:
        fprintf(stderr, "Unsupported conversion: %x -> %x, %x\n", internalFormat, format, type);
        break;
    }
    return 0;
}

static GLboolean _isSupportedFormat(GLenum format) {
    switch(format) {
    case GL_ALPHA:
    case GL_RED:
    case GL_RGB:
    case GL_RGBA:
    case GL_BGRA:
    case GL_COLOR_INDEX:
    case GL_UNSIGNED_SHORT_4_4_4_4_REV_TWID_KOS:
        return GL_TRUE;
    default:
        return GL_FALSE;
    }
}

GLboolean _glIsMipmapComplete(const TextureObject* obj) {

    // Non-square textures can't have mipmaps
    if(obj->width != obj->height) {
        return GL_FALSE;
    }

    if(!obj->mipmap || !obj->mipmapCount) {
        return GL_FALSE;
    }

    GLsizei i = 0;
    for(; i < (GLubyte) obj->mipmapCount; ++i) {
        if((obj->mipmap & (1 << i)) == 0) {
            return GL_FALSE;
        }
    }

    return GL_TRUE;
}

void _glAllocateSpaceForMipmaps(TextureObject* active) {
    if(active->data && active->baseDataOffset > 0) {
        /* Already done - mipmaps have a dataOffset */
        return;
    }

    /* We've allocated level 0 before, but now we're allocating
     * a level beyond that, we need to reallocate the data, copy level 0
     * then free the original
    */

    GLuint size = active->baseDataSize;

    /* Copy the data out of the pvr and back to ram */
    GLubyte* temp = NULL;
    if(active->data) {
        temp = (GLubyte*) malloc(size);
        memcpy(temp, active->data, size);

        /* Free the PVR data */
        yalloc_free(YALLOC_BASE, active->data);
        active->data = NULL;
    }

    /* Figure out how much room to allocate for mipmaps */
    GLuint bytes = _glGetMipmapDataSize(active);

    active->data = yalloc_alloc_and_defrag(bytes);

    gl_assert(active->data);

    if(temp) {
        /* If there was existing data, then copy it where it should go */
        memcpy(_glGetMipmapLocation(active, 0), temp, size);

        /* We no longer need this */
        free(temp);
    }

    /* Set the data offset depending on whether or not this is a
     * paletted texure */
    active->baseDataOffset = _glGetMipmapDataOffset(active, 0);
}

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define INFO_MSG(x) fprintf(stderr, "%s:%s > %s\n", __FILE__, TOSTRING(__LINE__), x)

void APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalFormat,
                           GLsizei width, GLsizei height, GLint border,
                           GLenum format, GLenum type, const GLvoid *data) {

    TRACE();

    if(target != GL_TEXTURE_2D) {
        INFO_MSG("");
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        return;
    }

    if (width > 1024 || height > 1024){
        INFO_MSG("Invalid texture size");
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return;
    }

    if(format != GL_COLOR_INDEX) {
        if(!_isSupportedFormat(format)) {
            INFO_MSG("Unsupported format");
            _glKosThrowError(GL_INVALID_ENUM, __func__);
            return;
        }

        /* Abuse determineStride to see if type is valid */
        if(_determineStride(GL_RGBA, type) == -1) {
            INFO_MSG("");
            _glKosThrowError(GL_INVALID_ENUM, __func__);
            return;
        }

        internalFormat = _cleanInternalFormat(internalFormat);
        if(internalFormat == -1) {
            INFO_MSG("");
            _glKosThrowError(GL_INVALID_VALUE, __func__);
            return;
        }
    } else {
        if(internalFormat != GL_COLOR_INDEX8_EXT && internalFormat != GL_COLOR_INDEX4_EXT) {
            INFO_MSG("");
            _glKosThrowError(GL_INVALID_ENUM, __func__);
            return;
        }
    }

    GLuint w = width;
    GLuint h = height;
    if(level == 0){
        if((w < 8 || (w & -w) != w)) {
            /* Width is not a power of two. Must be!*/
            INFO_MSG("");
            _glKosThrowError(GL_INVALID_VALUE, __func__);
            return;
        }


        if((h < 8 || (h & -h) != h)) {
            /* height is not a power of two. Must be!*/
            INFO_MSG("");
            _glKosThrowError(GL_INVALID_VALUE, __func__);
            return;
        }
    } else {
        /* Mipmap Errors, kos crashes if 1x1 */
        if((h < 2) || (w < 2)){
            gl_assert(ACTIVE_TEXTURE < MAX_GLDC_TEXTURE_UNITS);
            gl_assert(TEXTURE_UNITS[ACTIVE_TEXTURE]);
            TEXTURE_UNITS[ACTIVE_TEXTURE]->mipmap |= (1 << level);
            return;
        }
    }

    if(level < 0) {
        INFO_MSG("");
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return;
    }

    if(level > 0 && width != height) {
        INFO_MSG("Tried to set non-square texture as a mipmap level");
        printf("[GL ERROR] Mipmaps cannot be supported on non-square textures\n");
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        return;
    }

    if(border) {
        INFO_MSG("");
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return;
    }

    gl_assert(ACTIVE_TEXTURE < MAX_GLDC_TEXTURE_UNITS);
    TextureObject* active = TEXTURE_UNITS[ACTIVE_TEXTURE];

    if(!active) {
        INFO_MSG("Called glTexImage2D on unbound texture");
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        return;
    }

    gl_assert(active);
    GLuint original_id = active->index;

    GLboolean isPaletted = (internalFormat == GL_COLOR_INDEX8_EXT || internalFormat == GL_COLOR_INDEX4_EXT) ? GL_TRUE : GL_FALSE;

    /* Calculate the format that we need to convert the data to */
    GLuint pvr_format = _determinePVRFormat(internalFormat, type);

    if(active->data && (level == 0)) {
        /* pre-existing texture - check if changed */
        if(active->width != width ||
           active->height != height ||
           active->color != pvr_format) {
            /* changed - free old texture memory */
            yalloc_free(YALLOC_BASE, active->data);
            active->data = NULL;
            active->mipmap = 0;
            active->mipmapCount = 0;
            active->mipmap_bias = GL_KOS_INTERNAL_DEFAULT_MIPMAP_LOD_BIAS;
            active->dataStride = 0;
            active->baseDataOffset = 0;
            active->baseDataSize = 0;
        }
    }

    /* All colour formats are represented as shorts internally. Paletted textures
     * are represented by byte indexes (which look up into a color table)
     */
    GLint destStride = isPaletted ? 1 : 2;
    GLuint bytes = (width * height * destStride);

    //special case 4bpp
    if(internalFormat == GL_COLOR_INDEX4_EXT){
        bytes >>= 1;
    }

    if(!active->data) {
        gl_assert(active);
        gl_assert(width);
        gl_assert(height);
        gl_assert(destStride);

        /* need texture memory */
        active->width   = width;
        active->height  = height;
        active->color   = pvr_format;
        /* Set the required mipmap count */
        active->mipmapCount = _glGetMipmapLevelCount(active);
        active->mipmap_bias = GL_KOS_INTERNAL_DEFAULT_MIPMAP_LOD_BIAS;
        active->dataStride = destStride;
        active->baseDataSize = bytes;

        gl_assert(bytes);

        if(level > 0) {
            /* If we're uploading a mipmap level, we need to allocate the full amount of space */
            _glAllocateSpaceForMipmaps(active);
        } else {
            active->data = yalloc_alloc_and_defrag(active->baseDataSize);
        }

        active->isCompressed = GL_FALSE;
        active->isPaletted = isPaletted;
    }

    /* We're supplying a mipmap level, but previously we only had
     * data for the first level (level 0) */
    if(level > 0 && active->baseDataOffset == 0) {
        _glAllocateSpaceForMipmaps(active);
    }

    gl_assert(active->data);

    /* If we run out of PVR memory just return */
    if(!active->data) {
        _glKosThrowError(GL_OUT_OF_MEMORY, __func__);
        gl_assert(active->index == original_id);
        return;
    }

    /* Mark this level as set in the mipmap bitmask */
    active->mipmap |= (1 << level);

    /* Let's assume we need to convert */
    GLboolean needsConversion = GL_TRUE;

    /* Let's assume we need twiddling - we always store things twiddled! */
    GLboolean needsTwiddling = GL_TRUE;

    /*
     * These are the only formats where the source format passed in matches the pvr format.
     * Note the REV formats + GL_BGRA will reverse to ARGB which is what the PVR supports
     */
    if(format == GL_COLOR_INDEX) {
        /* Don't convert color indexes */
        needsConversion = GL_FALSE;

        if(type == GL_UNSIGNED_BYTE_TWID_KOS) {
            needsTwiddling = GL_FALSE;
        }
    } else if(format == GL_BGRA && type == GL_UNSIGNED_SHORT_4_4_4_4_REV && internalFormat == GL_RGBA) {
        needsConversion = GL_FALSE;
    } else if(format == GL_BGRA && type == GL_UNSIGNED_SHORT_1_5_5_5_REV && internalFormat == GL_RGBA) {
        needsConversion = GL_FALSE;
    } else if(format == GL_RGB && type == GL_UNSIGNED_SHORT_5_6_5 && internalFormat == GL_RGB) {
        needsConversion = GL_FALSE;
    } else if(format == GL_RGB && type == GL_UNSIGNED_SHORT_5_6_5_TWID_KOS && internalFormat == GL_RGB) {
        needsConversion = GL_FALSE;
        needsTwiddling = GL_FALSE;
    } else if(format == GL_BGRA && type == GL_UNSIGNED_SHORT_1_5_5_5_REV_TWID_KOS && internalFormat == GL_RGBA) {
        needsConversion = GL_FALSE;
        needsTwiddling = GL_FALSE;
    } else if(format == GL_BGRA && type == GL_UNSIGNED_SHORT_4_4_4_4_REV_TWID_KOS && internalFormat == GL_RGBA) {
        needsConversion = GL_FALSE;
        needsTwiddling = GL_FALSE;
    }

    GLubyte* targetData = (active->baseDataOffset == 0) ? active->data : _glGetMipmapLocation(active, level);
    gl_assert(targetData);

    GLubyte* conversionBuffer = NULL;

    if(!data) {
        /* No data? Do nothing! */
        gl_assert(active->index == original_id);
        return;
    } else if(!needsConversion && !needsTwiddling) {
        gl_assert(targetData);
        gl_assert(data);
        gl_assert(bytes);

        /* No conversion? Just copy the data, and the pvr_format is correct */
        FASTCPY(targetData, data, bytes);
        gl_assert(active->index == original_id);
        return;
    } else if(needsConversion) {
        TextureConversionFunc convert = _determineConversion(
            internalFormat,
            format,
            type
        );

        if(!convert) {
            INFO_MSG("Couldn't find conversion\n");
            _glKosThrowError(GL_INVALID_OPERATION, __func__);
            return;
        }

        GLint stride = _determineStride(format, type);
        gl_assert(stride > -1);

        if(stride == -1) {
            INFO_MSG("Stride was not detected\n");
            _glKosThrowError(GL_INVALID_OPERATION, __func__);
            return;
        }

        conversionBuffer = malloc(bytes);

        GLubyte* dest = conversionBuffer;
        const GLubyte* source = data;

        gl_assert(conversionBuffer);
        gl_assert(source);

        /* Perform the conversion */
        GLuint i;
        for(i = 0; i < bytes; i += destStride) {
            convert(source, dest);

            dest += destStride;
            source += stride;
        }
    }

    if(needsTwiddling) {
        const GLubyte *pixels = (GLubyte*) (conversionBuffer) ? conversionBuffer : data;

        if(internalFormat == GL_COLOR_INDEX8_EXT) {
            GPUTextureTwiddle8PPP((void*) pixels, targetData, width, height);
        }
        else{
            if(internalFormat == GL_COLOR_INDEX4_EXT) {
                GPUTextureTwiddle4PPP((void*) pixels, targetData, width, height);
            }
            else {
                GPUTextureTwiddle16BPP((void*) pixels, targetData, width, height);
            }
        }

        /* We make sure we remove nontwiddled and add twiddled. We could always
         * make it twiddled when determining the format but I worry that would make the
         * code less flexible to change in the future */
        active->color &= ~(1 << 26);
    } else {
        /* We should only get here if we converted twiddled data... which is never currently */
        gl_assert(conversionBuffer);

        // We've already converted the data and we
        // don't need to twiddle it!
        FASTCPY(targetData, conversionBuffer, bytes);
    }

    if(conversionBuffer) {
        free(conversionBuffer);
        conversionBuffer = NULL;
    }

    gl_assert(active->index == original_id);
    _glGPUStateMarkDirty();
}

void APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param) {
    TRACE();

    TextureObject* active = _glGetBoundTexture();

    if(!active) {
        return;
    }

    if(target == GL_TEXTURE_2D) {
        switch(pname) {
            case GL_TEXTURE_MAG_FILTER:
                switch(param) {
                    case GL_NEAREST:
                    case GL_LINEAR:
                    break;
                    default: {
                        _glKosThrowError(GL_INVALID_VALUE, __func__);
                        return;
                    }
                }
                active->magFilter = param;
            break;
            case GL_TEXTURE_MIN_FILTER:
                switch(param) {
                    case GL_NEAREST:
                    case GL_LINEAR:
                    case GL_NEAREST_MIPMAP_LINEAR:
                    case GL_NEAREST_MIPMAP_NEAREST:
                    case GL_LINEAR_MIPMAP_LINEAR:
                    case GL_LINEAR_MIPMAP_NEAREST:
                    break;
                    default: {
                        _glKosThrowError(GL_INVALID_VALUE, __func__);
                        return;
                    }
                }
                active->minFilter = param;
            break;
            case GL_TEXTURE_WRAP_S:
                switch(param) {
                    case GL_CLAMP_TO_EDGE:
                    case GL_CLAMP:
                        active->uv_clamp |= CLAMP_U;
                        break;

                    case GL_REPEAT:
                        active->uv_clamp &= ~CLAMP_U;
                        break;
                }

                break;

            case GL_TEXTURE_WRAP_T:
                switch(param) {
                    case GL_CLAMP_TO_EDGE:
                    case GL_CLAMP:
                        active->uv_clamp |= CLAMP_V;
                        break;

                    case GL_REPEAT:
                        active->uv_clamp &= ~CLAMP_V;
                        break;
                }

                break;
            case GL_SHARED_TEXTURE_BANK_KOS:
                active->shared_bank = param;
                break;
            default:
                break;
        }
    }

    _glGPUStateMarkDirty();
}

void APIENTRY glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    glTexParameteri(target, pname, (GLint) param);
}

GLAPI void APIENTRY glColorTableEXT(GLenum target, GLenum internalFormat, GLsizei width, GLenum format, GLenum type, const GLvoid *data) {

    GLint validTargets[] = {
        GL_TEXTURE_2D,
        GL_SHARED_TEXTURE_PALETTE_EXT,
        GL_SHARED_TEXTURE_PALETTE_0_KOS,GL_SHARED_TEXTURE_PALETTE_1_KOS,GL_SHARED_TEXTURE_PALETTE_2_KOS,GL_SHARED_TEXTURE_PALETTE_3_KOS,GL_SHARED_TEXTURE_PALETTE_4_KOS,GL_SHARED_TEXTURE_PALETTE_5_KOS,GL_SHARED_TEXTURE_PALETTE_6_KOS,GL_SHARED_TEXTURE_PALETTE_7_KOS,GL_SHARED_TEXTURE_PALETTE_8_KOS,GL_SHARED_TEXTURE_PALETTE_9_KOS,
        GL_SHARED_TEXTURE_PALETTE_10_KOS,GL_SHARED_TEXTURE_PALETTE_11_KOS,GL_SHARED_TEXTURE_PALETTE_12_KOS,GL_SHARED_TEXTURE_PALETTE_13_KOS,GL_SHARED_TEXTURE_PALETTE_14_KOS,GL_SHARED_TEXTURE_PALETTE_15_KOS,GL_SHARED_TEXTURE_PALETTE_16_KOS,GL_SHARED_TEXTURE_PALETTE_17_KOS,GL_SHARED_TEXTURE_PALETTE_18_KOS,GL_SHARED_TEXTURE_PALETTE_19_KOS,
        GL_SHARED_TEXTURE_PALETTE_20_KOS,GL_SHARED_TEXTURE_PALETTE_21_KOS,GL_SHARED_TEXTURE_PALETTE_22_KOS,GL_SHARED_TEXTURE_PALETTE_23_KOS,GL_SHARED_TEXTURE_PALETTE_24_KOS,GL_SHARED_TEXTURE_PALETTE_25_KOS,GL_SHARED_TEXTURE_PALETTE_26_KOS,GL_SHARED_TEXTURE_PALETTE_27_KOS,GL_SHARED_TEXTURE_PALETTE_28_KOS,GL_SHARED_TEXTURE_PALETTE_29_KOS,
        GL_SHARED_TEXTURE_PALETTE_30_KOS,GL_SHARED_TEXTURE_PALETTE_31_KOS,GL_SHARED_TEXTURE_PALETTE_32_KOS,GL_SHARED_TEXTURE_PALETTE_33_KOS,GL_SHARED_TEXTURE_PALETTE_34_KOS,GL_SHARED_TEXTURE_PALETTE_35_KOS,GL_SHARED_TEXTURE_PALETTE_36_KOS,GL_SHARED_TEXTURE_PALETTE_37_KOS,GL_SHARED_TEXTURE_PALETTE_38_KOS,GL_SHARED_TEXTURE_PALETTE_39_KOS,
        GL_SHARED_TEXTURE_PALETTE_40_KOS,GL_SHARED_TEXTURE_PALETTE_41_KOS,GL_SHARED_TEXTURE_PALETTE_42_KOS,GL_SHARED_TEXTURE_PALETTE_43_KOS,GL_SHARED_TEXTURE_PALETTE_44_KOS,GL_SHARED_TEXTURE_PALETTE_45_KOS,GL_SHARED_TEXTURE_PALETTE_46_KOS,GL_SHARED_TEXTURE_PALETTE_47_KOS,GL_SHARED_TEXTURE_PALETTE_48_KOS,GL_SHARED_TEXTURE_PALETTE_49_KOS,
        GL_SHARED_TEXTURE_PALETTE_50_KOS,GL_SHARED_TEXTURE_PALETTE_51_KOS,GL_SHARED_TEXTURE_PALETTE_52_KOS,GL_SHARED_TEXTURE_PALETTE_53_KOS,GL_SHARED_TEXTURE_PALETTE_54_KOS,GL_SHARED_TEXTURE_PALETTE_55_KOS,GL_SHARED_TEXTURE_PALETTE_56_KOS,GL_SHARED_TEXTURE_PALETTE_57_KOS,GL_SHARED_TEXTURE_PALETTE_58_KOS,GL_SHARED_TEXTURE_PALETTE_59_KOS,
        GL_SHARED_TEXTURE_PALETTE_60_KOS,GL_SHARED_TEXTURE_PALETTE_61_KOS,GL_SHARED_TEXTURE_PALETTE_62_KOS,GL_SHARED_TEXTURE_PALETTE_63_KOS,
        0};

    GLint validInternalFormats[] = {GL_RGB8, GL_RGBA8, GL_RGBA4, 0};
    GLint validFormats[] = {GL_RGB, GL_RGBA,GL_RGB5_A1, GL_RGB5_A1, GL_RGB565_KOS, GL_RGBA4, 0};
    GLint validTypes[] = {GL_UNSIGNED_BYTE, GL_BYTE, GL_UNSIGNED_SHORT, GL_SHORT, 0};

    if(_glCheckValidEnum(target, validTargets, __func__) != 0) {
        return;
    }

    if(_glCheckValidEnum(internalFormat, validInternalFormats, __func__) != 0) {
        return;
    }

    switch(format){
        case GL_PALETTE4_RGBA8_OES:
        case GL_PALETTE8_RGBA8_OES:
            format = GL_RGBA;
            break;
        case GL_PALETTE4_RGB8_OES:
        case GL_PALETTE8_RGB8_OES:
            format = GL_RGB;
            break;
        case GL_PALETTE4_R5_G6_B5_OES:
        case GL_PALETTE8_R5_G6_B5_OES:
        case GL_UNSIGNED_SHORT_5_6_5:
            format = GL_RGB565_KOS;
            break;
        case GL_PALETTE4_RGB5_A1_OES:
        case GL_PALETTE8_RGB5_A1_OES:
        case GL_UNSIGNED_SHORT_5_5_5_1:
            format = GL_RGB5_A1;
            break;
        case GL_PALETTE4_RGBA4_OES:
        case GL_PALETTE8_RGBA4_OES:
        case GL_UNSIGNED_SHORT_4_4_4_4:
            format = GL_RGBA4;
            break;

    }

    if(_glCheckValidEnum(format, validFormats, __func__) != 0) {
        return;
    }

    if(_glCheckValidEnum(type, validTypes, __func__) != 0) {
        return;
    }

    /* Only allow up to 256 colours in a palette */
    if(width > 256 || width == 0) {
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return;
    }

    GLint sourceStride = _determineStride(format, type);

    gl_assert(sourceStride > -1);

    TextureConversionFunc convert = _determineConversion(
        INTERNAL_PALETTE_FORMAT,
        format,
        type
    );

    if(!convert) {
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        return;
    }

    TexturePalette* palette = NULL;

    GLboolean sharedPaletteUsed = GL_FALSE;

    /* Custom extension - allow uploading to one of 4 custom palettes */
    if(target == GL_SHARED_TEXTURE_PALETTE_EXT || target == GL_SHARED_TEXTURE_PALETTE_0_KOS) {
        palette = SHARED_PALETTES[0];
        sharedPaletteUsed = GL_TRUE;
    }

    for (GLubyte i = 1; i < MAX_GLDC_SHARED_PALETTES; ++i) {
        if (target == GL_SHARED_TEXTURE_PALETTE_0_KOS + i) {
            palette = SHARED_PALETTES[i];
            sharedPaletteUsed = GL_TRUE;
        }
    }
    if (sharedPaletteUsed == GL_FALSE){
        TextureObject* active = _glGetBoundTexture();
        if(!active->palette) {
            active->palette = _initTexturePalette();
        }
        palette = active->palette;
    }

    gl_assert(palette);

    if(target) {
        free(palette->data);
        palette->data = NULL;
    }

    if(palette->bank > -1) {
        _glReleasePaletteSlot(palette->bank, palette->size);
        palette->bank = -1;
    }

    palette->data = (GLubyte*) malloc(width * 4);
    palette->format = format;
    palette->width = width;
    palette->size = (width > 16) ? 256 : 16;
    gl_assert(palette->size == 16 || palette->size == 256);

    palette->bank = _glGenPaletteSlot(palette->size);

    if(palette->bank < 0) {
        /* We ran out of slots! */
        _glKosThrowError(GL_INVALID_OPERATION, __func__);

        free(palette->data);
        palette->format = palette->width = palette->size = 0;
        return;
    }

    GLubyte* src = (GLubyte*) data;
    GLubyte* dst = (GLubyte*) palette->data;

    gl_assert(src);
    gl_assert(dst);

    /* Transform and copy the source palette to the texture */
    GLushort i = 0;
    for(; i < width; ++i) {
        convert(src, dst);

        src += sourceStride;
        dst += 4;
    }

    _glApplyColorTable(palette);

    _glGPUStateMarkDirty();
}

GLAPI void APIENTRY glColorSubTableEXT(GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, const GLvoid *data) {
    _GL_UNUSED(target);
    _GL_UNUSED(start);
    _GL_UNUSED(count);
    _GL_UNUSED(format);
    _GL_UNUSED(type);
    _GL_UNUSED(data);
    _glKosThrowError(GL_INVALID_OPERATION, __func__);
}

GLAPI void APIENTRY glGetColorTableEXT(GLenum target, GLenum format, GLenum type, GLvoid *data) {
    _GL_UNUSED(target);
    _GL_UNUSED(format);
    _GL_UNUSED(type);
    _GL_UNUSED(data);
    _glKosThrowError(GL_INVALID_OPERATION, __func__);
}

GLAPI void APIENTRY glGetColorTableParameterivEXT(GLenum target, GLenum pname, GLint *params) {
    _GL_UNUSED(target);
    _GL_UNUSED(pname);
    _GL_UNUSED(params);
    _glKosThrowError(GL_INVALID_OPERATION, __func__);
}

GLAPI void APIENTRY glGetColorTableParameterfvEXT(GLenum target, GLenum pname, GLfloat *params) {
    _GL_UNUSED(target);
    _GL_UNUSED(pname);
    _GL_UNUSED(params);
    _glKosThrowError(GL_INVALID_OPERATION, __func__);
}

GLAPI void APIENTRY glTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset,
    GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels) {
    _GL_UNUSED(target);
    _GL_UNUSED(level);
    _GL_UNUSED(xoffset);
    _GL_UNUSED(yoffset);
    _GL_UNUSED(width);
    _GL_UNUSED(height);
    _GL_UNUSED(format);
    _GL_UNUSED(type);
    _GL_UNUSED(pixels);
    gl_assert(0 && "Not Implemented");
}

GLAPI void APIENTRY glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    _GL_UNUSED(target);
    _GL_UNUSED(level);
    _GL_UNUSED(xoffset);
    _GL_UNUSED(yoffset);
    _GL_UNUSED(x);
    _GL_UNUSED(y);
    _GL_UNUSED(width);
    _GL_UNUSED(height);
    gl_assert(0 && "Not Implemented");
}

GLAPI void APIENTRY glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width) {
    _GL_UNUSED(target);
    _GL_UNUSED(level);
    _GL_UNUSED(xoffset);
    _GL_UNUSED(x);
    _GL_UNUSED(y);
    _GL_UNUSED(width);
    gl_assert(0 && "Not Implemented");
}

GLAPI void APIENTRY glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    _GL_UNUSED(target);
    _GL_UNUSED(level);
    _GL_UNUSED(internalformat);
    _GL_UNUSED(x);
    _GL_UNUSED(y);
    _GL_UNUSED(width);
    _GL_UNUSED(height);
    _GL_UNUSED(border);
    gl_assert(0 && "Not Implemented");
}

GLAPI void APIENTRY glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border) {
    _GL_UNUSED(target);
    _GL_UNUSED(level);
    _GL_UNUSED(internalformat);
    _GL_UNUSED(x);
    _GL_UNUSED(y);
    _GL_UNUSED(width);
    _GL_UNUSED(border);
    gl_assert(0 && "Not Implemented");
}

GLAPI void APIENTRY glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels) {
    _GL_UNUSED(x);
    _GL_UNUSED(y);
    _GL_UNUSED(width);
    _GL_UNUSED(height);
    _GL_UNUSED(format);
    _GL_UNUSED(type);
    _GL_UNUSED(pixels);
    gl_assert(0 && "Not Implemented");
}
GLuint _glMaxTextureMemory() {
    return YALLOC_SIZE;
}

GLuint _glFreeTextureMemory() {
    return yalloc_count_free(YALLOC_BASE);
}

GLuint _glUsedTextureMemory() {
    return YALLOC_SIZE - _glFreeTextureMemory();
}

GLuint _glFreeContiguousTextureMemory() {
    return yalloc_count_continuous(YALLOC_BASE);
}

GLAPI GLvoid APIENTRY glDefragmentTextureMemory_KOS(void) {
    yalloc_defrag_start(YALLOC_BASE);

    GLuint id;

    /* Replace all texture pointers */
    for(id = 0; id < MAX_TEXTURE_COUNT; id++){
        TextureObject* txr = (TextureObject*) named_array_get(&TEXTURE_OBJECTS, id);
        if(txr){
            gl_assert(txr->index == id);
            txr->data = yalloc_defrag_address(YALLOC_BASE, txr->data);
        }
    }

    yalloc_defrag_commit(YALLOC_BASE);
}

GLAPI void APIENTRY glGetTexImage(GLenum tex, GLint lod, GLenum format, GLenum type, GLvoid* img) {
    _GL_UNUSED(tex);
    _GL_UNUSED(lod);
    _GL_UNUSED(format);
    _GL_UNUSED(type);
    _GL_UNUSED(img);
}
