#include "fontdump.h"

#include <Windows.h>

#include <string>

std::wstring qt_getEnglishName(const std::wstring &familyName, bool includeStyle = false);

FontDump::FontDump()
{

}

bool qt_localizedName(const std::wstring &name)
{
    const wchar_t *c = name.c_str();
    for (size_t i = 0; i < name.length(); ++i) {
        if (c[i] >= 0x100)
            return true;
    }
    return false;
}

static int total = 0;

static int WINAPI populateFontFamilies(const LOGFONT *logFont, const TEXTMETRIC *textmetric,
                                                DWORD, LPARAM)
{
    // the "@family" fonts are just the same as "family". Ignore them.
    const ENUMLOGFONTEX *f = reinterpret_cast<const ENUMLOGFONTEX *>(logFont);
    const wchar_t *faceNameW = f->elfLogFont.lfFaceName;
    if (faceNameW[0] && faceNameW[0] != L'@' && wcsncmp(faceNameW, L"WST_", 4)) {
        std::wstring faceName = faceNameW;
        // Register current font's english name as alias
        const bool ttf = (textmetric->tmPitchAndFamily & TMPF_TRUETYPE);
        if (ttf && qt_localizedName(faceName)) {
            const std::wstring englishName = qt_getEnglishName(faceName);
            if (!englishName.empty())
                wprintf(L"Fout %s, %s\n", faceName.c_str(), englishName.c_str());
        } else {
            wprintf(L"Fout %s\n", faceName.c_str());
        }
    }
    ++total;
    return 1; // continue
}

void FontDump::populateFontDatabase()
{
    HDC dummy = GetDC(0);
    LOGFONT lf;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfFaceName[0] = 0;
    lf.lfPitchAndFamily = 0;
    EnumFontFamiliesEx(dummy, &lf, populateFontFamilies, 0, 0);
    wprintf(L"Total %d font families", total);
    ReleaseDC(0, dummy);
}

#define MAKE_TAG(ch1, ch2, ch3, ch4) (\
    (((DWORD)(ch4)) << 24) | \
    (((DWORD)(ch3)) << 16) | \
    (((DWORD)(ch2)) << 8) | \
    ((DWORD)(ch1)) \
    )

struct FontNames
{
    std::wstring name;   // e.g. "DejaVu Sans Condensed"
    std::wstring style;  // e.g. "Italic"
    std::wstring preferredName;  // e.g. "DejaVu Sans"
    std::wstring preferredStyle; // e.g. "Condensed Italic"
};

inline WORD qt_getUShort(const unsigned char *p)
{
    WORD val;
    val = *p++ << 8;
    val |= *p;

    return val;
}

enum FieldTypeValue {
    FamilyId = 1,
    StyleId = 2,
    PreferredFamilyId = 16,
    PreferredStyleId = 17,
};

enum PlatformFieldValue {
    PlatformId_Unicode = 0,
    PlatformId_Apple = 1,
    PlatformId_Microsoft = 3
};

static std::wstring readName(bool unicode, const unsigned char *string, int length)
{
    std::wstring out;
    if (unicode) {
        // utf16

        length /= 2;
        out.resize(length);
        wchar_t *uc = &out[0];
        for (int i = 0; i < length; ++i)
            uc[i] = qt_getUShort(string + 2*i);
    } else {
        // Apple Roman

        out.resize(length);
        wchar_t *uc = &out[0];
        for (int i = 0; i < length; ++i)
            uc[i] = wchar_t(char(string[i]));
    }
    return out;
}

static FontNames qt_getCanonicalFontNames(const unsigned char *table, DWORD bytes)
{
    FontNames out;
    const int NameRecordSize = 12;
    const int MS_LangIdEnglish = 0x009;

    // get the name table
    WORD count;
    WORD string_offset;
    const unsigned char *names;

    if (bytes < 8)
        return out;

    if (qt_getUShort(table) != 0)
        return out;

    count = qt_getUShort(table + 2);
    string_offset = qt_getUShort(table + 4);
    names = table + 6;

    if (string_offset >= bytes || 6 + count*NameRecordSize > string_offset)
        return out;

    enum PlatformIdType {
        NotFound = 0,
        Unicode = 1,
        Apple = 2,
        Microsoft = 3
    };

    PlatformIdType idStatus[4] = { NotFound, NotFound, NotFound, NotFound };
    int ids[4] = { -1, -1, -1, -1 };

    for (int i = 0; i < count; ++i) {
        // search for the correct name entries

        WORD platform_id = qt_getUShort(names + i*NameRecordSize);
        WORD encoding_id = qt_getUShort(names + 2 + i*NameRecordSize);
        WORD language_id = qt_getUShort(names + 4 + i*NameRecordSize);
        WORD name_id = qt_getUShort(names + 6 + i*NameRecordSize);

        PlatformIdType *idType = nullptr;
        int *id = nullptr;

        switch (name_id) {
        case FamilyId:
            idType = &idStatus[0];
            id = &ids[0];
            break;
        case StyleId:
            idType = &idStatus[1];
            id = &ids[1];
            break;
        case PreferredFamilyId:
            idType = &idStatus[2];
            id = &ids[2];
            break;
        case PreferredStyleId:
            idType = &idStatus[3];
            id = &ids[3];
            break;
        default:
            continue;
        }

        WORD length = qt_getUShort(names + 8 + i*NameRecordSize);
        WORD offset = qt_getUShort(names + 10 + i*NameRecordSize);
        if (DWORD(string_offset + offset + length) > bytes)
            continue;

        if ((platform_id == PlatformId_Microsoft
            && (encoding_id == 0 || encoding_id == 1))
            && ((language_id & 0x3ff) == MS_LangIdEnglish
                || *idType < Microsoft)) {
            *id = i;
            *idType = Microsoft;
        }
        // not sure if encoding id 4 for Unicode is utf16 or ucs4...
        else if (platform_id == PlatformId_Unicode && encoding_id < 4 && *idType < Unicode) {
            *id = i;
            *idType = Unicode;
        }
        else if (platform_id == PlatformId_Apple && encoding_id == 0 && language_id == 0 && *idType < Apple) {
            *id = i;
            *idType = Apple;
        }
    }

    std::wstring strings[4];
    for (int i = 0; i < 4; ++i) {
        if (idStatus[i] == NotFound)
            continue;
        int id = ids[i];
        WORD length = qt_getUShort(names +  8 + id * NameRecordSize);
        WORD offset = qt_getUShort(names + 10 + id * NameRecordSize);
        const unsigned char *string = table + string_offset + offset;
        strings[i] = readName(idStatus[i] != Apple, string, length);
    }

    out.name = strings[0];
    out.style = strings[1];
    out.preferredName = strings[2];
    out.preferredStyle = strings[3];
    return out;
}

static std::wstring qt_getEnglishName(const std::wstring &familyName, bool includeStyle)
{
    std::wstring i18n_name;
    std::wstring faceName = familyName;
    faceName.resize(LF_FACESIZE - 1);

    HDC hdc = GetDC( 0 );
    LOGFONT lf;
    memset(&lf, 0, sizeof(LOGFONT));
    faceName.copy(lf.lfFaceName, LF_FACESIZE);
    lf.lfFaceName[faceName.size()] = 0;
    lf.lfCharSet = DEFAULT_CHARSET;
    HFONT hfont = CreateFontIndirect(&lf);

    if (!hfont) {
        ReleaseDC(0, hdc);
        return std::wstring();
    }

    HGDIOBJ oldobj = SelectObject( hdc, hfont );

    const DWORD name_tag = MAKE_TAG( 'n', 'a', 'm', 'e' );

    // get the name table
    unsigned char *table = 0;

    DWORD bytes = GetFontData( hdc, name_tag, 0, 0, 0 );
    if ( bytes == GDI_ERROR ) {
        // ### Unused variable
        // int err = GetLastError();
        goto error;
    }

    table = new unsigned char[bytes];
    GetFontData(hdc, name_tag, 0, table, bytes);
    if ( bytes == GDI_ERROR )
        goto error;

    {
        const FontNames names = qt_getCanonicalFontNames(table, bytes);
        i18n_name = names.name;
        if (includeStyle)
            i18n_name += wchar_t(' ') + names.style;
    }
error:
    delete [] table;
    SelectObject( hdc, oldobj );
    DeleteObject( hfont );
    ReleaseDC( 0, hdc );

    //qDebug("got i18n name of '%s' for font '%s'", i18n_name.latin1(), familyName.toLocal8Bit().data());
    return i18n_name;
}
