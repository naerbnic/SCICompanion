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
#include "PMachine.h"

#include <array>
#include <cassert>
#include <string>
#include <unordered_set>
#include <absl/types/span.h>

#include "Version.h"

Opcode RawToOpcode(const SCIVersion &version, uint8_t rawOpcode)
{
    if (version.PackageFormat >= ResourcePackageFormat::SCI2)
    {
        switch (rawOpcode)
        {
            case 0x7d:
                return Opcode::Filename;
            case 0x7e:
                return Opcode::LineNumber;
        }
    }
    return (Opcode)(rawOpcode >> 1);
}

uint8_t OpcodeToRaw(const SCIVersion &version, Opcode opcode, bool wide)
{
    if (version.PackageFormat >= ResourcePackageFormat::SCI2)
    {
        switch (opcode)
        {
            case Opcode::Filename:
                return 0x7d;
            case Opcode::LineNumber:
                return 0x7e;
        }
    }
    return (((uint8_t)opcode) << 1) | (wide ? 0 : 1);
}

/******************************************************************************/
std::vector<OperandType> OpArgTypes_SCI0[TOTAL_OPCODES] = {
	/*bnot*/     {},
	/*add*/      {},
	/*sub*/      {},
	/*mul*/      {},
	/*div*/      {},
	/*mod*/      {},
	/*shr*/      {},
	/*shl*/      {},
	/*xor*/      {},
	/*and*/      {},
/*10*/
	/*or*/       {},
	/*neg*/      {},
	/*not*/      {},
	/*eq?*/      {},
	/*ne?*/      {},
	/*gt?*/      {},
	/*ge?*/      {},
	/*lt?*/      {},
	/*le?*/      {},
	/*ugt?*/     {},
/*20*/
	/*uge?*/     {},
	/*ult?*/     {},
	/*ule?*/     {},
	/*bt*/       {otLABEL},
	/*bnt*/      {otLABEL},
	/*jmp*/      {otLABEL},
	/*ldi*/      {otINT},
	/*push*/     {},
	/*pushi*/    {otINT},
	/*toss*/     {},
/*30*/
	/*dup*/      {},
	/*link*/     {otUINT},
    /*call*/     {otLABEL,otUINT8},
	/*callk*/    {otKERNEL,otUINT8},
	/*callb*/    {otPUBPROC,otUINT8},
	/*calle*/    {otUINT,otPUBPROC,otUINT8},
	/*ret*/      {},
	/*send*/     {otUINT8},
	/**/         {},
	/**/         {},
/*40*/
	/*class*/    {otCLASS},
	/**/         {},
    /*self*/     {otUINT8},
    /*super*/    {otCLASS, otUINT8},
	/*rest*/     {otPVAR},
	/*lea*/      {otUINT,otUINT},
	/*selfID*/   {},
	/**/         {},
	/*pprev*/    {},
	/*pToa*/     {otPROP},
/*50*/
	/*aTop*/     {otPROP},
	/*pTos*/     {otPROP},
	/*sTop*/     {otPROP},
	/*ipToa*/    {otPROP},
	/*dpToa*/    {otPROP},
	/*ipTos*/    {otPROP},
	/*dpTos*/    {otPROP},
	/*lofsa*/    {otOFFS},
	/*lofss*/    {otOFFS},
	/*push0*/    {},
/*60*/
	/*push1*/    {},
	/*push2*/    {},
	/*pushSelf*/ {},
    /**/         {},
	/*lag*/      {otVAR},
	/*lal*/      {otVAR},
	/*lat*/      {otVAR},
	/*lap*/      {otVAR},
	/*lsg*/      {otVAR},
	/*lsl*/      {otVAR},
/*70*/
	/*lst*/      {otVAR},
	/*lsp*/      {otVAR},
	/*lagi*/     {otVAR},
	/*lali*/     {otVAR},
	/*lati*/     {otVAR},
	/*lapi*/     {otVAR},
	/*lsgi*/     {otVAR},
	/*lsli*/     {otVAR},
	/*lsti*/     {otVAR},
	/*lspi*/     {otVAR},
/*80*/
	/*sag*/      {otVAR},
	/*sal*/      {otVAR},
	/*sat*/      {otVAR},
	/*sap*/      {otVAR},
	/*ssg*/      {otVAR},
	/*ssl*/      {otVAR},
	/*sst*/      {otVAR},
	/*ssp*/      {otVAR},
	/*sagi*/     {otVAR},
	/*sali*/     {otVAR},
/*90*/
	/*sati*/     {otVAR},
	/*sapi*/     {otVAR},
	/*ssgi*/     {otVAR},
	/*ssli*/     {otVAR},
	/*ssti*/     {otVAR},
	/*sspi*/     {otVAR},
	/*+ag*/      {otVAR},
	/*+al*/      {otVAR},
	/*+at*/      {otVAR},
	/*+ap*/      {otVAR},
/*100*/
	/*+sg*/      {otVAR},
	/*+sl*/      {otVAR},
	/*+st*/      {otVAR},
	/*+sp*/      {otVAR},
	/*+agi*/     {otVAR},
	/*+ali*/     {otVAR},
	/*+ati*/     {otVAR},
	/*+api*/     {otVAR},
	/*+sgi*/     {otVAR},
	/*+sli*/     {otVAR},
/*110*/
	/*+sti*/     {otVAR},
	/*+spi*/     {otVAR},
	/*-ag*/      {otVAR},
	/*-al*/      {otVAR},
	/*-at*/      {otVAR},
	/*-ap*/      {otVAR},
	/*-sg*/      {otVAR},
	/*-sl*/      {otVAR},
	/*-st*/      {otVAR},
	/*-sp*/      {otVAR},
/*120*/
	/*-agi*/     {otVAR},
	/*-ali*/     {otVAR},
	/*-ati*/     {otVAR},
	/*-api*/     {otVAR},
	/*-sgi*/     {otVAR},
	/*-sli*/     {otVAR},
	/*-sti*/     {otVAR},
	/*-spi*/     {otVAR},
};

std::vector<OperandType> OpArgTypes_SCI2[TOTAL_OPCODES] = {
    /*bnot*/{},
    /*add*/{},
    /*sub*/{},
    /*mul*/{},
    /*div*/{},
    /*mod*/{},
    /*shr*/{},
    /*shl*/{},
    /*xor*/{},
    /*and*/{},
    /*10*/
    /*or*/{},
    /*neg*/{},
    /*not*/{},
    /*eq?*/{},
    /*ne?*/{},
    /*gt?*/{},
    /*ge?*/{},
    /*lt?*/{},
    /*le?*/{},
    /*ugt?*/{},
    /*20*/
    /*uge?*/{},
    /*ult?*/{},
    /*ule?*/{},
    /*bt*/{otLABEL},
    /*bnt*/{otLABEL},
    /*jmp*/{otLABEL},
    /*ldi*/{otINT},
    /*push*/{},
    /*pushi*/{otINT},
    /*toss*/{},
    /*30*/
    /*dup*/{},
    /*link*/{ otUINT},
    /*call*/{ otLABEL, otUINT16},
    /*callk*/{ otKERNEL, otUINT16},
    /*callb*/{ otPUBPROC, otUINT16},
    /*calle*/{ otUINT, otPUBPROC, otUINT16 },
    /*ret*/{},
    /*send*/{ otUINT16},
    /**/{},
    /**/{},
    /*40*/
    /*class*/{ otCLASS},
    /**/{},
    /*self*/{ otUINT16},
    /*super*/{ otCLASS, otUINT16},
    /*rest*/{ otPVAR},
    /*lea*/{ otUINT, otUINT},
    /*selfID*/{},
    /**/{},
    /*pprev*/{},
    /*pToa*/{ otPROP},
    /*50*/
    /*aTop*/{ otPROP},
    /*pTos*/{ otPROP},
    /*sTop*/{ otPROP},
    /*ipToa*/{ otPROP},
    /*dpToa*/{ otPROP},
    /*ipTos*/{ otPROP},
    /*dpTos*/{ otPROP},
    /*lofsa*/{ otOFFS},
    /*lofss*/{ otOFFS},
    /*push0*/{},
    /*60*/
    /*push1*/{},
    /*push2*/{},
    /*pushSelf*/{},
    /**/{},
    /*lag*/{ otVAR},
    /*lal*/{ otVAR},
    /*lat*/{ otVAR},
    /*lap*/{ otVAR},
    /*lsg*/{ otVAR},
    /*lsl*/{ otVAR},
    /*70*/
    /*lst*/{ otVAR},
    /*lsp*/{ otVAR},
    /*lagi*/{ otVAR},
    /*lali*/{ otVAR},
    /*lati*/{ otVAR},
    /*lapi*/{ otVAR},
    /*lsgi*/{ otVAR},
    /*lsli*/{ otVAR},
    /*lsti*/{ otVAR},
    /*lspi*/{ otVAR},
    /*80*/
    /*sag*/{ otVAR},
    /*sal*/{ otVAR},
    /*sat*/{ otVAR},
    /*sap*/{ otVAR},
    /*ssg*/{ otVAR},
    /*ssl*/{ otVAR},
    /*sst*/{ otVAR},
    /*ssp*/{ otVAR},
    /*sagi*/{ otVAR},
    /*sali*/{ otVAR},
    /*90*/
    /*sati*/{ otVAR},
    /*sapi*/{ otVAR},
    /*ssgi*/{ otVAR},
    /*ssli*/{ otVAR},
    /*ssti*/{ otVAR},
    /*sspi*/{ otVAR},
    /*+ag*/{ otVAR},
    /*+al*/{ otVAR},
    /*+at*/{ otVAR},
    /*+ap*/{ otVAR},
    /*100*/
    /*+sg*/{ otVAR},
    /*+sl*/{ otVAR},
    /*+st*/{ otVAR},
    /*+sp*/{ otVAR},
    /*+agi*/{ otVAR},
    /*+ali*/{ otVAR},
    /*+ati*/{ otVAR},
    /*+api*/{ otVAR},
    /*+sgi*/{ otVAR},
    /*+sli*/{ otVAR},
    /*110*/
    /*+sti*/{ otVAR},
    /*+spi*/{ otVAR},
    /*-ag*/{ otVAR},
    /*-al*/{ otVAR},
    /*-at*/{ otVAR},
    /*-ap*/{ otVAR},
    /*-sg*/{ otVAR},
    /*-sl*/{ otVAR},
    /*-st*/{ otVAR},
    /*-sp*/{ otVAR},
    /*120*/
    /*-agi*/{ otVAR},
    /*-ali*/{ otVAR},
    /*-ati*/{ otVAR},
    /*-api*/{ otVAR},
    /*-sgi*/{ otVAR},
    /*-sli*/{ otVAR},
    /*-sti*/{ otVAR},
    /*-spi*/{ otVAR},
};

OperandType filenameOperands[3] = { otDEBUGSTRING};
OperandType lineNumberOperands[3] = { otUINT16};

absl::Span<const OperandType> GetOperandTypes(const SCIVersion &version, Opcode opcode)
{
    switch (opcode)
    {
    case Opcode::Filename:
        return filenameOperands;
    case Opcode::LineNumber:
        return lineNumberOperands;
    default:
        break;
    }

    if (version.PackageFormat == ResourcePackageFormat::SCI2)
    {
    
        return OpArgTypes_SCI2[static_cast<uint8_t>(opcode)];
    }
    else
    {
        assert(opcode != Opcode::Filename && opcode != Opcode::LineNumber);
        return OpArgTypes_SCI0[static_cast<uint8_t>(opcode)];
    }
}

// Corresponds to Opcode enum
std::array<const char*, 130> OpcodeNames ={
	"bnot",
	"add",
	"sub",
	"mul",
	"div",
	"mod",
	"shr",
	"shl",
	"xor",
	"and",
	"or",
	"neg",
	"not",
	"eq?",
	"ne?",
	"gt?",
	"ge?",
	"lt?",
	"le?",
	"ugt?",
	"uge?",
	"ult?",
	"ule?",
	"bt",
	"bnt",
	"jmp",
	"ldi",
	"push",
	"pushi",
	"toss",
	"dup",
	"link",
	"call",
	"callk",
	"callb",
	"calle",
	"ret",
	"send",
    "INVALID",
    "INVALID",
	"class",
    "INVALID",
	"self",
	"super",
	"&rest",
	"lea",
	"selfID",  
    "INVALID",
	"pprev",
	"pToa",
	"aTop",
	"pTos",
	"sTop",
	"ipToa",
	"dpToa",
	"ipTos",
	"dpTos",
	"lofsa",
	"lofss",
	"push0",
	"push1",
	"push2",
	"pushSelf",  
    "INVALID",
	"lag",
	"lal",
	"lat",
	"lap",
	"lsg",
	"lsl",
	"lst",
	"lsp",
	"lagi",
	"lali",
	"lati",
	"lapi",
	"lsgi",
	"lsli",
	"lsti",
	"lspi",
	"sag",
	"sal",
	"sat",
	"sap",
	"ssg",
	"ssl",
	"sst",
	"ssp",
	"sagi",
	"sali",
	"sati",
	"sapi",
	"ssgi",
	"ssli",
	"ssti",
	"sspi",
	"+ag",
	"+al",
	"+at",
	"+ap",
/*100*/
	"+sg",
	"+sl",
	"+st",
	"+sp",
	"+agi",
	"+ali",
	"+ati",
	"+api",
	"+sgi",
	"+sli",
/*110*/
	"+sti",
	"+spi",
	"-ag",
	"-al",
	"-at",
	"-ap",
	"-sg",
	"-sl",
	"-st",
	"-sp",
/*120*/
	"-agi",
	"-ali",
	"-ati",
	"-api",
	"-sgi",
	"-sli",
	"-sti",
	"-spi",
    "_file_",
    "_line_",
};

const char *OpcodeToName(Opcode opcode, uint16_t firstOperand)
{
    if ((opcode == Opcode::LEA) && ((firstOperand >> 1) & LEA_ACC_AS_INDEX_MOD))
    {
        return "leai";
    }
    return OpcodeNames[(int)opcode];
}

std::unordered_set<std::string> opcodeSet;
std::unordered_set<std::string> &GetOpcodeSet()
{
    if (opcodeSet.empty())
    {
        opcodeSet.insert(OpcodeNames.begin(), OpcodeNames.end());
        opcodeSet.insert("leai");   // Special case
    }
    return opcodeSet;
}

Opcode NameToOpcode(const std::string &opcodeName, bool &usesAccIndex)
{
    usesAccIndex = false;
    Opcode opcode = Opcode::INDETERMINATE;
    if (opcodeName == "leai")
    {
        usesAccIndex = true;
        return Opcode::LEA;
    }
    for (int i = 0; i < OpcodeNames.size(); i++)
    {
        if (OpcodeNames[i] == opcodeName)
        {
            opcode = (Opcode)i;
        }
    }
    return opcode;
}

bool IsOpcode(const std::string &theString)
{
    GetOpcodeSet();
    return opcodeSet.find(theString) != opcodeSet.end();
}

SaidToken SaidArgs[] = {
    {',', 0xF0}, /*"OR". Used to specify alternatives to words, such as "take , get".*/
    {'&', 0xF1}, /*Unknown. Probably used for debugging.*/
    {'/', 0xF2}, /*Sentence part separator. Only two of these tokens may be used, since sentences are split into a maximum of three parts.*/
    {'(', 0xF3}, /*Used together with ')' for grouping*/
    {')', 0xF4}, /*See '('*/
    {'[', 0xF5}, /*Used together with '[' for optional grouping. "[ a ]" means "either a or nothing" */
    {']', 0xF6}, /*See '['.*/
    {'#', 0xF7}, /*Unknown. Assumed to have been used exclusively for debugging, if at all.*/
    {'<', 0xF8}, /*Semantic reference operator (as in "get < up").*/
    {'>', 0xF9}  /*Instructs Said() not to claim the event passed to the previous Parse() call on a match. Used for successive matching.*/
};