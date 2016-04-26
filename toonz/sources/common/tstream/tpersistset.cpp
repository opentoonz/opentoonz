

// TnzCore includes
#include "tstream.h"

// tcg includes
#include "tcg/tcg_deleter_types.h"
#include "tcg/tcg_function_types.h"

// STD includes
#include <typeinfo>

#include "tpersistset.h"

//**************************************************************************************
//    TPersistSet  implementation
//**************************************************************************************

PERSIST_IDENTIFIER(TPersistSet, "persistSet")

//------------------------------------------------------------------

TPersistSet::~TPersistSet()
{
	std::for_each(m_objects.begin(), m_objects.end(), tcg::deleter<TPersist>());
}

//------------------------------------------------------------------

void TPersistSet::insert(std::auto_ptr<TPersist> object)
{
	struct locals {
		inline static bool sameType(TPersist *a, TPersist *b) { return (typeid(*a) == typeid(*b)); }
	};

	// Remove any object with the same type id
	std::vector<TPersist *>::iterator pt = std::remove_if(
		m_objects.begin(), m_objects.end(), tcg::bind1st(&locals::sameType, object.get()));

	std::for_each(pt, m_objects.end(), tcg::deleter<TPersist>());
	m_objects.erase(pt, m_objects.end());

	// Push back the supplied object
	m_objects.push_back(object.release());
}

//------------------------------------------------------------------

void TPersistSet::saveData(TOStream &os)
{
	std::vector<TPersist *>::iterator pt, pEnd = m_objects.end();
	for (pt = m_objects.begin(); pt != pEnd; ++pt)
		os << *pt;
}

//------------------------------------------------------------------

void TPersistSet::loadData(TIStream &is)
{
	while (!is.eos()) {
		TPersist *object = 0;
		is >> object;

		m_objects.push_back(object);
	}
}
