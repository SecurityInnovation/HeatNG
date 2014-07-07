#pragma once

#include "HeatNGFunction.h"
#include "HeatNGNativeFunction.h"



class HeatNGIntercept
{
    FunctionType    m_type;

    HeatNGFunction *m_originalFunction;
    HeatNGFunction *m_replacementFunction;

    bool            m_isFullyReentrant;


    void            *m_nativeintercept;
public:
    HeatNGIntercept(HeatNGFunction* originalFunction, HeatNGFunction* replacementFunction, bool isFullyReentrant = false);
    const void*        getInterceptObject() const;
    const FunctionType getType()            const;
};

