/*! \file	mxffile.cpp
 *	\brief	Implementation of MXFFile class
 *
 *			The MXFFile class holds data about an MXF file, either loaded 
 *          from a physical file or built in memory
 */
/*
 *	Copyright (c) 2003, Matt Beard
 *
 *	This software is provided 'as-is', without any express or implied warranty.
 *	In no event will the authors be held liable for any damages arising from
 *	the use of this software.
 *
 *	Permission is granted to anyone to use this software for any purpose,
 *	including commercial applications, and to alter it and redistribute it
 *	freely, subject to the following restrictions:
 *
 *	  1. The origin of this software must not be misrepresented; you must
 *	     not claim that you wrote the original software. If you use this
 *	     software in a product, an acknowledgment in the product
 *	     documentation would be appreciated but is not required.
 *	
 *	  2. Altered source versions must be plainly marked as such, and must
 *	     not be misrepresented as being the original software.
 *	
 *	  3. This notice may not be removed or altered from any source
 *	     distribution.
 */

// Required for strerror()
#include <string.h>


#include "mxflib.h"

using namespace mxflib;


//! Open the named MXF file
bool mxflib::MXFFile::Open(std::string FileName, bool ReadOnly /* = false */ )
{
	if(isOpen) Close();

	// Record the name
	Name = FileName;

	if(ReadOnly)
	{
		Handle = KLVFileOpenRead(FileName.c_str());
	}
	else
	{
		Handle = KLVFileOpen(FileName.c_str());
	}

	if(!KLVFileValid(Handle)) return false;

	isOpen = true;

	return ReadRunIn();
}


//! Read the files run-in (if it exists)
/*! The run-in is placed in property run-in
 *	After this function the file pointer is at the start of the non-run in data
 */
bool mxflib::MXFFile::ReadRunIn()
{
	RunInSize = 0;
	RunIn.Resize(0);

	Seek(0);
	DataChunkPtr Key = Read(16);

	// If we couldn't read 16-bytes then this isn't a valid MXF file
	if(Key->Size != 16) return false;

	// Locate a closed header type for key compares
	MDOTypePtr BaseHeader = MDOType::Find("ClosedHeader");

	if(!BaseHeader)
	{
		error("Cannot find \"ClosedHeader\" in current dictionary\n");
		return false;
	}

	// Index the start of the key
	Uint8 *BaseKey = BaseHeader->GetDict()->Key;

	// If no run-in end now
	if(memcmp(BaseKey,Key->Data,11) == 0)
	{
		Seek(0);
		return true;
	}

	// Perform search in memory
	// Maximum size plus enough to test the following key
	Seek(0);
	DataChunkPtr Search = Read(0x10000 + 11);

	Uint64 Scan = Search->Size - 11;
	Uint8 *ScanPtr = Search->Data;
	while(Scan)
	{
		// Run-in ends when a vaid MXF key is found
		if(memcmp(BaseKey,ScanPtr,11) == 0) 
		{
			RunIn.Set(RunInSize, Search->Data);
			Seek(0);
			return true;
		}

		Scan--;
		ScanPtr++;
		RunInSize++;
	}

	error("Cannot find valid key in first 65536 bytes of file \"%s\"\n", Name);
	Seek(0);
	return false;
}


//! Close the file
bool mxflib::MXFFile::Close(void)
{
	if(isOpen) KLVFileClose(Handle);

	isOpen = false;

	return true;
}


//! Read data from the file into a DataChunk
DataChunkPtr mxflib::MXFFile::Read(Uint64 Size)
{
	DataChunkPtr Ret = new DataChunk(Size);

	if(Size)
	{
		Uint64 Bytes = KLVFileRead(Handle, Ret->Data, Size);

		// Handle errors
		if(Bytes == (Uint64)-1)
		{
			error("Error reading file \"%s\" at 0x%s- %s\n", Name.c_str(), Int64toHexString(Tell(), 8).c_str(), strerror(errno));
			Bytes = 0;
		}

		if(Bytes != Size) Ret->Resize(Bytes);
	}

	return Ret;
}


/*
//! Read an MDObject from the current position
MDObjectPtr mxflib::MXFFile::ReadObject(void)
{
	MDObjectPtr Ret;

	Uint64 Location = Tell();
	ULPtr Key = ReadKey();

	// If we couldn't read the key then bug out
	if(!Key) return Ret;

	// Build the object (it may come back as an "unknown")
	Ret = new MDObject(Key);

	ASSERT(Ret);

	Uint64 Length = ReadBER();
	if(Length > 0)
	{
		DataChunkPtr Data = Read(Length);

		if(Data->Size != Length)
		{
			error("Not enough data in file for object %s at 0x%s\n", Ret->Name().c_str(), Int64toHexString(Location,8).c_str());
		}

		Ret->ReadValue(Data->Data, Data->Size);
	}

	return Ret;
}
*/

//! Read an MDObject from the current position
//template<class TP, class T> TP mxflib::MXFFile::ReadObjectBase(void)
//MDObjectPtr mxflib::MXFFile::ReadObject(void)
/*
template<class TP, class T> TP mxflib::MXFFile__ReadObjectBase(MXFFile *This)
{
	TP Ret;

	Uint64 Location = This->Tell();
	ULPtr Key = This->ReadKey();

	// If we couldn't read the key then bug out
	if(!Key) return Ret;

	// Build the object (it may come back as an "unknown")
	Ret = new T(Key);

	ASSERT(Ret);

	Uint64 Length = This->ReadBER();
	if(Length > 0)
	{
		DataChunkPtr Data = This->Read(Length);

		if(Data->Size != Length)
		{
			error("Not enough data in file for object %s at 0x%s\n", Ret->Name().c_str(), Int64toHexString(Location,8).c_str());
		}

		Ret->ReadValue(Data->Data, Data->Size);
	}

	return Ret;
}
*/

//! Read all partition packs from the open MXF file
/*! The new RIP is placed in property FileRIP
 *  \note This new RIP will represent what is in the physical
 *        file so any data in memory will not be considered
 *  \note The current contents of FileRIP will be destroyed
 */
bool mxflib::MXFFile::BuildRIP(void)
{
	// Remove any old data
	FileRIP.clear();

	FileRIP.isGenerated = 0;

	Seek(0);

	Uint64 Location = 0;

	// Locate a closed header type for key compares
	MDOTypePtr BaseHeader = MDOType::Find("ClosedHeader");
	if(!BaseHeader)
	{
		error("Cannot find \"ClosedHeader\" in current dictionary\n");
		return false;
	}

	// Read the first partition pack in the file
	PartitionPtr ThisPartition = ReadPartition();

	// If we couldn't read the first object then there are no partitions
	// Note that this is not stricty an error - the file could be empty!
	if(!ThisPartition) return true;

	// Check that the first KLV as a partition pack
	// DRAGONS: What if the first KLV is a filler? - This shouldn't be valid as it would look like a run-in!
	if(!(ThisPartition->GetType()->GetDict()->Base)
		|| ( strcmp(ThisPartition->GetType()->GetDict()->Base->Name, "PartitionMetadata") != 0) )
	{
		error("First KLV in file \"%s\" is not a known partition type\n", Name.c_str());
		return false;
	}

	while(!Eof())
	{
		Uint32 BodySID = 0;
		MDObjectPtr Ptr = ThisPartition["BodySID"];
		if(Ptr) BodySID = Ptr->Value->GetInt();

		FileRIP.AddPartition(ThisPartition, Location, BodySID);

		Uint64 Skip = 0;

		// Work out how far to skip ahead
		Ptr = ThisPartition["HeaderByteCount"];
		if(Ptr) Skip = Ptr->Value->GetInt64();
		Ptr = ThisPartition["IndexByteCount"];
		if(Ptr) Skip += Ptr->Value->GetInt64();

		if(Skip)
		{
			// Location before we skip
			Uint64 PreSkip = Tell();

			/* Check for version 10 HeaderByteCount and possible bug version */
			// Version 10 of MXF counts from the end of the partition pack, however
			// some version 10 code uses the version 11 counting so we check to 
			// see if the header is claimed to be an integer number of KAGs and
			// that there is a leading filler to take us to the start of the next KAG
			// In this case we are probably in a Version 10 HeaderByteCount bug situation...

			Uint16 Ver = 0;
			Ptr = ThisPartition["MinorVersion"];
			Ver = Ptr->Value->GetUint();

			bool SkipFiller = true;		// Version 11 behaviour

			if(Ver == 1)				// Ver = 1 for version 10 files
			{
				Uint32 KAGSize = 0;
				Ptr = ThisPartition["KAGSize"];
				if(Ptr) KAGSize = Ptr->GetInt();

				Uint64 HeaderByteCount = 0;
				Ptr = ThisPartition["HeaderByteCount"];
				if(Ptr) HeaderByteCount = Ptr->GetInt();

				if((KAGSize > 16) && (HeaderByteCount > 0))
				{
					Uint64 Pos = Tell();
					Uint64 U = HeaderByteCount / KAGSize;
					if((U * KAGSize) == HeaderByteCount)
					{
						MDObjectPtr First = ReadObject();
						if(!First)
						{
							// Can't tell what is next! This will probably cause an error later!
							SkipFiller = true;
							Seek(Pos);
						}
						else if(First->Name() == "KLVFill")
						{
							U = Tell() / KAGSize;
							if( (U * KAGSize) == Tell() )
							{
								warning("(Version 10 file) HeaderByteCount in %s at 0x%s in %s does not include the leading filler\n",
										ThisPartition->FullName().c_str(), Int64toHexString(ThisPartition->GetLocation(),8).c_str(), 
										ThisPartition->GetSource().c_str());

								// We are skipping filler - even though we have already done it!
								SkipFiller = true;
								PreSkip = Tell();
							}
							else
							{
								SkipFiller = false;
								Seek(Pos);
							}
						}
					}
				}
			}

			if(SkipFiller)
			{
				MDObjectPtr First = ReadObject();
				if(!First)
				{
					error("Error reading first KLV after %s at 0x%s in %s\n", ThisPartition->FullName().c_str(), 
						  Int64toHexString(ThisPartition->GetLocation(),8).c_str(), ThisPartition->GetSource().c_str());
					return false;
				}

				if(First->Name() == "KLVFill")
					PreSkip = Tell();
				else if(First->Name() != "Primer")
				{
					error("First KLV following a partition pack (and any trailing filler) must be a Primer, however %s found at 0x%s in %s\n", 
						  ThisPartition->FullName().c_str(), Int64toHexString(ThisPartition->GetLocation(),8).c_str(), 
						  ThisPartition->GetSource().c_str());
				}
			}

			// Skip over header
			Uint64 NextPos = PreSkip + Skip;
			if( Seek(NextPos) != NextPos)
			{
				error("Unexpected end of file in partition starting at 0x%s in file \"%s\"\n",
					  Int64toHexString(Location,8).c_str(), Name.c_str());
				return false;
			}

			// Check that we ended up in a sane place after the skip
			DataChunkPtr Test = Read(2);
			if( Test->Size != 2)
			{
				// Less than 2 bytes after the declared end of this metadata
				// Could be that the count is valid and points us to the end of the file
				// but to be safe check through the header to see if there is anything else
				Seek(PreSkip);
			} 
			else if((Test->Data[0] != 6) || (Test->Data[1] != 0x0e))
			{
				error("Byte counts in partition pack at 0x%s in file \"%s\" are not valid\n", Int64toHexString(Location,8).c_str(), Name.c_str());
				
				// Move back to end of partition pack and scan through the header
				Seek(PreSkip);
			}
			else
			{
				// Move back (the test moved the pointer 2 bytes forwards)
				Seek(NextPos);
			}
		}

		// Now scan until the next partition
		ULPtr Key;
		for(;;)
		{
			Location = Tell();
			Key = ReadKey();
			if(!Key) break;

			// Identify what we have found
			MDOTypePtr ThisType = MDOType::Find(Key);

			// Only check for this being a partition pack if we know the type
			if(ThisType)
			{
				const DictEntry *Dict = ThisType->GetDict();
//printf("Found %s at 0x%08x\n", Dict->Name, (Uint32)Location);
				if(Dict && Dict->Base)
				{
					if(strcmp(Dict->Base->Name, "PartitionMetadata") == 0)
					{
						break;
					}
				}
			}
//else printf("Found %s at 0x%08x\n", UL(Key->Data).GetString().c_str(), (Uint32)Location);

			Skip = ReadBER();
			Uint64 NextPos = Tell() + Skip;
			if( Seek(NextPos) != NextPos)
			{
				error("Unexpected end of file in KLV starting at 0x%s in file \"%s\"\n",
					  Int64toHexString(Location,8).c_str(), Name.c_str());
				return false;
			}
			
			if(Eof()) break;
		}

		// Check if we found anything
		if(!Key) break;
		if(Eof()) break;

		// By this point we have found a partition pack
		// Read it ...
		Seek(Location);
		ThisPartition = ReadPartition();
		
		// ... then loop back to add it
		continue;
	}

	return true;
}

/*
#############################
	ULPtr FirstKey = ReadKey();

	// If we couldn't read a key then there are no partitions
	// Note that this is not stricty an error - the file could be empty!
	if(!FirstKey) return true;

	// Locate a closed header type for key compares
	MDOTypePtr BaseHeader = MDOType::Find("ClosedHeader");

	if(!BaseHeader)
	{
		error("Cannot find \"ClosedHeader\" in current dictionary\n");
		return false;
	}

	// Identify the key we have found and build a partition object for it
	PartitionPtr ThisPartition = new Partition(FirstKey);

	if(!ThisPartition || !(ThisPartition->GetType()->GetDict()))
	{
		error("First KLV in file \"%s\" is not a known type\n", Name.c_str());
		return false;
	}

	if(!(ThisPartition->GetType()->GetDict()->Base)
		|| ( strcmp(ThisPartition->GetType()->GetDict()->Base->Name, "PartitionMetadata") != 0) )
	{
		error("First KLV in file \"%s\" is not a known partition type\n", Name.c_str());
		return false;
	}

//printf("Found %s at 0x%08x\n", ThisPartition->GetType()->GetDict()->Name, (Uint32)Location);
	while(!Eof())
	{
		Uint64 DataLen = ReadBER();
		DataChunkPtr Value = Read(DataLen);
		if(Value->Size != DataLen)
		{
			error("Incomplete KLV in file \"%s\" at 0x%s\n", Name.c_str(), Int64toHexString(Tell(),8).c_str());
			return false;
		}

		// Read the value into the partition object
		ThisPartition->ReadValue(Value->Data, DataLen);

		Uint32 BodySID = 0;
		MDObjectPtr Ptr = ThisPartition["BodySID"];
		if(Ptr) BodySID = Ptr->Value->GetInt();

		FileRIP.AddPartition(ThisPartition, Location, BodySID);

		Uint64 Skip = 0;

		// Work out how far to skip ahead
		Ptr = ThisPartition["HeaderByteCount"];
		if(Ptr) Skip = Ptr->Value->GetInt64();
		Ptr = ThisPartition["IndexByteCount"];
		if(Ptr) Skip += Ptr->Value->GetInt64();

		// Check for version 10 HeaderByteCount bug //
		// Some version 10 code does not count the leading filler into account
		// So we check to see if the header is claimed to be an integer number of KAGs
		// and that there is a leading filler to take us to the start of the next KAG
		// In this case we seem to be in an eVTR bug situation...
		
		Uint16 Ver = 0;
		Ptr = ThisPartition["MinorVersion"];
		Ver = Ptr->Value->GetUint();
		if(Ver == 1)
		{
			Uint32 KAGSize = 0;
			Ptr = ThisPartition["KAGSize"];
			if(Ptr) KAGSize = Ptr->GetInt();

			Uint64 HeaderByteCount = 0;
			Ptr = ThisPartition["HeaderByteCount"];
			if(Ptr) HeaderByteCount = Ptr->GetInt();

			if((KAGSize > 16) && (HeaderByteCount > 0))
			{
				Uint64 Pos = Tell();
				Uint64 U = HeaderByteCount / KAGSize;
				if((U * KAGSize) == HeaderByteCount)
				{
					MDObjectPtr First = ReadObject();
					if(!First)
					{
						Seek(Pos);
					}
					else if(First->Name() == "KLVFill")
					{
						U = Tell() / KAGSize;
						if( (U * KAGSize) == Tell() )
						{
							warning("HeaderByteCount does not include the leading filler\n");
						}
						else
						{
							Seek(Pos);
						}
					}
				}
			}
		}

		// Skip over header
		Uint64 PreSkip = Tell();
		Uint64 NextPos = PreSkip + Skip;
		if( Seek(NextPos) != NextPos)
		{
			error("Unexpected end of file in partition starting at 0x%s in file \"%s\"\n",
				  Int64toHexString(Location,8).c_str(), Name.c_str());
			return false;
		}

		// Flag that this is the first read after the skip
		// so we can validate the skip
		bool FirstRead = true;

		// Now scan until the next partition
		DataChunkPtr Key;
		for(;;)
		{
			// We don't use ReadKey() here to allow us to diagnose wrong byte counts in the partition pack
			Location = Tell();
			Key = Read(16);

			if(Key->Size == 0) break;
			if(Key->Size != 16)
			{
				error("Incomplete KLV in file \"%s\" at 0x%s\n", Name.c_str(), Int64toHexString(Tell(),8).c_str());
				return false;
			}

			// Validate the key read after performing a skip
			if(FirstRead)
			{
				FirstRead = false;
				if((Key->Data[0] != 6) || (Key->Data[1] != 0x0e))
				{
					error("Byte counts in partition pack ending at 0x%s in file \"%s\" are not valid\n", Int64toHexString(PreSkip,8).c_str(), Name.c_str());
					
					// Move back to end of partition pack and scan through the header
					Seek(PreSkip);
					continue;
				}
			}
			else
			{
				if((Key->Data[0] != 6) || (Key->Data[1] != 0x0e))
				{
					error("Invalid KLV key found at 0x%s in file \"%s\"\n", Int64toHexString(Tell(), 8).c_str(), Name.c_str());
					return false;
				}
			}

			// Identify what we have found
			MDOTypePtr ThisType = MDOType::Find(new UL(Key->Data));

			// Only check for this being a partition pack if we know the type
			if(ThisType)
			{
				const DictEntry *Dict = ThisType->GetDict();
//printf("Found %s at 0x%08x\n", Dict->Name, (Uint32)Location);
				if(Dict && Dict->Base)
				{
					if(strcmp(Dict->Base->Name, "PartitionMetadata") == 0)
					{
						break;
					}
				}
			}
//else printf("Found %s at 0x%08x\n", UL(Key->Data).GetString().c_str(), (Uint32)Location);

			Skip = ReadBER();
			Uint64 NextPos = Tell() + Skip;
			if( Seek(NextPos) != NextPos)
			{
				error("Unexpected end of file in KLV starting at 0x%s in file \"%s\"\n",
					  Int64toHexString(Location,8).c_str(), Name.c_str());
				return false;
			}
			
			if(Eof()) break;
		}

		// Check if we found anything
		if(Key->Size == 0) break;
		if(Eof()) break;

		// By this point we have found a partition pack - loop back to add it
		continue;
	}

	return true;
}
*/


//! Read a BER length from the open file
Uint64 mxflib::MXFFile::ReadBER(void)
{
	DataChunkPtr Length = Read(1);
	if(Length->Size < 1)
	{
		error("Incomplete BER length in file \"%s\" at 0x%s\n", Name.c_str(), Int64toHexString(Tell(),8).c_str());
		return false;
	}

	Uint64 Ret = Length->Data[0];
	if(Ret >= 0x80)
	{
		Uint32 i = Ret & 0x7f;
		Length = Read(i);
		if(Length->Size != i)
		{
			error("Incomplete BER length in file \"%s\" at 0x%s\n", Name.c_str(), Int64toHexString(Tell(),8).c_str());
			return false;
		}

		Ret = 0;
		Uint8 *p = Length->Data;
		while(i--) Ret = ((Ret<<8) + *(p++));
	}

	return Ret;
}


//! Read a Key length from the open file
ULPtr mxflib::MXFFile::ReadKey(void)
{
	ULPtr Ret;

	Uint64 Location = Tell();
	DataChunkPtr Key = Read(16);

	// If we couldn't read 16-bytes then bug out (this may be valid)
	if(Key->Size != 16) return Ret;

	// Sanity check the keys
	if((Key->Data[0] != 6) || (Key->Data[1] != 0x0e))
	{
		error("Invalid KLV key found at 0x%s in file \"%s\"\n", Int64toHexString(Location, 8).c_str(), Name.c_str());
		return Ret;
	}

	// Build the UL
	Ret = new UL(Key->Data);

	return Ret;
}
