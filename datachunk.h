/*! \file	datachunk.h
 *	\brief	Simple re-sizable data chunk object
 */

#ifndef MXFLIB__DATACHUNK_H
#define MXFLIB__DATACHUNK_H

namespace mxflib
{
	// Forward declare so the class can be used
	class DataChunk;

	//! A smart pointer to an DataChunk object
	typedef SmartPtr<DataChunk> DataChunkPtr;

	//! A list of smart pointers to DataChunk objects
	typedef std::list<DataChunkPtr> DataChunkList;
}


namespace mxflib
{
	class DataChunk
	{
	private:
		Uint32 DataSize;

	public:
		Uint32 Size;
		Uint8 *Data;

		//! Construct an empty data chunk
		DataChunk() : DataSize(0), Size(0) , Data(NULL) {};

		//! Construct a data chunk with a pre-allocated buffer
		DataChunk(Uint64 BufferSize) : DataSize(0), Size(0) , Data(NULL) { Resize(BufferSize); };

		//! Data chunk copy constructor
		DataChunk(const DataChunk &Chunk) : DataSize(0), Size(0) , Data(NULL) { Set(Chunk.Size, Chunk.Data); };

		~DataChunk() { if(Data) delete[] Data; };

		//! Resize the data chunk, preserving contents
		void Resize(Uint32 NewSize)
		{
			if(Size == NewSize) return;

			if(DataSize >= NewSize) 
			{
				Size = NewSize;
				return;
			}

			Uint8 *NewData = new Uint8[NewSize];
			if(Size) memcpy(NewData, Data, Size);
			if(Data) delete[] Data;
			Data = NewData;
			DataSize = Size = NewSize;
		}

		//! Set some data into the data chunk (expanding it if required)
		void Set(Uint32 MemSize, const Uint8 *Buffer, Uint32 Start = 0)
		{
			if(Size < (MemSize + Start)) Resize(MemSize + Start);

			memcpy(&Data[Start], Buffer, MemSize);
		}

		DataChunk& operator=(const DataChunk &Right)
		{
			Set(Right.Size, Right.Data);

			return *this;
		}

		bool operator==(const DataChunk &Right) const
		{
			if(Size != Right.Size) return false;
			
			if(memcmp(Data, Right.Data, Size) == 0) return true;

			return false;
		}

		bool operator!=(const DataChunk &Right) const { return !operator==(Right); };

		Uint32 fread(FILE *fp, size_t Size, Uint32 Start = 0)
		{
			int Ret;
			Resize(Size);
			Ret=::fread(&Data[Start],1,Size,fp);
			Size = Ret;
			return Ret;
		};
	};
}

#endif MXFLIB__DATACHUNK_H
