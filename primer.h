/*! \file	primer.h
 *	\brief	Definition of Primer class
 *
 *			The Primer class holds data about the mapping between local
 *          tags in a partition and the UL that gives access to the full
 *			definition
 */
/*
 *	Copyright (c) 2002, Matt Beard
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
#ifndef MXFLIB__PRIMER_H
#define MXFLIB__PRIMER_H

#include "mdtype.h"


#include <map>

namespace mxflib
{
	//! Holds local tag to metadata definition UL mapping
	class Primer : public std::map<Tag, UL>
	{
	public:
	};

	//! A smart pointer to a Primer object
	typedef SmartPtr<Primer> PrimerPtr;
}


#endif MXFLIB__PRIMER_H

