/*! \file	deftypes.cpp
 *	\brief	Defines known types
 *
 *	\version $Id$
 *
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

#include <mxflib/mxflib.h>
#include <mxflib/helper.h>
#include <mxflib/sopsax.h>

#include <stdarg.h>


using namespace mxflib;

/* SAX functions */
void DefTypes_startElement(void *user_data, const char *name, const char **attrs);
void DefTypes_endElement(void *user_data, const char *name);
void DefTypes_warning(void *user_data, const char *msg, ...);
void DefTypes_error(void *user_data, const char *msg, ...);
void DefTypes_fatalError(void *user_data, const char *msg, ...);


/*
** Our SAX handler
*/
static sopSAXHandler DefTypes_SAXHandler = {
    (startElementSAXFunc) DefTypes_startElement,		/* startElement */
    (endElementSAXFunc) DefTypes_endElement,			/* endElement */
    (warningSAXFunc) DefTypes_warning,					/* warning */
    (errorSAXFunc) DefTypes_error,						/* error */
    (fatalErrorSAXFunc) DefTypes_fatalError,			/* fatalError */
};

typedef enum
{
	DEFTYPES_STATE_START = 0,
	DEFTYPES_STATE_STARTED,					/*!< Inside the 'MXFTypes' tag */
	DEFTYPES_STATE_BASIC,					/*!< Processing 'Basic' section */
	DEFTYPES_STATE_INTERPRETATION,			/*!< Processing 'Interpretation' section */
	DEFTYPES_STATE_MULTIPLE,				/*!< Processing 'Multiple' section */
	DEFTYPES_STATE_COMPOUND,				/*!< Processing 'Compound' section */
	DEFTYPES_STATE_COMPOUNDITEMS,			/*!< Addind items to a 'Compound' */
	DEFTYPES_STATE_END,						/*!< Finished the dictionary */
	DEFTYPES_STATE_ERROR					/*!< Skip all else - error condition */
} DefTypesStateState;

typedef struct
{
	DefTypesStateState	State;				/*!< State machine current state */
	MDTypePtr			CurrentCompound;	/*!< The current compound being build (or NULL) */
	char				CompoundName[65];	/*!< Name of the current compound (or "") */
} DefTypesState;


//! Type to map type names to their handling traits
typedef std::map<std::string,MDTraitsPtr> TraitsMapType;

//! Map of type names to thair handling traits
static TraitsMapType TraitsMap;

//! Build the map of all known traits
static void DefineTraits(void)
{
	// Not a real type, but the default for basic types
	TraitsMap.insert(TraitsMapType::value_type("Default-Basic", new MDTraits_Raw));

	// Not a real type, but the default for array types
	TraitsMap.insert(TraitsMapType::value_type("Default-Array", new MDTraits_BasicArray));

	// Not a real type, but the default for compound types
	TraitsMap.insert(TraitsMapType::value_type("Default-Compound", new MDTraits_BasicCompound));

	TraitsMap.insert(TraitsMapType::value_type("RAW", new MDTraits_Raw));

	TraitsMap.insert(TraitsMapType::value_type("Int8", new MDTraits_Int8));
	TraitsMap.insert(TraitsMapType::value_type("Uint8", new MDTraits_Uint8));
	TraitsMap.insert(TraitsMapType::value_type("Int16", new MDTraits_Int16));
	TraitsMap.insert(TraitsMapType::value_type("Uint16", new MDTraits_Uint16));
	TraitsMap.insert(TraitsMapType::value_type("Int32", new MDTraits_Int32));
	TraitsMap.insert(TraitsMapType::value_type("Uint32", new MDTraits_Uint32));
	TraitsMap.insert(TraitsMapType::value_type("Int64", new MDTraits_Int64));
	TraitsMap.insert(TraitsMapType::value_type("Uint64", new MDTraits_Uint64));

	TraitsMap.insert(TraitsMapType::value_type("ISO7", new MDTraits_ISO7));
	TraitsMap.insert(TraitsMapType::value_type("UTF16", new MDTraits_UTF16));

	TraitsMap.insert(TraitsMapType::value_type("ISO7String", new MDTraits_BasicStringArray));
	TraitsMap.insert(TraitsMapType::value_type("UTF16String", new MDTraits_BasicStringArray));
	TraitsMap.insert(TraitsMapType::value_type("Uint8Array", new MDTraits_RawArray));

	TraitsMap.insert(TraitsMapType::value_type("UUID", new MDTraits_UUID));
	TraitsMap.insert(TraitsMapType::value_type("Label", new MDTraits_Label));

	TraitsMap.insert(TraitsMapType::value_type("UMID", new MDTraits_UMID));

	TraitsMap.insert(TraitsMapType::value_type("LabelCollection", new MDTraits_RawArrayArray));

	TraitsMap.insert(TraitsMapType::value_type("Rational", new MDTraits_Rational));
	TraitsMap.insert(TraitsMapType::value_type("Timestamp", new MDTraits_TimeStamp));
}


//! Load types from the specified XML definitions
/*! \return 0 if all OK
 *! \return -1 on error
 */
int mxflib::LoadTypes(char *TypesFile)
{
	// State data block passed through SAX parser
	DefTypesState State;

	// Define the known traits
	// Test before calling as two partial definition files could be loaded!
	if(TraitsMap.empty()) DefineTraits();

	State.State = DEFTYPES_STATE_START;
	State.CurrentCompound = NULL;
	State.CompoundName[0] = '\0';

	std::string XMLFilePath = LookupDictionaryPath(TypesFile);

	// Parse the file
	bool result = false;
	
	if(XMLFilePath.size()) result = sopSAXParseFile(&DefTypes_SAXHandler, &State, XMLFilePath.c_str());
	if (!result)
	{
		error("sopSAXParseFile failed for %s\n", TypesFile);
		return -1;
	}


	// Finally ensure we have a valid "Unknown" type
	if(!MDType::Find("Unknown"))
	{
		MDTypePtr Array = MDType::Find("Uint8Array");

		// Don't know Uint8Array - can't be a valid types file!
		if(!Array) 
		{
			error("Types definition file %s does not contain a definition for Uint8Array - is it a valid file?\n", TypesFile);
			return -1;
		}

		MDType::AddInterpretation("Unknown", MDType::Find("Uint8Array"));
	}

	return 0;
}


//! SAX callback - Deal with start tag of an element
void DefTypes_startElement(void *user_data, const char *name, const char **attrs)
{
	DefTypesState *State = (DefTypesState*)user_data;
	int this_attr = 0;

/* DEBUG */
	{
		debug("Element : %s\n", name);

		if(attrs != NULL)
		{
			while(attrs[this_attr])
			{
				debug("  Attribute : %s = \"%s\"\n", attrs[this_attr], attrs[this_attr+1]);
				this_attr += 2;
			}
		}
	}
/* /DEBUG */

	switch(State->State)
	{
		/* Skip if all has gone 'belly-up' */
		case DEFTYPES_STATE_ERROR:
		default:
			return;

		case DEFTYPES_STATE_START:
		{
			if(strcmp(name,"MXFTypes") != 0)
			{
				error("Types definitions file does not start with tag <MXFTypes>\n");
				State->State = DEFTYPES_STATE_ERROR;
				return;
			}

			State->State = DEFTYPES_STATE_STARTED;
			break;
		}

		case DEFTYPES_STATE_STARTED:
		{
			if(strcmp(name, "Basic") == 0)
			{
				State->State = DEFTYPES_STATE_BASIC;
			}
			else if(strcmp(name, "Interpretation") == 0)
			{
				State->State = DEFTYPES_STATE_INTERPRETATION;
			}
			else if(strcmp(name, "Multiple") == 0)
			{
				State->State = DEFTYPES_STATE_MULTIPLE;
			}
			else if(strcmp(name, "Compound") == 0)
			{
				State->State = DEFTYPES_STATE_COMPOUND;
			}
			else
			{
				error("Unexpected types definitions section tag <%s>\n", name);
				State->State = DEFTYPES_STATE_ERROR;
			}
			break;
		}

		case DEFTYPES_STATE_END:
		{
			error("Unexpected types definition tag <%s> after final end tag\n", name);
			State->State = DEFTYPES_STATE_ERROR;
			break;
		}

		case DEFTYPES_STATE_BASIC:
		{
			const char *Detail = "";
			int Size = 1;
			bool Endian = false;
			
			/* Process attributes */
			if(attrs != NULL)
			{
				this_attr = 0;
				while(attrs[this_attr])
				{
					char const *attr = attrs[this_attr++];
					char const *val = attrs[this_attr++];
					
					if(strcmp(attr, "detail") == 0)
					{
						Detail = val;
					}
					else if(strcmp(attr, "size") == 0)
					{
						Size = atoi(val);
					}
					else if(strcmp(attr, "endian") == 0)
					{
						if(strcasecmp(val,"yes") == 0) Endian = true;
					}
					else if(strcmp(attr, "ref") == 0)
					{
						// Ignore
					}
					else
					{
						error("Unexpected attribute \"%s\" in basic type \"%s\"\n", attr, name);
					}
				}
			}

			MDTypePtr Ptr = MDType::AddBasic(name, Size);
			if(Endian) Ptr->SetEndian(true);

			MDTraitsPtr Traits = TraitsMap[name];
			if(!Traits) Traits = TraitsMap["Default-Basic"];

			if(Traits) Ptr->SetTraits(Traits);

			break;
		}

		case DEFTYPES_STATE_INTERPRETATION:
		{
			const char *Detail = "";
			const char *Base = "";
			int Size = 0;
			
			/* Process attributes */
			if(attrs != NULL)
			{
				this_attr = 0;
				while(attrs[this_attr])
				{
					char const *attr = attrs[this_attr++];
					char const *val = attrs[this_attr++];
					
					if(strcmp(attr, "detail") == 0)
					{
						Detail = val;
					}
					else if(strcmp(attr, "base") == 0)
					{
						Base = val;
					}
					else if(strcmp(attr, "size") == 0)
					{
						Size = atoi(val);
					}
					else if(strcmp(attr, "ref") == 0)
					{
						// Ignore
					}
					else
					{
						error("Unexpected attribute \"%s\" in basic type \"%s\"\n", attr, name);
					}
				}
			}

			MDTypePtr BaseType = MDType::Find(Base);
			if(!BaseType)
			{
				error("Type \"%s\" specifies unknown base type \"%s\"\n", name, Base);
			}
			else
			{
				MDTypePtr Ptr = MDType::AddInterpretation(name, BaseType, Size);

				MDTraitsPtr Traits = TraitsMap[name];

				// If we don't have specific traits for this type
				// it will inherit the base type's traits
				if(Traits) Ptr->SetTraits(Traits);
			}

			break;
		}

		case DEFTYPES_STATE_MULTIPLE:
		{
			const char *Detail = "";
			const char *Base = "";
			MDArrayClass Class = ARRAYARRAY;
			int Size = 0;
			
			/* Process attributes */
			if(attrs != NULL)
			{
				this_attr = 0;
				while(attrs[this_attr])
				{
					char const *attr = attrs[this_attr++];
					char const *val = attrs[this_attr++];
					
					if(strcmp(attr, "detail") == 0)
					{
						Detail = val;
					}
					else if(strcmp(attr, "base") == 0)
					{
						Base = val;
					}
					else if(strcmp(attr, "size") == 0)
					{
						Size = atoi(val);
					}
					else if(strcmp(attr, "type") == 0)
					{
						if(strcasecmp(val, "Batch") == 0) Class = ARRAYBATCH;
					}
					else if(strcmp(attr, "ref") == 0)
					{
						// Ignore
					}
					else
					{
						error("Unexpected attribute \"%s\" in basic type \"%s\"\n", attr, name);
					}
				}
			}

			MDTypePtr BaseType = MDType::Find(Base);
			if(!BaseType)
			{
				error("Type \"%s\" specifies unknown base type \"%s\"\n", name, Base);
			}
			else
			{
				MDTypePtr Ptr = MDType::AddArray(name, BaseType, Size);
				if(Class == ARRAYBATCH) Ptr->SetArrayClass(ARRAYBATCH);

				MDTraitsPtr Traits = TraitsMap[name];
				if(!Traits) Traits = TraitsMap["Default-Array"];
				if(Traits) Ptr->SetTraits(Traits);
			}

			break;
		}

		case DEFTYPES_STATE_COMPOUND:
		{
			const char *Detail = "";
			
			/* Process attributes */
			if(attrs != NULL)
			{
				this_attr = 0;
				while(attrs[this_attr])
				{
					char const *attr = attrs[this_attr++];
					char const *val = attrs[this_attr++];
					
					if(strcmp(attr, "detail") == 0)
					{
						Detail = val;
					}
					else if(strcmp(attr, "ref") == 0)
					{
						// Ignore
					}
					else
					{
						error("Unexpected attribute \"%s\" in compound type \"%s\"\n", attr, name);
					}
				}
			}

			MDTypePtr Ptr = MDType::AddCompound(name);

			MDTraitsPtr Traits = TraitsMap[name];
			if(!Traits) Traits = TraitsMap["Default-Compound"];

			if(Traits) Ptr->SetTraits(Traits);

			State->State = DEFTYPES_STATE_COMPOUNDITEMS;

			State->CurrentCompound = Ptr;
			strncpy(State->CompoundName,name,64);

			break;
		}

		case DEFTYPES_STATE_COMPOUNDITEMS:
		{
			const char *Detail = "";
			const char *Type = "";
			int Size = 0;
			
			/* Process attributes */
			if(attrs != NULL)
			{
				this_attr = 0;
				while(attrs[this_attr])
				{
					char const *attr = attrs[this_attr++];
					char const *val = attrs[this_attr++];
					
					if(strcmp(attr, "detail") == 0)
					{
						Detail = val;
					}
					else if(strcmp(attr, "type") == 0)
					{
						Type = val;
					}
					else if(strcmp(attr, "size") == 0)
					{
						Size = atoi(val);
					}
					else if(strcmp(attr, "ref") == 0)
					{
						// Ignore
					}
					else
					{
						error("Unexpected attribute \"%s\" in compound item \"%s\"\n", attr, name);
					}
				}
			}

			MDTypePtr SubType = MDType::Find(Type);
			if(!SubType)
			{
				error("Compound Item \"%s\" specifies unknown type \"%s\"\n", name, Type);
			}
			else
			{
				// Add reference to sub-item type
				State->CurrentCompound->insert(MDType::value_type(std::string(name),SubType));
				State->CurrentCompound->ChildOrder.push_back(std::string(name));
			}

			break;
		}
	}

}


//! SAX callback - Deal with end tag of an element
extern void DefTypes_endElement(void *user_data, const char *name)
{
	DefTypesState *State = (DefTypesState*)user_data;

	/* Skip if all has gone 'belly-up' */
	if(State->State == DEFTYPES_STATE_ERROR) return;

	if(State->State == DEFTYPES_STATE_STARTED)
	{
		State->State = DEFTYPES_STATE_END;
		return;
	}

	if(State->State == DEFTYPES_STATE_BASIC)
	{
		if(strcmp(name,"Basic") == 0) State->State = DEFTYPES_STATE_STARTED;
	}
	else if(State->State == DEFTYPES_STATE_INTERPRETATION)
	{
		if(strcmp(name,"Interpretation") == 0) State->State = DEFTYPES_STATE_STARTED;
	}
	else if(State->State == DEFTYPES_STATE_MULTIPLE)
	{
		if(strcmp(name,"Multiple") == 0) State->State = DEFTYPES_STATE_STARTED;
	}
	else if(State->State == DEFTYPES_STATE_COMPOUND)
	{
		if(strcmp(name,"Compound") == 0) State->State = DEFTYPES_STATE_STARTED;
	}
	else if(State->State == DEFTYPES_STATE_COMPOUNDITEMS)
	{
		if(State->CompoundName && strcmp(name,State->CompoundName) == 0)
		{
			State->State = DEFTYPES_STATE_COMPOUND;
			State->CurrentCompound = NULL;
			State->CompoundName[0] = '\0';
		}
	}
}


//! SAX callback - Handle warnings during SAX parsing
extern void DefTypes_warning(void *user_data, const char *msg, ...)
{
    char Buffer[10240];			// DRAGONS: Could burst!!
	va_list args;

    va_start(args, msg);
	vsprintf(Buffer, msg, args);
	warning("XML WARNING: %s\n",Buffer);
    va_end(args);
}

//! SAX callback - Handle errors during SAX parsing
extern void DefTypes_error(void *user_data, const char *msg, ...)
{
    char Buffer[10240];			// DRAGONS: Could burst!!
	va_list args;

    va_start(args, msg);
	vsprintf(Buffer, msg, args);
	error("XML ERROR: %s\n",Buffer);
    va_end(args);
}

//! SAX callback - Handle fatal errors during SAX parsing
extern void DefTypes_fatalError(void *user_data, const char *msg, ...)
{
    char Buffer[10240];			// DRAGONS: Could burst!!
	va_list args;

    va_start(args, msg);
	vsprintf(Buffer, msg, args);
	error("XML FATAL ERROR: %s\n",Buffer);
    va_end(args);
}


