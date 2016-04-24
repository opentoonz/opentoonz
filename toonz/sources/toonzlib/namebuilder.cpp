

#include "toonz/namebuilder.h"
#include "tconvert.h"

//-------------------------------------------------------------------

NameBuilder *NameBuilder::getBuilder(std::wstring levelName)
{
	if (levelName == L"")
		return new NameCreator();
	else
		return new NameModifier(levelName);
}

//-------------------------------------------------------------------

std::wstring NameCreator::getNext()
{
	if (m_s.empty()) {
		m_s.push_back(0);
		return L"A";
	}
	int i = 0;
	int n = m_s.size();
	while (i < n) {
		m_s[i]++;
		if (m_s[i] <= 'Z' - 'A')
			break;
		m_s[i] = 0;
		i++;
	}
	if (i >= n) {
		n++;
		m_s.push_back(0);
	}
	std::wstring s;
	for (i = n - 1; i >= 0; i--)
		s.append(1, (wchar_t)(L'A' + m_s[i]));
	return s;
}

//-------------------------------------------------------------------

NameModifier::NameModifier(std::wstring name)
	: m_nameBase(name), m_index(0)
{
	int j = name.find_last_not_of(L"0123456789");
	if (j != (int)std::wstring::npos &&
		j + 1 < (int)name.length() &&
		name[j] == '_') {
		m_index = toInt(name.substr(j + 1));
		m_nameBase = name.substr(0, j);
	}
}

//-------------------------------------------------------------------

std::wstring NameModifier::getNext()
{
	int index = m_index++;
	if (index < 1)
		return m_nameBase;
	else
		return m_nameBase + L"_" + toWideString(index);
}
