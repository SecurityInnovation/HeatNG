// Implementation for the HeatNGIntercept class

#include "HeatNGIntercept.h"

#include "common.h"
#include "sinativeintercept.h"

HeatNGIntercept::HeatNGIntercept(HeatNGFunction* originalFunction,     HeatNGFunction* replacementFunction,        bool isFullyReentrant)
              : m_originalFunction(originalFunction), m_replacementFunction(replacementFunction), m_isFullyReentrant(isFullyReentrant), 
                m_nativeintercept(0)
{
    if (m_originalFunction->getType() != m_replacementFunction->getType())
        throw siexception(std::wstring(L"Original and Replacement functions of different types!"), 0, 0);

    switch(m_originalFunction->getType())
    {
    case NATIVE:
        m_type = NATIVE;
        m_nativeintercept = (void*) new sinativeintercept(((HeatNGNativeFunction*)originalFunction)->getFunction(),
                                                         ((HeatNGNativeFunction*)originalFunction)->getModule(),
                                                         ((HeatNGNativeFunction*)replacementFunction)->getFunction(),
                                                         ((HeatNGNativeFunction*)replacementFunction)->getModule(), isFullyReentrant);
        break;
    default:
        throw siexception(std::wstring(L"Unrecognized function type!"), 0, 0);
    }
}

const void*        HeatNGIntercept::getInterceptObject() const { return (void*) m_nativeintercept; }
const FunctionType HeatNGIntercept::getType()            const { return m_type;                    }

