#pragma once

#include <string>

enum FunctionType {NATIVE, DOTNET, JAVA};

class HeatNGFunction
{
protected:
    FunctionType m_type;
public:
    // this function doesn't have to be pure virtual, but we're forcing this class to become abstract
    virtual FunctionType getType() = 0;
};

