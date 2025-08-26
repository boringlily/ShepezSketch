#define CLAY_IMPLEMENTATION
#define RAYLIB_IMPLEMENTATION
#include "raylib.h"
#include "clay.h"
#include "raymath.h"

#include "stdint.h"
#include "cstring"
#include "stdio.h"
#include "stdlib.h"
#include <string>
#include <array>
#include <vector>

#include "Graphics.h"
#include "Components.cpp"

#define ASSETS_PATH "../assets/"
#define ICONS_PATH ASSETS_PATH "Icon/"
#define FONTS_PATH ASSETS_PATH "fonts/Nunito/static/"

namespace Graphics {
void Render(Clay_RenderCommandArray renderCommands);
};

static std::array<Font, 4> loadedFonts;

static inline void LoadAppFonts()
{
    struct FontLoadConfig {
        FontId id;
        u32 loadSizePixels;
        u8 filename[26];
    };
    static constexpr std::array<FontLoadConfig, 4u> fontLoadList {
        FontLoadConfig { .id = FontId::Regular, .loadSizePixels = 28, .filename = "Nunito-Regular.ttf" },
        FontLoadConfig { .id = FontId::Medium, .loadSizePixels = 32, .filename = "Nunito-Medium.ttf" },
        FontLoadConfig { .id = FontId::MediumItalic, .loadSizePixels = 32, .filename = "Nunito-MediumItalic.ttf" },
        FontLoadConfig { .id = FontId::Semibold, .loadSizePixels = 48, .filename = "Nunito-SemiBold.ttf" },
    };

    for (auto& font : fontLoadList) {
        static std::array<char, 60> fullpath;
        snprintf(const_cast<char*>(fullpath.data()), 60, FONTS_PATH "%s", font.filename);
        u8 index = static_cast<u8>(font.id);
        loadedFonts[index] = LoadFontEx(fullpath.data(), font.loadSizePixels, nullptr, 127);
        SetTextureFilter(loadedFonts[index].texture, TEXTURE_FILTER_BILINEAR);
    }
}

struct Icon {
    std::array<u8, 15> name;
    Texture2D texture {};
};

#define MAKE_STRINGS(VAR) #VAR,
const std::array<std::array<u8, 20>, 9> IconNames = {
    Make_Icons(MAKE_STRINGS)
};

static std::array<Texture2D, IconNames.size()> LoadedIcons;

void LoadAppIcons()
{
    u8 index { 0 };
    for (auto& name : IconNames) {
        static std::array<char, 60> iconPath;
        snprintf(const_cast<char*>(iconPath.data()), 60, ICONS_PATH "%s.png", name.data());
        LoadedIcons[index] = LoadTexture(iconPath.data());

        SetTextureFilter(LoadedIcons[index], TEXTURE_FILTER_BILINEAR);
        index++;
    }
}

Texture& GetIcon(IconId iconId)
{
    return LoadedIcons[iconId];
}

#define CLAY_RECTANGLE_TO_RAYLIB_RECTANGLE(rectangle) \
    (Rectangle) { .x = rectangle.x, .y = rectangle.y, .width = rectangle.width, .height = rectangle.height }
#define CLAY_COLOR_TO_RAYLIB_COLOR(color) \
    (Color) { .r = (unsigned char)roundf(color.r), .g = (unsigned char)roundf(color.g), .b = (unsigned char)roundf(color.b), .a = (unsigned char)roundf(color.a) }

static inline Clay_Dimensions Raylib_MeasureText(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData)
{
    // Measure string size for Font
    Clay_Dimensions textSize = { 0 };

    float maxTextWidth = 0.0f;
    float lineTextWidth = 0;
    int maxLineCharCount = 0;
    int lineCharCount = 0;

    float textHeight = config->fontSize;
    Font* fonts = (Font*)userData;
    Font fontToUse = fonts[config->fontId];
    // Font failed to load, likely the fonts are in the wrong place relative to the execution dir.
    // RayLib ships with a default font, so we can continue with that built in one.
    if (!fontToUse.glyphs) {
        fontToUse = GetFontDefault();
    }

    float scaleFactor = config->fontSize / (float)fontToUse.baseSize;

    for (int i = 0; i < text.length; ++i, lineCharCount++) {
        if (text.chars[i] == '\n') {
            maxTextWidth = fmax(maxTextWidth, lineTextWidth);
            maxLineCharCount = CLAY__MAX(maxLineCharCount, lineCharCount);
            lineTextWidth = 0;
            lineCharCount = 0;
            continue;
        }
        int index = text.chars[i] - 32;
        if (fontToUse.glyphs[index].advanceX != 0)
            lineTextWidth += fontToUse.glyphs[index].advanceX;
        else
            lineTextWidth += (fontToUse.recs[index].width + fontToUse.glyphs[index].offsetX);
    }

    maxTextWidth = fmax(maxTextWidth, lineTextWidth);
    maxLineCharCount = CLAY__MAX(maxLineCharCount, lineCharCount);

    textSize.width = maxTextWidth * scaleFactor + (lineCharCount * config->letterSpacing);
    textSize.height = textHeight;

    return textSize;
}

static inline Clay_Dimensions GetScreenSize()
{
    return Clay_Dimensions { .width = static_cast<float>(GetScreenWidth()),
        .height = static_cast<float>(GetScreenHeight()) };
}

static inline bool HasScreenSizeChanged(Clay_Dimensions& A, Clay_Dimensions& B)
{
    return (A.height != B.height || A.width != B.width);
}

void HandleClayErrors(Clay_ErrorData errorData)
{
    printf("%s", errorData.errorText.chars);
}
static Clay_Arena clay_memory;

#ifdef __cplusplus
extern "C" {
#endif

GRAPHICS_API
void Graphics::Initialize()
{
    constexpr int WINDOW_MIN_WIDTH = 1440;
    constexpr int WINDOW_MIN_HEIGHT = 744;
    std::string WINDOW_TITLE = "ShapezSketch";
    SetConfigFlags(
        FLAG_WINDOW_RESIZABLE
        | FLAG_WINDOW_HIGHDPI
        | FLAG_MSAA_4X_HINT
        | FLAG_VSYNC_HINT);

    InitWindow(WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT, const_cast<char*>(WINDOW_TITLE.c_str()));
    SetWindowMinSize(WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT);

    uint64_t clayRequiredMemory = Clay_MinMemorySize();

    clay_memory = Clay_CreateArenaWithCapacityAndMemory(
        clayRequiredMemory, malloc(clayRequiredMemory));

    SetExitKey(KEY_NULL); // Disable exit key
    Clay_Initialize(
        clay_memory, GetScreenSize(),
        (Clay_ErrorHandler) { HandleClayErrors });

    LoadAppFonts();
    LoadAppIcons();
    Clay_SetMeasureTextFunction(Raylib_MeasureText, loadedFonts.data());
}

GRAPHICS_API
void Graphics::Deactivate()
{
    CloseWindow();
}

GRAPHICS_API
void Graphics::BeginFrame()
{
    Clay_Dimensions newScreenSize = GetScreenSize();
    auto screenSize = newScreenSize;

    auto mousePosition = GetMousePosition();
    auto scrollDelta = GetMouseWheelMoveV();

    Clay_SetLayoutDimensions(screenSize);
    Clay_SetPointerState((Clay_Vector2) { mousePosition.x, mousePosition.y }, IsMouseButtonDown(0));
    Clay_UpdateScrollContainers(true, (Clay_Vector2) { scrollDelta.x, scrollDelta.y }, GetFrameTime());

    Clay_BeginLayout();
}

GRAPHICS_API
void Graphics::EndFrame()
{
    Clay_RenderCommandArray renderCommands = Clay_EndLayout();
    BeginDrawing();
    ClearBackground(BLACK);

    Render(renderCommands);
    EndDrawing();
}

#ifdef __cplusplus
}
#endif

void Graphics::Render(Clay_RenderCommandArray renderCommands)
{
    static std::vector<char> tempRenderBuffer(128u);

    for (int j = 0; j < renderCommands.length; j++) {
        Clay_RenderCommand* renderCommand = Clay_RenderCommandArray_Get(&renderCommands, j);
        Clay_BoundingBox boundingBox = { roundf(renderCommand->boundingBox.x), roundf(renderCommand->boundingBox.y), roundf(renderCommand->boundingBox.width), roundf(renderCommand->boundingBox.height) };

        switch (renderCommand->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            Clay_TextRenderData* textData = &renderCommand->renderData.text;
            Font fontToUse = loadedFonts[textData->fontId];
            int strlen = textData->stringContents.length + 1;
            if (strlen > tempRenderBuffer.capacity()) {
                tempRenderBuffer.resize(strlen + 1);
            }

            // Raylib uses standard C strings so isn't compatible with cheap slices, we need to clone the string to append null terminator
            memcpy(tempRenderBuffer.data(), textData->stringContents.chars, textData->stringContents.length);
            tempRenderBuffer[textData->stringContents.length] = '\0';
            DrawTextEx(fontToUse, tempRenderBuffer.data(), (Vector2) { boundingBox.x, boundingBox.y }, (float)textData->fontSize, (float)textData->letterSpacing, CLAY_COLOR_TO_RAYLIB_COLOR(textData->textColor));

            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
            Texture2D imageTexture = *(Texture2D*)renderCommand->renderData.image.imageData;
            Clay_Color tintColor = renderCommand->renderData.image.backgroundColor;
            if (tintColor.r == 0 && tintColor.g == 0 && tintColor.b == 0 && tintColor.a == 0) {
                tintColor = (Clay_Color) { 255, 255, 255, 255 };
            }
            DrawTexturePro(
                imageTexture,
                (Rectangle) { 0, 0, (float)imageTexture.width, (float)imageTexture.height },
                (Rectangle) { boundingBox.x, boundingBox.y, boundingBox.width, boundingBox.height },
                (Vector2) {},
                0,
                CLAY_COLOR_TO_RAYLIB_COLOR(tintColor));
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
            BeginScissorMode((int)roundf(boundingBox.x), (int)roundf(boundingBox.y), (int)roundf(boundingBox.width), (int)roundf(boundingBox.height));
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
            EndScissorMode();
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
            Clay_RectangleRenderData* config = &renderCommand->renderData.rectangle;
            if (config->cornerRadius.topLeft > 0) {
                float radius = (config->cornerRadius.topLeft * 2) / (float)((boundingBox.width > boundingBox.height) ? boundingBox.height : boundingBox.width);
                DrawRectangleRounded((Rectangle) { boundingBox.x, boundingBox.y, boundingBox.width, boundingBox.height }, radius, 8, CLAY_COLOR_TO_RAYLIB_COLOR(config->backgroundColor));
            } else {
                DrawRectangle(boundingBox.x, boundingBox.y, boundingBox.width, boundingBox.height, CLAY_COLOR_TO_RAYLIB_COLOR(config->backgroundColor));
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_BORDER: {
            Clay_BorderRenderData* config = &renderCommand->renderData.border;
            // Left border
            if (config->width.left > 0) {
                DrawRectangle((int)roundf(boundingBox.x), (int)roundf(boundingBox.y + config->cornerRadius.topLeft), (int)config->width.left, (int)roundf(boundingBox.height - config->cornerRadius.topLeft - config->cornerRadius.bottomLeft), CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
            }
            // Right border
            if (config->width.right > 0) {
                DrawRectangle((int)roundf(boundingBox.x + boundingBox.width - config->width.right), (int)roundf(boundingBox.y + config->cornerRadius.topRight), (int)config->width.right, (int)roundf(boundingBox.height - config->cornerRadius.topRight - config->cornerRadius.bottomRight), CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
            }
            // Top border
            if (config->width.top > 0) {
                DrawRectangle((int)roundf(boundingBox.x + config->cornerRadius.topLeft), (int)roundf(boundingBox.y), (int)roundf(boundingBox.width - config->cornerRadius.topLeft - config->cornerRadius.topRight), (int)config->width.top, CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
            }
            // Bottom border
            if (config->width.bottom > 0) {
                DrawRectangle((int)roundf(boundingBox.x + config->cornerRadius.bottomLeft), (int)roundf(boundingBox.y + boundingBox.height - config->width.bottom), (int)roundf(boundingBox.width - config->cornerRadius.bottomLeft - config->cornerRadius.bottomRight), (int)config->width.bottom, CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
            }
            if (config->cornerRadius.topLeft > 0) {
                DrawRing((Vector2) { roundf(boundingBox.x + config->cornerRadius.topLeft), roundf(boundingBox.y + config->cornerRadius.topLeft) }, roundf(config->cornerRadius.topLeft - config->width.top), config->cornerRadius.topLeft, 180, 270, 10, CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
            }
            if (config->cornerRadius.topRight > 0) {
                DrawRing((Vector2) { roundf(boundingBox.x + boundingBox.width - config->cornerRadius.topRight), roundf(boundingBox.y + config->cornerRadius.topRight) }, roundf(config->cornerRadius.topRight - config->width.top), config->cornerRadius.topRight, 270, 360, 10, CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
            }
            if (config->cornerRadius.bottomLeft > 0) {
                DrawRing((Vector2) { roundf(boundingBox.x + config->cornerRadius.bottomLeft), roundf(boundingBox.y + boundingBox.height - config->cornerRadius.bottomLeft) }, roundf(config->cornerRadius.bottomLeft - config->width.bottom), config->cornerRadius.bottomLeft, 90, 180, 10, CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
            }
            if (config->cornerRadius.bottomRight > 0) {
                DrawRing((Vector2) { roundf(boundingBox.x + boundingBox.width - config->cornerRadius.bottomRight), roundf(boundingBox.y + boundingBox.height - config->cornerRadius.bottomRight) }, roundf(config->cornerRadius.bottomRight - config->width.bottom), config->cornerRadius.bottomRight, 0.1, 90, 10, CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_CUSTOM: {
            Clay_CustomRenderData commandData = renderCommand->renderData.custom;

            switch (commandData.customCommandId) {
            case ClayCustomCommands::DrawRenderTexture: {
                RenderTexture& renderTexture = *reinterpret_cast<RenderTexture*>(commandData.customData);

                DrawTexturePro(
                    renderTexture.texture,
                    (Rectangle) { 0, 0, (float)renderTexture.texture.width, (float)-renderTexture.texture.height },
                    (Rectangle) { boundingBox.x, boundingBox.y, boundingBox.width, boundingBox.height },
                    (Vector2) {},
                    0,
                    Color { 255, 255, 255, 255 });
                break;
            }
            default: {
                printf("OOPs, custom command ID not handled.");
            }
            }

            break;
        }
        default: {
            printf("Error: unhandled render command.");
            exit(1);
        }
        }
    }
}

/// TODO: Move this into the canvas logic space, I think it belongs there.

// Get a ray trace from the screen position (i.e mouse) within a specific section of the screen
Ray GetScreenToWorldPointWithZDistance(Vector2 position, Camera3D camera, int screenWidth, int screenHeight, float zDistance)
{
    Ray ray = { 0 };

    // Calculate normalized device coordinates
    // NOTE: y value is negative
    float x = (2.0f * position.x) / (float)screenWidth - 1.0f;
    float y = 1.0f - (2.0f * position.y) / (float)screenHeight;
    float z = 1.0f;

    // Store values in a vector
    Vector3 deviceCoords = { x, y, z };

    // Calculate view matrix from camera look at
    Matrix matView = MatrixLookAt(camera.position, camera.target, camera.up);

    Matrix matProj = MatrixIdentity();

    if (camera.projection == CAMERA_PERSPECTIVE) {
        // Calculate projection matrix from perspective
        matProj = MatrixPerspective(camera.fovy * DEG2RAD, ((double)screenWidth / (double)screenHeight), 0.01f, zDistance);
    } else if (camera.projection == CAMERA_ORTHOGRAPHIC) {
        double aspect = (double)screenWidth / (double)screenHeight;
        double top = camera.fovy / 2.0;
        double right = top * aspect;

        // Calculate projection matrix from orthographic
        matProj = MatrixOrtho(-right, right, -top, top, 0.01, 1000.0);
    }

    // Unproject far/near points
    Vector3 nearPoint = Vector3Unproject((Vector3) { deviceCoords.x, deviceCoords.y, 0.0f }, matProj, matView);
    Vector3 farPoint = Vector3Unproject((Vector3) { deviceCoords.x, deviceCoords.y, 1.0f }, matProj, matView);

    // Calculate normalized direction vector
    Vector3 direction = Vector3Normalize(Vector3Subtract(farPoint, nearPoint));

    ray.position = farPoint;

    // Apply calculated vectors to ray
    ray.direction = direction;

    return ray;
}