/***************************************************************************
    Copyright (c) 2015 Philip Fortier

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
***************************************************************************/
// ResourceDocument.cpp : implementation file
//

#include "stdafx.h"
#include "AppState.h"
#include "ResourceDocument.h"
#include "ResourceMap.h"
#include "SaveResourceDialog.h"
#include "ResourceEntity.h"
#include "RasterOperations.h"
#include "ResourceEntity.h"
#include <errno.h>
#include "ResourceBlob.h"
#include "ImageUtil.h"

// CResourceDocument

IMPLEMENT_DYNAMIC(CResourceDocument, CDocument)

BEGIN_MESSAGE_MAP(CResourceDocument, CDocument)
	ON_COMMAND(ID_FILE_SAVE, OnFileSave)
    ON_COMMAND(ID_FILE_SAVE_AS, OnFileSaveAs)
    ON_COMMAND(ID_FILE_EXPORTASRESOURCE, OnExportAsResource)
    ON_UPDATE_COMMAND_UI(ID_FILE_SAVE, OnUpdateFileSave)
    ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_AS, OnUpdateAlwaysOn)
    ON_UPDATE_COMMAND_UI(ID_FILE_EXPORTASRESOURCE, OnUpdateAlwaysOn)
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_RESSIZE, OnUpdateResSize)
END_MESSAGE_MAP()

BOOL CResourceDocument::CanCloseFrame(CFrameWnd* pFrameArg)
{
	// Override this, since we may have more than one view in our childframes,
    // and if we do, the default implementation thinks there is still always
    // still one view around on the document (so it won't call SaveModified).
	return SaveModified();
}

void CResourceDocument::OnFileSave()
{
    if (IsModified())
    {
        DoPreResourceSave(FALSE);
    }
}

void CResourceDocument::OnFileSaveAs()
{
    DoPreResourceSave(TRUE);
}


// return TRUE if ok to continue
BOOL CResourceDocument::SaveModified()
{
	if (!IsModified())
		return TRUE;        // ok to continue

	CString prompt;
	AfxFormatString1(prompt, AFX_IDP_ASK_TO_SAVE, this->GetTitle());
	switch (AfxMessageBox(prompt, MB_YESNOCANCEL, AFX_IDP_ASK_TO_SAVE))
	{
	case IDCANCEL:
		return FALSE;       // don't continue

	case IDYES:
		// If so, either Save or Update, as appropriate
		if (!DoPreResourceSave(FALSE))
			return FALSE;       // don't continue
		break;

	case IDNO:
		// If not saving changes, revert the document
		break;

	default:
		ASSERT(FALSE);
		break;
	}
	return TRUE;    // keep going
}

void CResourceDocument::_UpdateTitle()
{
    TCHAR sz[20];
    const ResourceEntity *pResource = GetResource();
    if (pResource && (pResource->ResourceNumber != -1))
    {
        StringCchPrintf(sz, ARRAYSIZE(sz), TEXT("%s.%03d"), _GetTitleDefault(), pResource->ResourceNumber);
        // TODO: Use game.ini name for the title.
    }
    else
    {
        StringCchPrintf(sz, ARRAYSIZE(sz), TEXT("%s - new"), _GetTitleDefault());
    }

    // Set the title:
    SetTitle(sz);
    // Also update recency
    _fMostRecent = (appState->_resourceRecency.IsResourceMostRecent(GetResourceDescriptor()) != FALSE);
}

//
// Actually go ahead and serialize the resource, just to give an indication of its size!
//
void CResourceDocument::OnUpdateResSize(CCmdUI *pCmdUI)
{
    if (_needsResourceSizeUpdate)
    {
        TCHAR szBuf[MAX_PATH];
        StringCchCopy(szBuf, ARRAYSIZE(szBuf), TEXT("Unknown size"));
        const ResourceEntity *pResource = GetResource();
        if (pResource && pResource->CanWrite())
        {
            sci::ostream serial;
            try
            {
                pResource->WriteToTest(serial, false, pResource->ResourceNumber);
                StringCchPrintf(szBuf, ARRAYSIZE(szBuf), TEXT("%s: %d bytes"), _GetTitleDefault(), serial.tellp());
            }
            catch (std::exception)
            {
                StringCchPrintf(szBuf, ARRAYSIZE(szBuf), TEXT("unknown size"));
            }
            _needsResourceSizeUpdate = false;
        }
        _resSize = szBuf;
    }
    pCmdUI->SetText(_resSize.c_str());
}

void _ShowCantSaveMessage()
{
    AfxMessageBox("Saving this kind of resource is not supported.", MB_ERRORFLAGS);
}

BOOL CResourceDocument::DoPreResourceSave(BOOL fSaveAs)
{
    BOOL fRet = FALSE;
    if (appState->GetResourceMap().IsGameLoaded())
    {
        if (!appState->GetResourceMap().CanSaveResourcesToMap())
        {
            AfxMessageBox("Can't save to the version of the resource.map used by this game.", MB_ERRORFLAGS);
        }
        else
        {
            if (v_DoPreResourceSave())
            {
                // const_cast: special case, because we're saving... we're modifying the resource only
                // by giving it a resource package/number
                ResourceEntity *pResource = const_cast<ResourceEntity*>(GetResource());
                if (pResource)
                {
                    if (!pResource->CanWrite())
                    {
                        _ShowCantSaveMessage();
                    }
                    else
                    {
                        bool fCancelled = false;

                        // Make sure we have a package and resource number.
                        auto resource_location = pResource->GetResourceLocation();
                        std::string name;

                        if (fSaveAs || (resource_location.GetNumber() == -1) || (resource_location.GetPackageHint() == -1))
                        {
                            if (resource_location.GetNumber() == -1)
                            {
                                resource_location = resource_location.WithNumber(appState->GetResourceMap().SuggestResourceNumber(resource_location.GetType()));
                            }
                            // Invoke dialog.
                            SaveResourceDialog srd(true, pResource->GetType());
                            srd.Init(resource_location.GetPackageHint(), resource_location.GetNumber());
                            if (IDOK == srd.DoModal())
                            {
                                resource_location = resource_location.WithNumber(srd.GetResourceNumber()).WithPackageHint(srd.GetPackageNumber());
                                name = srd.GetName();
                            }
                            else
                            {
                                fCancelled = true;
                            }
                        }

                        if (!fCancelled && (resource_location.GetNumber() != -1) && (resource_location.GetPackageHint() != -1))
                        {
                            // We're good to go.
                            fRet = _DoResourceSave(resource_location, name);
                            if (fRet)
                            {
                                // We might have a new resource number, so update our title.
                                _UpdateTitle();
                            }
                        }
                    }
                }
            }
        }
    }
    else
    {
        // No game is loaded!  Assume the user wants to export.
        OnExportAsResource();
    }
    return fRet;
}

BOOL CResourceDocument::_DoResourceSave(const ResourceLocation& resource_location, const std::string &name)
{
    // Ignore path name.
    const ResourceEntity *pResource = static_cast<const ResourceEntity *>(GetResource());
    bool saved = false;
    int checksum = 0;
    if (pResource)
    {
        saved = appState->GetResourceMap().AppendResource(*pResource, resource_location, name, &checksum);
    }

    if (saved)
    {
        // If we successfully saved, make sure our resource has these
        // possibly new package/resource numbers. Need to do this prior to _OnSuccessfulSave!
        ResourceEntity *pEntityNC = const_cast<ResourceEntity*>(pResource);
        pEntityNC->PackageNumber = resource_location.GetPackageHint();
        pEntityNC->ResourceNumber = resource_location.GetNumber();

        _checksum = checksum;
        _OnSuccessfulSave(pResource);
        _UpdateTitle();
    }
    SetModifiedFlag(!saved);
    return saved;
}

void CResourceDocument::OnExportAsBitmap()
{
    const ResourceEntity *pResource = GetResource();
    if (pResource)
    {
        ExportResourceAsBitmap(*pResource);
    }
}

void ExportResourceAsBitmap(const ResourceEntity &resourceEntity)
{
    if (!resourceEntity.CanWrite())
    {
        return;
    }

    sci::ostream serial;
    resourceEntity.WriteToTest(serial, false, resourceEntity.ResourceNumber);
    ResourceBlob data;
    // Bring up the file dialog
    auto resource_location = resourceEntity.GetResourceLocation();
    if (resource_location.GetNumber() == -1)
    {
        resource_location = resource_location.WithNumber(0);
    }

    sci::istream readStream = istream_from_ostream(serial);

    auto resourceName = appState->GetResourceMap().Helper().FigureOutName(resource_location.GetResourceId());
    data.CreateFromBits(resourceName, resource_location, &readStream, appState->GetVersion(), ResourceSourceFlags::PatchFile);
    CBitmap bitmap;
    SCIBitmapInfo bmi;
    BYTE *pBitsDest;
    bitmap.Attach(CreateBitmapFromResource(resourceEntity, nullptr, &bmi, &pBitsDest));
    if ((HBITMAP)bitmap)
    {
        // Now let's encode it...
        if (EncodeResourceInBitmap(data, bmi, pBitsDest))
        {
            // Get a file name
            std::string name = data.GetName();
            name += ".bmp";
            CFileDialog fileDialog(FALSE, nullptr, name.c_str(), OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR, "Bitmaps (*.bitmap)|*.bitmap|All files (*.*)|*.*|");
            if (IDOK == fileDialog.DoModal())
            {
				std::string fileName = (PCSTR)fileDialog.GetPathName();
                if (!Save8BitBmp(fileName, bmi, pBitsDest, SCIResourceBitmapMarker))
                {
                    AfxMessageBox("Unable to save bitmap.", MB_ERRORFLAGS);
                }
            }
        }
        else
        {
            AfxMessageBox("Unable to put resource in bitmap.", MB_ERRORFLAGS);
        }
    }
}

void CResourceDocument::OnExportAsResource()
{
    // Ignore path name.
    const ResourceEntity *pResource = GetResource();
    if (pResource)
    {
        if (!pResource->CanWrite())
        {
            _ShowCantSaveMessage();
        }
        else
        {
            sci::ostream serial;
            bool fSaved = false;
            pResource->WriteToTest(serial, false, pResource->ResourceNumber);
            // Bring up the file dialog
            auto resource_location = pResource->GetResourceLocation();
            if (resource_location.GetNumber() == -1)
            {
                resource_location = resource_location.WithNumber(0);
            }

            std::string filename = GetFileNameFor(resource_location.GetResourceId(), appState->GetVersion());
            std::string filter = _GetFileDialogFilter();
            CFileDialog fileDialog(FALSE, nullptr, filename.c_str(), OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR, filter.c_str());
            if (IDOK == fileDialog.DoModal())
            {
                CString strFileName = fileDialog.GetPathName();
                ResourceBlob data;
                auto new_resource_location = resource_location.WithBase36(std::nullopt);
                auto resourceName = appState->GetResourceMap().Helper().FigureOutName(new_resource_location.GetResourceId());
                sci::istream readStream = istream_from_ostream(serial);
                if (SUCCEEDED(data.CreateFromBits(resourceName, new_resource_location, &readStream, appState->GetVersion(), ResourceSourceFlags::PatchFile)))
                {
                    HRESULT hr = data.SaveToFile((PCSTR)strFileName);
                    if (FAILED(hr))
                    {
                        DisplayFileError(hr, FALSE, strFileName);
                    }
                    fSaved = SUCCEEDED(hr);
                }
            }
            if (fSaved)
            {
                SetModifiedFlag(FALSE);
            }
            // else if we didn't save, we don't want to touch the modified flag.
            // The document may not have been dirty at all.
        }
    }
}

bool CResourceDocument::IsMostRecent() const
{
    return _fMostRecent;
}

void CResourceDocument::SetModifiedFlag(BOOL bModified)
{
    _needsResourceSizeUpdate = true;
    __super::SetModifiedFlag(bModified);
}

ResourceDescriptor CResourceDocument::GetResourceDescriptor() const
{
    int resource_number;
    uint32_t resource_base36;
    int package_number;

    if (const auto* pRes = GetResource())
    {
        resource_number = pRes->ResourceNumber;
        resource_base36 = pRes->Base36Number;
        package_number = pRes->PackageNumber;
    }
    else
    {
        resource_number = -1;
        resource_base36 = NoBase36;
        package_number = -1;
    }

    auto resource_num = ResourceNum::CreateWithBase36(resource_number, resource_base36);
    auto resource_id = ResourceId(_GetType(), resource_num);
    auto resource_location = ResourceLocation(package_number, resource_id);
    return ResourceDescriptor(resource_location, _checksum);
}

std::string CResourceDocument::_GetFileDialogFilter()
{
    return GetFileDialogFilterFor(_GetType(), appState->GetVersion());
}


// CResourceDocument commands
