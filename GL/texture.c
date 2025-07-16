#include "private.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "platform.h"

#include "alloc/alloc.h"

/* We always leave this amount of vram unallocated to prevent
 * issues with the allocator */
#define PVR_MEM_BUFFER_SIZE (64 * 1024)

/* Clamp settings go in lower bits of uv_wrap
 * Mirror settings go in upper bits */
#define MIRROR_U (1<<3)
#define MIRROR_V (1<<2)
#define CLAMP_U (1<<1)
#define CLAMP_V (1<<0)

static TextureObject* TEXTURE_UNITS[MAX_GLDC_TEXTURE_UNITS] = {NULL, NULL};
static NamedArray TEXTURE_OBJECTS;
GLubyte ACTIVE_TEXTURE = 0;

static TexturePalette* SHARED_PALETTES[MAX_GLDC_SHARED_PALETTES] = {NULL, NULL, NULL, NULL};

static GLuint _determinePVRFormat(GLint internalFormat);

static GLboolean BANKS_USED[MAX_GLDC_PALETTE_SLOTS];  // Each time a 256 colour bank is used, this is set to true
static GLboolean SUBBANKS_USED[MAX_GLDC_PALETTE_SLOTS][MAX_GLDC_4BPP_PALETTE_SLOTS]; // 4 counts of the used 16 colour banks within the 256 ones

static GLenum INTERNAL_PALETTE_FORMAT = GL_RGBA4;
static GLboolean TEXTURE_TWIDDLE_ENABLED = GL_FALSE;

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define INFO_MSG(x) fprintf(stderr, "%s:%s > %s\n", __FILE__, TOSTRING(__LINE__), x)

static void* ALLOC_BASE = NULL;
static size_t ALLOC_SIZE = 0;

static struct TwiddleTable {
    int32_t width;
    int32_t height;
    int32_t* table;
} TWIDDLE_TABLE = {0, 0, NULL};

static void calc_twiddle_factors(uint32_t w, uint32_t h, uint32_t* maskX, uint32_t* maskY) {
    *maskX = 0;
    *maskY = 0;
    int shift = 0;

    for (; w > 1 || h > 1; w >>= 1, h >>= 1) {
        if (w > 1 && h > 1) {
            // Add interleaved X and Y bits
            *maskX += 0x02 << shift;
            *maskY += 0x01 << shift;
            shift  += 2;
        } else if (w > 1) {
            // Add a linear X bit
            *maskX += 0x01 << shift;
            shift  += 1;		
        } else if (h > 1) {
            // Add a linear Y bit
            *maskY += 0x01 << shift;
            shift  += 1;
        }
    }
}

void build_twiddle_table(int32_t w, int32_t h) {
    free(TWIDDLE_TABLE.table);
    TWIDDLE_TABLE.table = (int32_t*) malloc(w * h * sizeof(int32_t));
    TWIDDLE_TABLE.width = w;
    TWIDDLE_TABLE.height = h;

    int32_t idx = 0;
    uint32_t idxX = 0, idxY = 0, maskX, maskY;
    calc_twiddle_factors(w, h, &maskX, &maskY);

    for (int32_t y = 0; y < h; y++) {
        idxX = 0;		
        for (int32_t x = 0; x < w; x++) {
            TWIDDLE_TABLE.table[idx++] = idxX | idxY;
            idxX = (idxX - maskX) & maskX;
        }
        idxY = (idxY - maskY) & maskY;
    }
}

static void twid_prepare_table(uint32_t w, uint32_t h) {
    if(TWIDDLE_TABLE.width != w || TWIDDLE_TABLE.height != h || !TWIDDLE_TABLE.table) {
        build_twiddle_table(w, h);
    }
}

/* Given a 0-based texel location, returns new 0-based texel location */
/* NOTE: twid_prepare_table must have been called beforehand for correct behaviour */
GL_FORCE_INLINE uint32_t twid_location(uint32_t i) {
    return TWIDDLE_TABLE.table[i];
}


static void* alloc_malloc_and_defrag(size_t size) {
    void* ret = alloc_malloc(ALLOC_BASE, size);

    if(!ret) {
        /* Tried to allocate, but out of room, let's try defragging
         * and repeating the alloc */
        fprintf(stderr, "Ran out of memory, defragmenting\n");
        glDefragmentTextureMemory_KOS();
        ret = alloc_malloc(ALLOC_BASE, size);
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
        GLushort bank = slot / MAX_GLDC_4BPP_PALETTE_SLOTS;
        GLushort subbank = slot % MAX_GLDC_4BPP_PALETTE_SLOTS;

        gl_assert(bank < MAX_GLDC_PALETTE_SLOTS);
        gl_assert(subbank < MAX_GLDC_4BPP_PALETTE_SLOTS);

        SUBBANKS_USED[bank][subbank] = GL_FALSE;

        for (i = 0; i < MAX_GLDC_4BPP_PALETTE_SLOTS; ++i) {
            if (SUBBANKS_USED[bank][i]) {
                return;
            }
        }
        BANKS_USED[bank] = GL_FALSE;
    }
    else {
        gl_assert(slot < MAX_GLDC_PALETTE_SLOTS);
        BANKS_USED[slot] = GL_FALSE;
        for (i = 0; i < MAX_GLDC_4BPP_PALETTE_SLOTS; ++i) {
            SUBBANKS_USED[slot][i] = GL_FALSE;
        }
    }
}

GLboolean _glGetTextureTwiddle() {
    return TEXTURE_TWIDDLE_ENABLED;
}

void _glSetTextureTwiddle(GLboolean v) {
    TEXTURE_TWIDDLE_ENABLED = v;
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

static GLint _determineStrideInternal(GLenum internalFormat) {
    switch(internalFormat) {
        case GL_RGB565_KOS:
        case GL_ARGB4444_KOS:
        case GL_ARGB1555_KOS:
        case GL_RGB565_TWID_KOS:
        case GL_ARGB4444_TWID_KOS:
        case GL_ARGB1555_TWID_KOS:
            return 2;
        case GL_COLOR_INDEX8_TWID_KOS:
        case GL_COLOR_INDEX4_TWID_KOS:
        case GL_COLOR_INDEX4_EXT:
        case GL_COLOR_INDEX8_EXT:
            return 1;
        case GL_RGBA8:
            return 4;
        case GL_RGB8:
            return 3;
        case GL_RGBA4:
            return 2;
    }

    return -1;
}


static GLint _determineStride(GLenum format, GLenum type) {
    switch(type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
        switch(format) {
        case GL_RED:
        case GL_ALPHA:
        case GL_COLOR_INDEX:
        case GL_COLOR_INDEX8_TWID_KOS:
        case GL_COLOR_INDEX4_TWID_KOS:  // We return 1 for 4bpp, but it gets sorted later
        case GL_COLOR_INDEX4_EXT:
        case GL_COLOR_INDEX8_EXT:
            return 1;
        case GL_RGB:
            return 3;
        case GL_RGBA:
            return 4;
        default:
            break;
        }
        break;
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

    fprintf(stderr, "Couldn't find stride for format: 0x%x type: 0x%x\n", format, type);
    _glKosThrowError(GL_INVALID_VALUE, __func__);
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
    txr->uv_wrap = 0;
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
    named_array_init(&TEXTURE_OBJECTS, sizeof(TextureObject), MAX_TEXTURE_COUNT);

    // Reserve zero so that it is never given to anyone as an ID!
    named_array_reserve(&TEXTURE_OBJECTS, 0);

    // Initialize zero as an actual texture object though because apparently it is!
    TextureObject* default_tex = (TextureObject*) named_array_get(&TEXTURE_OBJECTS, 0);
    _glInitializeTextureObject(default_tex, 0);
    TEXTURE_UNITS[0] = default_tex;
    TEXTURE_UNITS[1] = default_tex;

    for(int i = 0; i < MAX_GLDC_SHARED_PALETTES; i++){
        SHARED_PALETTES[i] = _initTexturePalette();
    }

    _glResetSharedPalettes();

    //memset((void*) BANKS_USED, 0x0, sizeof(BANKS_USED));
    //memset((void*) SUBBANKS_USED, 0x0, sizeof(SUBBANKS_USED));

    size_t vram_free = GPUMemoryAvailable();
    ALLOC_SIZE = vram_free - PVR_MEM_BUFFER_SIZE; /* Take all but 64kb VRAM */
    ALLOC_BASE = GPUMemoryAlloc(ALLOC_SIZE);

#ifdef _arch_dreamcast
    /* Ensure memory is aligned */
    gl_assert((uintptr_t) ALLOC_BASE % 32 == 0);
#endif

    alloc_init(ALLOC_BASE, ALLOC_SIZE);

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

GLint _glGetTextureInternalFormat() {
    TextureObject* obj = _glGetBoundTexture();
    if(!obj) {
        return -1;
    }

    return obj->internalFormat;
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

    for(GLsizei i = 0; i < n; ++i) {
        GLuint id = 0;
        TextureObject* txr = (TextureObject*) named_array_alloc(&TEXTURE_OBJECTS, &id);
        gl_assert(txr);
        gl_assert(id);  // Generated IDs must never be zero

        _glInitializeTextureObject(txr, id);


        gl_assert(txr->index == id);

        textures[i] = id;
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
                    // Reset to the default texture
                    TEXTURE_UNITS[j] = (TextureObject*) named_array_get(&TEXTURE_OBJECTS, 0);
                }
            }

            if(txr->data) {
                alloc_free(ALLOC_BASE, txr->data);
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
            glTexImage2D(GL_TEXTURE_2D, level, GL_COLOR_INDEX4_EXT, width, height, border, GL_COLOR_INDEX4_EXT, GL_UNSIGNED_BYTE, ptr);
            return;

        case GL_PALETTE4_RGBA8_OES:
            glColorTableEXT(GL_TEXTURE_2D, GL_RGBA8, 16, internalFormat, GL_UNSIGNED_BYTE, data);
            ptr += 16*4;
            glTexImage2D(GL_TEXTURE_2D, level, GL_COLOR_INDEX4_EXT, width, height, border, GL_COLOR_INDEX4_EXT, GL_UNSIGNED_BYTE, ptr);
            return;

        case GL_PALETTE4_R5_G6_B5_OES:
        case GL_PALETTE4_RGBA4_OES:
        case GL_PALETTE4_RGB5_A1_OES:
            glColorTableEXT(GL_TEXTURE_2D, GL_RGBA8, 16, internalFormat, GL_UNSIGNED_BYTE, data);
            ptr += 16*2;
            glTexImage2D(GL_TEXTURE_2D, level, GL_COLOR_INDEX4_EXT, width, height, border, GL_COLOR_INDEX4_EXT, GL_UNSIGNED_BYTE, ptr);
            return;

        case GL_PALETTE8_RGB8_OES:
            glColorTableEXT(GL_TEXTURE_2D, GL_RGBA8, 256, internalFormat, GL_UNSIGNED_BYTE, data);
            ptr += 256*3;
            glTexImage2D(GL_TEXTURE_2D, level, GL_COLOR_INDEX8_EXT, width, height, border, GL_COLOR_INDEX8_EXT, GL_UNSIGNED_BYTE, ptr);
            return;


        case GL_PALETTE8_RGBA8_OES:
            glColorTableEXT(GL_TEXTURE_2D, GL_RGBA8, 256, internalFormat, GL_UNSIGNED_BYTE, data);
            ptr += 256*4;
            glTexImage2D(GL_TEXTURE_2D, level, GL_COLOR_INDEX8_EXT, width, height, border, GL_COLOR_INDEX8_EXT, GL_UNSIGNED_BYTE, ptr);
            return;
        case GL_PALETTE8_RGBA4_OES:
        case GL_PALETTE8_RGB5_A1_OES:
        case GL_PALETTE8_R5_G6_B5_OES:

            glColorTableEXT(GL_TEXTURE_2D, GL_RGBA8, 256, internalFormat, GL_UNSIGNED_BYTE, data);
            ptr += 256*2;
            glTexImage2D(GL_TEXTURE_2D, level, GL_COLOR_INDEX8_EXT, width, height, border, GL_COLOR_INDEX8_EXT, GL_UNSIGNED_BYTE, ptr);
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
    active->internalFormat = internalFormat;
    active->color   = _determinePVRFormat(internalFormat);
    active->mipmapCount = _glGetMipmapLevelCount(active);
    active->mipmap = (mipmapped) ? ~0 : (1 << level);  /* Set only a single bit if this wasn't mipmapped otherwise set all */
    active->isCompressed = GL_TRUE;

    /* Odds are slim new data is same size as old, so free always */
    if(active->data) {
        alloc_free(ALLOC_BASE, active->data);
    }

    active->data = alloc_malloc_and_defrag(imageSize);

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

void APIENTRY glCompressedTexSubImage2DARB(GLenum target,
                                           GLint level,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLsizei width,
                                           GLsizei height,
                                           GLenum format,
                                           GLsizei imageSize,
                                           const GLvoid *data) {
    TRACE();

    if (target != GL_TEXTURE_2D) {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        return;
    }

    if (xoffset < 0 || yoffset < 0 || width <= 0 || height <= 0) {
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return;
    }

    gl_assert(ACTIVE_TEXTURE < MAX_GLDC_TEXTURE_UNITS);
    TextureObject* active = TEXTURE_UNITS[ACTIVE_TEXTURE];

    if (!active) {
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        return;
    }

    GLuint original_id = active->index;

    // Ensure that we're modifying the correct texture
    if (active->index != original_id) {
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        return;
    }

    // Copy raw compressed texture data into the texture buffer.
    GLubyte* targetData = active->data;
    GLubyte* src = (GLubyte*)data;

    // Copy the compressed data directly to the texture
    if (data) {
        FASTCPY(targetData + (yoffset * active->width + xoffset), src, imageSize);
    }

    _glGPUStateMarkDirty();
}

/**
 * Takes an internal format, and returns a GL format matching how we'd store
 * it internally, so it'll return one of the following:
 *
 * - GL_RGB565_KOS,
 * - GL_ARGB4444_KOS
 * - GL_ARGB1555_KOS
 * - GL_RGB565_TWID_KOS
 * - GL_ARGB4444_TWID_KOS
 * - GL_ARGB1555_TWID_KOS
 * - GL_COLOR_INDEX8_EXT
 * - GL_COLOR_INDEX4_EXT
 * - GL_COLOR_INDEX8_TWID_KOS
 * - GL_COLOR_INDEX4_TWID_KOS
 */
static GLint _cleanInternalFormat(GLint internalFormat) {
    switch (internalFormat) {
    /* All of these formats are fine as they are, no conversion needed */
    case GL_RGB565_KOS:
    case GL_ARGB4444_KOS:
    case GL_ARGB1555_KOS:
    case GL_RGB565_TWID_KOS:
    case GL_ARGB4444_TWID_KOS:
    case GL_ARGB1555_TWID_KOS:
    case GL_COLOR_INDEX8_TWID_KOS:
    case GL_COLOR_INDEX4_TWID_KOS:
        return internalFormat;
    /* Paletted textures are always twiddled.. otherwise they don't work! */
    case GL_COLOR_INDEX4_EXT:
        return GL_COLOR_INDEX4_TWID_KOS;
    case GL_COLOR_INDEX8_EXT:
        return GL_COLOR_INDEX8_TWID_KOS;

    case GL_RGB_TWID_KOS:
        return GL_RGB565_TWID_KOS;
    case GL_RGBA_TWID_KOS:
        return GL_ARGB4444_TWID_KOS;
    case GL_ALPHA:
    case GL_ALPHA4:
    case GL_ALPHA8:
    case GL_ALPHA12:
    case GL_ALPHA16:
       return (TEXTURE_TWIDDLE_ENABLED) ? GL_ARGB4444_TWID_KOS : GL_ARGB4444_KOS;
    case 1:
    case GL_LUMINANCE:
    case GL_LUMINANCE4:
    case GL_LUMINANCE8:
    case GL_LUMINANCE12:
    case GL_LUMINANCE16:
       return (TEXTURE_TWIDDLE_ENABLED) ? GL_ARGB1555_TWID_KOS : GL_ARGB1555_KOS;
    case 2:
    case GL_LUMINANCE_ALPHA:
    case GL_LUMINANCE4_ALPHA4:
    case GL_LUMINANCE6_ALPHA2:
    case GL_LUMINANCE8_ALPHA8:
    case GL_LUMINANCE12_ALPHA4:
    case GL_LUMINANCE12_ALPHA12:
    case GL_LUMINANCE16_ALPHA16:
       return (TEXTURE_TWIDDLE_ENABLED) ? GL_ARGB4444_TWID_KOS : GL_ARGB4444_KOS;
    case GL_INTENSITY:
    case GL_INTENSITY4:
    case GL_INTENSITY8:
    case GL_INTENSITY12:
    case GL_INTENSITY16:
       return (TEXTURE_TWIDDLE_ENABLED) ? GL_ARGB4444_TWID_KOS : GL_ARGB4444_KOS;
    case 3:
       return (TEXTURE_TWIDDLE_ENABLED) ? GL_RGB565_TWID_KOS : GL_RGB565_KOS;
    case GL_RGB:
    case GL_R3_G3_B2:
    case GL_RGB4:
    case GL_RGB5:
    case GL_RGB8:
    case GL_RGB10:
    case GL_RGB12:
    case GL_RGB16:
       return (TEXTURE_TWIDDLE_ENABLED) ? GL_RGB565_TWID_KOS : GL_RGB565_KOS;
    case 4:
       return (TEXTURE_TWIDDLE_ENABLED) ? GL_ARGB4444_TWID_KOS : GL_ARGB4444_KOS;
    case GL_RGBA:
    case GL_RGBA2:
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_RGBA8:
    case GL_RGB10_A2:
    case GL_RGBA12:
    case GL_RGBA16:
        return (TEXTURE_TWIDDLE_ENABLED) ? GL_ARGB4444_TWID_KOS : GL_ARGB4444_KOS;
    /* Support ARB_texture_rg */
    case GL_RED:
    case GL_R8:
    case GL_R16:
    case GL_COMPRESSED_RED:
        return (TEXTURE_TWIDDLE_ENABLED) ? GL_RGB565_TWID_KOS : GL_RGB565_KOS;
    case GL_RG:
    case GL_RG8:
    case GL_RG16:
    case GL_COMPRESSED_RG:
        return (TEXTURE_TWIDDLE_ENABLED) ? GL_RGB565_TWID_KOS : GL_RGB565_KOS;
    default:
        return -1;
    }
}

static GLuint _determinePVRFormat(GLint internalFormat) {
    /* Given a cleaned internalFormat, return the Dreamcast format
     * that can hold it
     */

    switch(internalFormat) {
    case GL_RGB565_KOS:
        return GPU_TXRFMT_RGB565 | GPU_TXRFMT_NONTWIDDLED;
    case GL_ARGB4444_KOS:
        return GPU_TXRFMT_ARGB4444 | GPU_TXRFMT_NONTWIDDLED;
    case GL_ARGB1555_KOS:
        return GPU_TXRFMT_ARGB1555 | GPU_TXRFMT_NONTWIDDLED;
    case GL_RGB565_TWID_KOS:
        return GPU_TXRFMT_RGB565 | GPU_TXRFMT_TWIDDLED;
    case GL_ARGB4444_TWID_KOS:
        return GPU_TXRFMT_ARGB4444 | GPU_TXRFMT_TWIDDLED;
    case GL_ARGB1555_TWID_KOS:
        return GPU_TXRFMT_ARGB1555 | GPU_TXRFMT_TWIDDLED;
    case GL_COLOR_INDEX8_EXT:
        return GPU_TXRFMT_PAL8BPP | GPU_TXRFMT_NONTWIDDLED;
    case GL_COLOR_INDEX4_EXT:
        return GPU_TXRFMT_PAL4BPP | GPU_TXRFMT_NONTWIDDLED;
    case GL_COLOR_INDEX8_TWID_KOS:
        return GPU_TXRFMT_PAL8BPP | GPU_TXRFMT_TWIDDLED;
    case GL_COLOR_INDEX4_TWID_KOS:
        return GPU_TXRFMT_PAL4BPP | GPU_TXRFMT_TWIDDLED;
    case GL_COMPRESSED_ARGB_1555_VQ_KOS:
        return GPU_TXRFMT_ARGB1555 | GPU_TXRFMT_VQ_ENABLE;
    case GL_COMPRESSED_ARGB_1555_VQ_TWID_KOS:
        return GPU_TXRFMT_ARGB1555 | GPU_TXRFMT_VQ_ENABLE | GPU_TXRFMT_TWIDDLED;
    case GL_COMPRESSED_ARGB_4444_VQ_KOS:
        return GPU_TXRFMT_ARGB4444 | GPU_TXRFMT_VQ_ENABLE;
    case GL_COMPRESSED_ARGB_4444_VQ_TWID_KOS:
        return GPU_TXRFMT_ARGB4444 | GPU_TXRFMT_VQ_ENABLE | GPU_TXRFMT_TWIDDLED;
    case GL_COMPRESSED_RGB_565_VQ_KOS:
        return GPU_TXRFMT_RGB565 | GPU_TXRFMT_VQ_ENABLE;
    case GL_COMPRESSED_RGB_565_VQ_TWID_KOS:
        return GPU_TXRFMT_RGB565 | GPU_TXRFMT_VQ_ENABLE | GPU_TXRFMT_TWIDDLED;
    case GL_COMPRESSED_ARGB_1555_VQ_MIPMAP_KOS:
        return GPU_TXRFMT_ARGB1555 | GPU_TXRFMT_VQ_ENABLE;
    case GL_COMPRESSED_ARGB_1555_VQ_MIPMAP_TWID_KOS:
        return GPU_TXRFMT_ARGB1555 | GPU_TXRFMT_VQ_ENABLE | GPU_TXRFMT_TWIDDLED;
    case GL_COMPRESSED_ARGB_4444_VQ_MIPMAP_KOS:
        return GPU_TXRFMT_ARGB4444 | GPU_TXRFMT_VQ_ENABLE;
    case GL_COMPRESSED_ARGB_4444_VQ_MIPMAP_TWID_KOS:
        return GPU_TXRFMT_ARGB4444 | GPU_TXRFMT_VQ_ENABLE | GPU_TXRFMT_TWIDDLED;
    case GL_COMPRESSED_RGB_565_VQ_MIPMAP_KOS:
        return GPU_TXRFMT_RGB565 | GPU_TXRFMT_VQ_ENABLE;
    case GL_COMPRESSED_RGB_565_VQ_MIPMAP_TWID_KOS:
        return GPU_TXRFMT_RGB565 | GPU_TXRFMT_VQ_ENABLE | GPU_TXRFMT_TWIDDLED;
    default:
        fprintf(stderr, "Unexpected format: %d\n", internalFormat);
        _glKosThrowError(GL_INVALID_ENUM, __func__);
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
    GLushort* d = (GLushort*) dest;

    uint16_t b = (source[2] >> 3) & 0x1f;
    uint16_t g = ((source[1] >> 2) & 0x3f) << 5;
    uint16_t r = ((source[0] >> 3) & 0x1f) << 11;

    *d = r | g | b;
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

GL_FORCE_INLINE void _argb1555_to_argb4444(const GLubyte* source, GLubyte* dest) {
    GLushort src = *((GLushort*) source);

    uint8_t a = (src >> 15) & 0x01;
    uint8_t r = (src >> 10) & 0x1F;
    uint8_t g = (src >> 5) & 0x1F;
    uint8_t b = src & 0x1F;

    // Alpha is either 0 or 15
    a = (a << 4) - a;

    // Bitwise magic to scale from 5 to 4 bits.
    // Shift left by 4 to multiply by 16, shift right by 5 to divide by 32.
    r = (r << 4) >> 5;
    g = (g << 4) >> 5;
    b = (b << 4) >> 5;

    *((GLushort*) dest) = (a << 12) | (r << 8) | (g << 4) | b;
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


enum ConversionType {
    CONVERSION_TYPE_NONE,
    CONVERSION_TYPE_CONVERT = 1,
    CONVERSION_TYPE_TWIDDLE = 2,
    CONVERSION_TYPE_PACK = 4,
    CONVERSION_TYPE_INVALID = -1
};


/* Given an cleaned internal format, and the passed format and type, this returns:
 *
 * 0 if not conversion is necessary
 * 1 if a conversion is necessary (func will be set)
 * 2 if twiddling is necessary
 * 3 if twiddling and conversion is necessary (func will be set)
 * -1 if a conversion is unsupported
 *
 */
static int _determineConversion(GLint internalFormat, GLenum format, GLenum type, TextureConversionFunc* func) {
    static struct Entry {
        TextureConversionFunc func;
        GLint internalFormat;
        GLenum format;
        GLenum type;
        bool twiddle;
        bool pack; // If true, each value is packed after conversion into half-bytes
    } conversions [] = {
        {_rgba8888_to_argb4444, GL_ARGB4444_KOS,      GL_RGBA,  GL_UNSIGNED_BYTE, false, false},
        {_rgba8888_to_argb4444, GL_ARGB4444_TWID_KOS, GL_RGBA,  GL_UNSIGNED_BYTE,  true, false},
        {_a8_to_argb4444,       GL_ARGB4444_KOS,      GL_ALPHA, GL_UNSIGNED_BYTE, false, false},
        {_a8_to_argb4444,       GL_ARGB4444_TWID_KOS, GL_ALPHA, GL_UNSIGNED_BYTE,  true, false},

        {_rgba4444_to_argb4444, GL_ARGB4444_KOS,      GL_RGBA,  GL_UNSIGNED_SHORT_4_4_4_4,              false, false},
        {_rgba4444_to_argb4444, GL_ARGB4444_TWID_KOS, GL_RGBA,  GL_UNSIGNED_SHORT_4_4_4_4,               true, false},
        {NULL,                  GL_ARGB4444_KOS,      GL_BGRA,  GL_UNSIGNED_SHORT_4_4_4_4_REV,          false, false},
        {NULL,                  GL_ARGB4444_TWID_KOS, GL_BGRA,  GL_UNSIGNED_SHORT_4_4_4_4_REV,           true, false},

        {NULL,                  GL_ARGB4444_TWID_KOS, GL_BGRA,  GL_UNSIGNED_SHORT_4_4_4_4_REV_TWID_KOS, false, false},

        {_argb1555_to_argb4444, GL_ARGB4444_KOS,      GL_BGRA,  GL_UNSIGNED_SHORT_1_5_5_5_REV,          false, false},
        {_argb1555_to_argb4444, GL_ARGB4444_TWID_KOS, GL_BGRA,  GL_UNSIGNED_SHORT_1_5_5_5_REV,           true, false},

        {NULL,                  GL_ARGB1555_KOS,      GL_BGRA,  GL_UNSIGNED_SHORT_1_5_5_5_REV,          false, false},
        {NULL,                  GL_ARGB1555_TWID_KOS, GL_BGRA,  GL_UNSIGNED_SHORT_1_5_5_5_REV,           true, false},

        {NULL,                  GL_ARGB1555_TWID_KOS, GL_BGRA,  GL_UNSIGNED_SHORT_1_5_5_5_REV_TWID_KOS, false, false},

        {_r8_to_rgb565,         GL_RGB565_KOS,        GL_RED,   GL_UNSIGNED_BYTE, false, false},
        {_r8_to_rgb565,         GL_RGB565_TWID_KOS,   GL_RED,   GL_UNSIGNED_BYTE,  true, false},
        {_rgb888_to_rgb565,     GL_RGB565_KOS,        GL_RGB,   GL_UNSIGNED_BYTE, false, false},
        {_rgb888_to_rgb565,     GL_RGB565_TWID_KOS,   GL_RGB,   GL_UNSIGNED_BYTE,  true, false},
        {_rgba8888_to_rgb565,   GL_RGB565_KOS,        GL_RGBA,  GL_UNSIGNED_BYTE, false, false},
        {_rgba8888_to_rgb565,   GL_RGB565_TWID_KOS,   GL_RGBA,  GL_UNSIGNED_BYTE,  true, false},

        {NULL,                  GL_RGB565_KOS,        GL_RGB,   GL_UNSIGNED_SHORT_5_6_5,          false, false},
        {NULL,                  GL_RGB565_TWID_KOS,   GL_RGB,   GL_UNSIGNED_SHORT_5_6_5,           true, false},
        {NULL,                  GL_RGB565_TWID_KOS,   GL_RGB,   GL_UNSIGNED_SHORT_5_6_5_TWID_KOS, false, false},

        {NULL, GL_COLOR_INDEX8_EXT,      GL_COLOR_INDEX,      GL_UNSIGNED_BYTE, false, false},
        {NULL, GL_COLOR_INDEX8_EXT,      GL_COLOR_INDEX,      GL_BYTE,          false, false},
        {NULL, GL_COLOR_INDEX8_TWID_KOS, GL_COLOR_INDEX,      GL_UNSIGNED_BYTE,  true, false},
        {NULL, GL_COLOR_INDEX8_TWID_KOS, GL_COLOR_INDEX,      GL_BYTE,           true, false},

        {NULL, GL_COLOR_INDEX8_EXT,      GL_COLOR_INDEX8_EXT, GL_UNSIGNED_BYTE, false, false},
        {NULL, GL_COLOR_INDEX8_EXT,      GL_COLOR_INDEX8_EXT, GL_BYTE,          false, false},
        {NULL, GL_COLOR_INDEX8_TWID_KOS, GL_COLOR_INDEX8_EXT, GL_UNSIGNED_BYTE,  true, false},
        {NULL, GL_COLOR_INDEX8_TWID_KOS, GL_COLOR_INDEX8_EXT, GL_BYTE,           true, false},

        {NULL, GL_COLOR_INDEX8_TWID_KOS, GL_COLOR_INDEX8_TWID_KOS, GL_UNSIGNED_BYTE, false, false},
        {NULL, GL_COLOR_INDEX8_TWID_KOS, GL_COLOR_INDEX8_TWID_KOS, GL_BYTE,          false, false},

        {NULL, GL_COLOR_INDEX4_EXT,      GL_COLOR_INDEX,      GL_UNSIGNED_BYTE, false,  true},
        {NULL, GL_COLOR_INDEX4_EXT,      GL_COLOR_INDEX,      GL_BYTE,          false,  true},
        {NULL, GL_COLOR_INDEX4_TWID_KOS, GL_COLOR_INDEX,      GL_UNSIGNED_BYTE,  true,  true},
        {NULL, GL_COLOR_INDEX4_TWID_KOS, GL_COLOR_INDEX,      GL_BYTE,           true,  true},

        {NULL, GL_COLOR_INDEX4_EXT,      GL_COLOR_INDEX4_EXT, GL_UNSIGNED_BYTE, false, false},
        {NULL, GL_COLOR_INDEX4_EXT,      GL_COLOR_INDEX4_EXT, GL_BYTE,          false, false},
        {NULL, GL_COLOR_INDEX4_TWID_KOS, GL_COLOR_INDEX4_EXT, GL_UNSIGNED_BYTE,  true, false},
        {NULL, GL_COLOR_INDEX4_TWID_KOS, GL_COLOR_INDEX4_EXT, GL_BYTE,           true, false},

        {NULL,                  GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false, false},
        {NULL,                  GL_RGBA8, GL_RGBA, GL_BYTE,          false, false},
        {_rgb888_to_rgba8888,   GL_RGBA8, GL_RGB,  GL_UNSIGNED_BYTE, false, false},
        {_rgb888_to_rgba8888,   GL_RGBA8, GL_RGB,  GL_BYTE,          false, false},

        {_rgb888_to_argb4444,   GL_RGBA4, GL_RGB,  GL_UNSIGNED_BYTE, false, false},
        {_rgb888_to_argb4444,   GL_RGBA4, GL_RGB,  GL_BYTE,          false, false},
        {_rgba8888_to_argb4444, GL_RGBA4, GL_RGBA, GL_UNSIGNED_BYTE, false, false},
        {_rgba8888_to_argb4444, GL_RGBA4, GL_RGBA, GL_BYTE,          false, false},
    };

    for(size_t i = 0; i < sizeof(conversions) / sizeof(struct Entry); ++i) {
        struct Entry* e = conversions + i;
        if(e->format == format && e->internalFormat == internalFormat && e->type == type) {
            *func = e->func;

            int ret = (e->func) ? CONVERSION_TYPE_CONVERT : CONVERSION_TYPE_NONE;

            if(e->twiddle) {
                ret |= CONVERSION_TYPE_TWIDDLE;
            }

            if(e->pack) {
                ret |= CONVERSION_TYPE_PACK;
            }

            return ret;
        }
    }

    fprintf(stderr, "No conversion found for format: 0x%x, internalFormat: 0x%x, type: 0x%x\n", format, internalFormat, type);
    return CONVERSION_TYPE_INVALID;
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
        alloc_free(ALLOC_BASE, active->data);
        active->data = NULL;
    }

    /* Figure out how much room to allocate for mipmaps */
    GLuint bytes = _glGetMipmapDataSize(active);

    active->data = alloc_malloc_and_defrag(bytes);

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

static bool _glTexImage2DValidate(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type) {
    if(target != GL_TEXTURE_2D) {
        INFO_MSG("");
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        return false;
    }

    if (width > 1024 || height > 1024){
        INFO_MSG("Invalid texture size");
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return false;
    }

    GLint validFormats [] = {
        GL_ALPHA,
        GL_LUMINANCE,
        GL_INTENSITY,
        GL_LUMINANCE_ALPHA,
        GL_RED,
        GL_RGB,
        GL_RGBA,
        GL_BGRA,
        GL_COLOR_INDEX,
        GL_COLOR_INDEX4_EXT,  /* Extension, this is just so glCompressedTexImage can pass-thru */
        GL_COLOR_INDEX8_EXT,  /* Extension, this is just so glCompressedTexImage can pass-thru */
        GL_COLOR_INDEX4_TWID_KOS,  /* Extension, this is just so glCompressedTexImage can pass-thru */
        GL_COLOR_INDEX8_TWID_KOS,  /* Extension, this is just so glCompressedTexImage can pass-thru */
        0
    };

    if(_glCheckValidEnum(format, validFormats, __func__) != 0) {
        return false;
    }

    if(format != GL_COLOR_INDEX4_EXT && format != GL_COLOR_INDEX4_TWID_KOS) {
        /* Abuse determineStride to see if type is valid */
        if(_determineStride(GL_RGBA, type) == -1) {
            INFO_MSG("Unsupported type");
            _glKosThrowError(GL_INVALID_ENUM, __func__);
            return false;
        }
    }

    if(_cleanInternalFormat(internalFormat) == -1) {
        INFO_MSG("Unsupported internal format");
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return false;
    }

    GLuint w = width;
    GLuint h = height;
    if(level == 0){
        if((w < 8 || (w & -w) != w)) {
            /* Width is not a power of two. Must be!*/
            INFO_MSG("Unsupported width");
            _glKosThrowError(GL_INVALID_VALUE, __func__);
            return false;
        }


        if((h < 8 || (h & -h) != h)) {
            /* height is not a power of two. Must be!*/
            INFO_MSG("Unsupported height");
            _glKosThrowError(GL_INVALID_VALUE, __func__);
            return false;
        }
    } else {
        /* Mipmap Errors, kos crashes if 1x1 */
        if((h < 2) || (w < 2)){
            gl_assert(ACTIVE_TEXTURE < MAX_GLDC_TEXTURE_UNITS);
            gl_assert(TEXTURE_UNITS[ACTIVE_TEXTURE]);
            TEXTURE_UNITS[ACTIVE_TEXTURE]->mipmap |= (1 << level);
            return false;
        }
    }

    if(level < 0) {
        INFO_MSG("Level must be >= 0");
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return false;
    }

    if(level > 0 && width != height) {
        INFO_MSG("Tried to set non-square texture as a mipmap level");
        printf("[GL ERROR] Mipmaps cannot be supported on non-square textures\n");
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        return false;
    }

    if(border) {
        INFO_MSG("Border not allowed");
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return false;
    }

    return true;
}

static bool _glTexSubImage2DValidate(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                     GLsizei width, GLsizei height, GLenum format, GLenum type,
                                     GLsizei textureWidth, GLsizei textureHeight) {
    if (target != GL_TEXTURE_2D) {
        INFO_MSG("Invalid target. Only GL_TEXTURE_2D is supported.");
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        return false;
    }

    if (width > 1024 || height > 1024) {
        INFO_MSG("Invalid subimage size. Maximum 1024x1024.");
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return false;
    }

    // Ensure format is valid
    GLint validFormats[] = {
        GL_ALPHA,
        GL_LUMINANCE,
        GL_INTENSITY,
        GL_LUMINANCE_ALPHA,
        GL_RED,
        GL_RGB,
        GL_RGBA,
        GL_BGRA,
        GL_COLOR_INDEX,
        GL_COLOR_INDEX4_EXT,  /* Extension, for glCompressedTexImage pass-thru */
        GL_COLOR_INDEX8_EXT,   /* Extension, for glCompressedTexImage pass-thru */
        GL_COLOR_INDEX4_TWID_KOS,  /* Extension, for glCompressedTexImage pass-thru */
        GL_COLOR_INDEX8_TWID_KOS,   /* Extension, for glCompressedTexImage pass-thru */
        0
    };

    if (_glCheckValidEnum(format, validFormats, __func__) != 0) {
        return false;
    }

    // Validate type
    if (format != GL_COLOR_INDEX4_EXT && format != GL_COLOR_INDEX4_TWID_KOS) {
        /* Use determineStride to see if type is valid */
        if (_determineStride(GL_RGBA, type) == -1) {
            INFO_MSG("Invalid pixel data type.");
            _glKosThrowError(GL_INVALID_ENUM, __func__);
            return false;
        }
    }

    // Validate offsets and dimensions using the underlying texture size
    if (xoffset < 0 || yoffset < 0 || 
        (xoffset + width) > textureWidth || 
        (yoffset + height) > textureHeight) {
        INFO_MSG("Subimage exceeds the dimensions of the texture.");
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return false;
    }

    if (level < 0) {
        INFO_MSG("Invalid mipmap level.");
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return false;
    }

    // Ensure no border (since glTexSubImage2D doesn't allow it)
    GLint border = 0; // No border allowed
    if (border != 0) {
        INFO_MSG("Border must be zero for glTexSubImage2D.");
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        return false;
    }

    return true;
}

static inline GLboolean is4BPPFormat(GLenum format) {
    return format == GL_COLOR_INDEX4_EXT || format == GL_COLOR_INDEX4_TWID_KOS;
}

void APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalFormat,
                           GLsizei width, GLsizei height, GLint border,
                           GLenum format, GLenum type, const GLvoid *data) {

    TRACE();
    if(!_glTexImage2DValidate(target, level, internalFormat, width, height, border, format, type)) {
        return;
    }

    gl_assert(ACTIVE_TEXTURE < MAX_GLDC_TEXTURE_UNITS);
    TextureObject* active = TEXTURE_UNITS[ACTIVE_TEXTURE];

    if(!active) {
        INFO_MSG("Called glTexImage2D on unbound texture");
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        return;
    }

    GLboolean isPaletted = (
        internalFormat == GL_COLOR_INDEX8_EXT ||
        internalFormat == GL_COLOR_INDEX4_EXT ||
        internalFormat == GL_COLOR_INDEX4_TWID_KOS ||
        internalFormat == GL_COLOR_INDEX8_TWID_KOS
    ) ? GL_TRUE : GL_FALSE;
    GLenum cleanInternalFormat = _cleanInternalFormat(internalFormat);
    GLuint pvrFormat = _determinePVRFormat(cleanInternalFormat);
    GLuint originalId = active->index;

    if(active->data && (level == 0)) {
        /* pre-existing texture - check if changed */
        if(active->width != width ||
           active->height != height ||
           active->internalFormat != cleanInternalFormat) {
            /* changed - free old texture memory */
            alloc_free(ALLOC_BASE, active->data);
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
    GLint destStride = _determineStrideInternal(cleanInternalFormat);
    GLint sourceStride = _determineStride(format, type);
    GLuint srcBytes = (width * height * sourceStride);
    GLuint destBytes = (width * height * destStride);

    TextureConversionFunc conversion;
    int needs_conversion = _determineConversion(cleanInternalFormat, format, type, &conversion);

    // Hack: If we're doing a 4bpp source (via glCompressedTexture...)
    // halve the srcBytes
    if(format == GL_COLOR_INDEX4_EXT || format == GL_COLOR_INDEX4_TWID_KOS) {
        srcBytes /= 2;
    }

    /* If we're packing stuff, then the dest size is half what it would be */
    if((needs_conversion & CONVERSION_TYPE_PACK) == CONVERSION_TYPE_PACK) {
        destBytes /= 2;
    } else if(internalFormat == GL_COLOR_INDEX4_EXT || internalFormat == GL_COLOR_INDEX4_TWID_KOS) {
        destBytes /= 2;
    }

    if(!active->data) {
        gl_assert(active);
        gl_assert(width);
        gl_assert(height);
        gl_assert(destStride);

        /* need texture memory */
        active->width   = width;
        active->height  = height;
        active->color   = pvrFormat;
        active->internalFormat = cleanInternalFormat;
        /* Set the required mipmap count */
        active->mipmapCount = _glGetMipmapLevelCount(active);
        active->mipmap_bias = GL_KOS_INTERNAL_DEFAULT_MIPMAP_LOD_BIAS;
        active->dataStride = destStride;
        active->baseDataSize = destBytes;

        gl_assert(destBytes);

        if(level > 0) {
            /* If we're uploading a mipmap level, we need to allocate the full amount of space */
            _glAllocateSpaceForMipmaps(active);
        } else {
            active->data = alloc_malloc_and_defrag(active->baseDataSize);
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
        gl_assert(active->index == originalId);
        return;
    }

    /* Mark this level as set in the mipmap bitmask */
    active->mipmap |= (1 << level);

    GLubyte* targetData = (active->baseDataOffset == 0) ? active->data : _glGetMipmapLocation(active, level);
    gl_assert(targetData);

    if(needs_conversion < 0) {
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        INFO_MSG("Couldn't find necessary texture conversion\n");
        return;
    } else if(needs_conversion > 0) {
        /* Convert the data */
        if(sourceStride == -1) {
            INFO_MSG("Stride was not detected\n");
            _glKosThrowError(GL_INVALID_OPERATION, __func__);
            return;
        }

        GLubyte* conversionBuffer = (GLubyte*) memalign(32, srcBytes);
        const GLubyte* src = data;
        GLubyte* dst = conversionBuffer;

        bool pack = (needs_conversion & CONVERSION_TYPE_PACK) == CONVERSION_TYPE_PACK;
        needs_conversion &= ~CONVERSION_TYPE_PACK;

        if(needs_conversion == CONVERSION_TYPE_CONVERT) {
            // Convert
            for(uint32_t i = 0; i < (width * height); ++i) {
                conversion(src, dst);
                dst += destStride;
                src += sourceStride;
            }
        } else if(needs_conversion == 2) {
            // Twiddle
            if(is4BPPFormat(internalFormat) && is4BPPFormat(format)) {
                // Special case twiddling. We have to unpack each src value
                // and repack into the right place
                twid_prepare_table(width, height);

                for(uint32_t i = 0; i < (width * height); ++i) {
                    uint32_t newLocation = twid_location(i);

                    assert(newLocation < (width * height));
                    assert((newLocation / 2) < destBytes);
                    assert((i / 2) < srcBytes);

                    // This is the src/dest byte, but we need to figure
                    // out which half based on the odd/even of i
                    src = &((uint8_t*) data)[i / 2];
                    dst = &conversionBuffer[newLocation / 2];

                    uint8_t src_value = (i % 2) == 0 ? (*src >> 4) : (*src & 0xF);

                    if(newLocation % 2 == 1) {
                        *dst = (*dst & 0xF) | (src_value << 4);
                    } else {
                        *dst = (*dst & 0xF0) | (src_value & 0xF);
                    }
                }
            } else {
                twid_prepare_table(width, height);

                for(uint32_t i = 0; i < (width * height); ++i) {
                    uint32_t newLocation = twid_location(i);
                    dst = conversionBuffer + (destStride * newLocation);

                    for(int j = 0; j < destStride; ++j)
                        *dst++ = *(src + j);

                    src += sourceStride;
                }
            }
        } else if(needs_conversion == 3) {
            // Convert + twiddle
            twid_prepare_table(width, height);

            for(uint32_t i = 0; i < (width * height); ++i) {
                uint32_t newLocation = twid_location(i);
                dst = conversionBuffer + (destStride * newLocation);
                src = data + (sourceStride * i);
                conversion(src, dst);
            }
        } else if(pack) {
            FASTCPY(conversionBuffer, data, srcBytes);
        }

        if(pack) {
            assert(isPaletted);
            size_t dst_byte = 0;
            for(size_t src_byte = 0; src_byte < srcBytes; ++src_byte) {
                uint8_t v = conversionBuffer[src_byte];

                if(src_byte % 1 == 0) {
                    conversionBuffer[dst_byte] = (conversionBuffer[dst_byte] & 0xF) | ((v & 0xF0) << 4);
                } else {
                    conversionBuffer[dst_byte] = (conversionBuffer[dst_byte] & 0xF0) | (v & 0xF);
                }

                if(src_byte % 1 == 0) {
                    dst_byte++;
                }
            }
        }

        FASTCPY(targetData, conversionBuffer, destBytes);
        free(conversionBuffer);
    } else {
        /* No conversion necessary, we can just upload data directly */
        gl_assert(targetData);
        gl_assert(data);
        gl_assert(destBytes);

        /* No conversion? Just copy the data, and the pvr_format is correct */
        FASTCPY(targetData, data, destBytes);
        gl_assert(active->index == originalId);
    }

    gl_assert(active->index == originalId);
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
                        active->uv_wrap |= CLAMP_U;
                        break;

                    case GL_REPEAT:
                        active->uv_wrap &= ~CLAMP_U;
                        break;

                    case GL_MIRRORED_REPEAT:
                        active->uv_wrap |= (CLAMP_U & MIRROR_U);
                        break;
                }

                break;

            case GL_TEXTURE_WRAP_T:
                switch(param) {
                    case GL_CLAMP_TO_EDGE:
                    case GL_CLAMP:
                        active->uv_wrap |= CLAMP_V;
                        break;

                    case GL_REPEAT:
                        active->uv_wrap &= ~CLAMP_V;
                        break;

                    case GL_MIRRORED_REPEAT:
                        active->uv_wrap |= (CLAMP_V & MIRROR_V);
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

    /* We always store the palette in RAM in RGBA8888. We pack when we set
     * the individual colours in the PVR */
    GLint destStride = 4;

    gl_assert(sourceStride > -1);

    TextureConversionFunc convert;
    int ret = _determineConversion(
        GL_RGBA8,
        format,
        type,
        &convert
    );

    if(ret < 0) {
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

    palette->data = (GLubyte*) malloc(width * destStride);
    palette->format = internalFormat;
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
        if(convert) {
            convert(src, dst);
            src += sourceStride;
            dst += destStride;
        } else {
            for(int j = 0; j < sourceStride; ++j) {
                *dst++ = *src++;
            }
        }
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

void APIENTRY glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                              GLsizei width, GLsizei height, GLenum format,
                              GLenum type, const GLvoid *data) {
    TRACE();

    gl_assert(ACTIVE_TEXTURE < MAX_GLDC_TEXTURE_UNITS);
    TextureObject* active = TEXTURE_UNITS[ACTIVE_TEXTURE];

    if (!active || !active->data) {
        INFO_MSG("Called glTexSubImage2D on unbound or uninitialized texture");
        _glKosThrowError(GL_INVALID_OPERATION, __func__);
        return;
    }

    // Retrieve the dimensions of the currently bound texture
    GLsizei textureWidth = active->width;
    GLsizei textureHeight = active->height;

    if (!_glTexSubImage2DValidate(target, level, xoffset, yoffset, width, height, format, type, textureWidth, textureHeight)) {
        INFO_MSG("Error: _glTexSubImage2DValidate failed\n");
        return;
    }

    GLboolean isPaletted = (
        active->internalFormat == GL_COLOR_INDEX8_EXT ||
        active->internalFormat == GL_COLOR_INDEX4_EXT ||
        active->internalFormat == GL_COLOR_INDEX4_TWID_KOS ||
        active->internalFormat == GL_COLOR_INDEX8_TWID_KOS
    ) ? GL_TRUE : GL_FALSE;

    GLenum cleanInternalFormat = _cleanInternalFormat(active->internalFormat);

    // Determine source stride
    GLint sourceStride = _determineStride(format, type);
    GLuint srcBytes = (width * height * sourceStride);

    // Calculate destination stride (this accounts for both POT and NPOT)
    GLint destStride = _determineStrideInternal(cleanInternalFormat);

    // Calculate destBytes using the texture's full dimensions
    GLuint destBytes = (textureWidth * textureHeight * destStride);

    TextureConversionFunc conversion;
    int needs_conversion = _determineConversion(cleanInternalFormat, format, type, &conversion);

    // Adjust source bytes for 4bpp formats
    if (format == GL_COLOR_INDEX4_EXT || format == GL_COLOR_INDEX4_TWID_KOS) {
        srcBytes /= 2;
    }

    if ((needs_conversion & CONVERSION_TYPE_PACK) == CONVERSION_TYPE_PACK) {
        destBytes /= 2;
    } else if (active->internalFormat == GL_COLOR_INDEX4_EXT || active->internalFormat == GL_COLOR_INDEX4_TWID_KOS) {
        destBytes /= 2;
    }

    if (needs_conversion < 0) {
        _glKosThrowError(GL_INVALID_VALUE, __func__);
        INFO_MSG("Couldn't find necessary texture conversion\n");
        return;
    }

    // Calculate the starting point for the subregion in the texture data
    GLubyte* targetData = active->data;
    if (needs_conversion > 0) {
        GLubyte* conversionBuffer = (GLubyte*) memalign(32, destBytes);

        const GLubyte* src = data;
        GLubyte* dst = conversionBuffer;

        bool pack = (needs_conversion & CONVERSION_TYPE_PACK) == CONVERSION_TYPE_PACK;
        needs_conversion &= ~CONVERSION_TYPE_PACK;

        // Only initialize the buffer with zeros if it's a partial update
        if (xoffset != 0 || yoffset != 0 || width != textureWidth || height != textureHeight) {
            memset(conversionBuffer, 0, destBytes);
        }
        if (needs_conversion == CONVERSION_TYPE_CONVERT) {
            for (uint32_t y = 0; y < height; ++y) {
                dst = conversionBuffer + ((y + yoffset) * textureWidth + xoffset) * destStride;
                for (uint32_t x = 0; x < width; ++x) {
                    conversion(src, dst);
                    dst += destStride;
                    src += sourceStride;
                }
            }
        } else if (needs_conversion == 2 || needs_conversion == 3) {
            twid_prepare_table(textureWidth, textureHeight);

            for (uint32_t y = yoffset; y < yoffset + height; ++y) {
                for (uint32_t x = xoffset; x < xoffset + width; ++x) {
                    uint32_t srcIndex = (y - yoffset) * width + (x - xoffset);
                    uint32_t newLocation = twid_location(y * textureWidth + x);
                    dst = conversionBuffer + (destStride * newLocation);

                    if (needs_conversion == 3) {
                        conversion(src + srcIndex * sourceStride, dst);
                    } else {
                        memcpy(dst, src + srcIndex * sourceStride, destStride);
                    }
                }
            }
        }

        if (pack) {
            assert(isPaletted);
            size_t dst_byte = 0;
            for (size_t src_byte = 0; src_byte < destBytes; ++src_byte) {
                uint8_t v = conversionBuffer[src_byte];

                if (src_byte % 2 == 0) {
                    conversionBuffer[dst_byte] = (conversionBuffer[dst_byte] & 0xF) | ((v & 0xF) << 4);
                } else {
                    conversionBuffer[dst_byte] = (conversionBuffer[dst_byte] & 0xF0) | (v & 0xF);
                    dst_byte++;
                }
            }
        }

        // Copy the converted data to the texture
        FASTCPY(targetData, conversionBuffer, destBytes);

        free(conversionBuffer);
    } else {
        // No conversion necessary, we can update data directly
        for (GLsizei y = 0; y < height; ++y) {
            GLsizei srcRowWidth = width * sourceStride;
            GLubyte* destRow = targetData + ((y + yoffset) * textureWidth + xoffset) * destStride;
            FASTCPY(destRow, (GLubyte*)data + y * srcRowWidth, srcRowWidth);
        }
    }

    _glGPUStateMarkDirty();
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
    return ALLOC_SIZE;
}

GLuint _glFreeTextureMemory() {
    return alloc_count_free(ALLOC_BASE);
}

GLuint _glUsedTextureMemory() {
    return ALLOC_SIZE - _glFreeTextureMemory();
}

GLuint _glFreeContiguousTextureMemory() {
    return alloc_count_continuous(ALLOC_BASE);
}

static void update_data_pointer(void* src, void* dst, void* data) {
    _GL_UNUSED(data);
    for(size_t id = 0; id < MAX_TEXTURE_COUNT; id++){
        TextureObject* txr = (TextureObject*) named_array_get(&TEXTURE_OBJECTS, id);
        if(txr && txr->data == src) {
            gl_assert(txr->index == id);
            txr->data = dst;
            return;
        }
    }
}

GLAPI GLvoid APIENTRY glDefragmentTextureMemory_KOS(void) {
    alloc_run_defrag(ALLOC_BASE, update_data_pointer, 5, NULL);
}

GLAPI void APIENTRY glGetTexImage(GLenum tex, GLint lod, GLenum format, GLenum type, GLvoid* img) {
    _GL_UNUSED(tex);
    _GL_UNUSED(lod);
    _GL_UNUSED(format);
    _GL_UNUSED(type);
    _GL_UNUSED(img);
}
