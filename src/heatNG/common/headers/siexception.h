// siexception header file
#pragma once

#include <stdlib.h>

#include <exception>
#include <string>

#pragma warning(disable: 4996)
namespace
{
    class siexception
    {
        std::wstring exceptionString;
    public:
        siexception(std::wstring errorString, unsigned long ulErrorInfo, unsigned long ulErrorCode)
        {
            wchar_t ec[10];
            _ltow(ulErrorInfo, ec, 10); errorString += (std::wstring(L": (")  + std::wstring(ec));
            _ltow(ulErrorCode, ec, 10); errorString += (std::wstring(L"): (") + std::wstring(ec) + std::wstring(L")"));
            exceptionString = errorString;
        }

        virtual const std::wstring what() const throw()
        {
            return this->exceptionString;
        }
    };

} // namespace

