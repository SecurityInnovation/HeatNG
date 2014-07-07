// implementation for the CIntercepts class
#include "CIntercepts.h"

CNode* CIntercepts::findNode(char* pszFuncName, char* pszFuncDLL)
{
	CNode* pNode = m_pFirst;
	while (pNode)
	{
		if ((!strcmp(pszFuncName, pNode->getFuncName())) && (!strcmp(pszFuncDLL, pNode->getFuncDLL())))
			return pNode;
		pNode = pNode->getNext();
	}
	return NULL;
}


CIntercepts::CIntercepts()
{
	m_pFirst = NULL;
}

void CIntercepts::removeAllIntercepts()
{
	while (m_pFirst)
	{
		CNode* pNext = m_pFirst->getNext();
		delete m_pFirst;
		m_pFirst = pNext;
	}
}

CIntercepts::~CIntercepts()
{
	removeAllIntercepts();
}

CNode* CIntercepts::addIntercept(char *pszFuncName, char *pszFuncDLL, char *pszHandlerFunc, char *pszHandlerDLL)
{
	if (findNode(pszFuncName, pszFuncDLL))
		return NULL;

	CNode* pNewNode = new CNode(pszFuncName, pszFuncDLL, pszHandlerFunc, pszHandlerDLL, NULL, m_pFirst);
	if (m_pFirst)
		m_pFirst->setPrev(pNewNode);
	m_pFirst = pNewNode;

	return pNewNode;
}

bool CIntercepts::removeIntercept(char *pszFuncName, char *pszFuncDLL)
{
	CNode* pNode = findNode(pszFuncName, pszFuncDLL);
	if (!pNode)
		return false;
	if (pNode->getPrev())
	{
		pNode->getPrev()->setNext(pNode->getNext());
		if (pNode->getNext())
			pNode->getNext()->setPrev(pNode->getPrev());
	}
	else
	{
		if (pNode->getNext())
			pNode->getNext()->setPrev(pNode->getPrev());
		m_pFirst = pNode->getNext();
	}

	delete pNode;

	return true;
}

CNode* CIntercepts::getIntercept(char *pszFuncName, char *pszFuncDLL)
{
	return findNode(pszFuncName, pszFuncDLL);
}

CNode* CIntercepts::getInterceptList()
{
	return m_pFirst;
}

void CIntercepts::markAllUnsent()
{
	CNode *pNode = m_pFirst;
	while (pNode)
	{
		if (pNode->getDeleted())
		{
			// If marked as deleted, go ahead and delete it
			CNode *pNextNode = pNode->getNext();
			if (pNode->getPrev())
			{
				pNode->getPrev()->setNext(pNode->getNext());
				if (pNode->getNext())
					pNode->getNext()->setPrev(pNode->getPrev());
			}
			else
			{
				if (pNode->getNext())
					pNode->getNext()->setPrev(pNode->getPrev());
				m_pFirst = pNode->getNext();
			}
			pNode = pNextNode;
			continue;
		}
		pNode->setSent(false);
		pNode = pNode->getNext();
	}
}
