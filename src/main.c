
#include <stdio.h>
#include <stdint.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_truetype.h>
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

#define GLOBAL static
#define INTERNAL static

#define OUT

#define KILOBYTES(value) ((value) << 10)
#define MEGABYTES(value) (KILOBYTES(value) << 10)
#define GIGABYTES(value) (MEGABYTES(value) << 10)

#define MAX_NAME 64
#define MAX_FILENAME 256
#define MAX_IMAGES 256
#define MAX_FONTS 16
#define MAX_CHARSET 4096
#define MAX_GLYPHS 128

typedef uint32_t bool32_t;

typedef struct
{
    char name[MAX_NAME];
    char filename[MAX_FILENAME];
    int x, y, width, height;
    uint8_t* pixels;
} image_t;

typedef struct
{
    float u0, v0, u1, v1;
    float xoff, yoff;
    float xadvance;
    int w, h;
} glyph_t;

typedef struct
{
    int codepoint;
    glyph_t glyph;
} glyph_mapping_t;

typedef struct
{
    char name[MAX_NAME];
    char filename[MAX_FILENAME];
    int size;
    stbtt_fontinfo info;
    uint8_t* data;
    float scale;
    int ascent, descent, line_gap;
    glyph_mapping_t glyphs[MAX_GLYPHS];
    int glyph_count;
    int codepoints[MAX_GLYPHS];
    int codepoint_count;
} font_t;

typedef struct
{
    int font_index;
    int codepoint;
    uint8_t* bitmap;
    int width, height;
    int xoff, yoff;
    float xadvance;
} packed_glyph_t;

typedef struct
{
    uint32_t width, height;
    uint8_t* pixels;
    image_t images[MAX_IMAGES];
    int image_count;
    font_t fonts[MAX_FONTS];
    int font_count;
} atlas_t;

GLOBAL atlas_t atlas;

//////////////////////////////////////////////////////////////////////////////
// Config parser
//////////////////////////////////////////////////////////////////////////////

INTERNAL int
DecodeUTF8(const char** str, OUT int* codepoint)
{
    const uint8_t* s = (uint8_t*) *str;

    if (s[0] == 0)
    {
         return 0;
    }

    if ((s[0] & 0x80) == 0)
    {
        *codepoint = s[0];
        *str += 1;
        return 1;
    }

    if ((s[0] & 0xE0) == 0xC0)
    {
        *codepoint = ((s[0] & 0x1F) << 6) | (s[1] & 0x3f);
        *str += 2;
        return 2;
    }

    if ((s[0] & 0xF0) == 0xE0)
    {
        *codepoint = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        *str += 3;
        return 3;
    }

    if ((s[0] & 0xF8) == 0xF0) {
        *codepoint = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        *str += 4;
        return 4;
    }
    
    *codepoint = '?';
    *str += 1;
    return 1;
}

INTERNAL bool32_t
LoadImage(const char* filename, const char* name, OUT image_t* image)
{
    int width, height, channels;
    uint8_t* data = stbi_load(filename, &width, &height, &channels, 4);
    if (!data)
    {
        printf("Error: Failed to load image: %s\n", filename);
        return 0;
    }

    strcpy(image->name, name);
    strcpy(image->filename, filename);

    image->width = width;
    image->height = height;
    image->pixels = data;

    return 1;
}

INTERNAL bool32_t
LoadFont(const char* filename, const char* name, char* charset, int size, OUT font_t* font)
{
    FILE* f = fopen(filename, "rb");
    if (!f)
    {
        printf("Error: Cannot open font file: %s\n", filename);
        return 0;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    font->data = (uint8_t*) calloc(file_size, 1);
    if (!font->data)
    {
        printf("Error: Cannot allocate memory for font: %s\n", filename);
        fclose(f);
        return 0;
    }

    fread(font->data, 1, file_size, f);
    fclose(f);

    if (!stbtt_InitFont(&font->info, font->data, 0))
    {
        printf("Error: Cannot initialize font: %s\n", filename);
        free(font->data);
        return 0;
    }

    strcpy(font->name, name);
    strcpy(font->filename, name);
    font->size = size;
    font->scale = stbtt_ScaleForPixelHeight(&font->info, (float)size);
    stbtt_GetFontVMetrics(&font->info, &font->ascent, &font->descent, &font->line_gap);
    font->glyph_count = 0;

    char* p = charset;
    while (*p && font->codepoint_count < MAX_GLYPHS)
    {
        if (*p == '\r' || *p == '\n' || *p == '\0')
        {
            break;
        }

        int codepoint;
        DecodeUTF8(&p, &codepoint);

        bool32_t found = 0;
        for (int i = 0; i < font->codepoint_count; ++i)
        {
            if (font->codepoints[i] == codepoint)
            {
                found = 1;
                break;
            }
        }

        if (!found)
        {
            font->codepoints[font->codepoint_count++] = codepoint;
        }
    }

    return 1;
}

INTERNAL bool32_t
ParseConfig(const char* config, OUT atlas_t* atlas)
{
    FILE* f = fopen(config, "r");
    if (!f)
    {
        printf("Error: Cannot open config file: %s\n", config);
        return 0;
    }

    atlas->font_count = 0;
    atlas->image_count = 0;

    char line[MAX_CHARSET];
    while (fgets(line, sizeof(line), f))
    {
        if (line[0] == '\0' || line[0] == '#' || line[0] == '\r' || line[0] == '\n')
        {
            continue;
        }

        char cmd[MAX_NAME];
        sscanf(line, "%s", cmd);

        if (strncmp(cmd, "ATLAS_SIZE", 10) == 0)
        {
            sscanf(line, "ATLAS_SIZE %d", &atlas->width);
            atlas->height = atlas->width;
        }
        else if (strncmp(cmd, "FONT", 4) == 0)
        {
            char filename[MAX_FILENAME], name[MAX_NAME], charset[MAX_CHARSET];
            int size;
            
            if (sscanf(line, "FONT %s %d %s %s", filename, &size, charset, name) == 4)
            {
                if (atlas->font_count < MAX_FONTS)
                {
                    if (LoadFont(filename, name, charset, size, &atlas->fonts[atlas->font_count]))
                    {
                        atlas->font_count++;
                    }
                    else
                    {
                        printf("Error: Failed to load font: %s\n", filename);
                        return 0;
                    }
                }
                else
                {
                    printf("Error: No more fonts can be loaded. Increase number of fonts allowed.\n");
                    return 0;
                }
            }
            else
            {
                printf("Error: Invalid font config: %s\n", line);
                return 0;
            }
        }
        else if (strncmp(cmd, "IMAGE", 5) == 0)
        {
            char filename[MAX_FILENAME], name[MAX_NAME];
            if (sscanf(line, "IMAGE %s %s", filename, name))
            {
                if (atlas->image_count < MAX_IMAGES)
                {
                    if (LoadImage(filename, name, &atlas->images[atlas->image_count]))
                    {
                        atlas->image_count++;
                    }
                    else
                    {
                        printf("Error: Failed to load image: %s\n", filename);
                        return 0;
                    }
                }
                else
                {
                    printf("Error: No more images can be loaded. Increase the number of allowed images.\n");
                    return 0;
                }
            }
            else
            {
                printf("Error: Invalid image format: %s\n", line);
            }
        }
    }

    fclose(f);

    // White image
    image_t* image = &atlas->images[atlas->image_count++];
    strcpy(image->name, "WHITE");
    image->width = 4;
    image->height = 4;
    image->pixels = 0;
    
    return (atlas->font_count > 0 || atlas->image_count > 0);
}

//////////////////////////////////////////////////////////////////////////////
// Max Rects
//////////////////////////////////////////////////////////////////////////////

typedef struct 
{
    int x, y, width, height;
} rect_t;

typedef struct
{
    rect_t* rects;
    int count;
    int capacity;
} maxrects_t;

INTERNAL maxrects_t
CreateMaxRects(int width, int height)
{
    maxrects_t result = { 0 };
    result.capacity = 512;
    result.rects = (rect_t*) calloc(result.capacity * sizeof(rect_t), 1);
    result.count = 1;
    result.rects[0] = (rect_t){0, 0, width, height};

    return result;
}

INTERNAL void
FreeMaxRects(maxrects_t* mr)
{
    free(mr->rects);
}

INTERNAL int
MaxRectsFindPosition(maxrects_t* mr, int width, int height, OUT int* x, OUT int* y)
{
    int best_short_side = INT_MAX;
    int best_long_side = INT_MAX;
    int best_index = -1;
    int best_x = 0, best_y = 0;

    for (int i = 0; i < mr->count; ++i)
    {
        rect_t r = mr->rects[i];

        if (r.width >= width && r.height >= height)
        {
            int leftover_width = r.width - width;
            int leftover_height = r.height - height;
            int short_side = leftover_height < leftover_width ? leftover_height : leftover_width;
            int long_side = leftover_height > leftover_width ? leftover_height : leftover_width;

            if (short_side < best_short_side || (short_side == best_short_side && long_side < best_long_side))
            {
                best_short_side = short_side;
                best_long_side = long_side;
                best_index = i;
                best_x = r.x;
                best_y = r.y;
            }
        }
    }

    if (best_index != -1)
    {
        *x = best_x;
        *y = best_y;
    }

    return best_index;
}

INTERNAL bool32_t
RectContains(rect_t a, rect_t b)
{
    return a.x <= b.x &&
           a.y <= b.y &&
           (a.x + a.width) >= (b.x + b.width) &&
           (a.y + a.height) >= (b.y + b.height);
}

INTERNAL void
MaxRectsPruneRects(maxrects_t* mr)
{
    // Remove rectangles that are contained within other rectangles
    for (int i = 0; i < mr->count; i++)
    {
        for (int j = i + 1; j < mr->count; j++)
        {
            if (RectContains(mr->rects[i], mr->rects[j]))
            {
                memmove(&mr->rects[j], &mr->rects[j + 1], (mr->count - j - 1) * sizeof(rect_t));
                mr->count--;
                j--;
            }
            else if (RectContains(mr->rects[j], mr->rects[i]))
            {
                memmove(&mr->rects[i], &mr->rects[i + 1], (mr->count - i - 1) * sizeof(rect_t));
                mr->count--;
                i--;
                break;
            }
        }
    }
}

INTERNAL void
MaxRectsAddRect(maxrects_t* mr, rect_t rect)
{
    if (mr->count >= mr->capacity)
    {
        mr->capacity *= 2;
        mr->rects = (rect_t*)realloc(mr->rects, mr->capacity * sizeof(rect_t));
    }
    mr->rects[mr->count++] = rect;
}

INTERNAL void
MaxRectsSplitFreeRect(maxrects_t* mr, int index, int x, int y, int width, int height)
{
    rect_t used_rect = {x, y, width, height};
    rect_t free_rect = mr->rects[index];

    memmove(&mr->rects[index], &mr->rects[index+1], (mr->count - index - 1) * sizeof(rect_t));
    mr->count--;

    rect_t new_rects[4];
    int new_count = 0;

    // Top
    if (y > free_rect.y)
    {
        new_rects[new_count++] = (rect_t){
            free_rect.x, free_rect.y,
            free_rect.width, y - free_rect.y
        };
    }

    // Bottom
    if (y + height < free_rect.y + free_rect.height)
    {
        new_rects[new_count++] = (rect_t){
            free_rect.x, y + height,
            free_rect.width, free_rect.y + free_rect.height - (y + height)
        };
    }
    
    // Left
    if (x > free_rect.x)
    {
        new_rects[new_count++] = (rect_t){
            free_rect.x, free_rect.y,
            x - free_rect.x, free_rect.height
        };
    }
    
    // Right
    if (x + width < free_rect.x + free_rect.width)
    {
        new_rects[new_count++] = (rect_t){
            x + width, free_rect.y,
            free_rect.x + free_rect.width - (x + width), free_rect.height
        };
    }

    // Add new rectangles, removing those that are contained by others
    for (int i = 0; i < new_count; i++)
    {
        bool32_t contained = 0;
        
        // Check if this rect is contained by any other new rect
        for (int j = 0; j < new_count; j++)
        {
            if (i != j && RectContains(new_rects[j], new_rects[i]))
            {
                contained = 1;
                break;
            }
        }
        
        if (!contained)
        {
            MaxRectsAddRect(mr, new_rects[i]);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// Create atlas
//////////////////////////////////////////////////////////////////////////////

typedef enum {
    TYPE_IMAGE,
    TYPE_GLYPH,
} rect_type_t;

typedef struct
{
    int width, height;
    int original_index;
    void* user_data;
    rect_type_t type; // 0=white, 1=image, 2=glyph
} packed_rect_t;

INTERNAL bool32_t
CreateAtlas(OUT atlas_t* atlas)
{
    atlas->pixels = (uint8_t*) calloc(atlas->width * atlas->height * 4, 1);
    if (!atlas->pixels)
    {
        printf("Error: Cannot allocate memory for atlas.\n");
        return 0;
    }

    maxrects_t maxrects = CreateMaxRects(atlas->width, atlas->height);
    int padding = 2;
    
    // Collect and sort all rects
    int total_rects = atlas->image_count + (atlas->font_count * MAX_GLYPHS) + 1;

    packed_rect_t* rects = (packed_rect_t*) calloc(total_rects * sizeof(packed_rect_t), 1);
    int rect_index = 0;

    // Images
    for (int i = 0; i < atlas->image_count; ++i)
    {
        rects[rect_index].width = atlas->images[i].width + (padding*2);
        rects[rect_index].height = atlas->images[i].height + (padding*2);
        rects[rect_index].type = TYPE_IMAGE;
        rects[rect_index].original_index = i;
        rects[rect_index].user_data = &atlas->images[i];
        rect_index++;
    }

    // Fonts
    int temp_glyph_count = 0;
    packed_glyph_t* temp_glyphs = (packed_glyph_t*) calloc(
        atlas->font_count * MAX_GLYPHS * sizeof(packed_glyph_t), 1);

    void* bitmap_memory = calloc(GIGABYTES(1), 1);
    if (!bitmap_memory)
    {
        printf("Error: Cannot allocate memory for font bitmaps.\n");
        return 0;
    }

    uint8_t* bitmap = (uint8_t*) bitmap_memory;
    for (int i = 0; i < atlas->font_count; ++i)
    {
        font_t* font = &atlas->fonts[i];

        for (int j = 0; j < font->codepoint_count; ++j)
        {
            int c = font->codepoints[j];
            
            int glyph_index = stbtt_FindGlyphIndex(&font->info, c);
            if (glyph_index == 0)
            {
                printf("Error: Cannot find glyph index for codepoint (%d) of %s\n", c, font->name);
                continue;
            }

            int ix0, iy0, ix1, iy1;
            stbtt_GetGlyphBitmapBox(&font->info, glyph_index, font->scale, font->scale, &ix0, &iy0, &ix1, &iy1);
            int gw = ix1 - ix0;
            int gh = iy1 - iy0;
            int xoff = ix0;
            int yoff = iy0;

            if ((gw == 0 && gh == 0))
            {
                printf("Error: Cannot get codepoint (%d) of %s\n", c, font->name);
                continue;
            }

            stbtt_MakeGlyphBitmap(&font->info, bitmap, gw, gh, gw, font->scale, font->scale, glyph_index);

            int advance, lsb;
            stbtt_GetGlyphHMetrics(&font->info, glyph_index, &advance, &lsb);

            temp_glyphs[temp_glyph_count].font_index = i;
            temp_glyphs[temp_glyph_count].codepoint = c;
            temp_glyphs[temp_glyph_count].bitmap = bitmap;
            temp_glyphs[temp_glyph_count].width = gw;
            temp_glyphs[temp_glyph_count].height = gh;
            temp_glyphs[temp_glyph_count].xoff = xoff;
            temp_glyphs[temp_glyph_count].yoff = yoff;
            temp_glyphs[temp_glyph_count].xadvance = advance * font->scale;

            rects[rect_index].width = gw + (padding*2);
            rects[rect_index].height = gh + (padding*2);
            rects[rect_index].type = TYPE_GLYPH;
            rects[rect_index].original_index = temp_glyph_count;
            rects[rect_index].user_data = &temp_glyphs[temp_glyph_count];

            bitmap = (uint8_t*) ((uintptr_t) bitmap + (gw * gh));
            rect_index++;
            temp_glyph_count++;
        }
    }

    // Sort by area (descending)
    for (int i = 0; i < rect_index - 1; ++i)
    {
        for (int j = i+1; j < rect_index; ++j)
        {
            int area_i = rects[i].width * rects[i].height;
            int area_j = rects[j].width * rects[j].height;

            if (area_j > area_i)
            {
                packed_rect_t temp = rects[i];
                rects[i] = rects[j];
                rects[j] = temp;
            }
        }
    }

    // Pack rectangles
    for (int i = 0; i < rect_index; ++i)
    {
        int x, y;
        int best_index = MaxRectsFindPosition(&maxrects, rects[i].width, rects[i].height, &x, &y);

        if (best_index == -1)
        {
            printf("Error: Atlas is too small.\n");
            return 0; // Program will exit, no need to free memory
        }

        int content_x = x + padding;
        int content_y = y + padding;

        // Place content based on type
        switch (rects[i].type)
        {
            case TYPE_IMAGE: { // Images
                image_t* img = (image_t*) rects[i].user_data;
                img->x = content_x;
                img->y = content_y;
                
                for (int py = 0; py < img->height; ++py)
                {
                    for (int px = 0; px < img->width; ++px)
                    {
                        int src = ((py * img->width) + px) * 4;
                        int dst = (((content_y + py) * atlas->width) + (content_x + px)) * 4;
                        if (img->pixels)
                        {
                            atlas->pixels[dst + 0] = img->pixels[src + 0];
                            atlas->pixels[dst + 1] = img->pixels[src + 1];
                            atlas->pixels[dst + 2] = img->pixels[src + 2];
                            atlas->pixels[dst + 3] = img->pixels[src + 3];
                        }
                        else
                        {
                            atlas->pixels[dst + 0] = 0xFF;
                            atlas->pixels[dst + 1] = 0xFF;
                            atlas->pixels[dst + 2] = 0xFF;
                            atlas->pixels[dst + 3] = 0xFF;
                        }
                    }
                }
            } break;
            
            case TYPE_GLYPH: { // Fonts
                packed_glyph_t* glyph = (packed_glyph_t*) rects[i].user_data;
                font_t* font = &atlas->fonts[glyph->font_index];

                for (int py = 0; py < glyph->height; ++py)
                {
                    for (int px = 0; px < glyph->width; ++px)
                    {
                        int src = ((py * glyph->width) + px);
                        int dst = (((content_y + py) * atlas->width) + (content_x + px)) * 4;
                        atlas->pixels[dst + 0] = 255;
                        atlas->pixels[dst + 1] = 255;
                        atlas->pixels[dst + 2] = 255;
                        atlas->pixels[dst + 3] = glyph->bitmap[src];
                    }
                }

                font->glyphs[font->glyph_count] = (glyph_mapping_t){
                    .codepoint = glyph->codepoint,
                    .glyph = {
                        .u0 = (float)content_x / (float)atlas->width,
                        .v0 = (float)content_y / (float)atlas->height,
                        .u1 = (float)(content_x + glyph->width) / (float)atlas->width,
                        .v1 = (float)(content_y + glyph->height) / (float)atlas->height,
                        .xoff = (float)glyph->xoff,
                        .yoff = (float)glyph->yoff,
                        .xadvance = glyph->xadvance,
                        .w = glyph->width,
                        .h = glyph->height,
                    }
                };
                font->glyph_count++;
            } break;
        }

        // Split the free rectangle
        rect_t placed = {x, y, rects[i].width, rects[i].height};
    
        for (int j = 0; j < maxrects.count; )
        {
            rect_t free_rect = maxrects.rects[j];
            
            // Check if this free rect intersects with our placed rect
            if (!(placed.x >= free_rect.x + free_rect.width ||
                placed.x + placed.width <= free_rect.x ||
                placed.y >= free_rect.y + free_rect.height ||
                placed.y + placed.height <= free_rect.y))
            {
                MaxRectsSplitFreeRect(&maxrects, j, placed.x, placed.y, placed.width, placed.height);
            }
            else
            {
                j++;
            }
        }
        
        // Periodically prune redundant rectangles
        if (i % 50 == 0)
        {
            MaxRectsPruneRects(&maxrects);
        }
    }

    MaxRectsPruneRects(&maxrects);

    // Cleanup
    free(rects);
    free(bitmap_memory);
    free(temp_glyphs);
    FreeMaxRects(&maxrects);
    
    return 1;
}

//////////////////////////////////////////////////////////////////////////////

INTERNAL void
ExportPng(atlas_t* atlas, const char* filename)
{
    stbi_write_png(filename, atlas->width, atlas->height, 4, atlas->pixels, atlas->width*4);
}

INTERNAL bool32_t
ExportHeader(atlas_t* atlas, const char* filename)
{
    FILE* f = fopen(filename, "w");
    if (!f)
    {
        printf("Error: Cannot create header file.\n");
        return 0;
    }

    fprintf(f, "// Auto-generated sprite atlas - DO NOT EDIT!\n"
                 "// Generated by sprite backer tool\n"
                 "// Contains %d fonts and %d images\n\n"
                 "#pragma once\n\n"
                 "#include <stdint.h>\n\n",
                 atlas->font_count, atlas->image_count);

    if (atlas->image_count > 0) {
        fprintf(f, "typedef struct\n{\n"
                   "    uint32_t x, y, w, h;    // Position and size in atlas\n"
                   "    float u0, v0, u1, v1;   // UV coordinates\n"
                   "} sprite_t;\n\n"
                   "typedef enum\n{\n");

        for (int i = 0; i < atlas->image_count; ++i)
        {
            fprintf(f, "    SPRITE_%s,\n", atlas->images[i].name);
        }

        fprintf(f, "    SPRITE_COUNT,\n"
                   "} sprite_id;\n\n"
                   "static const sprite_t BACKED_SPRITE_LIST[] = {\n");

        for (int i = 0; i < atlas->image_count; ++i)
        {
            image_t* image = &atlas->images[i];
            float u0 = (float)image->x / (float)atlas->width;
            float v0 = (float)image->y / (float)atlas->height;
            float u1 = (float)(image->x + image->width) / (float)atlas->width;
            float v1 = (float)(image->y + image->height) / (float)atlas->height;

            fprintf(f,
                "    [SPRITE_%s] = {%d, %d, %d, %d, %ff, %ff, %ff, %ff},\n",
                image->name,
                image->x, image->y,
                image->width, image->height,
                u0, v0, u1, v1
            );
        }
        fprintf(f, "};\n\n");
    }

    if (atlas->font_count > 0) {
        fprintf(f, "typedef struct\n{\n"
                    "    uint64_t codepoint;     // Unicode codepoint\n"
                    "    uint32_t w, h;          // Dimensions\n"
                    "    float u0, v0, u1, v1;   // UV coordinates\n"
                    "    float xoff, yoff;       // Offset from baseline\n"
                    "    float advance;          // Advance to next glyph\n"
                    "} glyph_t;\n\n"
                    "typedef struct\n{\n"
                    "    int size;                      // Size in pixels\n"
                    "    int ascent, descent, line_gap; // Metrics\n"
                    "    glyph_t ascii_cache[128];      // Glyphs\n"
                    "    glyph_t glyphs[%d];            // All glyphs\n"
                    "    uint32_t glyph_count;          // Number of glyphs\n"
                    "} font_t;\n\n"
                    "typedef enum\n{\n", MAX_GLYPHS);

        for (int i = 0; i < atlas->font_count; ++i)
        {
            fprintf(f, "    FONT_%s,\n", atlas->fonts[i].name);
        }

        fprintf(f, "    FONT_COUNT,\n"
                    "} font_id;\n\n"
                    "static const font_t BACKED_FONT_LIST[] = {\n");
        
        for (int i = 0; i < atlas->font_count; ++i)
        {
            // Sort glyphs by codepoint
            for (int gi = 0; gi < atlas->fonts[i].glyph_count - 1; ++gi)
            {
                for (int gj = gi+1; gj < atlas->fonts[i].glyph_count; ++gj)
                {
                    if (atlas->fonts[i].glyphs[gi].codepoint > atlas->fonts[i].glyphs[gj].codepoint)
                    {
                        glyph_mapping_t temp = atlas->fonts[i].glyphs[gi];
                        atlas->fonts[i].glyphs[gi] = atlas->fonts[i].glyphs[gj];
                        atlas->fonts[i].glyphs[gj] = temp;
                    }
                }
            }

            font_t* font = &atlas->fonts[i];
            fprintf(f, "    [FONT_%s] = {\n"
                        "        .size = %d,\n"
                        "        .ascent = %d,\n"
                        "        .descent = %d,\n"
                        "        .line_gap = %d,\n"
                        "        .ascii_cache = {\n",
                        font->name, font->size, font->ascent,
                        font->descent, font->line_gap);

            int ascii_count = 0;

            for (int j = 0; j < font->glyph_count; ++j)
            {
                glyph_mapping_t* glyph = &font->glyphs[j];
                if (glyph->codepoint < 128)
                {
                    ascii_count++;
                    fprintf(f, "            [0x%02X] = { %d, %d, %d, %ff, %ff, %ff, %ff, %ff, %ff, %ff },\n",
                                glyph->codepoint, glyph->codepoint,
                                glyph->glyph.w, glyph->glyph.h,
                                glyph->glyph.u0, glyph->glyph.v0,
                                glyph->glyph.u1, glyph->glyph.v1,
                                glyph->glyph.xoff, glyph->glyph.yoff,
                                glyph->glyph.xadvance);
                }
                else
                {
                    break;
                }
            }

            fprintf(f, "        },\n");

            if (font->glyph_count <= ascii_count)
            {
                fprintf(f, "        .glyph_count = 0,\n");
            }
            else
            {
                fprintf(f, "        .glyph_count = %d,\n"
                            "        .glyphs = {\n",
                            font->glyph_count - ascii_count);

                int index = 0;
                for (int j = 0; j < font->glyph_count; ++j)
                {
                    glyph_mapping_t* glyph = &font->glyphs[j];
                    if (glyph->codepoint >= 128)
                    {
                        fprintf(f, "            [%d] = { %d, %d, %d, %ff, %ff, %ff, %ff, %ff, %ff, %ff },\n",
                        index++, glyph->codepoint,
                        glyph->glyph.w, glyph->glyph.h,
                        glyph->glyph.u0, glyph->glyph.v0,
                        glyph->glyph.u1, glyph->glyph.v1,
                        glyph->glyph.xoff, glyph->glyph.yoff,
                        glyph->glyph.xadvance);
                    }
                }

                fprintf(f, "        },\n");
            }

            fprintf(f, "    },\n");
        }

        fprintf(f, "};\n");
    }
    
    fclose(f);
    return 1;
}

int
main(int argc, char* argv[])
{
    if (argc != 3)
    {
        printf("Usage: %s <config_file> <output_name>\n\n"
               "Example: %s config.txt spritesheet\n\n"
               "Example config.txt:\n"
               "ATLAS_SIZE 1024\n"
               "FONT fonts/RobotoMono.ttf 16 ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 BODY\n",
               argv[0], argv[0]);

        return 1;
    }

    if (!ParseConfig(argv[1], &atlas))
    {
        printf("Error: Invalid config file: %s\n", argv[1]);
        return 1;
    }

    if (!CreateAtlas(&atlas))
    {
        printf("Error: Failed to pack atlas\n");
        return 1;
    }
        
    char filename[256];
    snprintf(filename, sizeof(filename), "%s.gen.png", argv[2]);
    ExportPng(&atlas, filename);

    snprintf(filename, sizeof(filename), "%s.gen.h", argv[2]);
    
    if (!ExportHeader(&atlas, filename))
    {
        printf("Error: Failed to creat eheader file.\n");
        return 1;
    }

    return 0;
}
