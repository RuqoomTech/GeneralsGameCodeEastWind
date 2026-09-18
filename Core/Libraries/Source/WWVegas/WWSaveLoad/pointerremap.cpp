/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : WWSaveLoad                                                   *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/wwsaveload/pointerremap.cpp                  $*
 *                                                                                             *
 *                       Author:: Greg Hjelstrom                                               *
 *                                                                                             *
 *                     $Modtime:: 5/09/01 11:36a                                              $*
 *                                                                                             *
 *                    $Revision:: 9                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#include "pointerremap.h"
#include "WWDebug/wwdebug.h"


const int POINTER_TABLES_GROWTH_STEP = 4096;


PointerRemapClass::PointerRemapClass() :
	SaveContext(nullptr),
	NextSaveToken(1)
{
	SaveTokenTable.Set_Growth_Step(POINTER_TABLES_GROWTH_STEP);
	PointerPairTable.Set_Growth_Step(POINTER_TABLES_GROWTH_STEP);
	PointerRequestTable.Set_Growth_Step(POINTER_TABLES_GROWTH_STEP);
	RefCountRequestTable.Set_Growth_Step(POINTER_TABLES_GROWTH_STEP);
}

PointerRemapClass::~PointerRemapClass()
{
}

void PointerRemapClass::Reset()
{
	PointerPairTable.Delete_All();
	PointerRequestTable.Delete_All();
	RefCountRequestTable.Delete_All();
}

void PointerRemapClass::Process()
{
	if ( PointerPairTable.Count() > 0 ) {
		qsort(&PointerPairTable[0], PointerPairTable.Count(), sizeof(PointerPairTable[0]), ptr_pair_compare_function);
	}

	if ( PointerRequestTable.Count() > 0 ) {
		WWASSERT( PointerPairTable.Count() > 0 );
		qsort(&PointerRequestTable[0],PointerRequestTable.Count(), sizeof(PointerRequestTable[0]), ptr_request_compare_function);
		Process_Request_Table(PointerRequestTable,false);
	}

	// remap the ref-counted pointers
	if ( RefCountRequestTable.Count() > 0 ) {
		WWASSERT( PointerPairTable.Count() > 0 );
		qsort(&RefCountRequestTable[0],RefCountRequestTable.Count(), sizeof(RefCountRequestTable[0]), ptr_request_compare_function);
		Process_Request_Table(RefCountRequestTable,true);
	}
}

void PointerRemapClass::Process_Request_Table(DynamicVectorClass<PtrRemapStruct> &request_table, bool refcount)
{
	int pair_index = 0;

	for (int pointer_index = 0; pointer_index < request_table.Count(); pointer_index++) {
		const PersistPointerToken token_to_remap = request_table[pointer_index].OldToken;
		const int pre_search_index = pair_index;

		while ((pair_index < PointerPairTable.Count()) &&
			(PointerPairTable[pair_index].OldToken < token_to_remap)) {
			pair_index++;
		}

		if ((pair_index < PointerPairTable.Count()) && (PointerPairTable[pair_index].OldToken == token_to_remap)) {
			*request_table[pointer_index].PointerToRemap = PointerPairTable[pair_index].NewPointer;

			if (refcount) {
				RefCountClass *refptr = (RefCountClass *)(*request_table[pointer_index].PointerToRemap);
				refptr->Add_Ref();
			}
		} else {
			pair_index = pre_search_index;
			*request_table[pointer_index].PointerToRemap = nullptr;
#ifdef WWDEBUG
			const char *file = request_table[pointer_index].File;
			int line = request_table[pointer_index].Line;
			WWDEBUG_SAY(("Warning! Failed to re-map persistence token 0x%08lX  file = %s  line = %d",
				(unsigned long)token_to_remap, file, line));
			WWASSERT(0);
#endif
		}
	}
}

PersistPointerToken PointerRemapClass::Get_Save_Token(ChunkSaveClass &csave, const void *pointer)
{
	if (pointer == nullptr) {
		return 0;
	}

	if (SaveContext != &csave) {
		SaveContext = &csave;
		SaveTokenTable.Delete_All();
		NextSaveToken = 1;
	}

	for (int index = 0; index < SaveTokenTable.Count(); ++index) {
		if (SaveTokenTable[index].Pointer == pointer) {
			return SaveTokenTable[index].Token;
		}
	}

	WWASSERT(NextSaveToken != 0);
	const PersistPointerToken token = NextSaveToken++;
	SaveTokenTable.Add(PtrSaveTokenStruct(pointer, token));
	return token;
}

void PointerRemapClass::Register_Pointer(PersistPointerToken old_token, void *new_pointer)
{
	if (old_token != 0) {
		PointerPairTable.Add(PtrPairStruct(old_token, new_pointer));
	}
}

#ifdef WWDEBUG
void PointerRemapClass::Request_Pointer_Remap(PersistPointerToken old_token, void **pointer_to_convert, const char *file, int line)
{
	PtrRemapStruct remap;
	remap.OldToken = old_token;
	remap.PointerToRemap = pointer_to_convert;
	remap.File = file;
	remap.Line = line;
	PointerRequestTable.Add(remap);
}

void PointerRemapClass::Request_Ref_Counted_Pointer_Remap(PersistPointerToken old_token, RefCountClass **pointer_to_convert, const char *file, int line)
{
	PtrRemapStruct remap;
	remap.OldToken = old_token;
	remap.PointerToRemap = (void **)pointer_to_convert;
	remap.File = file;
	remap.Line = line;
	RefCountRequestTable.Add(remap);
}
#else
void PointerRemapClass::Request_Pointer_Remap(PersistPointerToken old_token, void **pointer_to_convert)
{
	PtrRemapStruct remap;
	remap.OldToken = old_token;
	remap.PointerToRemap = pointer_to_convert;
	PointerRequestTable.Add(remap);
}

void PointerRemapClass::Request_Ref_Counted_Pointer_Remap(PersistPointerToken old_token, RefCountClass **pointer_to_convert)
{
	PtrRemapStruct remap;
	remap.OldToken = old_token;
	remap.PointerToRemap = (void **)pointer_to_convert;
	RefCountRequestTable.Add(remap);
}
#endif

/*
** sort compare function for pointer pair structures
** sorts by the persisted token value
*/
int __cdecl PointerRemapClass::ptr_pair_compare_function(void const * ptr1, void const * ptr2)
{
	PersistPointerToken old1 = ((PointerRemapClass::PtrPairStruct const *)ptr1)->OldToken;
	PersistPointerToken old2 = ((PointerRemapClass::PtrPairStruct const *)ptr2)->OldToken;

	if (old1 == old2) {
		return(0);
	}
	if (old1 < old2) {
		return(-1);
	}
	return(1);
}

/*
** sort compare function for pointer remap structures
** sorts by the persisted token value
*/
int __cdecl PointerRemapClass::ptr_request_compare_function(void const * ptr1, void const * ptr2)
{
	PtrRemapStruct * remap1 = (PtrRemapStruct *)ptr1;
	PtrRemapStruct * remap2 = (PtrRemapStruct *)ptr2;

	PersistPointerToken old1 = remap1->OldToken;
	PersistPointerToken old2 = remap2->OldToken;

	if (old1 == old2) {
		return(0);
	}
	if (old1 < old2) {
		return(-1);
	}
	return(1);
}


