#include "stdafx.h"
#include "AppState.h"
#include "Vocab99x.h"
#include "ResourceMap.h"
#include "CompiledScript.h"
#include "format.h"
#include "GameFolderHelper.h"

const static int VocabClassTable = 996;
const static int VocabSelectorNames = 997;
const static int VocabKernelNames = 999;

using namespace std;

ResourceBlob *_GetVocabData(const GameFolderHelper &helper, int iVocab)
{
    return helper.MostRecentResource(ResourceType::Vocab, iVocab, ResourceEnumFlags::None).release();
}

// Default kernal name table, taken from ScummVM source.
// We use vocab999, and if that's not present, then we use this.
static const char *const s_defaultKernelNames[] = {
    /*0x00*/ "Load",
    /*0x01*/ "UnLoad",
    /*0x02*/ "ScriptID",
    /*0x03*/ "DisposeScript",
    /*0x04*/ "Clone",
    /*0x05*/ "DisposeClone",
    /*0x06*/ "IsObject",
    /*0x07*/ "RespondsTo",
    /*0x08*/ "DrawPic",
    /*0x09*/ "Show",
    /*0x0a*/ "PicNotValid",
    /*0x0b*/ "Animate",
    /*0x0c*/ "SetNowSeen",
    /*0x0d*/ "NumLoops",
    /*0x0e*/ "NumCels",
    /*0x0f*/ "CelWide",
    /*0x10*/ "CelHigh",
    /*0x11*/ "DrawCel",
    /*0x12*/ "AddToPic",
    /*0x13*/ "NewWindow",
    /*0x14*/ "GetPort",
    /*0x15*/ "SetPort",
    /*0x16*/ "DisposeWindow",
    /*0x17*/ "DrawControl",
    /*0x18*/ "HiliteControl",
    /*0x19*/ "EditControl",
    /*0x1a*/ "TextSize",
    /*0x1b*/ "Display",
    /*0x1c*/ "GetEvent",
    /*0x1d*/ "GlobalToLocal",
    /*0x1e*/ "LocalToGlobal",
    /*0x1f*/ "MapKeyToDir",
    /*0x20*/ "DrawMenuBar",
    /*0x21*/ "MenuSelect",
    /*0x22*/ "AddMenu",
    /*0x23*/ "DrawStatus",
    /*0x24*/ "Parse",
    /*0x25*/ "Said",
    /*0x26*/ "SetSynonyms",	// Portrait (KQ6 hires)
    /*0x27*/ "HaveMouse",
    /*0x28*/ "SetCursor",
    // FOpen (SCI0)
    // FPuts (SCI0)
    // FGets (SCI0)
    // FClose (SCI0)
    /*0x29*/ "SaveGame",
    /*0x2a*/ "RestoreGame",
    /*0x2b*/ "RestartGame",
    /*0x2c*/ "GameIsRestarting",
    /*0x2d*/ "DoSound",
    /*0x2e*/ "NewList",
    /*0x2f*/ "DisposeList",
    /*0x30*/ "NewNode",
    /*0x31*/ "FirstNode",
    /*0x32*/ "LastNode",
    /*0x33*/ "EmptyList",
    /*0x34*/ "NextNode",
    /*0x35*/ "PrevNode",
    /*0x36*/ "NodeValue",
    /*0x37*/ "AddAfter",
    /*0x38*/ "AddToFront",
    /*0x39*/ "AddToEnd",
    /*0x3a*/ "FindKey",
    /*0x3b*/ "DeleteKey",
    /*0x3c*/ "Random",
    /*0x3d*/ "Abs",
    /*0x3e*/ "Sqrt",
    /*0x3f*/ "GetAngle",
    /*0x40*/ "GetDistance",
    /*0x41*/ "Wait",
    /*0x42*/ "GetTime",
    /*0x43*/ "StrEnd",
    /*0x44*/ "StrCat",
    /*0x45*/ "StrCmp",
    /*0x46*/ "StrLen",
    /*0x47*/ "StrCpy",
    /*0x48*/ "Format",
    /*0x49*/ "GetFarText",
    /*0x4a*/ "ReadNumber",
    /*0x4b*/ "BaseSetter",
    /*0x4c*/ "DirLoop",
    /*0x4d*/ "CanBeHere",       // CantBeHere in newer SCI versions
    /*0x4e*/ "OnControl",
    /*0x4f*/ "InitBresen",
    /*0x50*/ "DoBresen",
    /*0x51*/ "Platform",        // DoAvoider (SCI0)
    /*0x52*/ "SetJump",
    /*0x53*/ "SetDebug",        // for debugging
    /*0x54*/ "InspectObj",      // for debugging
    /*0x55*/ "ShowSends",       // for debugging
    /*0x56*/ "ShowObjs",        // for debugging
    /*0x57*/ "ShowFree",        // for debugging
    /*0x58*/ "MemoryInfo",
    /*0x59*/ "StackUsage",      // for debugging
    /*0x5a*/ "Profiler",        // for debugging
    /*0x5b*/ "GetMenu",
    /*0x5c*/ "SetMenu",
    /*0x5d*/ "GetSaveFiles",
    /*0x5e*/ "GetCWD",
    /*0x5f*/ "CheckFreeSpace",
    /*0x60*/ "ValidPath",
    /*0x61*/ "CoordPri",
    /*0x62*/ "StrAt",
    /*0x63*/ "DeviceInfo",
    /*0x64*/ "GetSaveDir",
    /*0x65*/ "CheckSaveGame",
    /*0x66*/ "ShakeScreen",
    /*0x67*/ "FlushResources",
    /*0x68*/ "SinMult",
    /*0x69*/ "CosMult",
    /*0x6a*/ "SinDiv",
    /*0x6b*/ "CosDiv",
    /*0x6c*/ "Graph",
    /*0x6d*/ "Joystick",
    // End of kernel function table for SCI0
    /*0x6e*/ "ShiftScreen",     // never called?
    /*0x6f*/ "Palette",
    /*0x70*/ "MemorySegment",
    ///*0x71*/ "Intersections",	// MoveCursor (SCI1 late), PalVary (SCI1.1)
    /*0x71*/ "PalVary",	        // MoveCursor (SCI1 late), PalVary (SCI1.1)
    /*0x72*/ "Memory",
    /*0x73*/ "ListOps",         // never called?
    /*0x74*/ "FileIO",
    /*0x75*/ "DoAudio",
    /*0x76*/ "DoSync",
    /*0x77*/ "AvoidPath",
    /*0x78*/ "Sort",            // StrSplit (SCI01)
    /*0x79*/ "ATan",            // never called?
    /*0x7a*/ "Lock",
    /*0x7b*/ "StrSplit",
    /*0x7c*/ "Message",         // GetMessage (before SCI1.1)
    /*0x7d*/ "IsItSkip",
    /*0x7e*/ "MergePoly",
    /*0x7f*/ "ResCheck",
    /*0x80*/ "AssertPalette",
    /*0x81*/ "TextColors",
    /*0x82*/ "TextFonts",
    /*0x83*/ "Record",          // for debugging
    /*0x84*/ "PlayBack",        // for debugging
    /*0x85*/ "ShowMovie",
    /*0x86*/ "SetVideoMode",
    /*0x87*/ "SetQuitStr",
    /*0x88*/ "DbugStr"          // for debugging
};


bool CVocabWithNames::_Create(sci::istream &byteStream, bool fTruncationOk)
{
    _names.clear();
    uint16_t wMaxIndex;
    byteStream >> wMaxIndex;

    // if the uint16_t read 400, it means there are 401 entries.
    // REVIEW - really? Someone is asking for kernel 113 (Joystick), but they get a garbage character because of this
    // I'm changing this back to <
    // REVIEW: Yes, really - if we leave this as <, then we'll miss the last entry in the selector names, for example.
    // Hmm - only true for some vocabs it seems.  For non essential cases, the caller can pass true
    // in, and we won't try for the extra item.
    if (!fTruncationOk)
    {
        wMaxIndex++;
    }

    // Perf: reserve this capacity so we don't need to resize.
    _names.reserve(wMaxIndex);

    for (uint16_t i = 0; byteStream.good() && i < wMaxIndex; i++)
    {
        uint16_t wOffset;
        byteStream >> wOffset;
        uint32_t dwSavePos = byteStream.tellg();
        byteStream.seekg(wOffset);
        if (byteStream.good())
        {
            std::string str;
            // Vocab files strings are run-length encoded.
            byteStream.getRLE(str);
            if (byteStream.good())
            {
                _names.push_back(str);
            }
        }
        byteStream.seekg(dwSavePos); // Go back
    }
    return byteStream.good();
}

uint16_t CVocabWithNames::Add(const string &str)
{
    // Assert that this isn't already in here.
    assert(find(_names.begin(), _names.end(), str) == _names.end());
    _names.push_back(str);
    _fDirty = true;
    return static_cast<uint16_t>(_names.size() - 1);
}

const char c_szBadSelector[] = "BAD SELECTOR";

bool SelectorTable::_Create(sci::istream &byteStream)
{
    _names.clear();
    _indices.clear();
    uint16_t wMaxIndex;
    byteStream >> wMaxIndex;

    uint16_t totalCount = wMaxIndex + 1;

    unordered_set<string> defaultSelectorNames = GetDefaultSelectorNames(_version);

    // Perf: reserve this capacity so we don't need to resize.
    _indices.reserve(totalCount);
    _names.reserve(800);    // Just some fairly large number.
    _firstInvalidSelector = 0xffff;
    uint16_t firstBadOffset = 0;
    for (uint16_t i = 0; byteStream.good() && i < totalCount; i++)
    {
        uint16_t wOffset;
        byteStream >> wOffset;
        if (wOffset == firstBadOffset)
        {
            _indices.push_back(-1);
        }
        else
        {
            uint32_t dwSavePos = byteStream.tellg();
            byteStream.seekg(wOffset);
            if (byteStream.good())
            {
                std::string str;
                // Vocab files strings are run-length encoded.
                byteStream.getRLE(str);
                if (byteStream.good())
                {
                    if (str == c_szBadSelector)
                    {
                        _firstInvalidSelector = _indices.size();
                        _indices.push_back(-1);
                        firstBadOffset = wOffset;
                    }
                    else
                    {
                        _indices.push_back(_names.size());
                        _names.push_back(str);
                    }

                    // Cache the default selector values... possibly expensive
                    if (defaultSelectorNames.find(str) != defaultSelectorNames.end())
                    {
                        _defaultSelectors.insert((uint16_t)(_indices.size() - 1));
                    }
                }
            }
            byteStream.seekg(dwSavePos); // Go back
        }
    }

    if (_firstInvalidSelector == 0xffff)
    {
        _firstInvalidSelector = _indices.size();
    }

    return byteStream.good();
}

std::vector<std::string> SelectorTable::GetNamesForDisplay() const
{
    std::vector<std::string> displayNames;
    displayNames.reserve(_names.size());
    int selIndex = 0;
    for (size_t i : _indices)
    {
        if (i != -1)
        {
            displayNames.push_back(fmt::format("{0} (0x{0:x}): {1}", selIndex, _names[i]));
        }
        selIndex++;
    }
    return displayNames;
}

std::string SelectorTable::_GetMissingName(uint16_t wName) const
{
    return fmt::format("sel_{0}", wName);
}

bool SelectorTable::ReverseLookup(std::string name, uint16_t &wIndex)
{
    if (_nameToValueCache.empty())
    {
        for (size_t i = 0; i < _indices.size(); i++)
        {
            if (_indices[i] != -1)
            {
                const string &name = _names[_indices[i]];
                _nameToValueCache[name] = (uint16_t)i;
            }
        }
    }

    auto it = _nameToValueCache.find(name);
    if (it != _nameToValueCache.end())
    {
        wIndex = it->second;
        return true;
    }
    return false;
}

bool SelectorTable::Load(const GameFolderHelper &helper)
{
    _version = helper.Version;
    bool fRet = false;
    unique_ptr<ResourceBlob> blob(_GetVocabData(helper, VocabSelectorNames));
    if (blob)
    {
        fRet = _Create(blob->GetReadStream());
    }
    if (!fRet)
    {
        appState->LogInfo("Failed to load selector names from vocab resource");
    }
    return fRet;
}

uint16_t SelectorTable::Add(const std::string &str)
{
    assert(find(_names.begin(), _names.end(), str) == _names.end());
    uint16_t selValue = (uint16_t)_firstInvalidSelector;
    int stringIndex = (int)_names.size();
    _names.push_back(str);
    // Add at the insert point.
    if (_firstInvalidSelector < _indices.size())
    {
        assert(_indices[_firstInvalidSelector] == -1);
        _indices[_firstInvalidSelector] = stringIndex;
        _firstInvalidSelector++;
        while ((_firstInvalidSelector < _indices.size()) && (_indices[_firstInvalidSelector] != -1))
        {
            _firstInvalidSelector++;
        }
    }
    else
    {
        _firstInvalidSelector++;
        _indices.push_back(stringIndex);
    }
    _fDirty = true;
    _nameToValueCache[str] = selValue;
    return selValue;
}

bool SelectorTable::IsDefaultSelector(uint16_t value)
{
    return _defaultSelectors.find(value) != _defaultSelectors.end();
}

void SelectorTable::Save()
{
    if (_fDirty)
    {
        // Save ourselves
        uint16_t cItems = (uint16_t)_indices.size();
        vector<BYTE> output;
        push_word(output, cItems - 1);  // This is total size minus one (max index)
        // Then come the offsets - we can run through the strings to calculate these.
        uint16_t wOffset = (uint16_t)output.size() + cItems * 2; // the strings will start after the offsets.
        uint16_t badSelOffset = 0xffff;
        bool needBAD_SELECTOR = _firstInvalidSelector < _indices.size();
        if (needBAD_SELECTOR)
        {
            badSelOffset = wOffset;
            wOffset += lstrlen(c_szBadSelector) + 2;
        }
        for (int index : _indices)
        {
            if (index != -1)
            {
                push_word(output, wOffset);
                wOffset += (uint16_t)(_names[index].length() + 2); // Increase by size of rle string.
            }
            else
            {
                assert(needBAD_SELECTOR);
                push_word(output, badSelOffset);
            }
        }

        // Now write the strings
        if (needBAD_SELECTOR)
        {
            push_string_rle(output, c_szBadSelector);
        }
        for (int index : _indices)
        {
            if (index != -1)
            {
                push_string_rle(output, _names[index]);
            }
        }
        // Now create a resource data for it and save it.
        appState->GetResourceMap().AppendResource(ResourceBlob(nullptr, ResourceType::Vocab, output, _version.DefaultVolumeFile, VocabSelectorNames, appState->GetVersion(), ResourceSourceFlags::ResourceMap));
    }
}

string SelectorTable::Lookup(uint16_t wName) const
{
    if (_version.HasOldSCI0ScriptHeader)
    {
        wName >>= 1;
    }
    std::string strRet;
    if ((size_t)wName < _indices.size())
    {
        if (_indices[wName] == -1)
        {
            strRet = c_szBadSelector;
        }
        else
        {
            strRet = _names[_indices[wName]];
        }
    }
    else
    {
        strRet = _GetMissingName(wName);
    }
    return strRet;
}

bool KernelTable::Load(const GameFolderHelper &helper)
{
    bool fRet = false;
    unique_ptr<ResourceBlob> blob(_GetVocabData(helper, VocabKernelNames));
    if (blob)
    {
        fRet = _Create(blob->GetReadStream());
        // KQ1 is stored differently... nullptr terminated strings.
        // if the normal kind of create fails, we will try that instead...
        if (!fRet)
        {
            appState->LogInfo("Failed to load kernel names from vocab resource - trying alternate KQ1-style method");
            sci::istream byteStream = blob->GetReadStream();
            while (byteStream.good())
            {
                std::string kernelName;
                byteStream >> kernelName;
                if (byteStream.good())
                {
                    Add(kernelName);
                }
            }
            fRet = true;
        }
    }
    else
    {
        // Kernel names not present. Use the hardcoded list (this is the case in later SCI versions)
        assert(appState->GetVersion().MapFormat != ResourceMapFormat::SCI0); // Shouldn't happen for SCI0
        for (size_t i = 0; i < ARRAYSIZE(s_defaultKernelNames); i++)
        {
            _names.push_back(s_defaultKernelNames[i]);
        }
        fRet = true;
    }
    return fRet;
}

bool GlobalClassTable::Load(const GameFolderHelper &helper)
{
    bool fRet = false;
    unique_ptr<ResourceBlob> blob(_GetVocabData(helper, VocabClassTable));
    if (blob)
    {
        fRet = _Create(blob->GetReadStream());
    }
    if (!fRet)
    {
        appState->LogInfo("Failed to load class table from vocab resource");
    }
    return fRet;
}

bool GlobalClassTable::_Create(sci::istream &byteStream)
{
    // Collect the heap/script pairs first, since fetching the heap individually for each script is a performance issue.
    // Patch files win out.
    unordered_map<uint16_t, pair<unique_ptr<ResourceBlob>, unique_ptr<ResourceBlob>>> heapScriptPairs;
    auto scriptContainer = appState->GetResourceMap().Resources(ResourceTypeFlags::Script | ResourceTypeFlags::Heap, ResourceEnumFlags::MostRecentOnly);
    for (auto &scriptResource : *scriptContainer)
    {
        uint16_t scriptNumber = (uint16_t)scriptResource->GetNumber();
        pair<unique_ptr<ResourceBlob>, unique_ptr<ResourceBlob>> &scriptAndHeap = heapScriptPairs[scriptNumber];
        if (scriptResource->GetType() == ResourceType::Script)
        {
            scriptAndHeap.first = move(scriptResource);
        }
        else
        {
            scriptAndHeap.second = move(scriptResource); // Heap
        }
    }

    for (auto &numberToPair : heapScriptPairs)
    {
        pair<unique_ptr<ResourceBlob>, unique_ptr<ResourceBlob>> &scriptAndHeap = numberToPair.second;
        if (!appState->GetVersion().SeparateHeapResources ||
            (scriptAndHeap.first && scriptAndHeap.second))      // Must have both script and heap if SCI1.1
        {
            int emptyNameClassIndex = 0;
            uint16_t scriptNumber = numberToPair.first;
            _scriptNums.push_back(scriptNumber);

            unique_ptr<CompiledScript> compiledScript = make_unique<CompiledScript>(scriptNumber);
            std::unique_ptr<sci::istream> heapStream;
            if (scriptAndHeap.second)
            {
                // Hmm, this can happen if a patch is just for .hep or just for .scr (e.g. SQ5, 10.hep)
                //assert((scriptAndHeap.first->GetSourceFlags() == scriptAndHeap.second->GetSourceFlags()) && "Heap and script files being mixed and matched (patch vs package)");

                // Only SCI1.1+ games have separate heap resources.
                heapStream.reset(new sci::istream(scriptAndHeap.second->GetData(), scriptAndHeap.second->GetLength()));
            }
            // Load the script.
            if (compiledScript->Load(appState->GetResourceMap().Helper(), appState->GetVersion(), scriptNumber, scriptAndHeap.first->GetReadStream(), heapStream.get()))
            {
                CompiledScript *pCompiledScriptWeak = compiledScript.get();
                _scripts.push_back(move(compiledScript));

                // Add the species in here
                for (auto &compiledObject : pCompiledScriptWeak->GetObjects())
                {
                    if (!compiledObject->IsInstance())
                    {
                        uint16_t species = compiledObject->GetSpecies();
                        _nameToSpecies[compiledObject->GetName()] = species;
                        _speciesToScriptNumber[species] = scriptNumber;
                        _speciesToCompiledObjectWeak[species] = compiledObject.get(); // Owned by _scripts
                    }
                }
            }
        }
    }
    return true; // We're done when we run out of stuff to read... it's not failure.
}

bool GlobalClassTable::LookupSpecies(const std::string &className, uint16_t &species)
{
    
    auto it = _nameToSpecies.find(className);
    bool success = (it != _nameToSpecies.end());
    if (success)
    {
        species = it->second;
    }
    return success;
}

std::string GlobalClassTable::Lookup(uint16_t wIndex) const
{
    string name;
    auto it = _speciesToCompiledObjectWeak.find(wIndex);
    if (it != _speciesToCompiledObjectWeak.end())
    {
        name = it->second->GetName();
    }
    return name;
}

std::vector<CompiledScript*> GlobalClassTable::GetAllScripts()
{
    std::vector<CompiledScript*> scripts;
    for (auto &script : _scripts)
    {
        scripts.push_back(script.get());
    }
    return scripts;
}

bool GlobalClassTable::GetSpeciesScriptNumber(uint16_t species, uint16_t &scriptNumber)
{
    scriptNumber = 0;
    auto it = _speciesToScriptNumber.find(species);
    if (it != _speciesToScriptNumber.end())
    {
        scriptNumber = it->second;
        return true;
    }
    return false;
}

bool GlobalClassTable::GetSpeciesPropertySelector(uint16_t wSpeciesIndex, std::vector<uint16_t> &props, std::vector<CompiledVarValue> &values)
{
    auto it = _speciesToCompiledObjectWeak.find(wSpeciesIndex);
    bool success = (it != _speciesToCompiledObjectWeak.end());
    if (success)
    {
        CompiledObjectBase *object = it->second;
        values = object->GetPropertyValues();
        props = object->GetProperties();
        assert(values.size() == props.size());
    }
    return success;
}

std::vector<uint16_t> GlobalClassTable::GetSubclassesOf(uint16_t baseClass)
{
    vector<uint16_t> subclasses;
    for (auto &speciesObjectPair : _speciesToCompiledObjectWeak)
    {
        std::vector<uint16_t> potentialChildChain;
        potentialChildChain.reserve(15);
        CompiledObjectBase *object = speciesObjectPair.second;
        potentialChildChain.push_back(object->GetSpecies());
        // Follow the chain up 
        while (object->GetSuperClass() != 0xffff)
        {
            if (object->GetSuperClass() == baseClass)
            {
                copy(potentialChildChain.begin(), potentialChildChain.end(), back_inserter(subclasses));
                break;
            }
            object = _speciesToCompiledObjectWeak[object->GetSuperClass()];
            potentialChildChain.push_back(object->GetSpecies());
        }
    }
    return subclasses;
}

bool SpeciesTable::Load(const GameFolderHelper &helper)
{
    bool fRet = false;
    unique_ptr<ResourceBlob> blob(_GetVocabData(helper, VocabClassTable));
    if (blob)
    {
        fRet = _Create(blob->GetReadStream());
    }
    if (!fRet)
    {
        appState->LogInfo("Failed to load class table from vocab resource");
    }
    return fRet;
}

void SpeciesTable::Save()
{
    if (_fDirty)
    {
        // Save ourselves
        vector<BYTE> output;
        output.reserve(4 * _direct.size());
        for (vector<uint16_t>::const_iterator speciesIt = _direct.begin(); speciesIt != _direct.end(); ++speciesIt)
        {
            push_word(output, 0);
            push_word(output, *speciesIt);
        }
        // Now create a resource data for it
        appState->GetResourceMap().AppendResource(ResourceBlob(nullptr, ResourceType::Vocab, output, appState->GetVersion().DefaultVolumeFile, VocabClassTable, appState->GetVersion(), ResourceSourceFlags::ResourceMap));
    }
}

void SpeciesTable::PurgeOldClasses(const GameFolderHelper &helper)
{
    vector<uint16_t> newTable;
    GlobalClassTable globalClassTable;
    if (globalClassTable.Load(helper))
    {
        unordered_map<int, CompiledScript*> scriptMap;
        std::vector<CompiledScript*> allScripts = globalClassTable.GetAllScripts();
        for (CompiledScript *script : allScripts)
        {
            scriptMap[script->GetScriptNumber()] = script;
        }
        int lastScript = -1;
        int currentIndexInScript = 0;
        for (int scriptNum : _direct)
        {
            if (scriptNum != lastScript)
            {
                currentIndexInScript = 0;
            }
            // Does this script still exist?
            auto it = scriptMap.find(scriptNum);
            if (it != scriptMap.end())
            {
                // Are there that many classes in it?
                int classCount = count_if(it->second->GetObjects().begin(), it->second->GetObjects().end(),
                    [](unique_ptr<CompiledObjectBase> &object) { return !object->IsInstance(); });
                if (currentIndexInScript < classCount)
                {
                    newTable.push_back(scriptNum);
                }
            }

            currentIndexInScript++;
        }

        // Save it!
        _fDirty = true;
        _direct = newTable;
        this->Save();
        // Then reload.
        _map.clear();
        _direct.clear();
        _wNewSpeciesIndex = 0;
        _fDirty = false;
        this->Load(helper);
    }
}

bool SpeciesTable::_Create(sci::istream &byteStream)
{
    _wNewSpeciesIndex = 0;
    while (byteStream.good())
    {
        uint16_t offset;
        byteStream >> offset;   // Don't know what this is.
        uint16_t wScriptNum;
        byteStream >> wScriptNum;
        if (byteStream.good())
        {
            _direct.push_back(wScriptNum);
            _map[wScriptNum].push_back(_wNewSpeciesIndex++);
        }
    }
    return true;
}

bool SpeciesTable::GetSpeciesIndex(uint16_t wScript, uint16_t wClassIndexInScript, SpeciesIndex &wSpeciesIndex) const
{
    bool fRet = false;
    uint16_t wClassIndex = 0;
    species_map::const_iterator script = _map.find(wScript);
    if (script != _map.end())
    {
        const vector<uint16_t> &classesInScript = (*script).second;
        if (wClassIndexInScript < (uint16_t)classesInScript.size())
        {
            fRet = true;
            wSpeciesIndex = classesInScript[wClassIndexInScript];
        }
    }
    return fRet;
}

bool SpeciesTable::GetSpeciesLocation(SpeciesIndex wSpeciesIndex, uint16_t &wScript, uint16_t &wClassIndexInScript) const
{
    uint16_t wType = wSpeciesIndex.Type();
    bool fRet = (wType < static_cast<uint16_t>(_direct.size()));
    if (fRet)
    {
        wScript = _direct[wType];
        const vector<uint16_t> &classesInScript = _map.at(wScript);
        for (size_t i = 0; i < classesInScript.size(); i++)
        {
            if (classesInScript[i] == wSpeciesIndex)
            {
                fRet = true;
                wClassIndexInScript = (uint16_t)i;
                break;
            }
        }
    }
    return fRet;
}

SpeciesIndex SpeciesTable::MaybeAddSpeciesIndex(uint16_t wScript, uint16_t wClassIndexInScript)
{
    SpeciesIndex wSpeciesIndex;
    if (GetSpeciesIndex(wScript, wClassIndexInScript, wSpeciesIndex))
    {
        return wSpeciesIndex;
    }
    else
    {
        // REVIEW: this code is not exception safe.
        // For adding classes in the middle of other classes.
        // REVIEW -> what we're doing here is totally busted.  script numbers do not need to be
        // sequential.  Look at what SCIStudio does - it adds it at the end.  The only thing is,
        // the class #'s change for classes within the current file only.  That could still
        // be catastrophic for some files for the type system.
        // If this script didn't exist in the map before, it should now.
        uint16_t wNewSpeciesIndex = _wNewSpeciesIndex;
        _map[wScript].push_back(wNewSpeciesIndex);

        // Ok, not sure what this garbage was for. Let's just add it
        _direct.push_back(wScript); // Easy peasy
        /*
        // We need to search for this script number in _direct.
        std::vector<uint16_t>::iterator it = find(_direct.begin(), _direct.end(), wScript);
        if (it == _direct.end())
        {
            // This is the first time this script appears in the species table.  Add it.
            _direct.push_back(wScript);
        }
        else
        {
            // Find the next one that isn't this script.
            std::vector<uint16_t>::iterator itEnd = find_if(it, _direct.end(), bind2nd(not_equal_to<uint16_t>(), wScript));
            // Now insert this script here.
            _direct.insert(itEnd, wScript);
        }*/

        _wNewSpeciesIndex++;
        assert(_wNewSpeciesIndex == (uint16_t)_direct.size());
        _fDirty = true;
        return wNewSpeciesIndex;
    }
}

// Textual form of all the script names.
std::vector<std::string> SpeciesTable::GetNames() const
{
    std::vector<std::string> names;
    for (size_t i = 0; i < _direct.size(); ++i)
    {
        std::stringstream stream;
        stream << "species " << static_cast<DWORD>(i) << ": script " << _direct[i];
        names.push_back(stream.str());
    }
    return names;
}

bool CVocabWithNames::ReverseLookup(std::string name, uint16_t &wIndex) const
{
    // PERF: use a more efficient container for looking up words
    std::vector<std::string>::const_iterator position = find(_names.begin(), _names.end(), name);
    bool fRet = (position != _names.end());
    if (fRet)
    {
        wIndex = (uint16_t)distance(_names.begin(), position);
    }
    return fRet;
}

std::string CVocabWithNames::Lookup(uint16_t wName) const
{
    std::string strRet;
    if ((size_t)wName < _names.size())
    {
        strRet = _names[wName];
    }
    else
    {
        strRet = _GetMissingName(wName);
    }
    return strRet;
}

std::string KernelTable::_GetMissingName(uint16_t wName) const
{
    char sz[100];
    StringCchPrintf(sz, ARRAYSIZE(sz), "kernel[%d]", wName);
    return sz;
}

//
// For some reason, SCI0 games are missing the "Joystick" kernel in the kernel
// vocab resource (or rather, the name is garbage characters). Fix that here.
//
const char szMissingKernel[] = "Joystick";
uint16_t wMissingKernel = 113;
std::string KernelTable::Lookup(uint16_t wName) const
{
    std::string result = __super::Lookup(wName);
    if (result.empty() && (wName == wMissingKernel))
    {
        result = szMissingKernel;
    }
    return result;
}
bool KernelTable::ReverseLookup(std::string name, uint16_t &wIndex) const
{
    if (name == szMissingKernel)
    {
        wIndex = wMissingKernel;
        return true;
    }
    else
    {
        return __super::ReverseLookup(name, wIndex);
    }
}


