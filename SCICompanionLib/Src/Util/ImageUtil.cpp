#include "stdafx.h"
#include "ImageUtil.h"
#include "View.h"
#include "PaletteOperations.h"
#include "gif_lib.h"
#include "VGADither.h"

using namespace Gdiplus;

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    UINT  num = 0;          // number of image encoders
    UINT  size = 0;         // size of the image encoder array in bytes

    ImageCodecInfo* pImageCodecInfo = NULL;

    GetImageEncodersSize(&num, &size);
    if (size == 0)
        return -1;  // Failure

    pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL)
        return -1;  // Failure

    GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j)
    {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
        {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;  // Success
        }
    }

    free(pImageCodecInfo);
    return -1;  // Failure
}

//image / bmp
//image / jpeg
//image / gif
//image / tiff
//image / png

struct
{
    const char *fileExt;
    const WCHAR *imageFormat;
}
g_fileExtToFormat[] =
{
    { ".bmp", L"image/bmp" },
    { ".gif", L"image/gif" },
    { ".tif", L"image/tiff" },
    { ".png", L"image/png" },
};

// TODO: mark unused colors and squash the bits
bool Save8BitBmpGdiP(const char *filename, const Cel &cel, const PaletteComponent &palette)
{
    // We need to flip the data, so make a copy of it and do that.
    sci::array<uint8_t> buffer(cel.Data.size());
    buffer.assign(&cel.Data[0], &cel.Data[cel.Data.size()]);
    FlipImageData(&buffer[0], cel.size.cx, cel.size.cy, CX_ACTUAL(cel.size.cx));

    // Now "squish" the palette (and modify the data) so all the used colors are at the beginning.
    RGBQUAD newPaletteColors[256];
    int usedColors = SquishPalette(&buffer[0], buffer.size(), palette, newPaletteColors);

    Bitmap bitmap(cel.size.cx, cel.size.cy, CX_ACTUAL(cel.size.cx), PixelFormat8bppIndexed, &buffer[0]);
    ColorPalette *pcp = (ColorPalette*)malloc(sizeof(ColorPalette) + usedColors * 4);
    pcp->Count = usedColors;
    pcp->Flags = 0;
    for (int i = 0; i < usedColors; i++)
    {
        RGBQUAD rgb = newPaletteColors[i];
        ARGB argb = (rgb.rgbRed << RED_SHIFT) | (rgb.rgbGreen << GREEN_SHIFT) | (rgb.rgbBlue << BLUE_SHIFT);
        pcp->Entries[i] = argb;
    }
    bitmap.SetPalette(pcp);
    free(pcp);

    // Find the encoder we want
    const char *extension = PathFindExtension(filename);
    const WCHAR *encoder = nullptr;
    for (int i = 0; i < ARRAYSIZE(g_fileExtToFormat); i++)
    {
        if (0 == strncmp(g_fileExtToFormat[i].fileExt, extension, lstrlenA(g_fileExtToFormat[i].fileExt)))
        {
            encoder = g_fileExtToFormat[i].imageFormat;
            break;
        }
    }

    // In case we couldn't identify it...
    std::string hadToAddExt;
    if (encoder == nullptr)
    {
        encoder = g_fileExtToFormat[0].imageFormat;
        hadToAddExt = filename;
        hadToAddExt += g_fileExtToFormat[0].fileExt;
        filename = hadToAddExt.c_str();
    }

    CLSID encoderClsid;
    GetEncoderClsid(encoder, &encoderClsid);

    // GDI+ only deals with unicode.
    int a = lstrlenA(filename);
    BSTR unicodestr = SysAllocStringLen(nullptr, a);
    MultiByteToWideChar(CP_ACP, 0, filename, a, unicodestr, a);
    bitmap.Save(unicodestr, &encoderClsid, nullptr);
    //... when done, free the BSTR
    SysFreeString(unicodestr);

    return true;
}

void FlipImageData(uint8_t *data, int cx, int cy, int stride)
{
    sci::array<uint8_t> buffer(cx);
    for (int y = 0; y < (cy / 2); y++)
    {
        int line1 = y;
        int line2 = cy - 1 - y;
        memcpy(&buffer[0], data + (line1 * stride), cx);
        memcpy(data + line1 * stride, data + line2 * stride, cx);
        memcpy(data + line2 * stride, &buffer[0], cx);
    }
}

// Compress the palette so all the unused colors are at the beginning.
// Returns the count of used colors, and the new values in results[256]
int SquishPalette(uint8_t *data, size_t dataSize, const PaletteComponent &palette, RGBQUAD *results)
{
    uint8_t mapOldToNew[256] = { };
    uint8_t usedColors = 0;
    for (int i = 0; i < 256; i++)
    {
        if (palette.Colors[i].rgbReserved != 0x0)
        {
            mapOldToNew[i] = usedColors;
            results[usedColors] = palette.Colors[i];
            usedColors++;
        }
    }

    for (size_t i = 0; i < dataSize; i++)
    {
        data[i] = mapOldToNew[data[i]];
    }
    
    return usedColors;
}

int CountActualUsedColorsWorker(const Cel &cel, bool *used)
{
    const uint8_t *data = &cel.Data[0];
    for (int y = cel.size.cy - 1; y >= 0; y--)
    {
        int line = y * CX_ACTUAL(cel.size.cx);
        for (int x = cel.size.cx - 1; x >= 0; x--)
        {
            used[data[line + x]] = true;
        }
    }
    return std::count(used, used + 256, true);
}

int CountActualUsedColors(const Cel &cel, bool *used)
{
    memset(used, 0, 256);
    return CountActualUsedColorsWorker(cel, used);
}

int CountActualUsedColors(const std::vector<const Cel*> &cels, bool *used)
{
    memset(used, 0, 256);
    for (const Cel *cel : cels)
    {
        CountActualUsedColorsWorker(*cel, used);
    }
    return std::count(used, used + 256, true);
}

std::unique_ptr<PaletteComponent> GetPaletteFromImage(Gdiplus::Bitmap &bitmap, int *numberOfUsedEntriesOut)
{
    std::unique_ptr<PaletteComponent> originalPalette;
    PixelFormat pixelFormat = bitmap.GetPixelFormat();
    if ((pixelFormat & PixelFormat8bppIndexed) == PixelFormat8bppIndexed)
    {
        INT paletteSize = bitmap.GetPaletteSize();
        ColorPalette* cp = (ColorPalette*)malloc(paletteSize);
        Status status = bitmap.GetPalette(cp, paletteSize);
        if (status == Ok)
        {
            int numberOfPaletteEntries = (int)cp->Count;
            if (numberOfUsedEntriesOut)
            {
                *numberOfUsedEntriesOut = numberOfPaletteEntries;
            }
            originalPalette = std::make_unique<PaletteComponent>();
            for (int i = 0; i < numberOfPaletteEntries; i++)
            {
                ARGB sourcePaletteEntry = cp->Entries[i];
                RGBQUAD rgb;
                rgb.rgbRed = 0xff & (sourcePaletteEntry >> RED_SHIFT);
                rgb.rgbGreen = 0xff & (sourcePaletteEntry >> GREEN_SHIFT);
                rgb.rgbBlue = 0xff & (sourcePaletteEntry >> BLUE_SHIFT);
                rgb.rgbReserved = 0x1; // Or 0x3??? REVIEW
                originalPalette->Colors[i] = rgb;
            }
            RGBQUAD unused = { 0 };
            for (int i = numberOfPaletteEntries; i < 256; i++)
            {
                originalPalette->Colors[i] = unused;
            }
        }
        free(cp);
    }
    return originalPalette;
}

bool GetCelsAndPaletteFromGIFFile(const char *filename, std::vector<Cel> &cels, PaletteComponent &palette)
{
    int errorCode;
    GifFileType *fileType = DGifOpenFileName(filename, &errorCode);
    bool success = (DGifSlurp(fileType) == GIF_OK);
    if (success)
    {
        int bottom = fileType->Image.Top + fileType->Image.Height;
        int center = fileType->Image.Left + fileType->Image.Width / 2;  // Assume this is the center.

        // Get the palette.
        memset(palette.Colors, 0, sizeof(palette.Colors));
        if (fileType->SColorMap)
        {
            for (int i = 0; i < fileType->SColorMap->ColorCount; i++)
            {
                palette.Colors[i].rgbRed = fileType->SColorMap->Colors[i].Red;
                palette.Colors[i].rgbBlue = fileType->SColorMap->Colors[i].Blue;
                palette.Colors[i].rgbGreen = fileType->SColorMap->Colors[i].Green;
                palette.Colors[i].rgbReserved = 0x1;    // Or 0x3? REVIEW
            }
        }

        for (int i = 0; i < fileType->ImageCount; i++)
        {
            SavedImage &savedImage = fileType->SavedImages[i];
            cels.emplace_back();
            Cel &cel = cels.back();
            cel.size = size16((uint16_t)savedImage.ImageDesc.Width, (uint16_t)savedImage.ImageDesc.Height);
            cel.Data.allocate(CX_ACTUAL(cel.size.cx) * cel.size.cy);
            for (int y = 0; y < savedImage.ImageDesc.Height; y++)
            {
                int yUpsideDown = savedImage.ImageDesc.Height - y - 1;
                uint8_t *dest = &cel.Data[y * CX_ACTUAL(cel.size.cx)];
                uint8_t *src = savedImage.RasterBits + yUpsideDown * savedImage.ImageDesc.Width;
                memcpy(dest, src, savedImage.ImageDesc.Width);
            }

            int centerCurrent = savedImage.ImageDesc.Left + savedImage.ImageDesc.Width / 2;
            cel.placement.x = (int16_t)(centerCurrent - center);
            cel.placement.y = (int16_t)(savedImage.ImageDesc.Top + savedImage.ImageDesc.Height - bottom);

            GraphicsControlBlock gcb;
            if (GIF_ERROR != DGifSavedExtensionToGCB(fileType, 0, &gcb))
            {
                cel.TransparentColor = gcb.TransparentColor;
            }
            else
            {
                cel.TransparentColor = fileType->SBackGroundColor;
            }
        }
    }

    DGifCloseFile(fileType, &errorCode);
    return success;
}

void SaveCelsAndPaletteToGIFFile(const char *filename, const std::vector<Cel> &cels, int colorCount, const RGBQUAD *colors, const uint8_t *paletteMapping, uint8_t transparentIndex)
{
    int error;
    GifFileType *fileType = EGifOpenFileName(filename, false, &error);
    if (fileType)
    {
        // To start with, we'll ignore placement.
        fileType->SWidth = 0;
        fileType->SHeight = 0;
        for (const Cel &cel : cels)
        {
            fileType->SWidth = max(fileType->SWidth, cel.size.cx);
            fileType->SHeight = max(fileType->SHeight, cel.size.cy);
        }
        fileType->SColorResolution = 8;
        fileType->ImageCount = (int)cels.size();
        fileType->Image.Interlace = false;
        // Not sure what these 4 things are:
        fileType->Image.Left = 0;
        fileType->Image.Top = 0;
        fileType->Image.Width = fileType->SWidth;
        fileType->Image.Height = fileType->SHeight;

        GifColorType gifColors[256];
        for (int i = 0; i < colorCount; i++)
        {
            RGBQUAD rgb = colors[paletteMapping[i]];
            gifColors[i].Red = rgb.rgbRed;
            gifColors[i].Green = rgb.rgbGreen;
            gifColors[i].Blue = rgb.rgbBlue;
        }
        fileType->SColorMap = GifMakeMapObject(colorCount, gifColors);

        // Now the images
        fileType->SavedImages = new SavedImage[fileType->ImageCount];
        for (size_t i = 0; i < cels.size(); i++)
        {
            const Cel &cel = cels[i];
            fileType->SavedImages[i].ExtensionBlockCount = 0;
            fileType->SavedImages[i].ImageDesc.ColorMap = nullptr;
            fileType->SavedImages[i].ImageDesc.Interlace = false;
            fileType->SavedImages[i].ImageDesc.Left = 0;
            fileType->SavedImages[i].ImageDesc.Top = 0;
            fileType->SavedImages[i].ImageDesc.Width = cel.size.cx;
            fileType->SavedImages[i].ImageDesc.Height = cel.size.cy;

            fileType->SavedImages[i].RasterBits = new GifByteType[cel.size.cx * cel.size.cy];
            for (int y = 0; y < cel.size.cy; y++)
            {
                int yUpsideDown = fileType->SavedImages[i].ImageDesc.Height - y - 1;
                const uint8_t *src = &cel.Data[y * CX_ACTUAL(cel.size.cx)];
                uint8_t *dest = fileType->SavedImages[i].RasterBits + yUpsideDown * fileType->SavedImages[i].ImageDesc.Width;
                memcpy(dest, src, fileType->SavedImages[i].ImageDesc.Width);
            }
        }

        int result = EGifSpew(fileType);
        if (result != GIF_OK)
        {
            // If an error happens, then I have to close the file? What on earth?
            int closeError;
            EGifCloseFile(fileType, &closeError);
        }
        // If no error, then the file is already closed.
    }
    else
    {
        // TODO: throw up message box.
    }
}

bool GetCelsAndPaletteFromGdiplus(Gdiplus::Bitmap &bitmap, uint8_t transparentColor, std::vector<Cel> &cels, PaletteComponent &palette)
{
    std::unique_ptr<PaletteComponent> tempPalette = GetPaletteFromImage(bitmap, nullptr);
    bool success = (tempPalette != nullptr);
    if (success)
    {
        success = false;
        UINT cx = bitmap.GetWidth();
        UINT cy = bitmap.GetHeight();
        Gdiplus::Rect rect(0, 0, cx, cy);

        Gdiplus::BitmapData bitmapData;
        if (Ok == bitmap.LockBits(&rect, ImageLockModeRead, PixelFormat8bppIndexed, &bitmapData))
        {
            success = true;
            cels.emplace_back();
            Cel &cel = cels.back();
            cel.size = size16((uint16_t)cx, (uint16_t)cy);
            cel.TransparentColor = transparentColor;
            uint8_t *pDIBBits8 = (uint8_t *)bitmapData.Scan0;
            cel.Data.allocate(PaddedSize(cel.size));
            cel.Data.assign(pDIBBits8, pDIBBits8 + cel.Data.size());

            bitmap.UnlockBits(&bitmapData); // REVIEW: Not exception-safe

            // Transfer the palette now too.
            palette = *tempPalette;
        }
    }
    return success;
}

bool DoPalettesMatch(const PaletteComponent &paletteA, const PaletteComponent &paletteB)
{
    bool match = true;
    // Ignore the "used" bits
    for (size_t i = 0; match && (i < ARRAYSIZE(paletteA.Colors)); i++)
    {
        match = (paletteA.Colors[i].rgbRed == paletteB.Colors[i].rgbRed) &&
            (paletteA.Colors[i].rgbGreen == paletteB.Colors[i].rgbGreen) &&
            (paletteA.Colors[i].rgbBlue == paletteB.Colors[i].rgbBlue);
    }
    return match;
}

const uint8_t AlphaThreshold = 128;

uint8_t _FindBestPaletteIndexMatch(uint8_t transparentColor, RGBQUAD desiredColor, int colorCount, const uint8_t *mapping, const RGBQUAD *colors)
{
    int bestIndex = 1;  // Just something that's not zero, so we can determine when we failed.
    int bestDistance = INT_MAX;
    for (int i = 0; i < colorCount; i++)
    {
        RGBQUAD paletteColor = colors[mapping[i]];
        if ((i != (int)transparentColor) && (paletteColor.rgbReserved != 0x0))
        {
            int distance = GetColorDistance(desiredColor, paletteColor);
            if (distance < bestDistance)
            {
                bestIndex = i;
                bestDistance = distance;
            }
        }
    }
    return (uint8_t)bestIndex;
}


int g_dummyTrack[256];

void ConvertBitmapToCel(int *trackUsage, Gdiplus::Bitmap &bitmap, uint8_t transparentColor, bool isEGA16, bool performDither, int colorCount, const uint8_t *paletteMapping, const RGBQUAD *colors, std::vector<Cel> &cels)
{
    bool write = (trackUsage == nullptr);
    if (trackUsage)
    {
        performDither = false;
    }
    else
    {
        trackUsage = g_dummyTrack;
    }

    uint16_t width = (uint16_t)bitmap.GetWidth();
    uint16_t height = (uint16_t)bitmap.GetHeight();

    // Clamp size to allowed values.
    width = min(320, width);
    width = max(1, width);
    height = min(190, height);
    height = max(1, height);

    Cel celDummy;
    UINT count = bitmap.GetFrameDimensionsCount();
    std::unique_ptr<GUID[]> dimensionIds = std::make_unique<GUID[]>(count);
    bitmap.GetFrameDimensionsList(dimensionIds.get(), count);
    UINT frameCount = bitmap.GetFrameCount(&dimensionIds.get()[0]);
    for (UINT frame = 0; frame < frameCount; frame++)
    {
        GUID guid = FrameDimensionTime;
        bitmap.SelectActiveFrame(&guid, frame);

        if (write)
        {
            cels.emplace_back();
        }
        Cel &cel = write ? cels.back() : celDummy;
        cel.size = size16(width, height);
        cel.TransparentColor = transparentColor;
        cel.Data.allocate(CX_ACTUAL(width)* height);

        VGADither dither(width, height);

        for (int y = 0; y < height; y++)
        {
            uint8_t *pBitsRow = &cel.Data[0] + ((height - y - 1) * CX_ACTUAL(width));
            for (int x = 0; x < width; x++)
            {
                uint8_t dummy;
                uint8_t *valuePointer = write ? (pBitsRow + x) : &dummy;
                Color color;
                if (Ok == bitmap.GetPixel(x, y, &color))
                {
                    if (color.GetA() < AlphaThreshold)
                    {
                        *valuePointer = transparentColor;
                    }
                    else
                    {
                        // find closest match.
                        if (isEGA16)
                        {
                            // Special-case ega, because we can do dithering
                            EGACOLOR curColor = GetClosestEGAColor(1, performDither ? 3 : 2, color.ToCOLORREF());
                            *valuePointer = ((x^y) & 1) ? curColor.color1 : curColor.color2;
                            trackUsage[curColor.color1]++;
                        }
                        else
                        {
                            RGBQUAD rgb = { color.GetB(), color.GetG(), color.GetR(), 0 };
                            rgb = dither.ApplyErrorAt(rgb, x, y);
                            uint8_t bestMatch = _FindBestPaletteIndexMatch(transparentColor, rgb, colorCount, paletteMapping, colors);
                            trackUsage[bestMatch]++;
                            *valuePointer = bestMatch;

                            if (performDither)
                            {
                                dither.PropagateError(rgb, colors[bestMatch], x, y);
                            }
                        }
                    }
                }
            }
        }
    }
}

void ConvertCelToNewPalette(int *trackUsage, Cel &cel, const PaletteComponent &currentPalette, uint8_t transparentColor, bool isEGA16, bool egaDither, int colorCount, const uint8_t *paletteMapping, const RGBQUAD *colors)
{
    bool write = (trackUsage == nullptr);
    if (trackUsage)
    {
        egaDither = false;
    }
    else
    {
        trackUsage = g_dummyTrack;
    }
    int height = cel.size.cy;
    int width = cel.size.cx;
    for (int y = 0; y < height; y++)
    {
        uint8_t *pBitsRow = &cel.Data[0] + ((height - y - 1) * CX_ACTUAL(width));
        for (int x = 0; x < width; x++)
        {
            uint8_t dummy;
            uint8_t value = *(pBitsRow + x);
            uint8_t *setValuePointer = write ? (pBitsRow + x) : &dummy;
            if (value == cel.TransparentColor)
            {
                *setValuePointer = transparentColor;
            }
            else
            {
                RGBQUAD rgbExisting = currentPalette.Colors[value];
                // find closest match.
                if (isEGA16)
                {
                    // Special-case ega, because we can do dithering
                    EGACOLOR curColor = GetClosestEGAColor(1, egaDither ? 3 : 2, RGB(rgbExisting.rgbRed, rgbExisting.rgbGreen, rgbExisting.rgbBlue));
                    *setValuePointer = ((x^y) & 1) ? curColor.color1 : curColor.color2;
                    trackUsage[curColor.color1]++;
                }
                else
                {
                    uint8_t bestMatch = _FindBestPaletteIndexMatch(transparentColor, rgbExisting, colorCount, paletteMapping, colors);;
                    *setValuePointer = bestMatch;
                    trackUsage[bestMatch]++;
                }
            }
        }
    }

    if (write)
    {
        cel.TransparentColor = transparentColor;
    }
}

