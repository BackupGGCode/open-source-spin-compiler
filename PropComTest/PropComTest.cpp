//////////////////////////////////////////////////////////////
//                                                          //
// Propeller Spin/PASM Compiler Command Line Tool           //
// (c)2012 Parallax Inc. DBA Parallax Semiconductor.        //
// Adapted from Jeff Martin's Delphi code by Roy Eltham     //
// See end of file for terms of use.                        //
//                                                          //
//////////////////////////////////////////////////////////////
//
// PropComTest.cpp
//

#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <string.h>

#include "../PropellerCompiler/PropellerCompiler.h"

#define DataLimitStr        "64k"
#define ImageLimit          32768   // Max size of Propeller Application image file
#define ObjFileStackLimit   16

#define ListLimit           5000000
#define DocLimit            5000000
#define MaxObjInHeap        256

// {Object heap (compile-time objects)}
struct ObjHeap
{
    char*   ObjFilename;    // {Full filename of object}
    char*   Obj;            // {Object binary}
    int     ObjSize;        // {Size of object}
};

ObjHeap s_ObjHeap[MaxObjInHeap];
int     s_nObjHeapIndex = 0;
int     s_nObjStackPtr = 0;

CompilerData* s_pCompilerData = NULL;

bool CompileRecursively(char* pFilename);

int main(int argc, char* argv[])
{
    s_pCompilerData = InitStruct();

    s_pCompilerData->list = new char[ListLimit];
    s_pCompilerData->list_limit = ListLimit;
    memset(s_pCompilerData->list, 0, ListLimit);
    //s_pCompilerData->list[ListLimit] = 0;
    s_pCompilerData->doc = new char[DocLimit];
    s_pCompilerData->doc_limit = DocLimit;

    if (!CompileRecursively(argv[argc-1]))
    {
        return 1;
    }

    // do stuff with list and/or doc here

    // cleanup
    delete [] s_pCompilerData->list;
    delete [] s_pCompilerData->doc;

    return 0;
}

// returns NULL if the file failed to open or is 0 length
char* LoadFile(char* pFilename, int* pnLength)
{
    char* pBuffer = NULL;

    FILE* pFile = NULL;
    fopen_s(&pFile, pFilename, "rb");
    if (pFile != NULL)
    {
        // get the length of the file by seeking to the end and using ftell
        fseek(pFile, 0, SEEK_END);
        *pnLength = ftell(pFile);

        if (*pnLength > 0)
        {
            pBuffer = new char[*pnLength+1]; // allocate a buffer that is the size of the file plus one char
            pBuffer[*pnLength] = NULL; // set the end of the buffer to NULL

            // seek back to the beginning of the file and read it in
            fseek(pFile, 0, SEEK_SET);
            fread(pBuffer, 1, *pnLength, pFile);
        }
        fclose(pFile);
    }

    return pBuffer;
}

bool AddObjectToHeap(char* name)
{
    if (s_nObjHeapIndex < MaxObjInHeap)
    {
        int nNameBufferLength = (int)strlen(name)+1;
        s_ObjHeap[s_nObjHeapIndex].ObjFilename = new char[nNameBufferLength];
        strcpy_s(s_ObjHeap[s_nObjHeapIndex].ObjFilename, nNameBufferLength, name);
        s_ObjHeap[s_nObjHeapIndex].ObjSize = s_pCompilerData->obj_ptr;
        s_ObjHeap[s_nObjHeapIndex].Obj = new char[s_pCompilerData->obj_ptr];
        memcpy(s_ObjHeap[s_nObjHeapIndex].Obj, &(s_pCompilerData->obj[0]), s_pCompilerData->obj_ptr);
        s_nObjHeapIndex++;
        return true;
    }
    return false;
}

//{Returns index of object of Name in Object Heap.  Returns -1 if not found}
int IndexOfObjectInHeap(char* name)
{
    for (int i = s_nObjHeapIndex-1; i >= 0; i--)
    {
        if (_stricmp(s_ObjHeap[i].ObjFilename, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

void CleanObjectHeap()
{
    for (int i = 0; i < s_nObjHeapIndex; i++)
    {
        delete [] s_ObjHeap[i].ObjFilename;
        s_ObjHeap[i].ObjFilename = NULL;
        delete [] s_ObjHeap[i].Obj;
        s_ObjHeap[i].Obj = NULL;
        s_ObjHeap[i].ObjSize = 0;
    }
}

bool CompileSubObject(char* pFileName)
{
    // need to do locate file here
    return CompileRecursively(pFileName);
}

int GetData(unsigned char* pDest, char* pFileName, int nMaxSize)
{
    FILE* pFile = NULL;
    int nBytesRead = 0;
    fopen_s(&pFile, pFileName, "rb");

    if (pFile)
    {
        if (pDest)
        {
            nBytesRead = (int)fread(pDest, 1, nMaxSize, pFile);
        }
        fclose(pFile);
    }
    else
    {
        printf("Cannot find/open dat file: %s \n", pFileName);
        return -1;
    }

    return nBytesRead;
}

void UnicodeToPASCII(char* pBuffer, int nBufferLength, char* pPASCIIBuffer)
{
    // Translate Unicode source to PASCII (Parallax ASCII) source

    /*
     The Parallax font v1.0 utilizes the following Unicode addresses (grouped here into similar ranges):
         $0020- $007E, $00A1, $00A3, $00A5, $00B0- $00B3, $00B5, $00B9, $00BF- $00FE,
         $0394, $03A3, $03A9, $03C0,
         $2022, $2023, $20AC,
         $2190- $2193,
         $221A, $221E, $2248,
         $25B6, $2500, $2502, $250C, $2510, $2514, $2518, $251C, $2523, $2524, $252B, $252C, $2533, $2534, $253B, $253C, $254B, $25C0,
         $F000, $F001, $F008- $F00D, $F016- $F01F, $F07F- $F08F, $F0A0, $F0A2, $F0A6- $F0AF, $F0B4, $F0B6- $F0B8, $F0BA- $F0BE
     Propeller source files are sometimes Unicode-encoded (UTF-16) and can contain only the supported Parallax font characters with the following
     exceptions:  1) Run-time only characters ($F000, $F001, $F008-$F00D) are not valid
                  2) Some control code characters ($0009, $0010, $000D) are allowed.
     Any invalid characters will be translated to the PASCII infinity character ($00FF).
     All others will be mapped to their corresponding PASCII character.
     NOTE: The speediest translation would be a simple lookup table, but the natural size would be impractical.
     A little math solves this problem:
         ANDing the unicode character value with $07FF yields a range of $0000 to $07FF but causes the 20xx characters to inappropriately
         collide with other valid values.  ANDing with $07FF then ORing with ((CharVal >> 5) AND NOT(CharVal >> 4) AND $0100) corrects
         this by mapping 20xx into 21xx space and 22xx characters into 23xx space.  This allows for a practical translation table
         (of 2K bytes) to be used.
    */
    unsigned char aCharTxMap[0x800] = {
     // 0x00  0x01  0x02  0x03  0x04  0x05  0x06  0x07  0x08  0x09  0x0A  0x0B  0x0C  0x0D  0x0E  0x0F  0x10  0x11  0x12  0x13  0x14  0x15  0x16  0x17  0x18  0x19  0x1A  0x1B  0x1C  0x1D  0x1E  0x1F
/*000*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x09, 0x0A, 0xff, 0xff, 0x0D, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
/*020*/ 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
/*040*/ 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
/*060*/ 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
/*080*/ 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*0A0*/ 0xA0, 0xA1, 0xA2, 0xA3, 0xff, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
/*0C0*/ 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
/*0E0*/ 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xff,
/*100*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*120*/ 0xff, 0xff, 0x0F, 0x0E, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*140*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*160*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*180*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x02, 0x04, 0x03, 0x05, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*1A0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xA4, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*1C0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*1E0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*200*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*220*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*240*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*260*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*280*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*2A0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*2C0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*2E0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*300*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x15, 0xff, 0xff, 0xff, 0xFF, 0xff,
/*320*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*340*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x14, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*360*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*380*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x10, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*3A0*/ 0xff, 0xff, 0xff, 0x12, 0xff, 0xff, 0xff, 0xff, 0xff, 0x13, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*3C0*/ 0x11, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*3E0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*400*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*420*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*440*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*460*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*480*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*4A0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*4C0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*4E0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*500*/ 0x90, 0xff, 0x91, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9F, 0xff, 0xff, 0xff, 0x9E, 0xff, 0xff, 0xff, 0x9D, 0xff, 0xff, 0xff, 0x9C, 0xff, 0xff, 0xff, 0x95, 0xff, 0xff, 0xff,
/*520*/ 0xff, 0xff, 0xff, 0x99, 0x94, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x98, 0x97, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9B, 0x96, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9A, 0x92, 0xff, 0xff, 0xff,
/*540*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x93, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*560*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*580*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*5A0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*5C0*/ 0x06, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*5E0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*600*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*620*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*640*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*660*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*680*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*6A0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*6C0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*6E0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*700*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*720*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*740*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*760*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*780*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*7A0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*7C0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*7E0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };

    int nSourceOffset = 0;
    int nDestOffset = 0;
    while (nSourceOffset < nBufferLength)
    {
        short nChar = *((short*)(&pBuffer[nSourceOffset]));
        if (nChar != 0x000A && nChar != -257) // -257 == 0xFEFF
        {
            pPASCIIBuffer[nDestOffset] = aCharTxMap[(nChar | ((nChar >> 5) & !(nChar >> 4) & 0x0100)) & 0x07FF];
            nDestOffset++;
        }
        nSourceOffset += 2;
    }
    pPASCIIBuffer[nDestOffset] = 0;
}

void GetPASCIISource(char* pFilename)
{
    // read in file to temp buffer
    // convert to PASCII and assign to s_pCompilerData->source
    int nLength = 0;
    char* pBuffer = LoadFile(pFilename, &nLength);
    if (pBuffer)
    {
        char* pPASCIIBuffer = new char[nLength+1];
        UnicodeToPASCII(pBuffer, nLength, pPASCIIBuffer);
        s_pCompilerData->source = pPASCIIBuffer;

        delete [] pBuffer;
    }
    else
    {
        s_pCompilerData->source = NULL;
    }
}

bool CompileRecursively(char* pFilename)
{
    s_nObjStackPtr++;
    if (s_nObjStackPtr > ObjFileStackLimit)
    {
        printf("Object nesting exceeds limit of %d levels.\n", ObjFileStackLimit);
        return false;
    }

    GetPASCIISource(pFilename);

    if (s_pCompilerData->source == NULL)
    {
        printf("Can't find/open source file: %S \n", pFilename);
        return false;
    }

    // first pass on object
    char* pErrorString = Compile1();
    if (pErrorString != 0)
    {
        printf("%s\n", pErrorString);
        return false;
    }

    if (s_pCompilerData->obj_files > 0)
    {
        char filenames[file_limit*256];
        int	filename_start[file_limit];
        int	filename_finish[file_limit];
        int	instances[file_limit];
        
        for (int i = 0; i < s_pCompilerData->obj_files; i++)
        {
            // copy the obj filename appending .spin if it doesn't have it.
            strcpy_s(&filenames[i<<8], 256, &(s_pCompilerData->obj_filenames[i<<8]));
            if (strstr(&filenames[i<<8], ".spin") == NULL)
            {
                strcat_s(&filenames[i<<8], 256, ".spin");
            }
            filename_start[i] = s_pCompilerData->obj_name_start[i];
            filename_finish[i] = s_pCompilerData->obj_name_finish[i];
            instances[i] = s_pCompilerData->obj_instances[i];
        }

        for (int i = 0; i < s_pCompilerData->obj_files; i++)
        {
            if (!CompileSubObject(&filenames[i<<8]))
            {
                return false;
            }
        }

        // redo first pass on object
        GetPASCIISource(pFilename);
        pErrorString = Compile1();
        if (pErrorString != 0)
        {
            printf("%s\n", pErrorString);
            return false;
        }

        //{Load sub-objects' obj data}
        int p = 0;
        for (int i = 0; i < s_pCompilerData->obj_files; i++)
        {
            int nObjIdx = IndexOfObjectInHeap(&filenames[i<<8]);
            if (p + s_ObjHeap[nObjIdx].ObjSize > data_limit)
            {
                printf("Object files exceed 64k.\n");
                return false;
            }
            memcpy(&s_pCompilerData->obj_data[p], s_ObjHeap[nObjIdx].Obj, s_ObjHeap[nObjIdx].ObjSize);
            s_pCompilerData->obj_offsets[i] = p;
            s_pCompilerData->obj_lengths[i] = s_ObjHeap[nObjIdx].ObjSize;
            p += s_ObjHeap[nObjIdx].ObjSize;
        }
    }

    //{Load all DAT files}
    if (s_pCompilerData->dat_files > 0)
    {
        int p = 0;
        for (int i = 0; i < s_pCompilerData->dat_files; i++)
        {
            // {Get DAT's Files}

            //{Get name information}
            char filename[256];
            strcpy_s(&filename[0], 256, &(s_pCompilerData->dat_filenames[i<<8]));

            // not needed
            //s_pCompilerData->source_start = s_pCompilerData->dat_name_start[i]+1;
            //s_pCompilerData->source_finish = s_pCompilerData->dat_name_finish[i]-1;
            
            //{Find file}
            //if (!LocateSource(pFilename, ObjLevel))
            //{
            //	printf("Cannot find data file %s in work folder or library folder. %s", pFilename, GetSearchPaths);
            //}
            //{Load file}
            s_pCompilerData->dat_lengths[i] = GetData(&(s_pCompilerData->dat_data[p]), &filename[0], data_limit - p);
            if (s_pCompilerData->dat_lengths[i] == -1)
            {
                s_pCompilerData->dat_lengths[i] = 0;
                return false;
            }
            if (p + s_pCompilerData->dat_lengths[i] > data_limit)
            {
                printf("Object files exceed 64k.\n");
                return false;
            }
            s_pCompilerData->dat_offsets[i] = p;
            p += s_pCompilerData->dat_lengths[i];
        }
    }

    // second pass of object
    pErrorString = Compile2();
    if (pErrorString != 0)
    {
        printf("%s\n", pErrorString);
        return false;
    }

    // Check to make sure object fits into 32k
    int i = 0x10 + *((short*)&s_pCompilerData->obj[0]) + *((short*)&s_pCompilerData->obj[2]) + (s_pCompilerData->stack_requirement << 2);
    if (!s_pCompilerData->compile_mode && (i > 0x8000))
    {
        printf("Object exceeds runtime memory limit by %d longs.\n", (i - 0x8000) >> 2);
        return false;
    }

    if (!AddObjectToHeap(pFilename))
    {
        printf("Object Heap Overflow.\n");
        return false;
    }
    s_nObjStackPtr--;

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////
//                           TERMS OF USE: MIT License                                   //
///////////////////////////////////////////////////////////////////////////////////////////
// Permission is hereby granted, free of charge, to any person obtaining a copy of this  //
// software and associated documentation files (the "Software"), to deal in the Software //
// without restriction, including without limitation the rights to use, copy, modify,    //
// merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    //
// permit persons to whom the Software is furnished to do so, subject to the following   //
// conditions:                                                                           //
//                                                                                       //
// The above copyright notice and this permission notice shall be included in all copies //
// or substantial portions of the Software.                                              //
//                                                                                       //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   //
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         //
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    //
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION     //
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE        //
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                //
///////////////////////////////////////////////////////////////////////////////////////////
