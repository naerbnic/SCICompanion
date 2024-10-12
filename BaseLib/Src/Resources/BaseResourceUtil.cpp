#include "BaseResourceUtil.h"

#include <Shlwapi.h>

#include "format.h"
#include "ResourceTypes.h"

int ResourceNumberFromFileName(PCTSTR pszFileName)
{
    int iNumber = -1;
    PCTSTR pszExt = PathFindExtension(pszFileName);
    if (pszExt && *pszExt && *(pszExt + 1))
    {
        if (isdigit(*(pszExt + 1)))
        {
            iNumber = StrToInt(pszExt + 1);
        }
        else
        {
            PCTSTR pszJustFileName = PathFindFileName(pszFileName);
            if (pszJustFileName && *pszJustFileName && isdigit(*pszJustFileName))
            {
                iNumber = StrToInt(pszJustFileName);
            }
        }

    }
    return iNumber;
}

// Returns "n004" for input of 4
std::string default_reskey(int iNumber, uint32_t base36Number)
{
    if (base36Number == NoBase36)
    {
        return fmt::format("n{0:0>3}", iNumber);
    }
    else
    {
        // NNNNNVV.CCS
        return fmt::format("{0:0>3t}{1:0>2t}{2:0>2t}.{3:0>2t}{4:0>1t}",
            iNumber,
            (base36Number >> 8) & 0xff,
            (base36Number >> 16) & 0xff,
            (base36Number >> 0) & 0xff,
            ((base36Number >> 24) & 0xff) % 36
            // If this number (sequence) is 36, we want it to wrap 0 zero. Seq 0 not allowed.
        );
    }
}