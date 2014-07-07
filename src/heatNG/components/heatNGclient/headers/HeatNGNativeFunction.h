#pragma once

#include "HeatNGFunction.h"



class HeatNGNativeFunction : public HeatNGFunction
{
    std::string  m_function;
    std::wstring m_module;

public:
    HeatNGNativeFunction(const std::string &function, const std::wstring &module) : m_function(function), m_module(module) { m_type = NATIVE; }
    FunctionType getType() { return this->m_type; }

    const std::string getFunction() const { return static_cast<const std::string >(m_function); }
    const std::wstring getModule()  const { return static_cast<const std::wstring>(m_module);   }
};

