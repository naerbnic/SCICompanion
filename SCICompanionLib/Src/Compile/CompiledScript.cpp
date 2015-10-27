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
#include "stdafx.h"
#include "CompiledScript.h"
#include "ScriptOM.h" // Just for SourceCodeWriter...
#include "format.h"
#include "GameFolderHelper.h"
#include "scii.h"
#include "ResourceContainer.h"
#include "ResourceEntity.h"

using namespace std;
using namespace sci;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Turn on/off decompiler, which is a work in progress.
#define DECOMPILE

//#define TEST_OFFSET 0x000c // Pretend the script was loaded here.
#define TEST_OFFSET 0

const uint16_t InfoClassFlag = 0x8000;
const uint16_t InfoCloneFlag = 0x0001;  // probably never used in the editor


std::string _GetProcNameFromScriptOffset(uint16_t wOffset)
{
    return fmt::format("localproc_{:04x}", wOffset);
}

bool CompiledScript::Load(const GameFolderHelper &helper, SCIVersion version, int iScriptNumber, bool quick)
{
    _version = version;
    _wScript = (uint16_t)iScriptNumber;

    std::unique_ptr<ResourceBlob> scriptResource = helper.MostRecentResource(ResourceType::Script, iScriptNumber, ResourceEnumFlags::None);
    if (scriptResource)
    {
        Load(helper, version, iScriptNumber, scriptResource->GetReadStream());
        return true;
    }
    return false;
}

bool CompiledScript::IsExportAnObject(uint16_t wOffset) const
{
    return find(_exportedObjectInstances.begin(), _exportedObjectInstances.end(), wOffset) != _exportedObjectInstances.end();
}
bool CompiledScript::IsExportAProcedure(uint16_t wOffset, int *exportIndex) const
{
    bool result = !IsExportAnObject(wOffset) && (wOffset != 0);
    if (result && exportIndex)
    {
        *exportIndex = find(_exportsTO.begin(), _exportsTO.end(), wOffset) - _exportsTO.begin();
        result = (*exportIndex) < (int)_exportsTO.size();
    }
    return result;
}

CompiledObjectBase *CompiledScript::GetObjectForExport(uint16_t exportPointer) const
{
    if (!_version.SeparateHeapResources)
    {
        exportPointer -= 8;
    }

    for (size_t objPosition = 0; objPosition < _objects.size(); objPosition++)
    {
        if (_objectsOffsetTO[objPosition] == exportPointer)
        {
            return _objects[objPosition].get();
        }
    }
    return nullptr;
}

bool CompiledScript::Load(const GameFolderHelper &helper, SCIVersion version, int number, sci::istream &byteStream, sci::istream *heapStream)
{
    _version = version;
    if (version.SeparateHeapResources)
    {
        return _LoadSCI1_1(helper, number, byteStream, heapStream);
    }
    else
    {
        return _LoadSCI0_SCI1(byteStream);
    }
}

// Exports are just a list of offsets. We'll peek into the heapstream to see if
// it points to an object marker, and return true if so. Otherwise, we return
// false and should assume the export just points to public procedure code.
bool _DoesExportPointToObjectInstanceSCI1_1(uint16_t exportOffset, sci::istream heapStream)
{
    if ((uint16_t)heapStream.GetDataSize() >= (exportOffset + 2))
    {
        heapStream.seekg(exportOffset);
        uint16_t magicMarker;
        heapStream >> magicMarker;
        return (magicMarker == 0x1234);
    }
    return false;
}

void CompiledScript::_LoadStringOffsetsSCI1_1(uint16_t offset, sci::istream heapStream)
{
    heapStream.seekg(offset);
    uint16_t count;
    heapStream >> count;
    for (uint16_t i = 0; i < count; i++)
    {
        uint16_t stringOffset;
        heapStream >> stringOffset;
        _stringPointerOffsetsSCI1_1.push_back(stringOffset);
    }
}

bool CompiledScript::IsStringPointerSCI1_1(uint16_t value) const
{
    return find(_stringPointerOffsetsSCI1_1.begin(), _stringPointerOffsetsSCI1_1.end(), value) != _stringPointerOffsetsSCI1_1.end();
}

std::string CompiledScript::GetStringFromOffset(uint16_t value) const
{
    string stringValue;
    size_t index = find(_stringsOffset.begin(), _stringsOffset.end(), value) - _stringsOffset.begin();
    if (index < _strings.size())
    {
        stringValue = _strings[index];
    }
    return stringValue;
}

void _ReadHeapPointerOffsets(sci::istream scriptStream, uint16_t heapPointerListOffset, unordered_set<uint16_t> &heapPointerList)
{
    scriptStream.seekg(heapPointerListOffset);
    uint16_t count;
    scriptStream >> count;
    for (int i = 0; i < count; i++)
    {
        uint16_t heapPointer;
        scriptStream >> heapPointer;
        heapPointerList.insert(heapPointer);
    }
}

bool CompiledScript::_LoadSCI1_1(const GameFolderHelper &helper, int iScriptNumber, sci::istream &scriptStream, sci::istream *heapStream)
{
    bool isSuccess = false;

    if (scriptStream.GetDataSize() > 0)
    {
        isSuccess = true;
        // First thing to do is to load the heap resource
        unique_ptr<ResourceBlob> heapBlob;
        unique_ptr<sci::istream> heapStreamScope;
        if (heapStream == nullptr)
        {
            heapBlob = helper.MostRecentResource(ResourceType::Heap, iScriptNumber, ResourceEnumFlags::None);
            if (heapBlob)
            {
                heapStreamScope = make_unique<sci::istream>(heapBlob->GetReadStream());
                heapStream = heapStreamScope.get();
            }
        }

        if (heapStream)
        {
            // Make a copy of everything
            _scriptResource.resize(scriptStream.GetDataSize());
            scriptStream.read_data(&_scriptResource[0], scriptStream.GetDataSize());
            // Then go back to the beginning
            scriptStream.seekg(0);

            uint16_t earliestMethodCodeOffset = 0xffff;

            // Now we have both a heap and a script stream.

            // Figure out which things in the script stream actually point to the heap.
            uint16_t heapPointerListOffset;
            scriptStream >> heapPointerListOffset;
            unordered_set<uint16_t> heapPointerList;
            _ReadHeapPointerOffsets(scriptStream, heapPointerListOffset, heapPointerList);

            scriptStream.skip(4);   // This might have a preload flag, not sure.

            // Now read the exports
            uint16_t wNumExports;
            scriptStream >> wNumExports;
            for (uint16_t i = 0; i < wNumExports; i++)
            {
                bool isHeapPointer = heapPointerList.find((uint16_t)scriptStream.tellg()) != heapPointerList.end();
                uint16_t exportOffset;
                scriptStream >> exportOffset;
                _exportsTO.push_back(exportOffset);
                if (isHeapPointer)
                {
                    _exportedObjectInstances.push_back(exportOffset);
                    assert(_DoesExportPointToObjectInstanceSCI1_1(exportOffset, *heapStream));
                }
                // REVIEW: Many scripts (e.g. SQ5, 165) have lots of exports that point to zero. What's the purpose of this?
                else if (exportOffset != 0)
                {
                    // Just raw code (procedure)
                    earliestMethodCodeOffset = min(exportOffset, earliestMethodCodeOffset);
                }
            }

            uint16_t addressAfterLastObject = (uint16_t)scriptStream.tellg();
            if (isSuccess)
            {
                // Local variables
                uint16_t stringPointerOffsetsOffset;
                (*heapStream) >> stringPointerOffsetsOffset;
                _LoadStringOffsetsSCI1_1(stringPointerOffsetsOffset, *heapStream);
                uint16_t localsCount;
                (*heapStream) >> localsCount;
                for (int i = 0; i < localsCount; i++)
                {
                    bool isObjectOrString = IsStringPointerSCI1_1((uint16_t)heapStream->tellg());
                    uint16_t w;
                    (*heapStream) >> w;
                    _localVars.push_back({ w, isObjectOrString });
                }

                // Now we're into the objects.
                uint16_t magic;
                int classIndex = 0;
                while (isSuccess && heapStream->peek(magic) && (magic == 0x1234))
                {
                    unique_ptr<CompiledObjectBase> pObject = make_unique<CompiledObjectBase>();
                    // Is the current position of the heapstream (which points to an object) in the list of public instance exports?
                    pObject->IsPublic = (find(_exportedObjectInstances.begin(), _exportedObjectInstances.end(), (uint16_t)heapStream->tellg()) != _exportedObjectInstances.end());
                    uint16_t wInstanceOffsetTO, endOfObjectInScript;
                    isSuccess = pObject->Create_SCI1_1(*this, _version, scriptStream, *heapStream, &wInstanceOffsetTO, classIndex, &endOfObjectInScript);
                    if (isSuccess)
                    {
                        addressAfterLastObject = max(endOfObjectInScript, addressAfterLastObject);

                        // Keep track of the earliest code
                        for (uint16_t codePointer : pObject->GetMethodCodePointersTO())
                        {
                            earliestMethodCodeOffset = min(codePointer, earliestMethodCodeOffset);
                        }

                        _objectsOffsetTO.push_back(wInstanceOffsetTO);
                        _objects.push_back(move(pObject));
                    }
                    classIndex++;
                }

                uint16_t terminator;
                (*heapStream) >> terminator;
                assert(terminator == 0x0000); // Just guessing.
                std::string aString;
                do
                {
                    uint16_t offset = (uint16_t)heapStream->tellg();
                    (*heapStream) >> aString;
                    // We DO add empty strings to the offsets. However, we may have a bogus empty
                    // one at the end, since afterStrings is WORD-aligned.
                    if (!aString.empty() || (heapStream->tellg() < stringPointerOffsetsOffset))
                    {
                        _stringsOffset.push_back(offset);
                        _strings.push_back(aString);
                    }
                } while (heapStream->tellg() < stringPointerOffsetsOffset);

            }

            // So far, I'm assuming everything is in one code section in the script resource.
            // We have two hints to where the code begins: The object methods and export (which have explicit
            // pointers to look at), and the end of the last object in the script resource.
            assert(addressAfterLastObject <= earliestMethodCodeOffset);
            if (addressAfterLastObject < heapPointerListOffset)
            {
                CodeSection all;
                all.begin = addressAfterLastObject;
                all.end = heapPointerListOffset;
                _codeSections.push_back(all);
            }
        }
    }
    return isSuccess;
}

bool CompiledScript::_LoadSCI0_SCI1(sci::istream &byteStream)
{
    bool fRet = byteStream.GetDataSize() > 0;
    if (fRet)
    {
        // Make a copy of everything
        _scriptResource.resize(byteStream.GetDataSize());
        byteStream.read_data(&_scriptResource[0], byteStream.GetDataSize());
        // Then go back to the beginning
        byteStream.seekg(0);

        _rawScriptSections.clear();

        if (_version.HasOldSCI0ScriptHeader)
        {
            // e.g. KQ4.
            // There's no local vars section. Instead, they are initialized in code, and the first
            // WORD of the script file says how many there are.
            uint16_t localVarsCount;
            byteStream >> localVarsCount;
            _localVars.assign(localVarsCount, { 0, false });
        }

        int classIndex = 0;
        while (fRet)
        {
            DWORD dwSavePos = byteStream.tellg();
            // Read the type and size.
            uint16_t wType;
            uint16_t wSectionSize;
            byteStream >> wType;
            if (wType != 0)
            {
                byteStream >> wSectionSize;
                fRet = byteStream.good() && (wSectionSize >= 4);
                if (fRet)
                {
                    ScriptSection scriptSection = { (uint16_t)dwSavePos, wType, wSectionSize };
                    _rawScriptSections.push_back(scriptSection);

                    switch (wType)
                    {
                    case 1:
                    {
                        // instance
                        unique_ptr<CompiledObjectBase> pObject = make_unique<CompiledObjectBase>();
                        uint16_t wInstanceOffsetTO;
                        fRet = pObject->Create_SCI0(this->_wScript, _version, byteStream, FALSE, &wInstanceOffsetTO, classIndex);
                        if (fRet)
                        {
                            _objectsOffsetTO.push_back(wInstanceOffsetTO);
                            _objects.push_back(move(pObject));
                        }
                        classIndex++;
                    }
                    break;

                    case 2:
                    {
                        uint16_t wCodeLength = wSectionSize - 4;
                        uint16_t wCodePosTO = static_cast<uint16_t>(byteStream.tellg());
                        byteStream.seekg(wCodeLength); // Skip this section.
                        CodeSection section = { wCodePosTO, wCodePosTO + wCodeLength };
                        _codeSections.push_back(section);
                    }
                    break;

                    case 3:
                    {
                        // Synonym word lists.
                        // uint16_t A - wordgroup,
                        // uint16_t B - wordgroup, replacement for a
                        uint16_t wNumPairs = ((wSectionSize - 4) / 4);
                        for (uint16_t i = 0; fRet && i < wNumPairs; i++)
                        {
                            uint16_t wA;
                            byteStream >> wA;
                            uint16_t wB;
                            byteStream >> wB;
                            _synonyms[wA] = wB;
                            fRet = byteStream.good();
                        }
                    }
                    break;

                    case 4:
                    {
                        // Said specs
                        fRet = _ReadSaids(byteStream, wSectionSize - 4);
                    }
                    break;

                    case 5:
                    {
                        // Strings
                        fRet = _ReadStrings(byteStream, wSectionSize - 4);
                    }
                    break;

                    case 6:
                    {
                        // class
                        unique_ptr<CompiledObjectBase> pObject = make_unique<CompiledObjectBase>();
                        uint16_t wClassOffset;
                        fRet = pObject->Create_SCI0(this->_wScript, _version, byteStream, TRUE, &wClassOffset, classIndex);
                        if (fRet)
                        {
                            _objectsOffsetTO.push_back(wClassOffset);
                            _objects.push_back(move(pObject));
                        }
                        classIndex++;
                    }
                    break;

                    case 7:
                    {
                        // Just what are the exports table?  Why are there entries in scripts that have no exports???
                        // Because they have a room, which is a public instance.  Unclear what this is supposed to point
                        // to though... it appears to point to the species selector.
                        // (also: magic entry point for script 0, entry 0, the play method.
                        if (!IsFlagSet(_flags, CompiledScriptFlags::DontLoadExports))
                        {
                            fRet = _ReadExports(byteStream);
                        }
                    }
                    break;

                    case 8:
                    {
                        // Relocation table.
                        // This is a list of script-relative pointers to pointers in the script.  These are how the
                        // interpreter tells what it needs to adjust when loading a script.
                        // This begins with a uint16_t that indicates how many pointers there are here.
                        // Brian's compiler seems to use these for the strings, but not for other things like Saids.
                        // Why would they be needed for strings?  It seems like they are only used for the name
                        // selectors?
                        // But the main script has 57 of these entries, but only 42 strings.  So clearly they are
                        // used for more than just strings.
                        // If I compile main, I get 34 offset strings (42 - 8 that aren't object names) and 23 offset properties,
                        // which were the said strings used in properties.
                        // So it seems like it's used for object names, and things that are property values (said, string and var)
                        // That means you can assign local variables to properties.  e.g.  "x @myLocalVar"
                        // (Doesn't work with globals though.)
                        // I guess everywhere else we can use relative offsets!
                    }
                    break;

                    case 9:
                    {
                        _fPreloadText = TRUE;
                    }
                    break;

                    case 10:
                    {
                        assert(!_version.HasOldSCI0ScriptHeader);
                        // Local variables.
                        // Just a series of WORDs.  The notion of arrays, etc... is not encoded in the resource.
                        // That's all just in source code.  Although arrays can kind of be infered from which ones
                        // are accessed in the script.
                        uint16_t wNumVars = ((wSectionSize - 4) / 2);
                        for (uint16_t i = 0; fRet && i < wNumVars; i++)
                        {
                            uint16_t w;
                            byteStream >> w;
                            fRet = byteStream.good();
                            _localVars.push_back({ w, false });
                        }
                    }
                    break;
                    default:
                        // Nada.
                        break;
                    }
                }
            }
            if (wType == 0)
            {
                break; // Done
            }
            if (fRet)
            {
                ASSERT(wSectionSize > 0); // else we'll never get anywhere.
                if (wSectionSize > 0)
                {
                    byteStream.seekg(dwSavePos + wSectionSize);
                    fRet = byteStream.good();
                }
            }
        }

        // Let's mark instances as public if they're in the exports list
        assert(_objectsOffsetTO.size() == _objects.size());
        for (size_t i = 0; i < _objectsOffsetTO.size(); i++)
        {
            uint16_t addToOffset = (this->_version.SeparateHeapResources ? 0 : 8);
            _objects[i]->IsPublic =
                (find(_exportsTO.begin(), _exportsTO.end(), _objectsOffsetTO[i] + addToOffset) != _exportsTO.end());
            if (_objects[i]->IsPublic)
            {
                _exportedObjectInstances.push_back(_objectsOffsetTO[i] + addToOffset);
            }
        }

    }
    return fRet;
}

// Pretend the code has been loaded at an address other than 0x0000
uint16_t _GetTestStreamPosition(sci::istream *pStream)
{
    return (uint16_t)(pStream->tellg()) + TEST_OFFSET;
}

std::string _GenerateClassName(uint16_t scriptNumber, int &index)
{
    return fmt::format("Class_{0}_{1}", scriptNumber, index++);
}

// Very important: scriptStream is passed by value. Heapstream is not.
bool CompiledObjectBase::Create_SCI1_1(const CompiledScript &compiledScript, SCIVersion version, sci::istream scriptStream, sci::istream &heapStream, uint16_t *pwOffset, int classIndex, uint16_t *endOfObjectInScript)
{
    uint16_t scriptNum = compiledScript.GetScriptNumber();
    *pwOffset = heapStream.tellg();
    _version = version;
    uint16_t wMagic, numVars, varOffset, methodsOffset;
    heapStream >> wMagic;
    assert(wMagic == 0x1234);
    heapStream >> numVars;
    heapStream >> varOffset;
    heapStream >> methodsOffset;

    // First get the property values (since these are present for both instances
    // and classes)
    uint16_t wName = 0;

    //heapStream.skip(2); // just made this up?
    // Let's verify:
    uint16_t mysteryValue;
    heapStream >> mysteryValue;
    assert(mysteryValue == 0);

    // Get the property selectors, which are only present for classes.
    if (!_fInstance)
    {
        scriptStream.seekg(varOffset);
        for (uint16_t i = 0; i < numVars; i++)
        {
            uint16_t propertySelector;
            scriptStream >> propertySelector;
            _propertySelectors.push_back(propertySelector);
        }
    }

    // Now get the property values
    for (uint16_t i = 0; i < numVars; i++)
    {
        uint16_t propertyValue;
        if (i >= 5)
        {
            bool isString = compiledScript.IsStringPointerSCI1_1((uint16_t)heapStream.tellg());
            heapStream >> propertyValue;
            _propertyValues.push_back({ propertyValue, isString });
        }
        else
        {
            // These are the 5 initial selectors, which are not represented in the values.
            _propertyValues.push_back({ 0, false });
        }

        switch (i)
        {
        case 5:
            _wSpeciesIfClass = _propertyValues[i].value;
            break;
        case 6:
            _wSuperClass = _propertyValues[i].value;
            break;
        case 7:
            _wInfo = _propertyValues[i].value;
            _fInstance = ((_wInfo & InfoClassFlag) == 0);
            break;
        case 8:
            // TODO: Known issue with SQ5, script 943 (and others). Class without a name, and we're assuming position #8 is the name property,
            // when it's actually x. We can't technically determine if this is name without knowing the super classes.
            wName = _propertyValues[i].value;
            break;
        }
    }

    // We need to read function selectors, and code
    // _functionSelectors, _functionOffsetsTO
    scriptStream.seekg(methodsOffset);
    uint16_t numMethods;
    scriptStream >> numMethods;
    for (uint16_t i = 0; i < numMethods; i++)
    {
        uint16_t methodSelector, methodOffset;
        scriptStream >> methodSelector;
        scriptStream >> methodOffset;
        _functionSelectors.push_back(methodSelector);
        _functionOffsetsTO.push_back(methodOffset);
    }

    // Get the name
    if (wName != 0)
    {
        // Don't modify heapstream, it's right where we need it.
        sci::istream temp = heapStream;
        temp.seekg(wName);
        temp >> _strName;
    }
    else
    {
        // Create a name for unnamed classes (e.g. Control)
        _strName = _GenerateClassName(scriptNum, classIndex);
    }

    *endOfObjectInScript = (uint16_t)scriptStream.tellg();

    assert((_propertySelectors.size() == _propertyValues.size()) || (_fInstance && _propertySelectors.empty()));
    return true;
}

bool CompiledObjectBase::Create_SCI0(uint16_t scriptNum, SCIVersion version, sci::istream &stream, BOOL fClass, uint16_t *pwOffset, int classIndex)
{
    _version = version;
    *pwOffset = static_cast<uint16_t>(stream.tellg());
    _fInstance = !fClass;
    uint16_t wMagic;
    stream >> wMagic; //  ASSERT(wMagic == 0x1234);
    if (wMagic != 0x1234)
    {
        return false; // We'll hit this when loading KQ4 for example, which uses a different format
    }
    if (stream.good())
    {
        _wPosInResource = static_cast<uint16_t>(stream.tellg());
        uint16_t wLocalVarOffset;
        stream >> wLocalVarOffset;
        uint16_t wFunctionSelectorOffset;
        stream >> wFunctionSelectorOffset;
        uint16_t wNumVarSelectors;
        stream >> wNumVarSelectors;
        ASSERT(wNumVarSelectors >= 3); // Really 4, but iceman only has 3
        if (stream.good())
        {
            uint16_t wNumVarValuesLeft = wNumVarSelectors;
            while (stream.good() && wNumVarValuesLeft)
            {
                uint16_t wValue;
                stream >> wValue;
                if (stream.good())
                {
                    _propertyValues.push_back({ wValue, false });
                }
                wNumVarValuesLeft--;
            }
        }
        uint16_t wName = 0;
        if (wNumVarSelectors >= 3)
        {
            _wSpeciesIfClass = _propertyValues[0].value;
            _wSuperClass = _propertyValues[1].value;
            _wInfo = _propertyValues[2].value;
            if (wNumVarSelectors >= 4)
            {
                wName = _propertyValues[3].value;
            }
        }

        // Now read their selector IDs - but only if this is a class.
        // Instances don't need this, since all the selector IDs are defined by its class.
        if (stream.good() && fClass)
        {
            // If this is a class, it's now followed by selector IDs, matching the selectors.
            for (INT_PTR i = 0; stream.good() && i < (INT_PTR)wNumVarSelectors; i++)
            {
                uint16_t wSelectorID;
                stream >> wSelectorID;
                if (stream.good())
                {
                    _propertySelectors.push_back(wSelectorID);
                }
            }
        }

        // Now their function selectors, for both instances and classes.
        uint16_t wNumFunctionSelectors = 0;
        if (stream.good())
        {
            // Read function selectors.
            stream >> wNumFunctionSelectors;
            uint16_t wNumFunctionSelectorsLeft = wNumFunctionSelectors;

            //
            // The spec here is incorrect.
            // http://freesci.linuxgames.com/scihtml/c2890.html#AEN2894
            // For classes, it describes the function code pointers coming before the selectors for the functions.
            // But the selectors come before for both classes and instances.
            //
            while(stream.good() && wNumFunctionSelectorsLeft)
            {
                uint16_t wSelectorID;
                stream >> wSelectorID;
                if (stream.good())
                {
                    _functionSelectors.push_back(wSelectorID);
                }
                wNumFunctionSelectorsLeft--;
            }
        }

        // Now the method code pointers.
        uint16_t wZero;
        stream >> wZero;
        if (stream.good())
        {
            ASSERT(wZero == 0); // There is supposed to be a zero here.
            while (stream.good() && wNumFunctionSelectors)
            {
                uint16_t wPtr;
                stream >> wPtr;
                if (stream.good())
                {
                    // These are supposed to be offsets to within the script resource, so they
                    // had better be smaller!
                    ASSERT(stream.GetDataSize() > wPtr);
                    _functionOffsetsTO.push_back(wPtr + TEST_OFFSET);
                }
                wNumFunctionSelectors--;
            }
        }

        if (stream.good() && (wName != 0))
        {
            // Retrieve the name of the object.  wName is a pointer.
            DWORD dwSavePos = stream.tellg();
            stream.seekg(wName);
            stream >> _strName;
            // Restore
            stream.seekg(dwSavePos);
        }
        else
        {
            // Create a name for unnamed classes (e.g. Control)
            _strName = _GenerateClassName(scriptNum, classIndex);
        }

        // The rest of the stuff we don't care about!
    }

    assert((_propertySelectors.size() == _propertyValues.size()) || (!fClass && _propertySelectors.empty()));

    return stream.good();
}


bool CompiledScript::_ReadExports(sci::istream &stream)
{
    uint16_t wNumExports;
    stream >> wNumExports;
    if (stream.good())
    {
        for (uint16_t i = 0; stream.good() && i < wNumExports; i++)
        {
            uint16_t offset;
            if (_version.IsExportWide())
            {
                uint32_t offsetWide;
                stream >> offsetWide;
                assert((offsetWide & 0xffff0000) == 0);
                offset = (uint16_t)offsetWide;
            }
            else
            {
                stream >> offset;
            }
            if (stream.good())
            {
                _exportsTO.push_back(offset + TEST_OFFSET);
            }
        }
    }
    return stream.good();
}

bool CompiledScript::_ReadSaids(sci::istream &stream, uint16_t wDataSize)
{
    // A single Said spec consists of wordgroups (0x0000-0x0fff) and BYTE tokens (0xf0-0xf9),
    // terminated by 0xff.  I don't think there is any count of them.  But there is a count of
    // the total length of all of them of course.
    // We pass the offset of this thing to callk Said.  This would be a program-counter-relative
    // offset.
    DWORD dwMaxPos = stream.tellg() + wDataSize;
    while (stream.good() && (stream.tellg() < (dwMaxPos - 1))) // -1 since we need at least a little more data to read...
    {
        // Store the actual position in the stream (this is how other parts of the script refer to it).
        uint16_t wBeginingOfSaid = static_cast<uint16_t>(stream.tellg());
        vector<uint16_t> saidSequence;
        bool fDone = false;
        do
        {
            BYTE b;
            stream >> b;
            //fRet = pStream->ReadByte(&b);
            if (stream.good())
            {
                fDone = (b == 0xff);
                if (!fDone)
                {
                    if (b >= 0xf0)
                    {
                        // It's a operator.
                        saidSequence.push_back((uint16_t)b);
                    }
                    else
                    {
                        // It's a word group.
                        BYTE b2;
                        stream >> b2;
                        //fRet = pStream->ReadByte(&b2);
                        if (stream.good())
                        {
                            saidSequence.push_back((((uint16_t)b) << 8) | b2);
                        }
                    }
                }
            }
        } while (stream.good() && !fDone);
        if (stream.good())
        {
            _saidsOffset.push_back(wBeginingOfSaid);
            _saids.push_back(saidSequence);
        }

    } 

    // Just return TRUE.  Some of the code generated by brian's compiler seems to have an addition byte
    // in the overall length, so 0xff doesn't correspond to end the of the data stream (presumably so it ends up on a uint16_t boundary)
    return true;
}

bool CompiledScript::_ReadStrings(sci::istream &stream, uint16_t wDataSize)
{
    // I have a feeling these may map to objects, which would be great for us.
    // Look up a string, and then its index corresponds to an object?
    DWORD dwMaxPos = stream.tellg() + wDataSize;
    while (stream.good() && (stream.tellg() < dwMaxPos))
    {
        string str;
        DWORD dwOffset = stream.tellg();
        stream >> str;
        if (stream.good())
        {
            ASSERT(dwOffset <= 0xffff);
            _stringsOffset.push_back(static_cast<uint16_t>(dwOffset));
            _strings.push_back(str);
        }
    }
    return stream.good();
}

uint8_t g_OpcodeArgByteCount[256];
bool g_OpArgsInitialized = false;

//
// Calculate the offset wRelOffset from wHere, where wHere is the current location
// of the last operand in an instruction.  1 or 2 will be added to it prior to
// calculating the offset.
//
uint16_t CalcOffset(uint16_t wOperandStart, uint16_t wRelOffset, bool bByte, BYTE bRawOpcode)
{
    // Move to the pc post instruction.
    uint16_t wResult = wOperandStart + g_OpcodeArgByteCount[bRawOpcode];
    if (bByte)
    {
        if (wRelOffset >= 0x0080)
        {
            // It was a negative number, expressed in a byte.
            // We need to "sign extend" this thing.
            wRelOffset |= 0xff00;
        }
        wResult += wRelOffset;
    }
    else
    {
        wResult += wRelOffset; // Should just work
    }
    return wResult;
}

void CompiledScript::PopulateSaidStrings(const ILookupNames *pWords) const
{
    // Get readable strings for them
    for (size_t i = 0; i < _saids.size(); i++)
    {
        _saidStrings.push_back(SaidSequenceToString(_saids[i], pWords));
    }
    assert(_saidStrings.size() == _saidsOffset.size());
}



//
// Scan all the code in the script, looking for call instructions
//
set<uint16_t> CompiledScript::FindInternalCallsTO() const
{
    if (!g_OpArgsInitialized)
    {
        for (int i = 0; i < 256; i++)
        {
            int argumentByteCount = scii::GetInstructionSize(_version, (uint8_t)i) - 1;
            g_OpcodeArgByteCount[i] = (uint8_t)argumentByteCount;
        }
        g_OpArgsInitialized = true;
    }


    set<uint16_t> wOffsets;
    for (size_t i = 0; i < _codeSections.size(); i++)
    {
        uint16_t wCurrentOffsetTO = _codeSections[i].begin;
        const BYTE *pCur = &_scriptResource[wCurrentOffsetTO];
        if (pCur)
        {
            const BYTE *pEnd = &_scriptResource[_codeSections[i].end];
            while (pCur < pEnd)
            {
                uint16_t wRelOffset;
                BYTE bRawOpcode = *pCur;
                bool bByte = (bRawOpcode & 1);
                pCur++;
                wCurrentOffsetTO++;
                if (static_cast<Opcode>((bRawOpcode >> 1)) == Opcode::CALL)
                {
                    // This is one. The first operand is a word or byte
                    wRelOffset = (bByte ? ((uint16_t)*pCur) : (uint16_t)*pCur + (((uint16_t)*(pCur + 1)) << 8));
                    uint16_t theOffset = CalcOffset(wCurrentOffsetTO, wRelOffset, bByte, bRawOpcode);
                    if (GetOperandTypes(this->_version, Opcode::CALL)[0] == otLABEL_P1)
                    {
                        theOffset++;
                    }
                    wOffsets.insert(wOffsets.end(), theOffset);
                }
                // Skip past to the next instruction
                pCur += g_OpcodeArgByteCount[bRawOpcode];
                wCurrentOffsetTO += g_OpcodeArgByteCount[bRawOpcode];
            }
        }
    }
    return wOffsets;
}

std::string CompiledObjectBase::LookupPropertyName(ICompiledScriptLookups *pLookup, uint16_t wPropertyIndex) const
{
    // PERF: vector copy that is used frequently.
    std::vector<uint16_t> propertySelectorList;
    if (!pLookup->LookupSpeciesPropertyList(GetSpecies(), propertySelectorList))
    {
        // We might be a "private" class.  So use our own list... (REVIEW: can't we always do this?)
        propertySelectorList = _propertySelectors;
    }
    //ASSERT((wPropertyIndex %2) == 0);
    // REVIEW: Leisure Suit Larry 3, room 22, hits this ASSERT. As does SQ5 script 201.
    wPropertyIndex /= 2;
    if (wPropertyIndex < propertySelectorList.size())
    {
        return pLookup->LookupSelectorName(propertySelectorList[wPropertyIndex]);
    }
    else
    {
        return "--UNKNOWN-PROP-NAME--";
    }
}



//
// Provide things like saids or internal strings.
//
bool CompiledScript::LookupObjectName(uint16_t wOffset, ObjectType &type, std::string &name) const
{
    // See if it's an internal string:
    vector<uint16_t>::const_iterator offsetIndex = find(_stringsOffset.begin(), _stringsOffset.end(), wOffset);
    if (offsetIndex != _stringsOffset.end())
    {
        type = ObjectTypeString;
        name = _strings[distance(_stringsOffset.begin(), offsetIndex)];
        return true;
    }
    else
    {
        // See if it's a said:
        offsetIndex = find(_saidsOffset.begin(), _saidsOffset.end(), wOffset);
        if (offsetIndex != _saidsOffset.end())
        {
            type = ObjectTypeSaid;
            name = _saidStrings[distance(_saidsOffset.begin(), offsetIndex)];
            return true;
        }
        else
        {
            // See if it's an object
            // REVIEW: for some reason the offsets in the script point 8 bytes into the object.
            // Like, to the 4th selector... weird.  So add 8.
            // Hmm... in SCI1, they do not though (they point to the 0x1234). Or have I populated the _objectOffetsTO incorrectly?
            uint16_t subtractFromOffset = (this->_version.SeparateHeapResources ? 0 : 8);
            offsetIndex = find(_objectsOffsetTO.begin(), _objectsOffsetTO.end(), wOffset - subtractFromOffset);
            if (offsetIndex != _objectsOffsetTO.end())
            {
                type = ObjectTypeClass;
                name = _objects[distance(_objectsOffsetTO.begin(), offsetIndex)]->GetName();
                return true;
            }
        }
    }

    return false;
}

CompiledObjectBase *CompiledScript::_FindObjectWithSpecies(uint16_t wIndex)
{
    for (auto &object : _objects)
    {
        if ((object->GetSpecies() == wIndex) && (!object->IsInstance()))
        {
            return object.get();
        }
    }
    return nullptr;
}

std::string CompiledScript::LookupClassName(uint16_t wIndex)
{
    std::string ret;
    // Look for a class with this species index.
    CompiledObjectBase *pObject = _FindObjectWithSpecies(wIndex);
    if (pObject)
    {
        ret = pObject->GetName();
    }
    return ret;
}

bool CompiledScript::LookupSpeciesPropertyList(uint16_t wIndex, std::vector<uint16_t> &props)
{
    bool fRet = false;
    CompiledObjectBase *pObject = _FindObjectWithSpecies(wIndex);
    if (pObject)
    {
        props = pObject->GetProperties();
        fRet = true;
    }
    return fRet;
}

bool CompiledScript::LookupSpeciesPropertyListAndValues(uint16_t wIndex, std::vector<uint16_t> &props, std::vector<CompiledVarValue> &values)
{
    bool fRet = false;
    CompiledObjectBase *pObject = _FindObjectWithSpecies(wIndex);
    if (pObject)
    {
        props = pObject->GetProperties();
        values = pObject->GetPropertyValues();
        fRet = true;
    }
    return fRet;
}

//
// GlobalCompiledScriptLookups
//
bool GlobalCompiledScriptLookups::Load(const GameFolderHelper &helper)
{
    bool selOk = _selectors.Load(helper);
    bool kernelOk = _kernels.Load(helper);
    bool classesOk = _classes.Load(helper);
    return selOk && kernelOk && classesOk;
}
std::string GlobalCompiledScriptLookups::LookupSelectorName(uint16_t wIndex)
{
    std::string str = _selectors.Lookup(wIndex);
    if (str.empty())
    {
        // It is legit (e.g. script 99 in SQ3) for there to be selectors that don't have a name
        // in the official selector list. "private" selectors for "private" classes.
        std::stringstream ss;
        ss << "selector" << wIndex;
        str = ss.str();
    }
    return str;
}
std::string GlobalCompiledScriptLookups::LookupKernelName(uint16_t wIndex)
{
    return _kernels.Lookup(wIndex);
}
std::string GlobalCompiledScriptLookups::LookupClassName(uint16_t wIndex)
{
    return _classes.Lookup(wIndex);
}
bool GlobalCompiledScriptLookups::LookupSpeciesPropertyList(uint16_t wIndex, std::vector<uint16_t> &props)
{
    std::vector<CompiledVarValue> values; // Throw away
    return _classes.GetSpeciesPropertySelector(wIndex, props, values);
}
bool GlobalCompiledScriptLookups::LookupSpeciesPropertyListAndValues(uint16_t wIndex, std::vector<uint16_t> &props, std::vector<CompiledVarValue> &values)
{
    return _classes.GetSpeciesPropertySelector(wIndex, props, values);
}


//
// ObjectFileScriptLookups
//
std::string ObjectFileScriptLookups::ReverseLookupGlobalVariableName(WORD wIndex)
{
    // Get a global in main.
    CSCOFile scoFile;
    if (_GetSCOFile(0, scoFile))
    {
        return scoFile.GetVariableName(wIndex);
    }
    else
    {
        return "";
    }
}

std::string ObjectFileScriptLookups::ReverseLookupPublicExportName(WORD wScript, WORD wIndex)
{
    CSCOFile scoFile;
    if (_GetSCOFile(wScript, scoFile))
    {
        return scoFile.GetExportName(wIndex);
    }
    else
    {
        return "";
    }
}

bool ObjectFileScriptLookups::_GetSCOFile(WORD wScript, CSCOFile &scoFile)
{
    unordered_map<WORD, CSCOFile>::iterator it = _mapScriptToObject.find(wScript);
    bool fRet = (it != _mapScriptToObject.end());
    if (!fRet)
    {
        fRet = _LoadSCOFile(wScript);
        ASSERT(!fRet || (_mapScriptToObject.find(wScript) != _mapScriptToObject.end()));
    }
    if (fRet)
    {
        scoFile = _mapScriptToObject[wScript];
    }
    return fRet;
}

bool ObjectFileScriptLookups::_LoadSCOFile(WORD wScript)
{
    bool fRet = false;
    // Need the script name.
    std::string filename = _helper.GetScriptObjectFileName(wScript);
    if (!filename.empty())
    {
        HANDLE hFile = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            try
            {
                sci::streamOwner streamOwner(hFile);
                CSCOFile scoFile;
                if (scoFile.Load(streamOwner.getReader()))
                {
                    _mapScriptToObject[wScript] = scoFile;
                    fRet = true;
                }
            }
            catch (std::exception e)
            {
                fRet = false;
            }
            CloseHandle(hFile);
        }
    }
    return fRet;
}

