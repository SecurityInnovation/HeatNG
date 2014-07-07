// Implementation for the CNode class

#include "CNode.h"

CNode::CNode(char *pszFuncName, char *pszFuncDLL, char *pszHandlerFunc, char *pszHandlerDLL, CNode* pPrev, CNode* pNext):
	m_pPrev(pPrev), m_pNext(pNext)
{
	if (pszFuncName)
	{
		m_pszFuncName = new char[strlen(pszFuncName)+1];
		strcpy(m_pszFuncName, pszFuncName);
	}
	else
		pszFuncName = NULL;

	if (pszFuncDLL)
	{
		m_pszFuncDLL  = new char[strlen(pszFuncDLL)+1];
		strcpy(m_pszFuncDLL, pszFuncDLL);

        unsigned long ulStrLen = (unsigned long) strlen(pszFuncDLL);
		m_pwszFuncDLL = new wchar_t[ulStrLen + 1];
        mbstowcs(m_pwszFuncDLL, pszFuncDLL, ulStrLen); m_pwszFuncDLL[ulStrLen] = 0;
	}
	else
		pszFuncDLL = NULL;

	if (pszHandlerFunc)
	{
		m_pszHandlerFunc = new char[strlen(pszHandlerFunc)+1];
		strcpy(m_pszHandlerFunc, pszHandlerFunc);
	}
	else
		m_pszHandlerFunc = NULL;

	if (pszHandlerDLL)
	{
		m_pszHandlerDLL = new char[strlen(pszHandlerDLL)+1];
		strcpy(m_pszHandlerDLL, pszHandlerDLL);

        unsigned long ulStrLen = (unsigned long) strlen(pszHandlerDLL);
		m_pwszHandlerDLL = new wchar_t[ulStrLen + 1];
        mbstowcs(m_pwszHandlerDLL, pszHandlerDLL, ulStrLen); m_pwszHandlerDLL[ulStrLen] = 0;
	}
	else
		pszHandlerDLL = NULL;

	m_bIsSent = false;
	m_bIsDeleted = false;
}

CNode::~CNode()
{
	if (m_pszFuncName)
		delete[] m_pszFuncName;
	if (m_pszFuncDLL)
		delete[] m_pszFuncDLL;
	if (m_pszHandlerFunc)
		delete[] m_pszHandlerFunc;
	if (m_pszHandlerDLL)
		delete[] m_pszHandlerDLL;
}

void CNode::setNext(CNode* pNode)	{ m_pNext = pNode; }
CNode* CNode::getNext()			{ return m_pNext; }

void CNode::setPrev(CNode* pNode)	{ m_pPrev = pNode; }
CNode* CNode::getPrev()			{ return m_pPrev; }

char*    CNode::getFuncName()			{ return m_pszFuncName; }
char*    CNode::getFuncDLL()			{ return m_pszFuncDLL;  }
wchar_t* CNode::getFuncDLLW()			{ return m_pwszFuncDLL; }

char*    CNode::getHandlerFunc()		{ return m_pszHandlerFunc; }
char*    CNode::getHandlerDLL()		{ return m_pszHandlerDLL;  }
wchar_t* CNode::getHandlerDLLW()       { return m_pwszHandlerDLL; }

void CNode::setSent(bool bSent)	{ m_bIsSent = bSent; }
bool CNode::getSent()				{ return m_bIsSent; }

void CNode::setDeleted(bool bDeleted)	{ m_bIsDeleted = bDeleted; }
bool CNode::getDeleted()				{ return m_bIsDeleted; }
