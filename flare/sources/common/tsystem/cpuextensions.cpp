

#include "tsystem.h"
#ifdef _WIN32
#include <wtypes.h>
#include <winnt.h>
#include <emmintrin.h>
#endif

using namespace TSystem;

#ifdef x64
long TSystem::getCPUExtensions() {
  return TSystem::CpuSupportsSse |
         TSystem::CpuSupportsSse2;  // TSystem::CPUExtensionsNone
}

#else
#ifndef _MSC_VER
long TSystem::getCPUExtensions() { return TSystem::CPUExtensionsNone; }
#else
namespace {

long CPUExtensionsAvailable = TSystem::CPUExtensionsNone;
bool CPUExtensionsEnabled   = true;
bool FistTime               = true;

//#ifdef _WIN32

//------------------------------------------------------------------------------

long CPUCheckForSSESupport() {
  __try {
    //		__asm andps xmm0,xmm0

    __asm _emit 0x0f __asm _emit 0x54 __asm _emit 0xc0

  } __except (EXCEPTION_EXECUTE_HANDLER) {
    if (_exception_code() == STATUS_ILLEGAL_INSTRUCTION)
      CPUExtensionsAvailable &= ~(CpuSupportsSse | CpuSupportsSse2);
  }

  return CPUExtensionsAvailable;
}

//------------------------------------------------------------------------------

long __declspec(naked) CPUCheckForExtensions() {
        __asm {
		push	ebp
		push	edi
		push	esi
		push	ebx

		xor		ebp,ebp			;cpu flags - if we do not have CPUID, we probably
								;will not want to try FPU optimizations.

		;check for CPUID.

		pushfd					;flags -> EAX
		pop		eax
		or		eax,00200000h	;set the ID bit
		push	eax				;EAX -> flags
		popfd
		pushfd					;flags -> EAX
		pop		eax
		and		eax,00200000h	;ID bit set?
		jz		done			;nope...

		;CPUID exists, check for features register.

		mov		ebp,00000003h
		xor		eax,eax
		cpuid
		or		eax,eax
		jz		done			;no features register?!?

		;features register exists, look for MMX, SSE, SSE2.

		mov		eax,1
		cpuid
		mov		ebx,edx
		and		ebx,00800000h	;MMX is bit 23
		shr		ebx,21
		or		ebp,ebx			;set bit 2 if MMX exists

		mov		ebx,edx
		and		edx,02000000h	;SSE is bit 25
		shr		edx,25
		neg		edx
		and		edx,00000018h	;set bits 3 and 4 if SSE exists
		or		ebp,edx

		and		ebx,04000000h	;SSE2 is bit 26
		shr		ebx,21
		and		ebx,00000020h	;set bit 5
		or		ebp,ebx

		;check for vendor feature register (K6/Athlon).

		mov		eax,80000000h
		cpuid
		mov		ecx,80000001h
		cmp		eax,ecx
		jb		done

		;vendor feature register exists, look for 3DNow! and Athlon extensions

		mov		eax,ecx
		cpuid

		mov		eax,edx
		and		edx,80000000h	;3DNow! is bit 31
		shr		edx,25
		or		ebp,edx			;set bit 6

		mov		edx,eax
		and		eax,40000000h	;3DNow!2 is bit 30
		shr		eax,23
		or		ebp,eax			;set bit 7

		and		edx,00400000h	;AMD MMX extensions (integer SSE) is bit 22
		shr		edx,19
		or		ebp,edx

done:
		mov		eax,ebp
		mov		CPUExtensionsAvailable, ebp

		;Full SSE and SSE-2 require OS support for the xmm* registers.

		test	eax,00000030h
		jz		nocheck
		call	CPUCheckForSSESupport
nocheck:
		pop		ebx
		pop		esi
		pop		edi
		pop		ebp
		ret
	}
}

//#endif
//#endif
}  // anonymous namespace

//------------------------------------------------------------------------------

long TSystem::getCPUExtensions() {
  if (FistTime) {
    CPUCheckForExtensions();
    FistTime = false;
  }

  if (CPUExtensionsEnabled)
    return CPUExtensionsAvailable;
  else
    return TSystem::CPUExtensionsNone;
}

#endif
#endif
//------------------------------------------------------------------------------
/*
void TSystem::enableCPUExtensions(bool on)
{
  CPUExtensionsEnabled = on;
}
*/
